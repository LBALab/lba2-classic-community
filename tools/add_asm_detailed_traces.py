#!/usr/bin/env python3
"""
Add comprehensive intermediate debug traces to all pol_work ASM files.
Matches the CPP traces already in place.
"""
import re
import os

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def read_file(path):
    with open(os.path.join(BASE, path), 'r', encoding='utf-8', errors='replace') as f:
        return f.read()

def write_file(path, content):
    with open(os.path.join(BASE, path), 'w', encoding='utf-8') as f:
        f.write(content)


def make_trace(func, tid, desc, fmt_label, push_args):
    """Generate an ASM trace block. push_args: list of what to push (right-to-left for printf)."""
    lines = [
        f'\t\t; --- debug trace [ASM][{func}][{tid}] {desc} ---',
        '\t\tpushad',
    ]
    for arg in push_args:
        if arg.startswith('[') or arg.startswith('dword'):
            lines.append(f'\t\tpush\t{arg}')
        else:
            lines.append(f'\t\tpush\t{arg}')
    lines.append(f'\t\tpush\toffset {fmt_label}')
    lines.append('\t\tcall\tprintf')
    cleanup = 4 * (len(push_args) + 1)
    lines.append(f'\t\tadd\tesp, {cleanup}')
    lines.append('\t\tpopad')
    lines.append(f'\t\t; --- end debug trace ---')
    return '\n'.join(lines)


def add_fmt_to_data(content, label, text):
    """Add a format string to .DATA or _DATA section if not already present."""
    if label in content:
        return content
    # Try to find the .CODE or _TEXT section and insert before it
    for marker in ['\n\t\t\t.CODE', '\n\t\t.CODE', '\n_TEXT']:
        idx = content.find(marker)
        if idx >= 0:
            line = f'\n{label}\tDB\t"{text}", 10, 0\n'
            return content[:idx] + line + content[idx:]
    return content


def insert_before(content, anchor, block, start_after=0):
    """Insert block before the first occurrence of anchor after start_after."""
    idx = content.find(anchor, start_after)
    if idx < 0:
        return content, -1
    # Find start of the line containing anchor
    line_start = content.rfind('\n', 0, idx)
    if line_start < 0:
        line_start = 0
    else:
        line_start += 1
    return content[:line_start] + block + '\n' + content[line_start:], idx


def insert_after_line(content, anchor, block, start_after=0):
    """Insert block after the line containing anchor."""
    idx = content.find(anchor, start_after)
    if idx < 0:
        return content, -1
    eol = content.find('\n', idx)
    if eol < 0:
        eol = len(content)
    return content[:eol+1] + block + '\n', idx


# ═══════════════════════════════════════════════════════════════
# POLY.ASM — the most critical file
# ═══════════════════════════════════════════════════════════════
print("=== POLY.ASM ===")
content = read_file('LIB386/pol_work/POLY.ASM')

# Check if already comprehensively traced
if '[ASM][Draw_Triangle][3]' in content:
    print("  SKIP (already has comprehensive traces)")
