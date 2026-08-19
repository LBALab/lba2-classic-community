#ifndef LBA2_RECORD_FORMAT_H
#define LBA2_RECORD_FORMAT_H

/* The text header of a recording: the fields a reader looks up by name, and the two
 * binding tables the header carries.
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

#endif /* LBA2_RECORD_FORMAT_H */
