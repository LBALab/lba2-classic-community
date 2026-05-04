/* Standalone LZSS expand for LBA2 save payloads (same as ExpandLZ MinBloc=2). */
#include <SYSTEM/LZ.H>
#include <SYSTEM/ADELINE_TYPES.H>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr,
                "usage: %s <decompressed_size> [compressed.bin]\n"
                "  Reads compressed bytes from file or stdin.\n"
                "  Writes exactly <decompressed_size> bytes to stdout.\n",
                argv[0] ? argv[0] : "save_decompress");
        return 2;
    }

    char *end = NULL;
    unsigned long ds = strtoul(argv[1], &end, 10);
    if (!argv[1][0] || (end && *end) || ds == 0UL || ds > 0x7FFFFFFFUL) {
        fprintf(stderr, "error: invalid decompressed_size '%s'\n", argv[1]);
        return 2;
    }
    U32 decompSize = (U32)ds;

    unsigned char *src = NULL;
    size_t srcLen = 0;
    FILE *fin = stdin;

    if (argc == 3) {
        fin = fopen(argv[2], "rb");
        if (!fin) {
            perror(argv[2]);
            return 1;
        }
    }

    /* Read entire compressed stream */
    {
        size_t cap = 65536;
        src = (unsigned char *)malloc(cap);
        if (!src) {
            fprintf(stderr, "error: malloc\n");
            return 1;
        }
        for (;;) {
            size_t n = fread(src + srcLen, 1, cap - srcLen, fin);
            srcLen += n;
            if (n == 0)
                break;
            if (srcLen >= cap) {
                size_t ncap = cap * 2;
                unsigned char *p = (unsigned char *)realloc(src, ncap);
                if (!p) {
                    free(src);
                    fprintf(stderr, "error: realloc\n");
                    return 1;
                }
                src = p;
                cap = ncap;
            }
        }
        if (fin != stdin)
            fclose(fin);
    }

    /* Match game buffer slack (LZ.H RECOVER_AREA) for overlapping back-refs */
    size_t dstAlloc = (size_t)decompSize + (size_t)RECOVER_AREA;
    unsigned char *dst = (unsigned char *)malloc(dstAlloc);
    if (!dst) {
        free(src);
        fprintf(stderr, "error: malloc dst\n");
        return 1;
    }
    memset(dst, 0, dstAlloc);

    ExpandLZ(dst, src, decompSize, 2);
    free(src);

    if (fwrite(dst, 1, (size_t)decompSize, stdout) != (size_t)decompSize) {
        perror("stdout");
        free(dst);
        return 1;
    }
    free(dst);
    return 0;
}
