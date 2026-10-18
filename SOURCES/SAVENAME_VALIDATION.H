#ifndef SAVENAME_VALIDATION_H
#define SAVENAME_VALIDATION_H

#include <SYSTEM/ADELINE_TYPES.H>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* True if `s` ends in the 19-character `dddd-dd-dd dd-dd-dd` shape used by
 * the gamepad datetime-save flow — either bare (19 chars total) or scene-
 * prefixed with " - " before it (e.g. "Citadel Island - 2026-05-17 14-30-00").
 * Reserved so the named (keyboard-typed) and datetime (pad-generated) save
 * namespaces cannot collide — a keyboard user typing such a name would
 * otherwise be able to clobber a gamepad-created file (or vice versa).
 * A trailing word ("2026-05-17 hero") or partial date ("2026-05-17") is
 * still accepted. Extracted from GAMEMENU.CPP so it's unit-testable. */
extern S32 IsDatetimeReservedShape(const char *s);

/* Writes the file name stem (no extension) for a save called `name` into
 * `out`. A save's name is CP850 text stored inside the file, where the save
 * list reads it, so the file name only has to be unique and portable: accented
 * letters fold to ASCII ("D\x82sert" -> "Desert"), other bytes outside
 * printable ASCII and the characters Windows forbids become '_', trailing dots
 * and spaces are dropped, and a Windows device name (CON, NUL, COM1...) gets a
 * leading '_'. An empty result becomes "save". `out` is always terminated;
 * `outSize` must be at least 8. */
extern void SaveFileStemFromName(const char *name, char *out, size_t outSize);

#ifdef __cplusplus
}
#endif

#endif /* SAVENAME_VALIDATION_H */
