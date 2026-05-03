#define _GNU_SOURCE
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include "../include/io.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static unsigned long write_counter = 0;

void reset_write_counter(void) { write_counter = 0; }
unsigned long get_write_counter(void) { return write_counter; }

// Escribe todos los bytes de buf en fd, reintentando si hay interrupciones.
ssize_t write_all(int fd, const void *buf, size_t count) {
    const char *p = buf;
    size_t left = count;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        write_counter++;
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        left -= w;
        p += w;
    }
    return count;
}

// Abre archivo, escribe len bytes en bloques alineados a page_size (por defecto 4096).
// Reduce el número de syscalls al escribir en bloques más grandes.
int write_file_aligned(const char *path, const void *buf, size_t len, size_t page_size) {
    if (page_size == 0) page_size = 4096;
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) return -1;

    size_t offset = 0;
    while (offset < len) {
        size_t chunk = len - offset;
        if (chunk > page_size) chunk = page_size;
        ssize_t w = write_all(fd, (const char*)buf + offset, chunk);
        if (w < 0) { close(fd); return -1; }
        offset += (size_t)w;
    }
    close(fd);
    return 0;
}

// Alternativa a write_file_aligned: usa mmap para escribir en bloque completo.
// ftruncate crea archivo del tamaño correcto, mmap mapea memoria, memcpy copia datos, msync sincroniza.
int mmap_write_file(const char *path, const void *buf, size_t len) {
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) return -1;
    if (ftruncate(fd, (off_t)len) != 0) { close(fd); return -1; }
    void *map = mmap(NULL, len, PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { close(fd); return -1; }
    memcpy(map, buf, len);
    if (msync(map, len, MS_SYNC) != 0) { /* intentamos limpiar de todas formas */ }
    munmap(map, len);
    close(fd);
    return 0;
}
