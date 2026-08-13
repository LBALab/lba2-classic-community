//══════════════════════════════════════════════════════════════════════════
#include <SYSTEM/ADELINE.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/FILES.H>
#include <SYSTEM/LOADSAVE.H>
#include <SYSTEM/ITOA.H>
#include <SYSTEM/DEFFILE.H>
#include <SYSTEM/LIMITS.H>

#include <stdlib.h>
#include <string.h>

//══════════════════════════════════════════════════════════════════════════
static char DefString[256];
static S32 LenDefString;
static char *PtrEndIdent;
char *OrgPtrDef;
char *EndPtrDef;

S32 DefFileReadSkipComments = TRUE;

static S32 BufferSize;
static char FileName[ADELINE_MAX_PATH];

// Batch state. While active, FileName is redirected to a ".saving" working temp
// and each write is an ordinary buffered write; the temp is committed once at
// DefFileBufferEndBatch. See the API comment in DEFFILE.H.
static S32 BatchActive = FALSE;
static S32 BatchDirty = FALSE; // a write happened, so the temp exists to commit
static char BatchFinalPath[ADELINE_MAX_PATH];

// Set once a lower-priority layer has been appended to the buffer. The buffer
// then holds more than FileName does, so writing it back would persist those
// layers into the file. Writes refuse while it is set; DefFileBufferInit clears
// it. See the API comment in DEFFILE.H.
static S32 LayersAttached = FALSE;

//══════════════════════════════════════════════════════════════════════════
static inline S32 ReadBufferString(const char *identificateur) {
    char *ptrdef;
    U8 *ptr;
    S32 len;

    DefString[0] = 0;
    LenDefString = 0;

    if (!OrgPtrDef) {
        return FALSE;
    }

    len = strlen(identificateur);

    ptrdef = OrgPtrDef;

    while (ptrdef < EndPtrDef) {
        while (*ptrdef == 32)
            ptrdef++; // skip blanks

        if (!strncasecmp(ptrdef, identificateur, len)) // look for ident
        {
            ptrdef += len;

            while (*ptrdef == 32)
                ptrdef++; // skip blanks

            if ((*ptrdef == ':') OR(*ptrdef == '=')) {
                ptrdef++;

                PtrEndIdent = ptrdef;

                while (*ptrdef == 32)
                    ptrdef++; //skip leading spaces

                ptr = (U8 *)ptrdef;

                while (*ptr >= 32) {
                    if (DefFileReadSkipComments && (*ptr == ';')) {
                        ptr--;
                        break;
                    }
                    ptr++; // search EOL
                }

                while (*ptr <= 32)
                    ptr--; // skip ending spaces

                LenDefString = (char *)ptr - ptrdef + 1;

                if (LenDefString < 0)
                    LenDefString = 0; // security
                if (LenDefString > 255)
                    LenDefString = 255;

                memcpy(DefString, ptrdef, LenDefString); // copy line into buffer
                DefString[LenDefString] = 0;

                return TRUE;
            }
        }

        while ((U8)*ptrdef >= 32)
            ptrdef++; // next line
        while ((U8)*ptrdef < 32 && ptrdef < EndPtrDef)
            ptrdef++;
    }
    return FALSE;
}

//══════════════════════════════════════════════════════════════════════════
/* Close the read window: trim trailing blanks, then lay the terminator the
   reader stops on. ReadBufferString scans while ptr < EndPtrDef and steps over
   runs of sub-space bytes, so the 0/32/0 past the end is what stops it walking
   into whatever the buffer held before. Every path that moves EndPtrDef ends
   here, so the invariant has one definition. */
static void SealBuffer(void) {
    do {
        EndPtrDef--;
        if (EndPtrDef < OrgPtrDef) {
            break;
        }
    } while ((U8)*EndPtrDef <= 32);

    EndPtrDef++;

    EndPtrDef[0] = 0;
    EndPtrDef[1] = 32;
    EndPtrDef[2] = 0;
}

