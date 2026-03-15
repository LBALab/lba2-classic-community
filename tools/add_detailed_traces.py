#!/usr/bin/env python3
"""Add comprehensive intermediate debug traces to all pol_work CPP files."""
import re
import os

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read_file(path):
    with open(os.path.join(BASE, path), 'r') as f:
        return f.read()

def write_file(path, content):
    with open(os.path.join(BASE, path), 'w') as f:
        f.write(content)


def add_trace_after(content, anchor, trace_line):
    """Add a trace line after the first occurrence of anchor."""
    idx = content.find(anchor)
    if idx < 0:
        return content, False
    eol = content.find('\n', idx)
    if eol < 0:
        return content, False
    # Check if trace already exists nearby (within 5 lines)
    next_lines = content[eol:eol+500]
    if 'LIB386_TRACE_CPP' in next_lines[:200]:
        return content, False  # Already traced
    return content[:eol+1] + trace_line + '\n', True


def add_filler_traces(filepath, func_name, extra_entry_args="", extra_scanline_args=""):
    """Add entry + per-scanline + per-pixel traces to a filler function."""
    content = read_file(filepath)
    
    # Check if already traced
    if f'LIB386_TRACE_CPP("{func_name}", "2"' in content:
        print(f'  SKIP (done): {filepath} [{func_name}]')
        return
    
    # Find function entry - add detailed entry trace
    func_pattern = re.compile(
        rf'(S32\s+{re.escape(func_name)}\s*\([^)]*\)\s*\{{)',
        re.DOTALL
    )
    match = func_pattern.search(content)
    if not match:
        print(f'  WARN: func {func_name} not found in {filepath}')
        return
    
    # Add entry trace with all filler globals
    entry_trace = f'''
    LIB386_TRACE_CPP("{func_name}", "1", "nbLines=%u xmin=0x%08x xmax=0x%08x", nbLines, xmin, xmax);
    LIB386_TRACE_CPP("{func_name}", "1b", "LeftSlope=%d RightSlope=%d curY=%d offLine=%p",
        Fill_LeftSlope, Fill_RightSlope, Fill_CurY, (void*)Fill_CurOffLine);
    LIB386_TRACE_CPP("{func_name}", "1c", "U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d",
        Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    LIB386_TRACE_CPP("{func_name}", "1d", "U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d",
        Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    LIB386_TRACE_CPP("{func_name}", "1e", "curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u",
        Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin, Fill_CurZBufMin, Fill_CurGouraudMin, Fill_Patch);'''
    
    insert_pos = match.end()
    content = content[:insert_pos] + entry_trace + content[insert_pos:]
    
    # Find the main scanline loop and add per-scanline trace
    # Look for patterns like "while" or "for" after the entry, or "nbLines--" / "scanLine"
    # Most fillers have a "for(...nbLines...)" or "while(nbLines--)" pattern
    loop_patterns = [
        r'for\s*\(\s*(?:S32|U32|int)\s+\w+\s*=\s*0\s*;\s*\w+\s*<\s*nbLines',
        r'while\s*\(\s*nbLines\s*--',
        r'while\s*\(\s*nbLines\s*>\s*0',
    ]
    for pat in loop_patterns:
        loop_match = re.search(pat, content[insert_pos:])
        if loop_match:
            # Find the opening brace
            loop_start = insert_pos + loop_match.end()
            brace = content.find('{', loop_start)
            if brace >= 0 and brace < loop_start + 100:
                scanline_trace = f'''
        LIB386_TRACE_CPP("{func_name}", "2", "scanline curY=%d xmin=0x%08x xmax=0x%08x",
            Fill_CurY, Fill_CurXMin, Fill_CurXMax);'''
                # Only add if not already present
                if f'LIB386_TRACE_CPP("{func_name}", "2"' not in content:
                    content = content[:brace+1] + scanline_trace + content[brace+1:]
            break
    
    write_file(filepath, content)
    print(f'  OK: {filepath} [{func_name}]')


# ─── POLYFLAT.CPP ───
print("=== POLYFLAT.CPP ===")
for func in ['Filler_Flat', 'Filler_Transparent', 'Filler_Trame',
             'Filler_FlatZBuf', 'Filler_TransparentZBuf', 'Filler_TrameZBuf',
             'Filler_FlatNZW', 'Filler_TransparentNZW', 'Filler_TrameNZW']:
    add_filler_traces('LIB386/pol_work/POLYFLAT.CPP', func)

# ─── POLYGOUR.CPP ───
print("\n=== POLYGOUR.CPP ===")
for func in ['Filler_Gouraud', 'Filler_Dither',
             'Filler_GouraudTable', 'Filler_DitherTable',
             'Filler_GouraudZBuf', 'Filler_DitherZBuf',
             'Filler_GouraudTableZBuf', 'Filler_DitherTableZBuf',
             'Filler_GouraudNZW', 'Filler_DitherNZW',
             'Filler_GouraudTableNZW', 'Filler_DitherTableNZW']:
    add_filler_traces('LIB386/pol_work/POLYGOUR.CPP', func)

