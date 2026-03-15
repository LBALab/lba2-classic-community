#!/usr/bin/env python3
"""
Instrument remaining LIB386 ASM and CPP files with debug printf traces (Phase 2).

Handles SVGA, pol_work, OBJECT, SYSTEM, MENU directories.
"""

import re
import os

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read_file(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()


def write_file(path, content):
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)


def ensure_asm_printf_extrn(content):
    """Add EXTRN C printf:NEAR if not already present."""
    if 'printf' in content:
        return content
    # Insert before .CODE section
    code_match = re.search(r'(\n[;\t ]*\.CODE)', content, re.IGNORECASE)
    if code_match:
        insert_pos = code_match.start()
        return content[:insert_pos] + '\n\n\t\tEXTRN\tC\tprintf\t:NEAR' + content[insert_pos:]
    return content


def add_asm_fmt_strings(content, fmt_entries):
    """Add format strings before .CODE section.
    fmt_entries: list of (label, text) tuples
    """
    new_lines = []
    for label, text in fmt_entries:
        if label in content:
            continue
        new_lines.append(f'{label}\tDB\t"{text}", 10, 0')
    
    if not new_lines:
        return content
    
    # Find .CODE and insert before it
    code_match = re.search(r'(\n[;\t ]*\.CODE)', content, re.IGNORECASE)
    if code_match:
        insert_pos = code_match.start()
        block = '\n\n' + '\n'.join(new_lines) + '\n'
        return content[:insert_pos] + block + content[insert_pos:]
    return content


def make_trace_block(func_name, trace_id, desc, fmt_label, push_args):
    """Generate an ASM debug trace block."""
    lines = [
        f'\t\t; --- debug trace [ASM][{func_name}][{trace_id}] {desc} ---',
        '\t\tpushad',
    ]
    for arg in push_args:
        lines.append(f'\t\tpush\t{arg}')
    lines.append(f'\t\tpush\toffset {fmt_label}')
    lines.append('\t\tcall\tprintf')
    cleanup = 4 * (len(push_args) + 1)
    lines.append(f'\t\tadd\tesp, {cleanup}')
    lines.append('\t\tpopad')
    lines.append(f'\t\t; --- end debug trace ---')
    return '\n'.join(lines)


def insert_after_anchor(content, anchor, trace_block):
    """Insert trace block after a specific anchor line."""
    idx = content.find(anchor)
    if idx < 0:
        return None
    # Find end of the anchor line
    eol = content.find('\n', idx)
    if eol < 0:
        eol = len(content)
    return content[:eol+1] + '\n' + trace_block + '\n\n' + content[eol+1:]


def insert_before_anchor(content, anchor, trace_block):
    """Insert trace block before a specific anchor line."""
    idx = content.find(anchor)
    if idx < 0:
        return None
    return content[:idx] + trace_block + '\n\n\t\t' + content[idx:]


def instrument_cpp(filepath, func_name, trace_args):
    """Add entry trace to CPP file."""
    fullpath = os.path.join(BASE, filepath)
    if not os.path.exists(fullpath):
        print(f"  CPP SKIP (not found): {filepath}")
        return False
    
    content = read_file(fullpath)
    
    if f'LIB386_TRACE_CPP("{func_name}"' in content:
        print(f"  CPP SKIP (done): {filepath} [{func_name}]")
        return True
    
    # Add include
    if 'DEBUG_TRACES.H' not in content:
        # Find last #include
        lines = content.split('\n')
        last_inc = -1
        for i, line in enumerate(lines):
            if line.strip().startswith('#include'):
                last_inc = i
        if last_inc >= 0:
            lines.insert(last_inc + 1, '#include <SYSTEM/DEBUG_TRACES.H>')
            content = '\n'.join(lines)
    
    # Find function and add trace after {
    # Try various patterns
    patterns = [
        rf'{re.escape(func_name)}\s*\([^)]*\)\s*\{{',
        rf'{re.escape(func_name)}\s*\([^)]*\)\s*\n\s*\{{',
    ]
    for pat in patterns:
        match = re.search(pat, content, re.DOTALL)
        if match:
            insert_pos = match.end()
            trace = f'\n    LIB386_TRACE_CPP("{func_name}", "1", {trace_args});'
            content = content[:insert_pos] + trace + content[insert_pos:]
            write_file(fullpath, content)
            print(f"  CPP OK: {filepath} [{func_name}]")
            return True
    
    print(f"  CPP WARN: func {func_name} not found in {filepath}")
    return False


