#!/usr/bin/env python3
"""Fix all fmt_ label references in pol_work ASM files to match definitions."""
import re

files = [
    'LIB386/pol_work/POLYFLAT.ASM',
    'LIB386/pol_work/POLYGOUR.ASM',
    'LIB386/pol_work/POLYTEXT.ASM',
    'LIB386/pol_work/POLYTEXZ.ASM',
    'LIB386/pol_work/POLYTZG.ASM',
    'LIB386/pol_work/POLYTZF.ASM',
    'LIB386/pol_work/POLYGTEX.ASM',
]

for filepath in files:
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Build a map: function_name -> defined_label
    # From: fmt_xxx_1  DB  "[ASM][FuncName][1] ..."
    label_defs = {}
    for m in re.finditer(r'^(fmt_\w+)\s+DB\s+"\[ASM\]\[(\w+)\]\[1\]', content, re.MULTILINE):
        label = m.group(1)
        func_name = m.group(2)
        label_defs[func_name] = label
    
    if not label_defs:
        print(f'  SKIP: {filepath}')
        continue
    
    modified = False
    
    # For each trace block, find the function name and update the offset reference
    for func_name, correct_label in label_defs.items():
        # Find trace block: ; --- debug trace [ASM][FuncName][1] ...
        trace_pattern = re.compile(
            rf'(; --- debug trace \[ASM\]\[{re.escape(func_name)}\]\[1\].*?push\s+offset\s+)(fmt_\w+)',
            re.DOTALL
        )
        match = trace_pattern.search(content)
        if match:
            old_label = match.group(2)
            if old_label != correct_label:
                content = content[:match.start(2)] + correct_label + content[match.end(2):]
                print(f'  FIX: {filepath}: {func_name}: {old_label} -> {correct_label}')
                modified = True
    
    if modified:
        with open(filepath, 'w') as f:
            f.write(content)
    else:
        print(f'  OK (no changes needed): {filepath}')

print('\nDone!')