else:
    # Add format strings
    fmt_strings = [
        ('fmt_dt_1b', '[ASM][Draw_Triangle][1b] p0=(%d,%d) u=%u v=%u w=%d light=%u zo=%u'),
        ('fmt_dt_1c', '[ASM][Draw_Triangle][1c] p1=(%d,%d) u=%u v=%u w=%d light=%u zo=%u'),
        ('fmt_dt_1d', '[ASM][Draw_Triangle][1d] p2=(%d,%d) u=%u v=%u w=%d light=%u zo=%u'),
        ('fmt_dt_3', '[ASM][Draw_Triangle][3] PtA=(%d,%d) PtB=(%d,%d) PtC=(%d,%d) type=%u'),
        ('fmt_dt_9', '[ASM][Draw_Triangle][9] return=%d LeftSlope=%d'),
        ('fmt_ts_1', '[ASM][Test_Scan][1] diffY=%d readFlag=%u curY=%d xmin=0x%08x xmax=0x%08x'),
        ('fmt_ts_2', '[ASM][Test_Scan][2] LeftSlope=%d RightSlope=%d'),
        ('fmt_ts_4', '[ASM][Test_Scan][4] calling filler nbLines=%d xmin=0x%08x xmax=0x%08x'),
        ('fmt_trne_1', '[ASM][Triangle_ReadNextEdge][1] readFlag=%u curY=%d'),
        ('fmt_trne_2', '[ASM][Triangle_ReadNextEdge][2] left new=(%d,%d) CurXMin=0x%08x'),
        ('fmt_rnr_1', '[ASM][Read_Next_Right][1] readFlag=%u curY=%d'),
        ('fmt_rnr_2', '[ASM][Read_Next_Right][2] right new=(%d,%d) CurXMax=0x%08x RSlope=%d'),
        ('fmt_fpc_1', '[ASM][Fill_PolyClip][1] nb=%d pts=%p type=%u clipFlag=%u color=%u'),
        ('fmt_txzxs_9', '[ASM][Calc_TextureZXSlopeFPU][9] U_XS=%d V_XS=%d W_XS=%d'),
        ('fmt_txzgxs_9', '[ASM][Calc_TextureZGouraudXSlopeFPU][9] U_XS=%d V_XS=%d G_XS=%d W_XS=%d'),
        ('fmt_zbxs_9', '[ASM][Calc_ZBufXSlopeFPU][9] ZBuf_XS=%d'),
        ('fmt_txzxsz_9', '[ASM][Calc_TextureZXSlopeZBufFPU][9] U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d'),
        ('fmt_txzls_9', '[ASM][Calc_TextureZLeftSlopeFPU][9] LS=%d U_LS=%d V_LS=%d W_LS=%d curU=%d curV=%d curW=%d'),
        ('fmt_txzgls_9', '[ASM][Calc_TextureZGouraudLeftSlopeFPU][9] LS=%d G_LS=%d U_LS=%d V_LS=%d W_LS=%d'),
        ('fmt_zbls_9', '[ASM][Calc_ZBufLeftSlopeFPU][9] ZBuf_LS=%d CurZBuf=%u'),
        ('fmt_gxs_9', '[ASM][Calc_GouraudXSlopeFPU][9] G_XS=%d'),
        ('fmt_txs_9', '[ASM][Calc_TextureXSlopeFPU][9] U_XS=%d V_XS=%d'),
        ('fmt_gls_9', '[ASM][Calc_GouraudLeftSlopeFPU][9] LS=%d G_LS=%d curG=%d'),
        ('fmt_tls_9', '[ASM][Calc_TextureLeftSlopeFPU][9] LS=%d U_LS=%d V_LS=%d curU=%d curV=%d'),
    ]
    for label, text in fmt_strings:
        content = add_fmt_to_data(content, label, text)
    
    # --- Draw_Triangle entry: add vertex detail traces ---
    # Find existing entry trace and add vertex detail traces after it
    dt_end_marker = '; --- end debug trace ---'
    dt_entry = content.find('[ASM][Draw_Triangle][1]')
    if dt_entry >= 0:
        dt_end = content.find(dt_end_marker, dt_entry)
        if dt_end >= 0:
            dt_end += len(dt_end_marker)
            # ESI points to Ptr_Points at this point
            # Each Struc_Point is 32 bytes: Pt_XE(+0,s16), Pt_YE(+2,s16), 
            # Pt_MapU(+12,u16), Pt_MapV(+14,u16), Pt_W(+16,s32), Pt_Light(+20,u16), Pt_ZO(+22,u16)
            traces = []
            for i, label in enumerate(['fmt_dt_1b', 'fmt_dt_1c', 'fmt_dt_1d']):
                off = i * 32  # sizeof(Struc_Point) = 32 bytes
                traces.append(f'''
\t\t; --- debug trace [ASM][Draw_Triangle][1{chr(98+i)}] vertex {i} ---
\t\tpushad
\t\tmovsx\teax, word ptr [esi+{off}+22]  ; ZO
\t\tpush\teax
\t\tmovsx\teax, word ptr [esi+{off}+20]  ; Light
\t\tpush\teax
\t\tpush\tdword ptr [esi+{off}+16]       ; W
\t\tmovzx\teax, word ptr [esi+{off}+14]  ; MapV
\t\tpush\teax
\t\tmovzx\teax, word ptr [esi+{off}+12]  ; MapU
\t\tpush\teax
\t\tmovsx\teax, word ptr [esi+{off}+2]   ; YE
\t\tpush\teax
\t\tmovsx\teax, word ptr [esi+{off}]     ; XE
\t\tpush\teax
\t\tpush\toffset {label}
\t\tcall\tprintf
\t\tadd\tesp, 32
\t\tpopad
\t\t; --- end debug trace ---''')
            content = content[:dt_end] + '\n'.join(traces) + '\n' + content[dt_end:]
            print("  OK: Draw_Triangle vertex detail traces")
    
    # --- Draw_Triangle[3]: After vertex sort, before denom calc ---
    # The sorted vertices are PtA(edi), PtB(esi), PtC(ebx) at this point
    # Find "fdivp  st(3), st" which is right after denom calc
    # Actually, let's find the point after sorting and fild denom calc
    # Look for "InvDenom" comment or the fdivp that computes it
    
    # --- Draw_Triangle[9]: return trace ---
    # Find "@@EndFPU:" label and add return trace after fldcw
    endfpu = content.find('@@EndFPU:')
    if endfpu >= 0:
        # Find the ret after @@EndFPU
        ret_after_endfpu = content.find('\t\tret', endfpu)
        if ret_after_endfpu >= 0:
            ret_trace = make_trace('Draw_Triangle', '9', 'return', 'fmt_dt_9', [
                'dword ptr [Fill_LeftSlope]', 'eax'
            ])
            content = content[:ret_after_endfpu] + ret_trace + '\n' + content[ret_after_endfpu:]
            print("  OK: Draw_Triangle return trace")
    
    # --- Test_Scan traces ---
    # @@Test_Scan is the dispatch point. At entry, ecx=diffY, and the jmp [Fill_Filler] is the call
    ts_label = content.find('@@Test_Scan:')
    if ts_label >= 0:
        # Add entry trace right after the label
        ts_trace = make_trace('Test_Scan', '1', 'entry', 'fmt_ts_1', [
            'dword ptr [Fill_CurXMax]', 'dword ptr [Fill_CurXMin]',
            'dword ptr [Fill_CurY]', 'dword ptr [Fill_ReadFlag]', 'ecx'
        ])
        ts_trace2 = make_trace('Test_Scan', '2', 'slopes', 'fmt_ts_2', [
            'dword ptr [Fill_RightSlope]', 'dword ptr [Fill_LeftSlope]'
        ])
        eol = content.find('\n', ts_label)
        content = content[:eol+1] + '\n' + ts_trace + '\n' + ts_trace2 + '\n' + content[eol+1:]
        print("  OK: Test_Scan entry traces")
    
    # --- Fill_PolyClip entry trace ---
    fpc = content.find('Fill_PolyClip\t\tPROC')
    if fpc < 0:
        fpc = content.find('Fill_PolyClip\tPROC')
    if fpc >= 0:
        # Find first instruction after PROC
        eol = content.find('\n', fpc)
        # Skip blank/comment lines
        next_line = eol + 1
        while content[next_line:next_line+2] in ['\n', '\t;', '; ']:
            next_line = content.find('\n', next_line) + 1
        fpc_trace = make_trace('Fill_PolyClip', '1', 'params', 'fmt_fpc_1', [
            'dword ptr [Fill_Color]', 'dword ptr [Fill_ClipFlag]',
            'dword ptr [Fill_Type]', 'esi', 'ecx'
        ])
        content = content[:next_line] + fpc_trace + '\n\n' + content[next_line:]
        print("  OK: Fill_PolyClip entry trace")

    # --- Calc_TextureZXSlopeFPU return trace ---
    # This is the fish after fistp [Fill_W_XSlope] followed by jmp Triangle_ReadNextEdge
    # Find line: fistp [Fill_W_XSlope] followed by jmp Triangle_ReadNextEdge
    # There are multiple slope calculators. Let me add return traces before each jmp Triangle_ReadNextEdge
    # BUT some functions don't write W_XSlope (e.g. Gouraud only writes Gouraud_XSlope)
    
    # Strategy: find each "jmp Triangle_ReadNextEdge" and add a trace before it
    # based on what fistp writes happened before it
    
    # Actually, a simpler approach: add return traces for the KEY slope calculators
    # Find "Calc_TextureZXSlopeFPU" PROC area (it's after a comment with that name)
    
    # Add traces before specific jmp Triangle_ReadNextEdge calls
    # Line 3682: after fistp Fill_W_XSlope - this is the TextureZ x-slope calc
    # Line 4154: after fistp Fill_W_XSlope - this is TextureZGouraud x-slope
    # Line 4834: after fistp Fill_W_XSlope - this is TextureZXSlopeZBuf
    
    # Add return traces for slope calculators by finding fistp patterns
    txz_pattern = 'fistp'
    # Use regex to match fistp with any whitespace before [Fill_W_XSlope]
    import re as _re
    pos = 0
    found_txz = 0
    for m in _re.finditer(r'fistp\s+\[Fill_W_XSlope\]', content):
        idx = m.start()
        eol = content.find('\n', idx)
        if eol < 0:
            continue
        # Check if trace already present right after
        if 'debug trace' in content[eol:eol+200]:
            continue
        # Check what fistp came before - need at least U and V
        region = content[max(0,idx-800):idx]
        has_u = bool(_re.search(r'fistp\s+\[Fill_MapU_XSlope\]', region))
        has_v = bool(_re.search(r'fistp\s+\[Fill_MapV_XSlope\]', region))
        has_g = bool(_re.search(r'fistp\s+\[Fill_Gouraud_XSlope\]', region))
        has_z = bool(_re.search(r'fistp\s+\[Fill_ZBuf_XSlope\]', region))
        
        if has_u and has_v and not has_g and not has_z:
            # TextureZ basic
            trace = make_trace('Calc_TextureZXSlopeFPU', '9', 'return', 'fmt_txzxs_9', [
                'dword ptr [Fill_W_XSlope]',
                'dword ptr [Fill_MapV_XSlope]',
                'dword ptr [Fill_MapU_XSlope]',
            ])
            content = content[:eol+1] + trace + '\n' + content[eol+1:]
            found_txz += 1
        elif has_u and has_v and has_g and not has_z:
            # TextureZGouraud
            trace = make_trace('Calc_TextureZGouraudXSlopeFPU', '9', 'return', 'fmt_txzgxs_9', [
                'dword ptr [Fill_W_XSlope]',
                'dword ptr [Fill_Gouraud_XSlope]',
                'dword ptr [Fill_MapV_XSlope]',
                'dword ptr [Fill_MapU_XSlope]',
            ])
            content = content[:eol+1] + trace + '\n' + content[eol+1:]
            found_txz += 1
        elif has_u and has_v and has_z:
            # TextureZXSlopeZBuf
            trace = make_trace('Calc_TextureZXSlopeZBufFPU', '9', 'return', 'fmt_txzxsz_9', [
                'dword ptr [Fill_ZBuf_XSlope]',
                'dword ptr [Fill_W_XSlope]',
                'dword ptr [Fill_MapV_XSlope]',
                'dword ptr [Fill_MapU_XSlope]',
            ])
            content = content[:eol+1] + trace + '\n' + content[eol+1:]
            found_txz += 1
    print(f"  OK: Added {found_txz} TextureZ x-slope return traces")
    
    # Add ZBuf x-slope trace (after fistp [Fill_ZBuf_XSlope] NOT followed by TextureZ fistp)
    pos = 0
    found_zb = 0
    for m in _re.finditer(r'fistp\s+\[Fill_ZBuf_XSlope\]', content):
        idx = m.start()
        eol = content.find('\n', idx)
        if 'debug trace' in content[eol:eol+200]:
            continue
        # Check if followed by fistp Fill_MapU (which means it's a composite calc, already traced)
        next_200 = content[eol:eol+200]
        if not _re.search(r'fistp\s+\[Fill_MapU_XSlope\]', next_200):
            trace = make_trace('Calc_ZBufXSlopeFPU', '9', 'return', 'fmt_zbxs_9', [
                'dword ptr [Fill_ZBuf_XSlope]',
            ])
            content = content[:eol+1] + trace + '\n' + content[eol+1:]
            found_zb += 1
    print(f"  OK: Added {found_zb} ZBuf x-slope return traces")
    
    # Add Gouraud x-slope trace (after fistp [Fill_Gouraud_XSlope] not followed by W)
    pos = 0
    found_g = 0
    for m in _re.finditer(r'fistp\s+\[Fill_Gouraud_XSlope\]', content):
        idx = m.start()
        eol = content.find('\n', idx)
        if 'debug trace' in content[eol:eol+200]:
            continue
        next_100 = content[eol:eol+100]
        if not _re.search(r'fistp\s+\[Fill_W_XSlope\]', next_100):
            # Standalone Gouraud (not TextureZGouraud)
            trace = make_trace('Calc_GouraudXSlopeFPU', '9', 'return', 'fmt_gxs_9', [
                'dword ptr [Fill_Gouraud_XSlope]',
            ])
            content = content[:eol+1] + trace + '\n' + content[eol+1:]
            found_g += 1
    print(f"  OK: Added {found_g} Gouraud x-slope return traces")
    
    # Add Texture x-slope trace (after fistp [Fill_MapV_XSlope] not followed by G or W)
    pos = 0
    found_t = 0
    for m in _re.finditer(r'fistp\s+\[Fill_MapV_XSlope\]', content):
        idx = m.start()
        eol = content.find('\n', idx)
        if 'debug trace' in content[eol:eol+200]:
            continue
        next_300 = content[eol:eol+300]
        if not _re.search(r'fistp\s+\[Fill_Gouraud_XSlope\]', next_300) and not _re.search(r'fistp\s+\[Fill_W_XSlope\]', next_300):
            trace = make_trace('Calc_TextureXSlopeFPU', '9', 'return', 'fmt_txs_9', [
                'dword ptr [Fill_MapV_XSlope]',
                'dword ptr [Fill_MapU_XSlope]',
            ])
            content = content[:eol+1] + trace + '\n' + content[eol+1:]
            found_t += 1
    print(f"  OK: Added {found_t} Texture x-slope return traces")

    # --- LeftSlope return traces ---
    # Find fistp [Fill_LeftSlope] patterns and add traces based on context
    pos = 0
    found_ls = 0
    for m in _re.finditer(r'fistp\s+\[Fill_LeftSlope\]', content):
        idx = m.start()
        eol = content.find('\n', idx)
        if 'debug trace' in content[eol:eol+200]:
            continue
        # Check what follows within 800 bytes
        region_after = content[eol:eol+800]
        
        # Determine which left slope calculator this is
        has_w_ls = bool(_re.search(r'fistp\s+\[Fill_W_LeftSlope\]', region_after))
        has_g_ls = bool(_re.search(r'fistp\s+\[Fill_Gouraud_LeftSlope\]', region_after))
        has_u_ls = bool(_re.search(r'fistp\s+\[Fill_MapU_LeftSlope\]', region_after))
        has_z_ls = bool(_re.search(r'fistp\s+\[Fill_ZBuf_LeftSlope\]', region_after))
        
        # Find the jmp Read_Next_Right after this
        jmp_rnr = content.find('jmp\tRead_Next_Right', eol)
        if jmp_rnr < 0 or jmp_rnr > eol + 3000:
            continue  # Can't find the end of this function
        
        # Make sure no other debug trace is already before jmp_rnr
        if 'debug trace' in content[jmp_rnr-200:jmp_rnr]:
            continue
        
        if has_w_ls and not has_g_ls and not has_z_ls:
            # TextureZ left slope
            trace = make_trace('Calc_TextureZLeftSlopeFPU', '9', 'return', 'fmt_txzls_9', [
                'dword ptr [Fill_CurWMin]',
                'dword ptr [Fill_CurMapVMin]',
                'dword ptr [Fill_CurMapUMin]',
                'dword ptr [Fill_W_LeftSlope]',
                'dword ptr [Fill_MapV_LeftSlope]',
                'dword ptr [Fill_MapU_LeftSlope]',
                'dword ptr [Fill_LeftSlope]',
            ])
            content = content[:jmp_rnr] + trace + '\n' + content[jmp_rnr:]
            found_ls += 1
        elif has_g_ls and has_w_ls:
            # TextureZGouraud left slope
            trace = make_trace('Calc_TextureZGouraudLeftSlopeFPU', '9', 'return', 'fmt_txzgls_9', [
                'dword ptr [Fill_W_LeftSlope]',
                'dword ptr [Fill_MapV_LeftSlope]',
                'dword ptr [Fill_MapU_LeftSlope]',
                'dword ptr [Fill_Gouraud_LeftSlope]',
                'dword ptr [Fill_LeftSlope]',
            ])
            content = content[:jmp_rnr] + trace + '\n' + content[jmp_rnr:]
            found_ls += 1
        elif has_g_ls and not has_w_ls and not has_u_ls:
            # Gouraud only left slope
            trace = make_trace('Calc_GouraudLeftSlopeFPU', '9', 'return', 'fmt_gls_9', [
                'dword ptr [Fill_CurGouraudMin]',
                'dword ptr [Fill_Gouraud_LeftSlope]',
                'dword ptr [Fill_LeftSlope]',
            ])
            content = content[:jmp_rnr] + trace + '\n' + content[jmp_rnr:]
            found_ls += 1
        elif has_u_ls and not has_w_ls and not has_g_ls:
            # Texture (non-perspective) left slope
            trace = make_trace('Calc_TextureLeftSlopeFPU', '9', 'return', 'fmt_tls_9', [
                'dword ptr [Fill_CurMapVMin]',
                'dword ptr [Fill_CurMapUMin]',
                'dword ptr [Fill_MapV_LeftSlope]',
                'dword ptr [Fill_MapU_LeftSlope]',
                'dword ptr [Fill_LeftSlope]',
            ])
            content = content[:jmp_rnr] + trace + '\n' + content[jmp_rnr:]
            found_ls += 1
    print(f"  OK: Added {found_ls} LeftSlope return traces")

    write_file('LIB386/pol_work/POLY.ASM', content)
    print("  DONE: POLY.ASM")