# ─── SVGA FILES ───

SVGA_DEFS = [
    # (asm_file, cpp_file, func_name, fmt_text, asm_push_args, 
    #  asm_anchor_after, cpp_trace_args)
    ('LIB386/SVGA/SAVBLOCK.ASM', 'LIB386/SVGA/SAVBLOCK.CPP',
     'SaveBlock', '[ASM][SaveBlock][1] scr=%p buf=%p x0=%d y0=%d x1=%d y1=%d',
     ['y1', 'x1', 'y0', 'x0', 'buffer', 'screen'],
     'mov esi, screen', '"scr=%p buf=%p x0=%d y0=%d x1=%d y1=%d", screen, buffer, x0, y0, x1, y1'),
    
    ('LIB386/SVGA/RESBLOCK.ASM', 'LIB386/SVGA/RESBLOCK.CPP',
     'RestoreBlock', '[ASM][RestoreBlock][1] scr=%p buf=%p x0=%d y0=%d x1=%d y1=%d',
     ['y1', 'x1', 'y0', 'x0', 'buffer', 'screen'],
     'mov edi, screen', '"scr=%p buf=%p x0=%d y0=%d x1=%d y1=%d", screen, buffer, x0, y0, x1, y1'),
    
    ('LIB386/SVGA/CPYBLOCK.ASM', 'LIB386/SVGA/CPYBLOCK.CPP',
     'CopyBlock', '[ASM][CopyBlock][1] x0=%d y0=%d x1=%d y1=%d src=%p xd=%d yd=%d dst=%p',
     ['dst', 'yd', 'xd', 'src', 'y1', 'x1', 'y0', 'x0'],
     'mov eax, x0', '"x0=%d y0=%d x1=%d y1=%d src=%p xd=%d yd=%d dst=%p", x0, y0, x1, y1, src, xd, yd, dst'),
    
    ('LIB386/SVGA/CPYBLOCI.ASM', 'LIB386/SVGA/CPYBLOCI.CPP',
     'CopyBlockIncrust', '[ASM][CopyBlockIncrust][1] x0=%d y0=%d x1=%d y1=%d src=%p xd=%d yd=%d dst=%p',
     ['dst', 'yd', 'xd', 'src', 'y1', 'x1', 'y0', 'x0'],
     None, '"x0=%d y0=%d x1=%d y1=%d src=%p xd=%d yd=%d dst=%p", x0, y0, x1, y1, src, xd, yd, dst'),
    
    ('LIB386/SVGA/SCALEBOX.ASM', 'LIB386/SVGA/SCALEBOX.CPP',
     'ScaleBox', '[ASM][ScaleBox][1] xs0=%d ys0=%d xs1=%d ys1=%d',
     ['ys1', 'xs1', 'ys0', 'xs0'],
     'mov edx, xs0', '"xs0=%d ys0=%d xs1=%d ys1=%d ptrs=%p", xs0, ys0, xs1, ys1, ptrs'),
    
    ('LIB386/SVGA/CALCMASK.ASM', 'LIB386/SVGA/CALCMASK.CPP',
     'CalcGraphMsk', '[ASM][CalcGraphMsk][1] num=%d bank=%p mask=%p',
     ['ptmask', 'bankbrick', 'numbrick'],
     None, '"num=%d bank=%p mask=%p", numbrick, (void*)bankbrick, (void*)ptmask'),
    
    ('LIB386/SVGA/SCALESPI.ASM', 'LIB386/SVGA/SCALESPI.CPP',
     'ScaleSprite', '[ASM][ScaleSprite][1] num=%d x=%d y=%d fx=%d fy=%d',
     ['factory', 'factorx', 'ybrick', 'xbrick', 'numbrick'],
     None, '"num=%d x=%d y=%d fx=%d fy=%d bank=%p", num, x, y, factorx, factory, (void*)ptrbank'),
    
    ('LIB386/SVGA/SCALESPT.ASM', 'LIB386/SVGA/SCALESPT.CPP',
     'ScaleSpriteTransp', '[ASM][ScaleSpriteTransp][1] num=%d x=%d y=%d fx=%d fy=%d',
     ['factory', 'factorx', 'ybrick', 'xbrick', 'numbrick'],
     None, '"num=%d x=%d y=%d fx=%d fy=%d", num, x, y, factorx, factory'),
]

