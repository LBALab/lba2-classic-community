# ─────────────────────────────────────────────────────────────────────────────
# patch_asm_flat.cmake — Patch .model SMALL → .model FLAT for testing
#
# Called at build time with:
#   cmake -DSRC=<original.ASM> -DDST=<patched.ASM> -P patch_asm_flat.cmake
#
# The .model SMALL directive produces OMF objects with segmented relocations
# that cannot link into a flat 32-bit Linux ELF binary.  Switching to
# .model FLAT produces standard R_386_32 relocations while preserving
# identical machine-code instruction encoding.  This makes the patched
# objects safe for ASM-vs-CPP equivalence testing on Linux.
# ─────────────────────────────────────────────────────────────────────────────

file(READ "${SRC}" _content)

# .model SMALL, C  →  .model FLAT, C   (preserve calling convention suffix)
string(REGEX REPLACE
    "\\.[mM][oO][dD][eE][lL]([ \t]+)SMALL"
    ".model\\1FLAT"
    _content "${_content}")

# ASSUME DS:SEG <symbol>  →  ASSUME DS:FLAT
string(REGEX REPLACE
    "ASSUME([ \t]+)[Dd][Ss]:SEG[ \t]+[A-Za-z0-9_]+"
    "ASSUME\\1DS:FLAT"
    _content "${_content}")

file(WRITE "${DST}" "${_content}")
