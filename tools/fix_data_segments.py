#!/usr/bin/env python3
"""Fix pol_work ASM files: move format strings inside _DATA SEGMENT."""
import re

files = [
    'LIB386/pol_work/POLYGOUR.ASM',
    'LIB386/pol_work/POLYTEXT.ASM',
    'LIB386/pol_work/POLYTEXZ.ASM',
    'LIB386/pol_work/POLYTZG.ASM',
    'LIB386/pol_work/POLYTZF.ASM',
    'LIB386/pol_work/POLYGTEX.ASM',
    'LIB386/pol_work/POLYCLIP.ASM',
]

for filepath in files:
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Find _DATA ENDS line
    data_ends_match = re.search(r'^(_DATA\s+ENDS)\s*$', content, re.MULTILINE)
    if not data_ends_match:
        print(f'  SKIP (no _DATA ENDS): {filepath}')
        continue
    
    data_ends_pos = data_ends_match.start()
    
    # Find all format string lines and printf extrn that are AFTER _DATA ENDS
    # Collect them
    after_data_ends = content[data_ends_match.end():]
    
    lines_to_move = []
    lines_to_keep = []
    for line in after_data_ends.split('\n'):
        stripped = line.strip()
        if stripped.startswith('EXTRN') and 'printf' in stripped:
            lines_to_move.append(line)
        elif stripped.startswith('fmt_') and 'DB' in stripped:
            lines_to_move.append(line)
        else:
            lines_to_keep.append(line)
    
    if not lines_to_move:
        print(f'  SKIP (nothing to move): {filepath}')
        continue
    
    # Reconstruct: insert lines_to_move before _DATA ENDS
    before_data_ends = content[:data_ends_pos]
    moved_block = '\n'.join(lines_to_move)
    remaining = '\n'.join(lines_to_keep)
    
    new_content = before_data_ends + '\n' + moved_block + '\n\n' + data_ends_match.group(1) + remaining
    
    with open(filepath, 'w') as f:
        f.write(new_content)
    print(f'  OK: {filepath} (moved {len(lines_to_move)} lines)')

print('\nDone!')