//══════════════════════════════════════════════════════════════════════════
S32 DefFileBufferInit(char *file, void *buffer, S32 maxsize) {
    S32 buffersize;

    buffersize = FileSize(file);

    OrgPtrDef = NULL;

    if (!buffer) {
        Log_Error("DefFileBufferInit: NULL buffer pointer");
        return FALSE;
    }

    if (buffersize >= maxsize - 3) {
        Log_Error("DefFileBufferInit: buffer too small");
        return FALSE;
    }

    Load(file, buffer);

    /* Reject a blob with embedded NUL bytes as corrupt. A partial or garbage
       write (e.g. a process killed mid-rewrite) injects NULs, and the in-place
       load-modify-rewrite below would propagate them on every save -- sticky
       corruption that never self-heals. Discarding it (treat as empty) makes
       the next write regenerate a clean file from defaults. */
    if (buffersize > 0 && memchr(buffer, 0, (size_t)buffersize) != NULL) {
        Log_Warn("DefFileBufferInit: config has embedded NULs (corrupt) - discarding");
        buffersize = 0;
    }

    /* Strip blatantly non-config lines: a run far longer than any real
       "ident: value" pair with no ':' separator. Stale graphics bytes stamped
       into the file by an interrupted write surface this way -- e.g. a long run
       of one palette index left in the shared ScreenAux buffer. Such a line has
       no ':' so the parser silently skips it on read and the writer copies it
       back verbatim on every save: sticky corruption the NUL guard above misses
       because the bytes are all printable. Excise only the offending line(s) and
       compact in place, preserving every valid key, so reads are clean now and
       the next write persists the healed file. Legitimate lines are short and
       colon-delimited, so an over-long separator-less run can only be junk. */
    if (buffersize > 0) {
        const S32 maxLineLen = 512; // >> any real key:value line; << a graphics run
        char *b = (char *)buffer;
        S32 r = 0; // read cursor
        S32 w = 0; // write cursor (compacted end)
        S32 stripped = 0;
        while (r < buffersize) {
            const S32 lineStart = r;
            S32 lineHasSep = FALSE;
            while (r < buffersize && (U8)b[r] >= 32) { // a line is a run of printables
                if (b[r] == ':')
                    lineHasSep = TRUE;
                r++;
            }
            const S32 lineLen = r - lineStart;
            while (r < buffersize && (U8)b[r] < 32) // consume the trailing CR/LF/etc.
                r++;
            if (lineLen > maxLineLen && !lineHasSep) {
                stripped++; // drop this line together with its terminator
                continue;
            }
            if (w != lineStart) // keep: compact toward the front
                memmove(b + w, b + lineStart, (size_t)(r - lineStart));
            w += r - lineStart;
        }
        if (stripped) {
            Log_Warn("DefFileBufferInit: stripped %d over-long separator-less line(s) (corrupt)", stripped);
            buffersize = w;
        }
    }

    // Re-entry from DefFileBufferWriteString passes the module-static FileName
    // back in as `file`, which aliases the strcpy destination. Self-copy is UB
    // (ASan: strcpy-param-overlap); skip the copy when source and dest alias.
    if (file != FileName) {
        strcpy(FileName, file);
    }

    BufferSize = maxsize;
    LayersAttached = FALSE; // a fresh load owns exactly what FileName holds
    OrgPtrDef = (char *)buffer;
    EndPtrDef = OrgPtrDef + buffersize;
    SealBuffer(); // trailing trim, then the 0/32/0 the read loop stops on

    return TRUE;
}

//══════════════════════════════════════════════════════════════════════════

/* Room for a separator, `size` bytes of layer, and the three end sentinels. */
static S32 LayerFits(S32 size) {
    return ((S32)(EndPtrDef - OrgPtrDef) + 1 + size + 3) <= BufferSize;
}

/* A NUL anywhere in a layer would end every read that reaches it, hiding both
   the layers below and whatever this one meant to supply. Refuse the layer
   rather than attach a buffer that reads short. */
static S32 LayerIsClean(const char *bytes, S32 size) {
    if (memchr(bytes, 0, (size_t)size) == NULL) {
        return TRUE;
    }
    Log_Warn("DefFileBufferAppend: layer has embedded NULs (corrupt) - skipping");
    return FALSE;
}

//══════════════════════════════════════════════════════════════════════════
void DefFileBufferSaveState(T_DEFFILE_STATE *out) {
    if (out == NULL) {
        return;
    }
    out->org = OrgPtrDef;
    out->end = EndPtrDef;
    out->bufferSize = BufferSize;
    out->layersAttached = LayersAttached;
    strcpy(out->fileName, FileName);
}