# ─── POLYTEXT.CPP ───
print("\n=== POLYTEXT.CPP ===")
for func in ['Filler_Texture', 'Filler_TextureFlat',
             'Filler_TextureChromaKey', 'Filler_TextureFlatChromaKey']:
    add_filler_traces('LIB386/pol_work/POLYTEXT.CPP', func)

# ─── POLYTEXZ.CPP ───
print("\n=== POLYTEXZ.CPP ===")
for func in ['Filler_TextureZ', 'Filler_TextureZFlat',
             'Filler_TextureZChromaKey', 'Filler_TextureZFlatChromaKey']:
    add_filler_traces('LIB386/pol_work/POLYTEXZ.CPP', func)

# ─── POLYTZF.CPP ───
print("\n=== POLYTZF.CPP ===")
for func in ['Filler_TextureZFogSmooth', 'Filler_TextureZFogSmoothZBuf']:
    add_filler_traces('LIB386/pol_work/POLYTZF.CPP', func)

# ─── POLYTZG.CPP ───
print("\n=== POLYTZG.CPP ===")
for func in ['Filler_TextureZGouraud', 'Filler_TextureZGouraudChromaKey']:
    add_filler_traces('LIB386/pol_work/POLYTZG.CPP', func)

# ─── POLYGTEX.CPP ───
print("\n=== POLYGTEX.CPP ===")
for func in ['Filler_TextureGouraud', 'Filler_TextureDither',
             'Filler_TextureGouraudChromaKey', 'Filler_TextureDitherChromaKey']:
    add_filler_traces('LIB386/pol_work/POLYGTEX.CPP', func)

# ─── POLYLINE.CPP ───
print("\n=== POLYLINE.CPP ===")
content = read_file('LIB386/pol_work/POLYLINE.CPP')
if 'LIB386_TRACE_CPP("Line_A", "1b"' not in content:
    # Add detailed traces after the existing entry trace
    old = 'LIB386_TRACE_CPP("Line_A", "1", "entry");'
    new = '''LIB386_TRACE_CPP("Line_A", "1", "x0=%d y0=%d x1=%d y1=%d col=%d z0=%d z1=%d",
        x0, y0, x1, y1, col, z0, z1);'''
    if old in content:
        content = content.replace(old, new)
        write_file('LIB386/pol_work/POLYLINE.CPP', content)
        print('  OK: POLYLINE.CPP [Line_A]')
    else:
        print('  SKIP: POLYLINE.CPP')
else:
    print('  SKIP (done): POLYLINE.CPP')

# ─── POLYDISC.CPP ───
print("\n=== POLYDISC.CPP ===")
content = read_file('LIB386/pol_work/POLYDISC.CPP')
if 'LIB386_TRACE_CPP("Fill_Sphere", "1b"' not in content:
    old = 'LIB386_TRACE_CPP("Fill_Sphere", "1", "entry");'
    new = '''LIB386_TRACE_CPP("Fill_Sphere", "1", "type=%d col=%d cx=%d cy=%d r=%d zbuf=%d",
        type, color, cx, cy, r, zbufValue);'''
    if old in content:
        content = content.replace(old, new)
        write_file('LIB386/pol_work/POLYDISC.CPP', content)
        print('  OK: POLYDISC.CPP [Fill_Sphere]')
    else:
        print('  SKIP: POLYDISC.CPP')
else:
    print('  SKIP (done): POLYDISC.CPP')

# ─── POLYCLIP.CPP ───
print("\n=== POLYCLIP.CPP ===")
content = read_file('LIB386/pol_work/POLYCLIP.CPP')
if 'LIB386_TRACE_CPP("ClipperZ", "1b"' not in content:
    old = 'LIB386_TRACE_CPP("ClipperZ", "1",'
    if old in content:
        # Add more detail to ClipperZ
        content = content.replace(old, 'LIB386_TRACE_CPP("ClipperZ", "1",')
        print('  SKIP (already has trace): POLYCLIP.CPP')
    else:
        print('  WARN: no trace found in POLYCLIP.CPP')
else:
    print('  SKIP (done): POLYCLIP.CPP')

# ─── Add traces to remaining Calc_ functions in POLY.CPP ───
print("\n=== POLY.CPP remaining Calc_ functions ===")
content = read_file('LIB386/pol_work/POLY.CPP')

