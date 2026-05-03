#include <stdio.h>
#include <string.h>
#include "../include/huffman.h"
#include "../include/editor.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Uso:\n  %s encode entrada salida\n  %s decode entrada salida\n  %s edit archivo\n", argv[0], argv[0], argv[0]);
        return 1;
    }
    const char *cmd = argv[1];
    if (strcmp(cmd, "encode") == 0) {
        if (argc < 4) { fprintf(stderr, "encode requiere entrada salida\n"); return 1; }
        return huffman_encode_file(argv[2], argv[3]);
    } else if (strcmp(cmd, "decode") == 0) {
        if (argc < 4) { fprintf(stderr, "decode requiere entrada salida\n"); return 1; }
        return huffman_decode_file(argv[2], argv[3]);
    } else if (strcmp(cmd, "edit") == 0) {
        return editor_run(argv[2]);
    } else {
        fprintf(stderr, "Comando desconocido: %s\n", cmd);
        return 1;
    }
}