//══════════════════════════════════════════════════════════════════════════
void DefFileBufferRestoreState(const T_DEFFILE_STATE *in) {
    if (in == NULL) {
        return;
    }
    OrgPtrDef = in->org;
    EndPtrDef = in->end;
    BufferSize = in->bufferSize;
    LayersAttached = in->layersAttached;
    strcpy(FileName, in->fileName);
}

//══════════════════════════════════════════════════════════════════════════
S32 DefFileBufferAppendFile(const char *file) {
    if (!OrgPtrDef || (file == NULL)) {
        return FALSE;
    }

    const S32 size = (S32)FileSize(file);
    if (size <= 0) {
        return FALSE; // absent or empty: nothing to layer, and not an error
    }
    if (!LayerFits(size)) {
        Log_Warn("DefFileBufferAppendFile: no room for %s (%d bytes)", file, size);
        return FALSE;
    }

    /* Load straight into the tail, past the separator, so the file is never
       copied twice. Its own line, so the last key above cannot run into the
       first key here.

       Read with a bound rather than a bare Load, and size the window from what
       came back rather than from the FileSize above. The two can disagree --
       removable media, an image remounted, the file rewritten between the stat
       and the read -- and an unbounded read of a file that grew would be past
       the end of the space LayerFits approved before anything could notice. */
    char *layer = EndPtrDef + 1;
    const S32 loaded = LoadSize((char *)file, layer, size);
    if (loaded <= 0) {
        return FALSE;
    }
    if (!LayerIsClean(layer, loaded)) {
        return FALSE; // EndPtrDef untouched, so the window still excludes it
    }

    *EndPtrDef = 10;
    EndPtrDef = layer + loaded;
    SealBuffer();
    LayersAttached = TRUE;
    return TRUE;
}

//══════════════════════════════════════════════════════════════════════════
char *DefFileBufferReadString(const char *ident) {
    char *result;

    if (ReadBufferString(ident)) {
        result = DefString;
    } else {
        result = NULL;
    }

    return result;
}

//══════════════════════════════════════════════════════════════════════════
const char *DefFileBufferReadStringDefault(const char *ident, const char *def) {
    char *temp;

    temp = DefFileBufferReadString(ident);

    return temp ? temp : def;
}

//══════════════════════════════════════════════════════════════════════════
S32 DefFileBufferReadValue2(const char *ident, S32 *result) {
    char *ptrdef;
    S32 i;
    char c;
    S32 value;

    DefFileBufferReadString(ident);
    if (!LenDefString) {
        return FALSE;
    }

    ptrdef = DefString;
    while ((U8)*ptrdef > 32)
        ptrdef++;
    *ptrdef = 0;
    LenDefString = ptrdef - DefString;

    c = (char)(DefString[LenDefString - 1] & ~32);
    if (c == 'H') {
        // hexa
        value = 0;
        for (i = 0; i < strlen(DefString) - 1; i++) {
            c = DefString[i];
            if (c <= '9')
                c -= '0';
            else
                c = (char)((c & ~32) - 'A' + 10);
            value = value * 16 + c;
        }
    } else {
        // deci
        value = atoi(DefString);
    }

    *result = value;
    return TRUE;
}

//══════════════════════════════════════════════════════════════════════════
S32 DefFileBufferReadValueDefault(const char *ident, S32 def) {
    DefFileBufferReadValue2(ident, &def);

    return def;
}

//══════════════════════════════════════════════════════════════════════════
S32 DefFileBufferReadValue(const char *ident) {
    return DefFileBufferReadValueDefault(ident, -1);
}