# ─── SYSTEM + MENU FILES ───

SYSM_DEFS = [
    ('LIB386/SYSTEM/LZ.ASM', 'LIB386/SYSTEM/LZ.CPP',
     'ExpandLZ', '[ASM][ExpandLZ][1] Dst=%p Src=%p Size=%u MinBloc=%u',
     ['MinBloc', 'DecompSize', 'Src', 'Dst'],
     None, '"Dst=%p Src=%p Size=%u MinBloc=%u", Dst, Src, DecompSize, MinBloc'),
    
    ('LIB386/SYSTEM/FASTCPYF.ASM', 'LIB386/SYSTEM/FASTCPYF.CPP',
     'FastCopyF', '[ASM][FastCopyF][1] dst=%p src=%p len=%u',
     ['len_', 'src', 'dst'],
     None, '"dst=%p src=%p len=%u", dst, src, len'),
]


def process_asm_file(asm_path, func_name, fmt_text, push_args, anchor_after):
    """Process a single ASM file - add printf, fmt string, and trace block."""
    fullpath = os.path.join(BASE, asm_path)
    if not os.path.exists(fullpath):
        print(f"  ASM SKIP (not found): {asm_path}")
        return False
    
    content = read_file(fullpath)
    
    if 'debug trace' in content:
        print(f"  ASM SKIP (done): {asm_path}")
        return True
    
    # Add printf extern
    content = ensure_asm_printf_extrn(content)
    
    # Add format string
    fmt_label = f'fmt_{func_name.lower()[:8]}_1'
    content = add_asm_fmt_strings(content, [(fmt_label, fmt_text)])
    
    # Add trace block
    trace = make_trace_block(func_name, '1', 'params', fmt_label, push_args)
    
    if anchor_after:
        result = insert_after_anchor(content, anchor_after, trace)
        if result:
            content = result
        else:
            print(f"  ASM WARN: anchor not found in {asm_path}: {anchor_after!r}")
            # Try to find the PROC and insert after its parameter loading
            proc_match = re.search(rf'{re.escape(func_name)}\s+(?:PROC|proc)', content)
            if proc_match:
                # Find first non-comment, non-blank line after PROC
                pos = proc_match.end()
                # Skip to first instruction line (after PROC ... mov ...)
                lines = content[pos:].split('\n')
                insert_idx = 0
                for i, line in enumerate(lines):
                    stripped = line.strip()
                    if stripped and not stripped.startswith(';') and not stripped.startswith('uses') and not stripped.startswith('\\'):
                        if 'mov' in stripped.lower() or 'push' in stripped.lower() or 'xor' in stripped.lower() or 'lea' in stripped.lower() or 'cmp' in stripped.lower():
                            insert_idx = i
                            break
                if insert_idx > 0:
                    before = content[:pos] + '\n'.join(lines[:insert_idx]) + '\n'
                    after = '\n'.join(lines[insert_idx:])
                    content = before + '\n' + trace + '\n\n\t\t' + after
                    print(f"  ASM OK (auto-anchor): {asm_path}")
                    write_file(fullpath, content)
                    return True
            
            write_file(fullpath, content)  # Still write printf extrn + fmt
            return False
    else:
        # Auto-find: insert after PROC declaration + param loading
        proc_match = re.search(rf'{re.escape(func_name)}\s+(?:PROC|proc)', content)
        if proc_match:
            pos = proc_match.end()
            rest = content[pos:]
            # Find the first real instruction (skip uses/params/newlines)
            lines = rest.split('\n')
            insert_idx = 0
            found_instruction = False
            for i, line in enumerate(lines):
                stripped = line.strip()
                if not stripped or stripped.startswith(';') or stripped.startswith('\\') or 'uses ' in stripped.lower():
                    continue
                if any(stripped.lower().startswith(kw) for kw in ['mov', 'push', 'xor', 'lea', 'cmp', 'add', 'sub', 'test', 'or ', 'and ']):
                    insert_idx = i
                    found_instruction = True
                    break
            
            if found_instruction:
                before = content[:pos] + '\n'.join(lines[:insert_idx]) + '\n'
                after = '\n'.join(lines[insert_idx:])
                content = before + '\n' + trace + '\n\n\t\t' + after
        
    write_file(fullpath, content)
    print(f"  ASM OK: {asm_path}")
    return True


