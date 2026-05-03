# Reto 3 - Editor de Archivos con Optimización de Bus I/O en Entornos Linux/C

## Descripcion del Proyecto

Este proyecto implementa un editor de texto en C que comprime en user-space antes de escribir a disco.
El flujo es transparente para el usuario:

- Si se guarda con extension .huff, el archivo se guarda comprimido.
- Si se abre un .huff, el editor lo descomprime automaticamente.

Objetivo tecnico:

- Reducir volumen de datos a disco.
- Reducir llamadas al kernel (syscalls write).
- Demostrar con evidencia real de strace, time y valgrind.

## Matriz de Diseno del Pipeline I/O

Diagrama de flujo del paso de datos:

    Usuario -> ./bin/editor edit archivo.txt
           -> editor_run()
           -> comando w salida.huff
           -> save_auto()
           -> huffman_encode_file()
           -> construye header + payload comprimido en RAM
           -> write_file_aligned(path, buffer, len, 4096)
           -> write() por bloques de 4KB
           -> archivo .huff en disco

Flujo inverso al abrir .huff:

    ./bin/editor edit archivo.huff
           -> load_auto()
           -> huffman_decode_file()
           -> texto plano en memoria
           -> edicion normal

## Gestion de Memoria en C (struct, padding, ciclo de vida)

El proyecto usa memoria dinamica de forma controlada para el arbol Huffman, los buffers de entrada y las tablas auxiliares. La idea es reservar solo lo necesario durante la ejecucion y liberar todo al finalizar cada operacion.

Estructura principal (nodo Huffman):

- uint64_t freq
- unsigned char byte
- Node *left
- Node *right
- int is_leaf

### Diseño de la estructura:

- Se ordenan los campos para mantener accesos eficientes en CPU.
- Se acepta padding natural del compilador porque evitarlo con packed puede degradar rendimiento y complicar accesos alineados.
- No se usa packed porque el proyecto prioriza velocidad y estabilidad de acceso sobre ahorrar unos pocos bytes por nodo.
- El arbol Huffman es pequeño frente al tamano de entrada, por lo que el costo de padding es marginal.

### Como se evita desperdicio excesivo de memoria:

- Los nodos solo se crean para simbolos realmente presentes en el archivo.
- No se reserva memoria fija masiva para estructuras que puedan ser dinamicas.
- Los buffers se dimensionan segun el archivo real de entrada y el payload generado.

### Evidencia de no fugas:

- Valgrind reporta:
  - in use at exit: 0 bytes in 0 blocks
  - All heap blocks were freed -- no leaks are possible
  - ERROR SUMMARY: 0 errors

### Ciclo de vida de la memoria:

- huffman_encode_file reserva buffer de entrada, nodos del arbol, tabla de codigos y payload temporal.
- Al terminar la escritura, cada buffer se libera con free.
- free_tree recorre el arbol en postorden y libera todos los nodos.
- editor_run libera la lista de lineas al salir del modo edicion.
- huffman_decode_file sigue el mismo esquema: reserva lo necesario para decodificar y libera todo al finalizar.

## Manejo de Texto Enriquecido / Formato Binario

Formato de archivo .huff implementado:

    [uint64_t orig_size]      8 bytes
    [uint64_t freq[256]]      2048 bytes
    [payload comprimido]      variable

Detalles:

- orig_size permite reconstruir exactamente el tamaño original.
- freq[256] permite reconstruir el arbol Huffman en decode.
- payload contiene el stream de bits comprimido.

## Reporte de Profiling (Evidencia de Ingenieria)

### Archivo de prueba

- samples/test_50mb.txt

### Resultados reales capturados

Con strace -c (50MB):

- write: 6665 llamadas
- total syscalls: 6713

Con /usr/bin/time -v (50MB):

- User time (seconds): 0.34
- System time (seconds): 0.23
- Elapsed (wall clock): 0:00.95
- Maximum resident set size (kbytes): 105600

Con valgrind:

- total heap usage: 80 allocs, 80 frees
- no leaks are possible
- ERROR SUMMARY: 0 errors

### Tabla de benchmark (real)

| Metrica | Enfoque clasico (referencia previa) | Enfoque propuesto (real medido) | Impacto |
|---|---:|---:|---:|
| Volumen a disco | 50 MB | aprox. 15-17 MB | -65% a -70% |
| Llamadas write() | alto (escritura ineficiente) | 6665 | reduccion fuerte de interrupciones |
| CPU user mode | bajo | 0.34 s | aumento esperado por compresion |
| CPU sys mode | mas alto por syscall frecuente | 0.23 s | menos sobrecarga de kernel |
| Tiempo total | mayor en enfoque clasico | 0:00.95 | mejora global |

## Comandos de ejecucion 

Compilar:

    cd editor
    make clean
    make

Flujo funcional:

    ./bin/editor encode samples/test.txt /tmp/test.huff
    ./bin/editor decode /tmp/test.huff /tmp/test.dec.txt
    cmp -s samples/test.txt /tmp/test.dec.txt && echo OK

Strace:

    strace -c ./bin/editor encode samples/test_50mb.txt /tmp/test_50mb_strace.huff

Time:

    /usr/bin/time -v ./bin/editor encode samples/test_50mb.txt /tmp/test_50mb_time.huff

Valgrind:

    valgrind --leak-check=full --show-leak-kinds=all ./bin/editor encode samples/test_50mb.txt /tmp/test_50mb_vg.huff



