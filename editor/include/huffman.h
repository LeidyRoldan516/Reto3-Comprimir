#ifndef HUFFMAN_H
#define HUFFMAN_H

// Codifica (comprime) un archivo usando Huffman.
// Devuelve 0 en éxito, código de error en caso contrario.
int huffman_encode_file(const char *in_path, const char *out_path);

// Decodifica (descomprime) un archivo previamente codificado con Huffman.
// Devuelve 0 en éxito, código de error en caso contrario.
int huffman_decode_file(const char *in_path, const char *out_path);

#endif
