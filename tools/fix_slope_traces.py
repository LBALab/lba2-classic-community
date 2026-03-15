#!/usr/bin/env python3
"""Fix pass: add missing slope return traces to POLY.ASM using regex patterns."""
import re
import os

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
filepath = os.path.join(BASE, 'LIB386/pol_work/POLY.ASM')

with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
    content = f.read()


def make_trace(func, tid, desc, fmt_label, push_args):
    lines = [
        f'\t\t; --- debug trace [ASM][{func}][{tid}] {desc} ---',
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


# Find ALL fistp [Fill_W_XSlope] and add TextureZ return traces
print("=== Adding TextureZ x-slope return traces ===")
count = 0
for m in re.finditer(r'fistp\s+\[Fill_W_XSlope\]', content):
    idx = m.start()
    eol = content.find('\n', idx)
    # Skip if trace already there
    if 'debug trace' in content[eol:eol+300]:
        continue
    region = content[max(0,idx-800):idx]
    has_u = bool(re.search(r'fistp\s+\[Fill_MapU_XSlope\]', region))
    has_v = bool(re.search(r'fistp\s+\[Fill_MapV_XSlope\]', region))
    has_g = bool(re.search(r'fistp\s+\[Fill_Gouraud_XSlope\]', region))
    has_z = bool(re.search(r'fistp\s+\[Fill_ZBuf_XSlope\]', region))
    
    if has_u and has_v and not has_g and not has_z:
        trace = make_trace('Calc_TextureZXSlopeFPU', '9', 'return', 'fmt_txzxs_9', [
            'dword ptr [Fill_W_XSlope]',
            'dword ptr [Fill_MapV_XSlope]',
            'dword ptr [Fill_MapU_XSlope]',
        ])
    elif has_u and has_v and has_g and not has_z:
        trace = make_trace('Calc_TextureZGouraudXSlopeFPU', '9', 'return', 'fmt_txzgxs_9', [
            'dword ptr [Fill_W_XSlope]',
            'dword ptr [Fill_Gouraud_XSlope]',
            'dword ptr [Fill_MapV_XSlope]',
            'dword ptr [Fill_MapU_XSlope]',
        ])
    elif has_u and has_v and has_z:
        trace = make_trace('Calc_TextureZXSlopeZBufFPU', '9', 'return', 'fmt_txzxsz_9', [
            'dword ptr [Fill_ZBuf_XSlope]',
            'dword ptr [Fill_W_XSlope]',
            'dword ptr [Fill_MapV_XSlope]',
            'dword ptr [Fill_MapU_XSlope]',
        ])
    else:
        continue
    
    content = content[:eol+1] + trace + '\n' + content[eol+1:]
    count += 1
    print(f"  Added at offset {idx}")
print(f"Total W_XSlope traces: {count}")

# Find ALL fistp [Fill_LeftSlope] and add LeftSlope return traces
print("\n=== Adding LeftSlope return traces ===")
count = 0
for m in re.finditer(r'fistp\s+\[Fill_LeftSlope\]', content):
    idx = m.start()
    eol = content.find('\n', idx)
    # Check what comes after (within 1500 bytes)
    region_after = content[eol:eol+1500]
    
    has_w_ls = bool(re.search(r'fistp\s+\[Fill_W_LeftSlope\]', region_after))
    has_g_ls = bool(re.search(r'fistp\s+\[Fill_Gouraud_LeftSlope\]', region_after))
    has_u_ls = bool(re.search(r'fistp\s+\[Fill_MapU_LeftSlope\]', region_after))
    has_z_ls = bool(re.search(r'fistp\s+\[Fill_ZBuf_LeftSlope\]', region_after))
    
    # Find jmp Read_Next_Right
    jmp_rnr = content.find('jmp\tRead_Next_Right', eol)
    if jmp_rnr < 0 or jmp_rnr > eol + 3000:
        continue
    
    # Skip if trace already exists before jmp
    if 'debug trace' in content[jmp_rnr-300:jmp_rnr]:
        continue
    
    if has_w_ls and not has_g_ls and not has_z_ls:
        label = 'fmt_txzls_9'
        func = 'Calc_TextureZLeftSlopeFPU'
        args = [
            'dword ptr [Fill_CurWMin]', 'dword ptr [Fill_CurMapVMin]',
            'dword ptr [Fill_CurMapUMin]', 'dword ptr [Fill_W_LeftSlope]',
            'dword ptr [Fill_MapV_LeftSlope]', 'dword ptr [Fill_MapU_LeftSlope]',
            'dword ptr [Fill_LeftSlope]',
        ]
    elif has_g_ls and has_w_ls:
        label = 'fmt_txzgls_9'
        func = 'Calc_TextureZGouraudLeftSlopeFPU'
        args = [
            'dword ptr [Fill_W_LeftSlope]', 'dword ptr [Fill_MapV_LeftSlope]',
            'dword ptr [Fill_MapU_LeftSlope]', 'dword ptr [Fill_Gouraud_LeftSlope]',
            'dword ptr [Fill_LeftSlope]',
        ]
    elif has_u_ls and not has_w_ls and not has_g_ls:
        label = 'fmt_tls_9'
        func = 'Calc_TextureLeftSlopeFPU'
        args = [
            'dword ptr [Fill_CurMapVMin]', 'dword ptr [Fill_CurMapUMin]',
            'dword ptr [Fill_MapV_LeftSlope]', 'dword ptr [Fill_MapU_LeftSlope]',
            'dword ptr [Fill_LeftSlope]',
        ]
    elif has_g_ls and not has_w_ls and not has_u_ls:
        # Already handled by first pass
        continue
    else:
        continue
    
    trace = make_trace(func, '9', 'return', label, args)
    content = content[:jmp_rnr] + trace + '\n' + content[jmp_rnr:]
    count += 1
    print(f"  Added {func} at offset {idx}")
print(f"Total LeftSlope traces: {count}")

# Find fistp [Fill_ZBuf_LeftSlope] and add ZBuf left slope trace
print("\n=== Adding ZBuf LeftSlope traces ===")
count = 0
for m in re.finditer(r'fistp\s+\[Fill_ZBuf_LeftSlope\]', content):
    idx = m.start()
    eol = content.find('\n', idx)
    if 'debug trace' in content[eol:eol+300]:
        continue
    # Check it's not followed by Fill_MapU_LeftSlope (composite calc)
    next_300 = content[eol:eol+300]
    if re.search(r'fistp\s+\[Fill_MapU_LeftSlope\]', next_300):
        continue
    trace = make_trace('Calc_ZBufLeftSlopeFPU', '9', 'return', 'fmt_zbls_9', [
        'dword ptr [Fill_CurZBufMin]',
        'dword ptr [Fill_ZBuf_LeftSlope]',
    ])
    content = content[:eol+1] + trace + '\n' + content[eol+1:]
    count += 1
print(f"Total ZBuf LeftSlope traces: {count}")

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(content)

# Summary
print("\n=== Verifying ===")
with open(filepath, 'r') as f:
    final = f.read()
for pat in ['TextureZXSlopeFPU][9]', 'TextureZGouraudXSlopeFPU][9]', 
            'TextureZXSlopeZBufFPU][9]', 'TextureZLeftSlopeFPU][9]',
            'TextureZGouraudLeftSlopeFPU][9]', 'ZBufLeftSlopeFPU][9]',
            'TextureLeftSlopeFPU][9]']:
    n = final.count(pat)
    print(f"  {pat}: {n} occurrences")
print("\nDone!")
