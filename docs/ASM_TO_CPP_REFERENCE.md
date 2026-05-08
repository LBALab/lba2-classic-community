# ASM to C++ reference

In the original codebase (lba2-classic), many performance-critical modules were written in assembly. In lba2-classic-community, some of these have been ported to C++ and are built instead of the ASM. The original ASM files remain in the repository but are not used in the default build.

This page is a quick reference for which modules are ported (built as CPP) vs still assembly in the current community build. It may change as more ports are completed. Build configuration: `SOURCES/CMakeLists.txt` and `SOURCES/3DEXT/CMakeLists.txt`.

## SOURCES (main game)

| Original ASM module | Community build | Notes |
|---------------------|-----------------|--------|
| COMPRESS.ASM        | COMPRESS.CPP    | Ported to C++. |
| FIRE.ASM            | FIRE.CPP        | Inline assembly in .CPP file; banner and French comment preserved from ASM. |
| FLOW_A.ASM          | FLOW_A.CPP      | Ported. FLOW.CPP is the main particle/game logic (existed in original). |
| FUNC.ASM            | FUNC.CPP        | Ported to C++. |
| GRILLE_A.ASM        | GRILLE_A.CPP    | Ported. GRILLE.CPP is the high-level grid logic (existed in original). |
| PLASMA.ASM          | PLASMA.CPP      | Ported to C++. |

**Present in repo but not in current build:**

| Module       | Notes |
|--------------|-------|
| DEC_XCF.ASM  | DOS-specific decoding. Listed in `SOURCES_FILES_ASM_DOS` but not added to the executable. |
| HERCUL_A.ASM | Hercules text display. Same as above. |
| KEYB.ASM     | DOS-specific keyboard handling. Same as above. |
| DEC.ASM      | Referenced in comments only. |
| COPY.ASM     | Referenced in comments only. |

## SOURCES/3DEXT (3D extensions)

| Original ASM module | Community build | Notes |
|---------------------|-----------------|--------|
| BOXZBUF.ASM         | BOXZBUF.CPP     | Ported to C++. |
| LINERAIN.ASM        | LINERAIN.CPP    | Ported to C++. |

## LIB386 (engine libraries)

LIB386 still contains many ASM files that are used in the build (3D, ANIM, SVGA, OBJECT, pol_work, SYSTEM, etc.). A full LIB386 ASM↔CPP map is not listed here; the focus of this reference is SOURCES and 3DEXT where the community has explicitly switched to C++ in the build. LIB386 ports can be documented here later if desired.
