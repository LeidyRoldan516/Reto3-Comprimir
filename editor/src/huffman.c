#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../include/huffman.h"
#include "../include/io.h"

// Nodo del árbol de Huffman
typedef struct Node {
    uint64_t freq;           // Frecuencia del byte (o suma en nodos internos)
    unsigned char byte;      // Byte representado (solo en hojas)
    struct Node *left, *right; // Subárboles
    int is_leaf;             // 1 si es hoja, 0 si es interno
} Node;

// Crea un nodo hoja con byte b y frecuencia f
static Node *node_create(unsigned char b, uint64_t f) {
    Node *n = malloc(sizeof(Node));
    if (!n) return NULL;
    n->freq = f;
    n->byte = b;
    n->left = n->right = NULL;
    n->is_leaf = 1;
    return n;
}

// Crea un nodo interno que une dos nodos
static Node *node_join(Node *a, Node *b) {
    Node *n = malloc(sizeof(Node));
    if (!n) return NULL;
    n->freq = a->freq + b->freq;
    n->left = a;
    n->right = b;
    n->is_leaf = 0;
    return n;
}

// Libera recursivamente el árbol
static void free_tree(Node *n) {
    if (!n) return;
    free_tree(n->left);
    free_tree(n->right);
    free(n);
}

// Construye tabla de códigos Huffman (0 y 1 en strings) a partir del árbol
static void build_codes(Node *root, char **codes, char *buf, int depth) {
    if (!root) return;
    if (root->is_leaf) {
        buf[depth] = '\0';
        codes[root->byte] = strdup(buf);
        return;
    }
    buf[depth] = '0';
    build_codes(root->left, codes, buf, depth+1);
    buf[depth] = '1';
    build_codes(root->right, codes, buf, depth+1);
}

// Codifica (comprime) archivo entrada: lee, computa frecuencias, construye árbol,
// codifica bits, y escribe en bloques alineados.
int huffman_encode_file(const char *in_path, const char *out_path) {
    FILE *fin = fopen(in_path, "rb");
    if (!fin) return 2;
    if (fseek(fin, 0, SEEK_END) != 0) { fclose(fin); return 3; }
    long fsize = ftell(fin);
    if (fsize < 0) { fclose(fin); return 3; }
    rewind(fin);

    // Lee archivo en memoria
    unsigned char *data = malloc(fsize>0 ? (size_t)fsize : 1);
    if (!data) { fclose(fin); return 4; }
    size_t read = fread(data, 1, (size_t)fsize, fin);
    fclose(fin);

    // Computa frecuencias de cada byte
    uint64_t freq[256] = {0};
    for (size_t i = 0; i < read; ++i) freq[data[i]]++;

    // Construye nodos iniciales (hojas)
    Node *nodes[512];
    int ncount = 0;
    for (int i = 0; i < 256; ++i) {
        if (freq[i] > 0) nodes[ncount++] = node_create((unsigned char)i, freq[i]);
    }
    
    // Archivo vacío: escribe header con tamaño 0
    if (ncount == 0) {
        FILE *fout = fopen(out_path, "wb");
        if (!fout) { free(data); return 5; }
        uint64_t orig = 0;
        fwrite(&orig, sizeof(orig), 1, fout);
        uint64_t zeros[256] = {0};
        fwrite(zeros, sizeof(zeros), 1, fout);
        fclose(fout);
        free(data);
        return 0;
    }

    // Si solo hay un símbolo, duplicamos para crear árbol válido
    if (ncount == 1) {
        Node *dup = node_create(nodes[0]->byte, nodes[0]->freq);
        nodes[ncount++] = dup;
    }

    // Construye árbol Huffman combinando los dos nodos con menor frecuencia
    while (ncount > 1) {
        int i1 = -1, i2 = -1;
        for (int i = 0; i < ncount; ++i) {
            if (i1 == -1 || nodes[i]->freq < nodes[i1]->freq) { i2 = i1; i1 = i; }
            else if (i2 == -1 || nodes[i]->freq < nodes[i2]->freq) { i2 = i; }
        }
        if (i2 == -1) break;
        Node *a = nodes[i1];
        Node *b = nodes[i2];
        if (i1 > i2) { int t=i1;i1=i2;i2=t; }
        Node *newn = node_join(a,b);
        // Remueve i2 luego i1
        if (i2 < ncount-1) nodes[i2] = nodes[ncount-1];
        ncount--;
        if (i1 < ncount-1) nodes[i1] = nodes[ncount-1];
        nodes[--ncount] = newn;
        ncount++;
    }

    Node *root = nodes[0];

    // Construye tabla de códigos
    char *codes[256];
    for (int i = 0; i < 256; ++i) codes[i] = NULL;
    char buf[512];
    build_codes(root, codes, buf, 0);

    // Codifica datos a stream de bits
    size_t out_cap = (read > 0) ? (read / 2 + 1024) : 1024;
    unsigned char *outbuf = malloc(out_cap);
    if (!outbuf) { free_tree(root); free(data); return 6; }
    size_t out_len = 0;
    unsigned char curbyte = 0;
    int bitpos = 0; // Próximo bit a escribir (0..7)

    // Escribe códigos en bitstream
    for (size_t i = 0; i < read; ++i) {
        const char *c = codes[data[i]];
        for (size_t j = 0; c && c[j]; ++j) {
            if (c[j] == '1') curbyte |= (1 << (7 - bitpos));
            bitpos++;
            if (bitpos == 8) {
                if (out_len >= out_cap) { out_cap *= 2; outbuf = realloc(outbuf, out_cap); }
                outbuf[out_len++] = curbyte;
                curbyte = 0; bitpos = 0;
            }
        }
    }
    // Bits finales no alineados
    if (bitpos > 0) {
        if (out_len >= out_cap) { out_cap += 1; outbuf = realloc(outbuf, out_cap); }
        outbuf[out_len++] = curbyte;
    }

    // Prepara buffer: [tamaño_original] + [tabla_frecuencias] + [datos_comprimidos]
    uint64_t orig_size = (uint64_t)read;
    size_t header_size = sizeof(orig_size) + sizeof(freq);
    size_t total_size = header_size + out_len;
    unsigned char *full = malloc(total_size);
    if (!full) { free_tree(root); free(data); free(outbuf); return 7; }
    memcpy(full, &orig_size, sizeof(orig_size));
    memcpy(full + sizeof(orig_size), freq, sizeof(freq));
    if (out_len > 0) memcpy(full + header_size, outbuf, out_len);

    // Escribe usando I/O alineado (bloques de 4096 bytes para reducir syscalls)
    reset_write_counter();
    if (write_file_aligned(out_path, full, total_size, 4096) != 0) {
        free(full); free_tree(root); free(data); free(outbuf); return 8;
    }
    unsigned long wc = get_write_counter();
    fprintf(stderr, "ENCODE_llamadas_write=%lu\n", wc);
    free(full);

    // Limpieza
    for (int i = 0; i < 256; ++i) if (codes[i]) free(codes[i]);
    free_tree(root);
    free(data);
    free(outbuf);
    return 0;
}

