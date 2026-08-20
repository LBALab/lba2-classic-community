#ifndef LBA2_RECORD_FORMAT_H
#define LBA2_RECORD_FORMAT_H

/* The on-disk shape of a recording: the header fields a reader looks up by name, the
 * two binding tables the header carries, and the frame around a savegame the file
 * carries inline.
 *
 * Pure string and table work. No engine state, no file IO, no globals, which is the
 * line this module is cut along and what lets tests/record_format drive it with
 * nothing underneath. RECORD.CPP keeps everything that cannot be: the stream, the
 * clock, the state hash and the tables the bindings are installed into.
 *
 * The header is `key=value\n` lines, so a recording stays readable without a decoder,
 * and a reader finds a field by searching for its key rather than by position. Two
 * consequences, each of which has already gone wrong here:
 *
 *   - The value starts at `strlen(key)` past the match. Every reader below computes
 *     that; a hand-written `sscanf(strstr(hdr, "setup.reloaded=") + 15, ...)` has to
 *     be recounted whenever the key is renamed, and reads the wrong field when it is
 *     not.
 *
 *   - A binding line is one number pair per slot and runs to a few hundred characters,
 *     which is longer than a header line reader is likely to assume. The writer below
 *     reports truncation rather than emitting a short table that would parse as a
 *     valid but different set of bindings.
 */

#include <SYSTEM/ADELINE_TYPES.H>

#include <stddef.h> /* size_t */

#include "INPUT_BINDINGS.H" /* T_DEF_KEY, MAX_INPUT_SLOTS */

/* Writes `<key>=k1:k2,k1:k2,...\n` covering `slots` pairs from `tab`.
 * Returns 1 on success, 0 if `out` was too small to hold the whole table, in which
 * case what was written must not be used: a truncated table parses as a shorter one. */
extern S32 RecFmt_WriteBindings(char *out, size_t n, const T_DEF_KEY *tab, S32 slots,
                                const char *key);

/* Reads a line written by RecFmt_WriteBindings into `tab`, which must hold `slots`
 * entries. Returns 1 only when every slot was read; a partial table is a table with
 * different bindings, so there is no useful half-answer. `key` includes the `=`. */
extern S32 RecFmt_ParseBindings(const char *hdr, const char *key, T_DEF_KEY *tab, S32 slots);

/* Field readers. Each returns 1 when the key was present and its value parsed, and
 * leaves `*out` untouched otherwise, so a caller can pre-set its default. `key`
 * includes the `=`. RecFmt_ReadStr reads to the end of the line, so a value may
 * contain spaces (a path does). */
extern S32 RecFmt_ReadU32(const char *hdr, const char *key, U32 *out);
extern S32 RecFmt_ReadInt(const char *hdr, const char *key, S32 *out);
extern S32 RecFmt_ReadStr(const char *hdr, const char *key, char *out, size_t n);

/* Chunk framing, for the savegames a recording carries inline.
 *
 * A recording is one file: the snapshot it starts from and the one it ends at sit in
 * the stream rather than beside it. Both are framed the same way, and the frame is
 * built for a writer that may be killed in the middle of it:
 *
 *     [op][u32 len][len bytes][u32 len][u32 magic]
 *
 * The length is known before the payload is written, so nothing is ever seeked back to
 * and patched -- a file that stops part way through a chunk stops with bytes missing
 * off the end, never with a length that claims more than is there. The repeated length
 * and the magic word are what make that detectable: a torn chunk has no valid tail, so
 * it is refused rather than handed to the save loader as a short savegame.
 *
 * The tail is also what lets the end chunk be found from the end of the file, which is
 * how a reader tells the trailer apart from the record stream in front of it.
 *
 * Buffer work only, like everything else here. RECORD.CPP does the file IO around it. */
#define REC_CHUNK_HEAD 5 /* op, then the length */
#define REC_CHUNK_TAIL 8 /* the length again, then the magic */

/* Build the two ends of a frame. Return 0 if the buffer is smaller than the constant
 * above says, which is a caller bug rather than a runtime condition. */
extern S32 RecFmt_PutChunkHead(U8 *out, size_t n, U8 op, U32 len);
extern S32 RecFmt_PutChunkTail(U8 *out, size_t n, U32 len);

/* Read a head. Returns 1 and fills `op` and `len` when `n` is large enough. */
extern S32 RecFmt_GetChunkHead(const U8 *in, size_t n, U8 *op, U32 *len);

/* Whether a tail closes a chunk of `len` bytes: the length written twice and the magic
 * behind it. This is the whole of the torn-write check, so it is the one function here
 * whose failure case is the interesting one. */
extern S32 RecFmt_ChunkTailOk(const U8 *in, size_t n, U32 len);

/* The length a tail claims, for a reader working backwards from the end of a file: the
 * end chunk's payload starts REC_CHUNK_TAIL + len bytes before the tail, and its head
 * REC_CHUNK_HEAD bytes before that. Returns 0 when the magic is not there, which is the
 * ordinary case for a recording that never reached its end chunk. */
extern S32 RecFmt_ReadChunkTailLen(const U8 *in, size_t n, U32 *len);

#endif /* LBA2_RECORD_FORMAT_H */