def main():
    print("=== SVGA Files ===")
    for entry in SVGA_DEFS:
        asm_path, cpp_path, func, fmt_text, push_args, anchor, cpp_trace = entry
        print(f"\n--- {func} ---")
        process_asm_file(asm_path, func, fmt_text, push_args, anchor)
        instrument_cpp(cpp_path, func, cpp_trace)
    
    print("\n=== SYSTEM + MENU Files ===")
    for entry in SYSM_DEFS:
        asm_path, cpp_path, func, fmt_text, push_args, anchor, cpp_trace = entry
        print(f"\n--- {func} ---")
        process_asm_file(asm_path, func, fmt_text, push_args, anchor)
        instrument_cpp(cpp_path, func, cpp_trace)
    
    # pol_work filler functions - these use global state, no params
    print("\n=== pol_work Filler Functions ===")
    polwork_fillers = [
        ('LIB386/pol_work/POLYFLAT.ASM', 'LIB386/pol_work/POLYFLAT.CPP', [
            'Filler_Flat', 'Filler_Transparent', 'Filler_Trame',
            'Filler_FlatZBuf', 'Filler_TransparentZBuf', 'Filler_TrameZBuf',
            'Filler_FlatNZW', 'Filler_TransparentNZW', 'Filler_TrameNZW',
        ]),
        ('LIB386/pol_work/POLYGOUR.ASM', 'LIB386/pol_work/POLYGOUR.CPP', [
            'Filler_Gouraud', 'Filler_Dither',
            'Filler_GouraudTable', 'Filler_DitherTable',
            'Filler_GouraudZBuf', 'Filler_DitherZBuf',
            'Filler_GouraudTableZBuf', 'Filler_DitherTableZBuf',
            'Filler_GouraudNZW', 'Filler_DitherNZW',
            'Filler_GouraudTableNZW', 'Filler_DitherTableNZW',
        ]),
        ('LIB386/pol_work/POLYTEXT.ASM', 'LIB386/pol_work/POLYTEXT.CPP', [
            'Filler_Texture', 'Filler_TextureFlat',
            'Filler_TextureChromaKey', 'Filler_TextureFlatChromaKey',
        ]),
        ('LIB386/pol_work/POLYTEXZ.ASM', 'LIB386/pol_work/POLYTEXZ.CPP', [
            'Filler_TextureZ', 'Filler_TextureZFlat',
            'Filler_TextureZChromaKey', 'Filler_TextureZFlatChromaKey',
        ]),
        ('LIB386/pol_work/POLYTZG.ASM', 'LIB386/pol_work/POLYTZG.CPP', [
            'Filler_TextureZGouraud', 'Filler_TextureZGouraudChromaKey',
        ]),
        ('LIB386/pol_work/POLYTZF.ASM', 'LIB386/pol_work/POLYTZF.CPP', [
            'Filler_TextureZFogSmooth',
        ]),
        ('LIB386/pol_work/POLYGTEX.ASM', 'LIB386/pol_work/POLYGTEX.CPP', [
            'Filler_TextureGouraud', 'Filler_TextureDither',
            'Filler_TextureGouraudChromaKey', 'Filler_TextureDitherChromaKey',
        ]),
    ]
    
    for asm_path, cpp_path, funcs in polwork_fillers:
        fullpath = os.path.join(BASE, asm_path)
        if not os.path.exists(fullpath):
            print(f"  SKIP (not found): {asm_path}")
            continue
        
        content = read_file(fullpath)
        if 'debug trace' in content:
            print(f"  SKIP (already done): {asm_path}")
            # Still check CPP
            for func in funcs:
                instrument_cpp(cpp_path, func, '"entry"')
            continue
        
        content = ensure_asm_printf_extrn(content)
        
        # Add format strings for all functions
        fmt_entries = []
        for func in funcs:
            label = f'fmt_{func.lower()[:12]}_1'
            text = f'[ASM][{func}][1] entry'
            fmt_entries.append((label, text))
        content = add_asm_fmt_strings(content, fmt_entries)
        
        # Add trace blocks at each PROC
        for func in funcs:
            label = f'fmt_{func.lower()[:12]}_1'
            trace = make_trace_block(func, '1', 'entry', label, [])
            
            # Find PROC declaration
            proc_pat = re.compile(rf'({re.escape(func)}\s+(?:PROC|proc)\b[^\n]*(?:\n[^\n]*\\)*)', re.IGNORECASE)
            match = proc_pat.search(content)
            if match:
                # Find end of PROC declaration (after all continuation lines)
                end_pos = match.end()
                # Skip to next non-empty, non-comment line
                rest = content[end_pos:]
                lines = rest.split('\n')
                insert_idx = 0
                for i, line in enumerate(lines):
                    stripped = line.strip()
                    if not stripped or stripped.startswith(';'):
                        continue
                    insert_idx = i
                    break
                
                if insert_idx > 0:
                    before = content[:end_pos] + '\n'.join(lines[:insert_idx]) + '\n'
                    after = '\n'.join(lines[insert_idx:])
                    content = before + '\n' + trace + '\n\n\t\t' + after
                else:
                    # Insert right after PROC line
                    content = content[:end_pos] + '\n\n' + trace + '\n' + content[end_pos:]
                
                print(f"  ASM trace added: {func}")
            else:
                print(f"  ASM WARN: PROC {func} not found in {asm_path}")
        
        write_file(fullpath, content)
        
        # CPP: add traces
        for func in funcs:
            instrument_cpp(cpp_path, func, '"entry"')
    
    # Remaining pol_work files with specific params
    print("\n=== pol_work Special Functions ===")
    special_polwork = [
        ('LIB386/pol_work/POLYCLIP.ASM', 'LIB386/pol_work/POLYCLIP.CPP',
         'ClipperZ', '[ASM][ClipperZ][1] dst=%p src=%p n=%d zclip=%d',
         ['flag', 'zclip', 'nbvertex', 'src', 'dst'],
         None, '"dst=%p src=%p n=%d zclip=%d flag=%d", dst, src, nbvertex, zclip, flag'),
        
        ('LIB386/pol_work/TESTVUEF.ASM', 'LIB386/pol_work/TESTVUEF.CPP',
         'TestVuePolyF', '[ASM][TestVuePolyF][1] input=%p',
         ['esi'],
         None, '"input=%p", (void*)input'),
        
        ('LIB386/pol_work/POLYLINE.ASM', 'LIB386/pol_work/POLYLINE.CPP',
         'Line', '[ASM][Line][1] entry',
         [],
         None, '"entry"'),
        
        ('LIB386/pol_work/POLYDISC.ASM', 'LIB386/pol_work/POLYDISC.CPP',
         'Fill_Sphere', '[ASM][Fill_Sphere][1] entry',
         [],
         None, '"entry"'),
    ]
    
    for entry in special_polwork:
        asm_path, cpp_path, func, fmt_text, push_args, anchor, cpp_trace = entry
        print(f"\n--- {func} ---")
        process_asm_file(asm_path, func, fmt_text, push_args, anchor)
        instrument_cpp(cpp_path, func, cpp_trace)
    
    # POLY.ASM - main Draw_Triangle
    print("\n=== POLY.ASM (Draw_Triangle) ===")
    process_asm_file('LIB386/pol_work/POLY.ASM', 'Draw_Triangle',
                     '[ASM][Draw_Triangle][1] entry', [], None)
    instrument_cpp('LIB386/pol_work/POLY.CPP', 'Draw_Triangle', '"entry"')
    
    # AFF_OBJ
    print("\n=== AFF_OBJ ===")
    for func, fmt, args, cpp_args in [
        ('ObjectDisplay', '[ASM][ObjectDisplay][1] obj=%p', ['esi'],
         '"obj=%p", (void*)This'),
        ('BodyDisplay', '[ASM][BodyDisplay][1] entry', [],
         '"entry"'),
    ]:
        process_asm_file('LIB386/OBJECT/AFF_OBJ.ASM', func, fmt, args, None)
        instrument_cpp('LIB386/OBJECT/AFF_OBJ.CPP', func, cpp_args)
    
    # SORT
    print("\n=== SORT ===")
    process_asm_file('LIB386/MENU/SORT.ASM', 'MySortCompFunc',
                     '[ASM][MySortCompFunc][1] a=%p b=%p', ['ptrb', 'ptra'], None)
    instrument_cpp('LIB386/MENU/SORT.CPP', 'MySortCompFunc',
                   '"a=%p b=%p", (void*)ptra, (void*)ptrb')
    
    # SVGA files that need special handling
    print("\n=== SVGA Special ===")
    for func, asm_path, cpp_path, fmt_text, push_args, cpp_trace in [
        ('CopyMask', 'LIB386/SVGA/COPYMASK.ASM', 'LIB386/SVGA/COPYMASK.CPP',
         '[ASM][CopyMask][1] num=%d x=%d y=%d bank=%p src=%p',
         ['src', 'bankmask', 'y', 'x', 'nummask'],
         '"num=%d x=%d y=%d bank=%p src=%p", nummask, x, y, (void*)bankmask, (void*)src'),
        ('BlitBoxF', 'LIB386/SVGA/BLITBOXF.ASM', 'LIB386/SVGA/BLITBOXF.CPP',
         '[ASM][BlitBoxF][1] dst=%p src=%p',
         ['esi', 'edi'],
         '"dst=%p src=%p", dst, src'),
        ('Box', 'LIB386/SVGA/BOX.ASM', 'LIB386/SVGA/BOX.CPP',
         '[ASM][Box][1] x0=%d y0=%d x1=%d y1=%d col=%d',
         ['col', 'y1', 'x1', 'y0', 'x0'],
         '"x0=%d y0=%d x1=%d y1=%d col=%d", x0, y0, x1, y1, col'),
    ]:
        process_asm_file(asm_path, func, fmt_text, push_args, None)
        instrument_cpp(cpp_path, func, cpp_trace)
    
    # CLRBOXF has ClearBoxF (function pointer name vs PROC name)
    process_asm_file('LIB386/SVGA/CLRBOXF.ASM', 'ClearBoxF',
                     '[ASM][ClearBoxF][1] dst=%p tab=%p box=%p',
                     ['box', 'TabOffDst', 'dst'], None)
    instrument_cpp('LIB386/SVGA/CLRBOXF.CPP', 'ClearBoxF',
                   '"dst=%p tab=%p box=%p", dst, (void*)TabOffDst, (void*)box')
    
    print("\n=== Done! ===")


if __name__ == '__main__':
    main()