# ═══════════════════════════════════════════════════════════════
# FILLER ASM FILES — add comprehensive entry traces
# ═══════════════════════════════════════════════════════════════

# Common filler globals to dump at entry
FILLER_ENTRY_FMTS = [
    ('1b', 'LeftSlope=%d RightSlope=%d curY=%d', [
        'dword ptr [Fill_CurY]',
        'dword ptr [Fill_RightSlope]',
        'dword ptr [Fill_LeftSlope]',
    ]),
    ('1c', 'U_XS=%d V_XS=%d W_XS=%d ZBuf_XS=%d G_XS=%d', [
        'dword ptr [Fill_Gouraud_XSlope]',
        'dword ptr [Fill_ZBuf_XSlope]',
        'dword ptr [Fill_W_XSlope]',
        'dword ptr [Fill_MapV_XSlope]',
        'dword ptr [Fill_MapU_XSlope]',
    ]),
    ('1d', 'U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d', [
        'dword ptr [Fill_Gouraud_LeftSlope]',
        'dword ptr [Fill_ZBuf_LeftSlope]',
        'dword ptr [Fill_W_LeftSlope]',
        'dword ptr [Fill_MapV_LeftSlope]',
        'dword ptr [Fill_MapU_LeftSlope]',
    ]),
    ('1e', 'curU=%d curV=%d curW=%d curZBuf=%u curG=%d patch=%u', [
        'dword ptr [Fill_Patch]',
        'dword ptr [Fill_CurGouraudMin]',
        'dword ptr [Fill_CurZBufMin]',
        'dword ptr [Fill_CurWMin]',
        'dword ptr [Fill_CurMapVMin]',
        'dword ptr [Fill_CurMapUMin]',
    ]),
]

