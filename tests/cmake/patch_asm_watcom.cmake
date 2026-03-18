# patch_asm_watcom.cmake — Strip C-adaptation wrappers from ASM procs
#
# Some ASM procs were adapted for C calling convention by adding
# "PROC uses ... x:DWORD, y:DWORD" and "mov reg, param" preambles.
# This patch reverts those to bare "proc" declarations so internal
# Watcom register-convention calls between ASM functions work correctly.
#
# Usage:
#   cmake -DSRC=<input.ASM> -DDST=<output.ASM> -P patch_asm_watcom.cmake

file(READ "${SRC}" _content)

# Replace C-adapted PROC declarations with bare proc.
# Pattern: "Name PROC \" followed by "uses ..." and param lines, ending before
# the first "mov reg, paramname" block.
#
# Strategy: Replace multiline PROC ... with just "proc", then remove the
# "mov reg, paramname" lines that load stack params into registers.

# Step 1: Replace "PROC \" continuation lines with just "proc"
# Match: PROC \ <newline> uses/whitespace lines <newline> paramname: DWORD...
# This handles the multi-line PROC format used in the codebase.
string(REGEX REPLACE
    "PROC[ \t]+\\\\[ \t]*\n[^\n]*uses[^\n]*\\\\[ \t]*\n[^\n]*:[^\n]*DWORD[^\n]*\n"
    "proc\n"
    _content "${_content}")

# Also handle single-line PROC with uses and params (no continuation)
string(REGEX REPLACE
    "PROC[ \t]+uses[^\n]*,[^\n]*DWORD[^\n]*\n"
    "proc\n"
    _content "${_content}")

# Step 2: Remove "mov reg, paramname" lines that were added for C-adaptation.
# These load named PROC parameters into registers.  Parameter names are
# identifiers that are NOT register names (eax..esi, ax..si, al..dh)
# and NOT memory references ([...]) or immediates (digits).
# We match: "mov ereg, <identifier>" where identifier doesn't look like
# a register or memory operand.  The parameter names in the codebase are
# things like: x, y, z, angle, alpha, beta, gamma, xLow, xHigh, dst, etc.
# We exclude: eax/ebx/ecx/edx/edi/esi/ebp/esp and any [...] or digit-start.
# A simple heuristic: only strip lines where source is 1-10 chars, all alpha
# or underscore, and does NOT start with 'e' followed by two alpha chars
# (which would be a 32-bit register name).
#
# Actually, the safest approach: only remove mov lines that appear between
# the proc declaration and the first non-mov instruction.  Since the
# C-adaptation adds these moves right after the PROC line, we find them
# by looking for blocks of consecutive "mov reg, word" lines right after "proc".

# First, extract param names from the original PROC declaration
# so we know exactly which identifiers to look for.
# Since we already stripped the PROC decl, we need a different approach.

# Re-read original to extract param names
file(READ "${SRC}" _orig)
string(REGEX MATCHALL "[a-zA-Z_][a-zA-Z0-9_]*:[ \t]*DWORD" _param_matches "${_orig}")
set(_param_names "")
foreach(_pm ${_param_matches})
    string(REGEX REPLACE ":[ \t]*DWORD" "" _pname "${_pm}")
    list(APPEND _param_names "${_pname}")
endforeach()

# Remove "mov reg, paramname" lines only for the extracted parameter names
foreach(_pname ${_param_names})
    string(REGEX REPLACE
        "\n[ \t]+mov[ \t]+(eax|ebx|ecx|edx|edi|esi),[ \t]*${_pname}[ \t]*\n"
        "\n"
        _content "${_content}")
endforeach()

file(WRITE "${DST}" "${_content}")