// Decodifica (descomprime) archivo codificado: lee header, reconstruye árbol,
// decodifica bitstream recorriendo árbol hasta hojas.
int huffman_decode_file(const char *in_path, const char *out_path) {
    FILE *fin = fopen(in_path, "rb");
    if (!fin) return 2;
    uint64_t orig_size = 0;
    if (fread(&orig_size, sizeof(orig_size), 1, fin) != 1) { fclose(fin); return 3; }
    uint64_t freq[256];
    if (fread(freq, sizeof(freq), 1, fin) != 1) { fclose(fin); return 4; }

    // Reconstruye nodos del árbol
    Node *nodes[512];
    int ncount = 0;
    for (int i = 0; i < 256; ++i) if (freq[i] > 0) nodes[ncount++] = node_create((unsigned char)i, freq[i]);
    
    // Archivo vacío: crea salida vacía
    if (ncount == 0) {
        FILE *fout = fopen(out_path, "wb");
        if (!fout) { fclose(fin); return 5; }
        fclose(fout);
        fclose(fin);
        return 0;
    }
    
    if (ncount == 1) {
        Node *dup = node_create(nodes[0]->byte, nodes[0]->freq);
        nodes[ncount++] = dup;
    }
    
    // Reconstruye árbol combinando nodos
    while (ncount > 1) {
        int i1 = -1, i2 = -1;
        for (int i = 0; i < ncount; ++i) {
            if (i1 == -1 || nodes[i]->freq < nodes[i1]->freq) { i2 = i1; i1 = i; }
            else if (i2 == -1 || nodes[i]->freq < nodes[i2]->freq) { i2 = i; }
        }
        if (i2 == -1) break;
        Node *a = nodes[i1]; Node *b = nodes[i2];
        if (i1 > i2) { int t=i1;i1=i2;i2=t; }
        Node *newn = node_join(a,b);
        if (i2 < ncount-1) nodes[i2] = nodes[ncount-1];
        ncount--;
        if (i1 < ncount-1) nodes[i1] = nodes[ncount-1];
        nodes[--ncount] = newn;
        ncount++;
    }
    Node *root = nodes[0];

    // Lee payload comprimido
    if (fseek(fin, 0, SEEK_END) != 0) { fclose(fin); free_tree(root); return 6; }
    long endpos = ftell(fin);
    long data_offset = sizeof(orig_size) + sizeof(freq);
    long comp_len = endpos - data_offset;
    if (comp_len < 0) comp_len = 0;
    rewind(fin);
    if (fseek(fin, data_offset, SEEK_SET) != 0) { fclose(fin); free_tree(root); return 7; }
    unsigned char *comp = malloc(comp_len>0 ? comp_len : 1);
    if (!comp) { fclose(fin); free_tree(root); return 8; }
    size_t rr = fread(comp, 1, comp_len, fin);
    (void)rr;
    fclose(fin);

    FILE *fout = fopen(out_path, "wb");
    if (!fout) { free(comp); free_tree(root); return 9; }

    // Decodifica: recorre árbol bit a bit hasta encontrar hoja
    Node *cur = root;
    uint64_t written = 0;
    for (long i = 0; i < comp_len && written < orig_size; ++i) {
        unsigned char byte = comp[i];
        for (int b = 0; b < 8 && written < orig_size; ++b) {
            int bit = (byte & (1 << (7 - b))) ? 1 : 0;
            cur = bit ? cur->right : cur->left;
            if (!cur) { fclose(fout); free(comp); free_tree(root); return 10; }
            if (cur->is_leaf) {
                fputc(cur->byte, fout);
                written++;
                cur = root;
            }
        }
    }

    fclose(fout);
    free(comp);
    free_tree(root);
    return 0;
}
