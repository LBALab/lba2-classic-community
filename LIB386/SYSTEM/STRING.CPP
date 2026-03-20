#include <SYSTEM/STRING.H>

#include <SDL3/SDL_stdinc.h>

#include <stdarg.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
char *ade_strcasestr(const char *haystack, const char *needle) {
    // Copied from SDL since Ubuntu does not support the latest releases of SDL
    size_t length = SDL_strlen(needle);
    while (*haystack) {
        if (SDL_strncasecmp(haystack, needle, length) == 0) {
            return (char *)haystack;
        }
        ++haystack;
    }
    return NULL;
}

U8 UnicodeToCp850(U32 codePoint) {
    if (codePoint < 0x80) {
        return (U8)codePoint;
    }

    switch (codePoint) {
    case 0x00A0:
        return 0xFF;
    case 0x00A1:
        return 0xAD;
    case 0x00A2:
        return 0xBD;
    case 0x00A3:
        return 0x9C;
    case 0x00A4:
        return 0xCF;
    case 0x00A5:
        return 0xBE;
    case 0x00A6:
        return 0xDD;
    case 0x00A7:
        return 0xF5;
    case 0x00A8:
        return 0xF9;
    case 0x00A9:
        return 0xB8;
    case 0x00AA:
        return 0xA6;
    case 0x00AB:
        return 0xAE;
    case 0x00AC:
        return 0xAA;
    case 0x00AD:
        return 0xF0;
    case 0x00AE:
        return 0xA9;
    case 0x00AF:
        return 0xEE;
    case 0x00B0:
        return 0xF8;
    case 0x00B1:
        return 0xF1;
    case 0x00B2:
        return 0xFD;
    case 0x00B3:
        return 0xFC;
    case 0x00B4:
        return 0xEF;
    case 0x00B5:
        return 0xE6;
    case 0x00B6:
        return 0xF4;
    case 0x00B7:
        return 0xFA;
    case 0x00B8:
        return 0xF7;
    case 0x00B9:
        return 0xFB;
    case 0x00BA:
        return 0xA7;
    case 0x00BB:
        return 0xAF;
    case 0x00BC:
        return 0xAC;
    case 0x00BD:
        return 0xAB;
    case 0x00BE:
        return 0xF3;
    case 0x00BF:
        return 0xA8;
    case 0x00C0:
        return 0xB7;
    case 0x00C1:
        return 0xB5;
    case 0x00C2:
        return 0xB6;
    case 0x00C3:
        return 0xC7;
    case 0x00C4:
        return 0x8E;
    case 0x00C5:
        return 0x8F;
    case 0x00C6:
        return 0x92;
    case 0x00C7:
        return 0x80;
    case 0x00C8:
        return 0xD4;
    case 0x00C9:
        return 0x90;
    case 0x00CA:
        return 0xD2;
    case 0x00CB:
        return 0xD3;
    case 0x00CC:
        return 0xDE;
    case 0x00CD:
        return 0xD6;
    case 0x00CE:
        return 0xD7;
    case 0x00CF:
        return 0xD8;
    case 0x00D0:
        return 0xD1;
    case 0x00D1:
        return 0xA5;
    case 0x00D2:
        return 0xE3;
    case 0x00D3:
        return 0xE0;
    case 0x00D4:
        return 0xE2;
    case 0x00D5:
        return 0xE5;
    case 0x00D6:
        return 0x99;
    case 0x00D7:
        return 0x9E;
    case 0x00D8:
        return 0x9D;
    case 0x00D9:
        return 0xEB;
    case 0x00DA:
        return 0xE9;
    case 0x00DB:
        return 0xEA;
    case 0x00DC:
        return 0x9A;
    case 0x00DD:
        return 0xED;
    case 0x00DE:
        return 0xE8;
    case 0x00DF:
        return 0xE1;
    case 0x00E0:
        return 0x85;
    case 0x00E1:
        return 0xA0;
    case 0x00E2:
        return 0x83;
    case 0x00E3:
        return 0xC6;
    case 0x00E4:
        return 0x84;
    case 0x00E5:
        return 0x86;
    case 0x00E6:
        return 0x91;
    case 0x00E7:
        return 0x87;
    case 0x00E8:
        return 0x8A;
    case 0x00E9:
        return 0x82;
    case 0x00EA:
        return 0x88;
    case 0x00EB:
        return 0x89;
    case 0x00EC:
        return 0x8D;
    case 0x00ED:
        return 0xA1;
    case 0x00EE:
        return 0x8C;
    case 0x00EF:
        return 0x8B;
    case 0x00F0:
        return 0xD0;
    case 0x00F1:
        return 0xA4;
    case 0x00F2:
        return 0x95;
    case 0x00F3:
        return 0xA2;
    case 0x00F4:
        return 0x93;
    case 0x00F5:
        return 0xE4;
    case 0x00F6:
        return 0x94;
    case 0x00F7:
        return 0xF6;
    case 0x00F8:
        return 0x9B;
    case 0x00F9:
        return 0x97;
    case 0x00FA:
        return 0xA3;
    case 0x00FB:
        return 0x96;
    case 0x00FC:
        return 0x81;
    case 0x00FD:
        return 0xEC;
    case 0x00FE:
        return 0xE7;
    case 0x00FF:
        return 0x98;
    case 0x0131:
        return 0xD5;
    case 0x0192:
        return 0x9F;
    case 0x2018:
    case 0x2019:
        return '\'';
    case 0x201C:
    case 0x201D:
        return '"';
    case 0x2013:
    case 0x2014:
        return '-';
    case 0x2026:
        return '.';
    default:
        return '?';
    }
}

void CopyUtf8ToCp850(char *dst, size_t dstSize, const char *src) {
    const U8 *ptr = (const U8 *)src;
    size_t outIndex = 0;

    if (dstSize == 0) {
        return;
    }

    while (*ptr != '\0' && outIndex + 1 < dstSize) {
        U8 first = *ptr;

        if (first < 0x80) {
            dst[outIndex++] = (char)first;
            ptr++;
            continue;
        }

        if ((first & 0xE0) == 0xC0 && (ptr[1] & 0xC0) == 0x80) {
            U32 codePoint = ((U32)(first & 0x1F) << 6) | (U32)(ptr[1] & 0x3F);
            dst[outIndex++] = (char)UnicodeToCp850(codePoint);
            ptr += 2;
            continue;
        }

        if ((first & 0xF0) == 0xE0 && (ptr[1] & 0xC0) == 0x80 && (ptr[2] & 0xC0) == 0x80) {
            U32 codePoint = ((U32)(first & 0x0F) << 12) |
                            ((U32)(ptr[1] & 0x3F) << 6) |
                            (U32)(ptr[2] & 0x3F);
            dst[outIndex++] = (char)UnicodeToCp850(codePoint);
            ptr += 3;
            continue;
        }

        dst[outIndex++] = (char)first;
        ptr++;
    }

    dst[outIndex] = '\0';
}

void FormatUtf8ToCp850(char *dst, size_t dstSize, const char *fmt, ...) {
    va_list args;

    if (dstSize == 0) {
        return;
    }

    char *utf8Buffer = (char *)SDL_malloc(dstSize);
    if (!utf8Buffer) {
        dst[0] = '\0';
        return;
    }

    va_start(args, fmt);
    vsnprintf(utf8Buffer, dstSize, fmt, args);
    va_end(args);

    CopyUtf8ToCp850(dst, dstSize, utf8Buffer);

    SDL_free(utf8Buffer);
}

// =============================================================================
#ifdef __cplusplus
}
#endif
