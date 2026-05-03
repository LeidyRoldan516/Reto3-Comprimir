#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "../include/editor.h"
#include "../include/huffman.h"

// Editor simple en memoria por líneas. Comandos en stdin:
//  p           - imprime primeras 100 líneas
//  w <ruta>    - guarda buffer a ruta (si termina con .huff, comprime)
//  q           - sale

static char **lines = NULL;
static size_t nlines = 0;

// Añade una línea al buffer interno
static int add_line(const char *s) {
    char *c = strdup(s);
    if (!c) return -1;
    char *nl = strchr(c, '\n'); if (nl) *nl = '\0';
    char **tmp = realloc(lines, sizeof(char*) * (nlines + 1));
    if (!tmp) { free(c); return -1; }
    lines = tmp;
    lines[nlines++] = c;
    return 0;
}

// Libera todas las líneas en memoria
static void free_lines(void) {
    for (size_t i = 0; i < nlines; ++i) free(lines[i]);
    free(lines); lines = NULL; nlines = 0;
}

// Carga archivo de texto plano en memoria por líneas
static int load_plain(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) add_line(buf);
    fclose(f);
    return 0;
}

// Carga archivo (detecta .huff y lo descomprime a temporal primero)
static int load_auto(const char *path) {
    size_t L = strlen(path);
    if (L >= 5 && strcmp(path + L - 5, ".huff") == 0) {
        // Descomprime a archivo temporal
        char tmp[] = "/tmp/editor_tmp_XXXXXX";
        int fd = mkstemp(tmp);
        if (fd < 0) return -1;
        if (close(fd) != 0) { remove(tmp); return -1; }
        if (huffman_decode_file(path, tmp) != 0) { remove(tmp); return -1; }
        int r = load_plain(tmp);
        remove(tmp);
        return r;
    } else {
        return load_plain(path);
    }
}

// Guarda buffer a archivo de texto plano
static int save_plain(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    for (size_t i = 0; i < nlines; ++i) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
    return 0;
}

// Guarda archivo (detecta .huff y comprime si es necesario)
static int save_auto(const char *path) {
    size_t L = strlen(path);
    if (L >= 5 && strcmp(path + L - 5, ".huff") == 0) {
        // Escribe a temporal en texto plano, luego comprime
        char tmp[] = "/tmp/editor_tmp_XXXXXX";
        int fd = mkstemp(tmp);
        if (fd < 0) return -1;
        close(fd);
        if (save_plain(tmp) != 0) { remove(tmp); return -1; }
        int r = huffman_encode_file(tmp, path);
        remove(tmp);
        return r;
    } else {
        return save_plain(path);
    }
}

// Ejecuta el ciclo interactivo del editor
int editor_run(const char *path) {
    if (load_auto(path) != 0) {
        // Si no carga, inicia vacío
        fprintf(stderr, "(editor) no se pudo cargar %s, iniciando vacío\n", path);
    }
    printf("Editor simple: comandos: p (imprimir), w <ruta> (guardar), q (salir)\n");
    char cmd[4096];
    while (1) {
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        // Recorta espacios iniciales
        char *s = cmd; while (isspace((unsigned char)*s)) s++;
        if (*s == '\0') continue;
        if (s[0] == 'p') {
            size_t upto = nlines < 100 ? nlines : 100;
            for (size_t i = 0; i < upto; ++i) printf("%4zu: %s\n", i+1, lines[i]);
            continue;
        }
        if (s[0] == 'q') break;
        if (s[0] == 'w') {
            // Extrae ruta de comando
            char *arg = s+1;
            while (isspace((unsigned char)*arg)) arg++;
            if (*arg == '\0') { printf("uso: w <ruta>\n"); continue; }
            // Quita salto de línea
            char *nl = strchr(arg, '\n'); if (nl) *nl = '\0';
            printf("guardando -> %s\n", arg);
            if (save_auto(arg) == 0) printf("guardado ok\n"); else printf("error al guardar\n");
            continue;
        }
        printf("comando no reconocido\n");
    }
    free_lines();
    return 0;
}
