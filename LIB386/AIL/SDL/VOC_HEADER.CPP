#include "VOC_HEADER.H"

#include <string.h>

#define VOC_HEADER_MAGIC "reative Voice File\x1A"

int ParseVocHeader(const U8 *buffer, S32 sizeBytes, const U8 **data, S32 *payloadSize,
                   S32 *outHz, int *outIs8bit, S32 defaultHz) {
    if (sizeBytes < 26)
        return 0;
    /* The 19-byte signature at offset 1 is the real magic. The first byte is 'C' in a standard
       VOC but varies across LBA voice banks (0x00, and 0x01 in some LBA1 VOX entries), so it is
       not a reliable discriminator; gate on the signature alone. */
    if (memcmp(buffer + 1, VOC_HEADER_MAGIC, 19) != 0)
        return 0;
    *outHz = defaultHz;
    *outIs8bit = 1;
    S32 pos = 26;
    S32 extendedRate = 0;
    while (pos + 4 <= sizeBytes) {
        U8 type = buffer[pos];
        S32 blockSize = (S32)(buffer[pos + 1] | (buffer[pos + 2] << 8) | (buffer[pos + 3] << 16));
        if (type == 0x00)
            break;
        if (pos + 4 + blockSize > sizeBytes)
            break;
        if (type == 0x08 && blockSize >= 4) {
            S32 freqDiv = (S32)(buffer[pos + 4] | (buffer[pos + 5] << 8));
            int ch = (int)(buffer[pos + 7]) + 1;
            if (ch < 1 || ch > 8)
                ch = 1;
            if (freqDiv != 65535 && ch > 0)
                extendedRate = 256000000 / ((65536 - freqDiv) * ch);
        }
        if (type == 0x01 && blockSize >= 2) {
            U8 freqDiv = buffer[pos + 4];
            U8 codec = buffer[pos + 5];
            if (extendedRate > 0)
                *outHz = extendedRate;
            else if (freqDiv != 255)
                *outHz = 1000000 / (256 - (S32)freqDiv);
            if (*outHz <= 0 || *outHz > 192000)
                *outHz = defaultHz;
            *data = buffer + pos + 6;
            *payloadSize = blockSize - 2;
            *outIs8bit = (codec == 0) ? 1 : 0;
            return 1;
        }
        pos += 4 + blockSize;
    }
    return 0;
}
