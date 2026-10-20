#include <OBJECT/AFF_OBJ.H>
#include <SYSTEM/LE.H>

/* Bodies are little-endian on disk and the renderer reads them through typed
   pointers, so a loader calls ObjectBodyFromLE once on each freshly loaded body.
   On a little-endian CPU it does nothing.

   Layout: a 0x60-byte header (S32 Info, S16 SizeHeader, S16 Dummy, then 22 S32:
   the bounding box and a count and offset per section), then the sections.
   Groups, points, normals, face normals, lines and spheres are records of four
   U16. Polygons come in blocks, each a header (U16 TypePoly, U16 NbPoly, U32
   OffNextType) followed by NbPoly records of U16 only, whose size depends on the
   type. Textures are U32 handles. Every offset is checked against the body's size
   so a damaged entry cannot make this write outside it. */

#define BODY_HEADER_SIZE 0x60

static S32 SectionFits(S32 off, S32 count, S32 recsize, S32 size) {
    return off >= BODY_HEADER_SIZE && count >= 0 && count <= (size - off) / recsize;
}

static S32 PolyRecordSize(U16 type) {
    if (type & (1 << 14)) // environment mapped
        return 16;
    if ((type & 0xFF) <= 7) // flat, gouraud, dither and the like
        return 12;
    return (type & (1 << 15)) ? 32 : 24; // textured quad or triangle
}

void ObjectBodyFromLE(void *body, S32 size) {
    if (!LBA_BIG_ENDIAN || !body || size < BODY_HEADER_SIZE)
        return;

    U8 *b = (U8 *)body;
    FromLE32InPlace(b, 1);
    FromLE16InPlace(b + 4, 2);
    FromLE32InPlace(b + 8, (BODY_HEADER_SIZE - 8) / 4);

    const T_BODY_HEADER *h = (const T_BODY_HEADER *)body;

    if (SectionFits(h->OffGroupes, h->NbGroupes, 8, size))
        FromLE16InPlace(b + h->OffGroupes, (U32)h->NbGroupes * 4);
    if (SectionFits(h->OffPoints, h->NbPoints, 8, size))
        FromLE16InPlace(b + h->OffPoints, (U32)h->NbPoints * 4);
    if (SectionFits(h->OffNormales, h->NbNormales, 8, size))
        FromLE16InPlace(b + h->OffNormales, (U32)h->NbNormales * 4);
    if (SectionFits(h->OffNormFaces, h->NbNormFaces, 8, size))
        FromLE16InPlace(b + h->OffNormFaces, (U32)h->NbNormFaces * 4);
    if (SectionFits(h->OffLines, h->NbLines, 8, size))
        FromLE16InPlace(b + h->OffLines, (U32)h->NbLines * 4);
    if (SectionFits(h->OffSpheres, h->NbSpheres, 8, size))
        FromLE16InPlace(b + h->OffSpheres, (U32)h->NbSpheres * 4);
    if (SectionFits(h->OffTextures, h->NbTextures, 4, size))
        FromLE32InPlace(b + h->OffTextures, (U32)h->NbTextures);

    // Polygon blocks run from OffPolys up to the lines section
    S32 off = h->OffPolys;
    const S32 end = h->OffLines;
    if (off < BODY_HEADER_SIZE || end > size)
        return;
    while (off + 8 <= end) {
        const U16 type = ReadLE_U16(b + off);
        const S32 nb = ReadLE_U16(b + off + 2);
        FromLE16InPlace(b + off, 2);
        FromLE32InPlace(b + off + 4, 1);
        const S32 bytes = nb * PolyRecordSize(type);
        off += 8;
        if (bytes > end - off)
            return;
        FromLE16InPlace(b + off, (U32)bytes / 2);
        off += bytes;
    }
}
