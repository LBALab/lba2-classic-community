#!/usr/bin/env python3
"""Fix: move format strings inside _DATA segment in all pol_work ASM files."""
import re
import os

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

files = [
    'LIB386/pol_work/POLYFLAT.ASM',
    'LIB386/pol_work/POLYGOUR.ASM',
    'LIB386/pol_work/POLYTEXT.ASM',
    'LIB386/pol_work/POLYTEXZ.ASM',
    'LIB386/pol_work/POLYTZF.ASM',
    'LIB386/pol_work/POLYTZG.ASM',
    'LIB386/pol_work/POLYGTEX.ASM',
    'LIB386/pol_work/POLY.ASM',
]

for relpath in files:
    filepath = os.path.join(BASE, relpath)
    if not os.path.exists(filepath):
        print(f"  SKIP (not found): {relpath}")
        continue
    
    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()
    
    # Find all fmt_ DB lines that are between _DATA ENDS and _TEXT SEGMENT
    # Pattern: line starting with fmt_ containing DB
    lines = content.split('\n')
    
    # Find _DATA ENDS line
    data_ends_idx = None
    text_segment_idx = None
    for i, line in enumerate(lines):
        if re.match(r'^_DATA\s+ENDS', line):
            data_ends_idx = i
        if re.match(r'^_TEXT\s+SEGMENT', line) and data_ends_idx is not None:
            text_segment_idx = i
            break
    
    if data_ends_idx is None or text_segment_idx is None:
        # Try .CODE pattern
        for i, line in enumerate(lines):
            if '.CODE' in line and data_ends_idx is not None:
                text_segment_idx = i
                break
        if text_segment_idx is None:
            print(f"  SKIP (no _DATA/_TEXT structure): {relpath}")
            continue
    
    # Find fmt_ lines between data_ends and text_segment
    fmt_lines = []
    other_lines = []
    for i in range(data_ends_idx + 1, text_segment_idx):
        line = lines[i]
        if re.match(r'^fmt_\w+\s+DB\s+', line):
            fmt_lines.append(line)
        else:
            other_lines.append(line)
    
    if not fmt_lines:
        print(f"  OK (no stray fmt_ lines): {relpath}")
        continue
    
    print(f"  FIX: {relpath} - moving {len(fmt_lines)} fmt_ lines into _DATA")
    
    # Reconstruct: put fmt_lines before _DATA ENDS, then other_lines, then _TEXT
    new_lines = lines[:data_ends_idx]
    new_lines.append('')  # blank line before fmt strings
    new_lines.extend(fmt_lines)
    new_lines.append('')  # blank line after fmt strings
    new_lines.append(lines[data_ends_idx])  # _DATA ENDS
    new_lines.extend(other_lines)
    new_lines.extend(lines[text_segment_idx:])
    
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write('\n'.join(new_lines))

print("\nDone!")