//══════════════════════════════════════════════════════════════════════════
S32 DefFileBufferWriteString(const char *ident, const char *string) {
    char crlf[2] = {13, 10};
    S32 handle;
    char *result;

    if (!OrgPtrDef) {
        return FALSE;
    }

    /* The buffer holds lower-priority layers the file does not, and a write
       rewrites the whole buffer, so persisting it would fold the game data's
       config into the player's. Callers that write re-load the file they own
       first (WriteConfigFile does), which clears the layers; reaching here with
       them attached is a bug, not a state to paper over. */
    if (LayersAttached) {
        Log_Error("DefFileBufferWriteString('%s'): buffer has read-only layers attached", ident);
        return FALSE;
    }

    result = DefFileBufferReadString(ident);

    // Atomic write: the new content is spliced into a temp file which then
    // atomically replaces FileName. OpenWrite would truncate FileName to zero up
    // front, so a crash/kill anywhere in the splice below (and this runs ~30
    // times per WriteConfigFile) would leave the live config truncated. Here the
    // config is only ever the old complete file or the new complete file.
    //
    // In a batch FileName already points at a private working temp, so an
    // ordinary buffered write is enough -- the single fsync + rename happens once
    // at DefFileBufferEndBatch instead of once per key.
    handle = BatchActive ? OpenWrite(FileName) : OpenWriteAtomic(FileName);
    if (!handle) {
        return FALSE;
    }

    if (result) // ident found
    {
        Write(handle, OrgPtrDef, PtrEndIdent - OrgPtrDef);

        while ((U8)*PtrEndIdent >= 32)
            PtrEndIdent++; // skip until EOL
        if (*PtrEndIdent == 13)
            PtrEndIdent++;
        if (*PtrEndIdent == 10)
            PtrEndIdent++;
        if (PtrEndIdent > EndPtrDef) {
            PtrEndIdent = EndPtrDef;
        }
    } else // ident not found
    {
        if (OrgPtrDef) // file exist
        {
            Write(handle, OrgPtrDef, EndPtrDef - OrgPtrDef);
        }
        Write(handle, crlf, 2);
        Write(handle, ident, strlen(ident));
        Write(handle, ":", 1);
    }

    // write new value
    Write(handle, " ", 1);
    Write(handle, string, strlen(string));
    Write(handle, crlf, 2);

    if (result) {
        // end of file
        Write(handle, PtrEndIdent, EndPtrDef - PtrEndIdent);
    }

    if (BatchActive) {
        Close(handle);
        BatchDirty = TRUE; // the working temp now holds content to commit
    } else if (!CommitWriteAtomic(handle)) {
        // Commit failed: the temp was cleaned up and FileName is untouched, so
        // the in-memory buffer still matches what is on disk. Leave it as-is.
        return FALSE;
    }

    DefFileBufferInit(FileName, (void *)OrgPtrDef, BufferSize);

    return TRUE;
}

//══════════════════════════════════════════════════════════════════════════
S32 DefFileBufferBeginBatch(void) {
    /* Same reason DefFileBufferWriteString refuses: every write in the batch
       would refuse anyway, so the batch would commit nothing and still report
       success. Fail at the boundary, where the caller can still notice. */
    if (LayersAttached) {
        Log_Error("DefFileBufferBeginBatch: buffer has read-only layers attached");
        return FALSE;
    }

    if (BatchActive OR !OrgPtrDef) {
        return FALSE;
    }

    // ".saving" + the NUL must fit. Redirect FileName to a working temp beside
    // the real destination; the buffer already holds the destination's content
    // (the caller DefFileBufferInit'd it), so the first splice carries it into
    // the temp and every later key builds on the temp.
    if (strlen(FileName) + sizeof(".saving") > ADELINE_MAX_PATH) {
        return FALSE;
    }
    strcpy(BatchFinalPath, FileName);
    strcat(FileName, ".saving");

    BatchActive = TRUE;
    BatchDirty = FALSE;
    return TRUE;
}

//══════════════════════════════════════════════════════════════════════════
S32 DefFileBufferEndBatch(void) {
    char tempPath[ADELINE_MAX_PATH];
    S32 ok = TRUE;

    if (!BatchActive) {
        return FALSE;
    }
    BatchActive = FALSE;

    strcpy(tempPath, FileName);       // FileName currently == the working temp
    strcpy(FileName, BatchFinalPath); // restore the real destination

    if (BatchDirty) {
        ok = CommitFileAtomic(tempPath, FileName);
        if (!ok) {
            Delete(tempPath); // commit failed: drop the temp, keep the old cfg
        }
    }

    // Re-sync the buffer to the committed file so later reads/writes stay
    // consistent with what is on disk.
    DefFileBufferInit(FileName, (void *)OrgPtrDef, BufferSize);
    return ok;
}

//══════════════════════════════════════════════════════════════════════════
