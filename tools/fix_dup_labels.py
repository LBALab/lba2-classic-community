#!/usr/bin/env python3
"""Fix duplicate format string labels in pol_work ASM files."""
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

def make_unique_label(func_name):
    """Create a unique short label from function name."""
    # Remove common prefixes
    name = func_name.replace('Filler_', 'f_')
    # Truncate smartly
    return 'fmt_' + name.lower()[:16] + '_1'

for filepath in files:
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Find all format string lines with their function names
    # Pattern: fmt_xxx_1  DB  "[ASM][FuncName][1] entry", 10, 0
    fmt_pattern = re.compile(r'^(fmt_\w+)\s+DB\s+"(\[ASM\]\[(\w+)\]\[1\] \w+)"', re.MULTILINE)
    
    matches = list(fmt_pattern.finditer(content))
    if not matches:
        print(f'  SKIP (no fmt lines): {filepath}')
        continue
    
    # Build replacement map: old_label -> new_unique_label
    replacements = {}
    seen_labels = set()
    
    for match in matches:
        old_label = match.group(1)
        func_name = match.group(3)
        new_label = make_unique_label(func_name)
        
        # Ensure uniqueness
        if new_label in seen_labels:
            # Add more characters
            new_label = 'fmt_' + func_name.lower()[:20] + '_1'
        if new_label in seen_labels:
            new_label = 'fmt_' + func_name.lower() + '_1'
        
        seen_labels.add(new_label)
        
        if old_label != new_label:
            # Replace this specific line's label AND all references to it
            replacements[match.start(1)] = (old_label, new_label, func_name)
    
    if not replacements:
        print(f'  SKIP (labels OK): {filepath}')
        continue
    
    # Apply replacements in reverse order to maintain positions
    # First, build a full mapping of old->new for each fmt line
    # We need to be careful: multiple fmt lines may have the same old label
    # but need different new labels. So we replace line by line.
    
    lines = content.split('\n')
    new_lines = []
    label_map = {}  # For each fmt DB line index, old_label -> new_label
    
    for i, line in enumerate(lines):
        m = fmt_pattern.match(line)
        if m:
            old_label = m.group(1)
            func_name = m.group(3)
            new_label = make_unique_label(func_name)
            
            # Replace the label in this line
            new_line = line.replace(old_label, new_label, 1)
            new_lines.append(new_line)
            label_map[func_name] = new_label
        else:
            new_lines.append(line)
    
    content = '\n'.join(new_lines)
    
    # Now fix all references: push offset old_label -> push offset new_label
    # For each trace block, find which function it's in and use the right label
    # The trace blocks reference fmt_xxx_1 which we need to update
    
    # Find all "push offset fmt_xxx_1" lines and update them
    # We need to match the trace block to the function name
    # The trace block is inside a PROC, and the fmt label name tells us which function
    
    # Actually, let's just replace all "offset fmt_xxx_1" references
    # with the correct label based on the function they're in
    
    # For each function that has a trace block, find the push offset fmt_ line
    # and replace the label with the correct one from label_map
    
    for func_name, new_label in label_map.items():
        # Find the trace block for this function
        # Pattern: ; --- debug trace [ASM][FuncName]...
        #          pushad
        #          push offset fmt_xxx_1
        trace_start = content.find(f'[ASM][{func_name}]')
        if trace_start < 0:
            continue
        # Find the push offset line after this
        push_match = re.search(r'push\s+offset\s+(fmt_\w+)', content[trace_start:trace_start+300])
        if push_match:
            old_ref = push_match.group(1)
            if old_ref != new_label:
                # Replace just this one occurrence
                start = trace_start + push_match.start(1)
                end = trace_start + push_match.end(1)
                content = content[:start] + new_label + content[end:]
    
    with open(filepath, 'w') as f:
        f.write(content)
    print(f'  OK: {filepath} (fixed {len(label_map)} labels)')

print('\nDone!')