FILLER_FILES = {
    'LIB386/pol_work/POLYFLAT.ASM': [
        'Filler_Flat', 'Filler_Transparent', 'Filler_Trame',
        'Filler_FlatZBuf', 'Filler_TransparentZBuf', 'Filler_TrameZBuf',
        'Filler_FlatNZW', 'Filler_TransparentNZW', 'Filler_TrameNZW',
    ],
    'LIB386/pol_work/POLYGOUR.ASM': [
        'Filler_Gouraud', 'Filler_Dither',
        'Filler_GouraudTable', 'Filler_DitherTable',
        'Filler_GouraudZBuf', 'Filler_DitherZBuf',
        'Filler_GouraudTableZBuf', 'Filler_DitherTableZBuf',
        'Filler_GouraudNZW', 'Filler_DitherNZW',
        'Filler_GouraudTableNZW', 'Filler_DitherTableNZW',
    ],
    'LIB386/pol_work/POLYTEXT.ASM': [
        'Filler_Texture', 'Filler_TextureFlat',
        'Filler_TextureChromaKey', 'Filler_TextureFlatChromaKey',
    ],
    'LIB386/pol_work/POLYTEXZ.ASM': [
        'Filler_TextureZ', 'Filler_TextureZFlat',
        'Filler_TextureZChromaKey', 'Filler_TextureZFlatChromaKey',
    ],
    'LIB386/pol_work/POLYTZF.ASM': [
        'Filler_TextureZFogSmooth', 'Filler_TextureZFogSmoothZBuf',
    ],
    'LIB386/pol_work/POLYTZG.ASM': [
        'Filler_TextureZGouraud', 'Filler_TextureZGouraudChromaKey',
    ],
    'LIB386/pol_work/POLYGTEX.ASM': [
        'Filler_TextureGouraud', 'Filler_TextureDither',
        'Filler_TextureGouraudChromaKey', 'Filler_TextureDitherChromaKey',
    ],
}