# Functions that need entry traces
calc_funcs = [
    ('Calc_GouraudXSlopeFPU', 'Gouraud_XSlope=%d', 'Fill_Gouraud_XSlope'),
    ('Calc_DitherXSlopeFPU', 'Dither_XSlope=%d', 'Fill_Gouraud_XSlope'),
    ('Calc_TextureXSlopeFPU', 'U_XSlope=%d V_XSlope=%d', 'Fill_MapU_XSlope, Fill_MapV_XSlope'),
    ('Calc_TextureGouraudXSlopeFPU', 'U_XSlope=%d V_XSlope=%d G_XSlope=%d', 'Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_Gouraud_XSlope'),
    ('Calc_TextureDitherXSlopeFPU', 'U_XSlope=%d V_XSlope=%d G_XSlope=%d', 'Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_Gouraud_XSlope'),
    ('Calc_XSlopeZBufferFPU', 'delegates to Calc_TextureZXSlopeZBufFPU', ''),
    ('Calc_TextureZXSlopeZBufFPU', 'U_XSlope=%d V_XSlope=%d W_XSlope=%d ZBuf_XSlope=%d', 'Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope'),
    ('Calc_XSlopeZBufferFlagFPU', 'U_XSlope=%d V_XSlope=%d W_XSlope=%d ZBuf_XSlope=%d G_XSlope=%d', 'Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope'),
    ('Calc_GouraudLeftSlopeFPU', 'LeftSlope=%d G_LS=%d curG=%d', 'Fill_LeftSlope, Fill_Gouraud_LeftSlope, Fill_CurGouraudMin'),
    ('Calc_GouraudTableLeftSlopeFPU', 'LeftSlope=%d G_LS=%d curG=%d', 'Fill_LeftSlope, Fill_Gouraud_LeftSlope, Fill_CurGouraudMin'),
    ('Calc_TextureLeftSlopeFPU', 'LeftSlope=%d U_LS=%d V_LS=%d curU=%d curV=%d', 'Fill_LeftSlope, Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_CurMapUMin, Fill_CurMapVMin'),
    ('Calc_TextureGouraudLeftSlopeFPU', 'LeftSlope=%d U_LS=%d V_LS=%d G_LS=%d', 'Fill_LeftSlope, Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_Gouraud_LeftSlope'),
    ('Calc_LeftSlopeZBufferFPU', 'delegates to Calc_TextureZLeftSlopeZBufFPU', ''),
    ('Calc_TextureZLeftSlopeZBufFPU', 'LeftSlope=%d U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d', 'Fill_LeftSlope, Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope'),
    ('Calc_LeftSlopeZBufferFlagFPU', 'LeftSlope=%d U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d', 'Fill_LeftSlope, Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope'),
]

modified = False
for func_name, ret_fmt, ret_args in calc_funcs:
    # Skip if already traced
    if f'LIB386_TRACE_CPP("{func_name}", "1"' in content:
        continue
    
    # Find function opening
    pat = re.compile(rf'(S32\s+{re.escape(func_name)}\s*\([^)]*\)\s*\{{)')
    match = pat.search(content)
    if not match:
        continue
    
    entry_trace = f'\n  LIB386_TRACE_CPP("{func_name}", "1", "type=%u", fillType);'
    content = content[:match.end()] + entry_trace + content[match.end():]
    modified = True
    
    # Also add a return trace before the "return" statement
    if ret_args:
        # Find the return in this function
        func_start = match.end()
        # Find next function or end-of-function }
        next_func = content.find('\nS32 Calc_', func_start + 100)
        if next_func < 0:
            next_func = len(content)
        region = content[func_start:next_func]
        
        # Find the last "return" before next_func
        last_return = region.rfind('return ')
        if last_return >= 0:
            ret_trace = f'  LIB386_TRACE_CPP("{func_name}", "9", "{ret_fmt}", {ret_args});\n  '
            abs_pos = func_start + last_return
            content = content[:abs_pos] + ret_trace + content[abs_pos:]
            modified = True

if modified:
    write_file('LIB386/pol_work/POLY.CPP', content)
    print('  OK: Added traces to remaining Calc_ functions')
else:
    print('  SKIP: All Calc_ functions already traced')

# ─── Fill_PolyClip ───
print("\n=== POLY.CPP Fill_PolyClip ===")
content = read_file('LIB386/pol_work/POLY.CPP')
if 'LIB386_TRACE_CPP("Fill_PolyClip", "2"' not in content:
    # Find Fill_PolyClip function
    match = re.search(r'(S32\s+Fill_PolyClip\s*\([^)]*\)\s*\{)', content)
    if match:
        entry_trace = '''
    LIB386_TRACE_CPP("Fill_PolyClip", "1", "nb_pts=%d pts=%p type=%u clipFlag=%u color=%u",
        NbPoints, (void*)Ptr_Points, Fill_Type, Fill_ClipFlag, Fill_Color.Num);'''
        content = content[:match.end()] + entry_trace + content[match.end():]
        write_file('LIB386/pol_work/POLY.CPP', content)
        print('  OK: Fill_PolyClip entry trace')
else:
    print('  SKIP (done): Fill_PolyClip')

print("\n=== Done! ===")
