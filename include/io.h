#ifndef IO_H
#define IO_H

#include <stddef.h>

ssize_t write_all(int fd, const void *buf, size_t count);

// Escribe len bytes desde buf en path usando bloques alineados a page_size.
// Devuelve 0 en éxito, -1 en error.
int write_file_aligned(const char *path, const void *buf, size_t len, size_t page_size);

// Escribe len bytes en path usando mmap (ftruncate + mmap + memcpy + msync).
// Devuelve 0 en éxito, -1 en error.
int mmap_write_file(const char *path, const void *buf, size_t len);

// Instrumentación: contador de llamadas a write() realizadas por las funciones
// definidas en este módulo. Útil cuando strace no está disponible.
void reset_write_counter(void);
unsigned long get_write_counter(void);

#endif