for filepath, funcs in FILLER_FILES.items():
    print(f"\n=== {os.path.basename(filepath)} ===")
    content = read_file(filepath)
    
    if '[1b]' in content:
        print(f"  SKIP (already has detailed traces)")
        continue
    
    modified = False
    for func in funcs:
        # Add format strings for this function
        short = func.lower()[:12]
        for tid, desc, _ in FILLER_ENTRY_FMTS:
            label = f'fmt_{short}_{tid}'
            text = f'[ASM][{func}][{tid}] {desc}'
            content = add_fmt_to_data(content, label, text)
        
        # Find the existing entry trace for this function and add detail traces after it
        marker = f'[ASM][{func}][1]'
        idx = content.find(marker)
        if idx < 0:
            print(f"  WARN: no existing trace for {func}")
            continue
        
        # Find the "end debug trace" after it
        end_marker = '; --- end debug trace ---'
        end_idx = content.find(end_marker, idx)
        if end_idx < 0:
            continue
        end_idx += len(end_marker)
        
        # Add detail traces after the existing entry trace
        traces = []
        for tid, desc, push_args in FILLER_ENTRY_FMTS:
            label = f'fmt_{short}_{tid}'
            t = make_trace(func, tid, desc.split('=')[0], label, push_args)
            traces.append(t)
        
        content = content[:end_idx] + '\n' + '\n'.join(traces) + content[end_idx:]
        modified = True
        print(f"  OK: {func} detailed entry traces")
    
    if modified:
        write_file(filepath, content)

print("\n=== Done! ===")
