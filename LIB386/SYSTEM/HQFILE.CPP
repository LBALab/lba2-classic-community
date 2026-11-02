/*──────────────────────────────────────────────────────────────────────────*/
#include <SYSTEM/ADELINE.H>
#include <SYSTEM/FILES.H>
#include <SYSTEM/LZ.H>
#include <SYSTEM/HQR.H>
#include <SYSTEM/HQFILE.H>
#include <SYSTEM/LOG.H>

#include <stdio.h>
#include <string.h>

/*──────────────────────────────────────────────────────────────────────────*/
S32 HQF_File;
static COMPRESSED_HEADER HQF_header;

/* TRUE when all n bytes arrived; a short read or an error is FALSE. */
static S32 HQF_ReadAll(S32 handle, void *buffer, U32 n) {
    S32 got = Read(handle, buffer, n);
    return got >= 0 && (U32)got == n;
}

/*──────────────────────────────────────────────────────────────────────────*/
U32 HQF_Init(const char *name, S32 index) {
    S32 nbbloc;
    S32 offset;
    U8 raw[4];

    /* A failed Init leaves no header behind, so HQF_LoadClose never unpacks with the
       sizes of the previous load. */
    memset(&HQF_header, 0, sizeof(HQF_header));

    HQF_File = OpenRead(name);
    if (!HQF_File) {
        printf("Error loading file %s\n", name);
        return 0;
    }

    if (index < 0 || !HQF_ReadAll(HQF_File, raw, 4)) {
        goto error;
    }
    nbbloc = ReadLE_S32(raw);

    if (index >= nbbloc / 4) {
        goto error;
    }

    Seek(HQF_File, index * 4, SEEK_FROM_START);
    if (!HQF_ReadAll(HQF_File, raw, 4)) {
        goto error;
    }
    offset = ReadLE_S32(raw);

    if (offset <= 0) {
        goto error;
    }

    /* The last offset of an HQR is the end of the file, not an entry: the header read
       there comes back short. */
    Seek(HQF_File, offset, SEEK_FROM_START);
    if (!HQF_ReadAll(HQF_File, &HQF_header, sizeof(HQF_header))) {
        Log_Warn("HQR %s: entry %d has no header (offset %d)", name, index, offset);
        goto error;
    }
    HQR_HeaderFromLE(&HQF_header);

    /* HQF_LoadClose reads a compressed entry so that it ends RECOVER_AREA past SizeFile:
       a larger compressed size would start before the caller's buffer. */
    if (HQF_header.CompressMethod != 0 && (U64)HQF_header.CompressedSizeFile > (U64)HQF_header.SizeFile + RECOVER_AREA) {
        Log_Warn("HQR %s: entry %d is damaged (compressed %u, size %u)", name, index,
                 HQF_header.CompressedSizeFile, HQF_header.SizeFile);
        goto error;
    }

    return HQF_header.SizeFile;

error:
    memset(&HQF_header, 0, sizeof(HQF_header));
    HQF_Close();
    return 0;
}

/*──────────────────────────────────────────────────────────────────────────*/
void HQF_Close() {
    if (HQF_File) {
        Close(HQF_File);
        HQF_File = 0;
    }
}

/*──────────────────────────────────────────────────────────────────────────*/
U32 HQF_LoadClose(void *ptr) {
    if (!HQF_File) {
        return 0;
    }

    if (HQF_header.CompressMethod == 0) {
        /* Stored */
        if (!HQF_ReadAll(HQF_File, ptr, HQF_header.SizeFile)) {
            HQF_header.SizeFile = 0;
        }
    } else if (HQF_header.CompressMethod <= 2) {
        void *ptrdecomp;

        ptrdecomp = (void *)((U8 *)ptr + HQF_header.SizeFile - HQF_header.CompressedSizeFile + RECOVER_AREA);

        if (!HQF_ReadAll(HQF_File, ptrdecomp, HQF_header.CompressedSizeFile)) {
            HQF_header.SizeFile = 0;
        } else {
            /* LZSS/LZMIT */
            ExpandLZ(ptr, ptrdecomp, HQF_header.SizeFile, HQF_header.CompressMethod + 1);
        }
    } else {
        HQF_header.SizeFile = 0;
    }

    HQF_Close();
    return HQF_header.SizeFile;
}

/*──────────────────────────────────────────────────────────────────────────*/
S32 HQF_ResSize(const char *name, S32 index) {
    U32 size;

    size = HQF_Init(name, index);
    HQF_Close();

    return size;
}

/*──────────────────────────────────────────────────────────────────────────*/
// retourne le nombre de bloc d'un HQR ( 0 si HQR non trouvé )
S32 HQF_NbRes(char *name) {
    S32 handle;
    S32 nbbloc;
    U8 raw[4];

    handle = OpenRead(name);
    if (!handle) {
        return 0;
    }

    Read(handle, raw, 4);
    nbbloc = ReadLE_S32(raw);
    Close(handle);

    return (nbbloc / 4 - 1);
}

/*──────────────────────────────────────────────────────────────────────────*/
