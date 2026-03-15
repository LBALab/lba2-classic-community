#!/usr/bin/env python3
"""Instrument FONT, GRAPH, MASK, AFFSTR ASM+CPP files."""
import re, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from add_traces_phase2 import *

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# FONT.ASM - multiple PROCs: SizeFont, SizeFontS8, SizeStringS8
# GRAPH.ASM - AffGraph
# MASK.ASM - AffMask (Watcom, bare proc), ClippingMask (has params)
# AFFSTR.ASM - AffString (has stack params x,y,string)

entries = [
    ('LIB386/SVGA/FONT.ASM', 'LIB386/SVGA/FONT.CPP',
     'SizeFont', '[ASM][SizeFont][1] string=%p',
     ['string'], 'mov esi, string', '"string=%p", (void*)string'),
    
    ('LIB386/SVGA/GRAPH.ASM', 'LIB386/SVGA/GRAPH.CPP',
     'AffGraph', '[ASM][AffGraph][1] num=%d x=%d y=%d bank=%p',
     ['bankgraph', 'y', 'x', 'numgraph'], 'mov eax, numgraph',
     '"num=%d x=%d y=%d bank=%p", numgraph, x, y, (void*)bankgraph'),
    
    ('LIB386/SVGA/AFFSTR.ASM', 'LIB386/SVGA/AFFSTR.CPP',
     'AffString', '[ASM][AffString][1] x=%d y=%d str=%p',
     ['string', 'y', 'x'], 'mov eax, x',
     '"x=%d y=%d str=%p", x, y, (void*)string'),
]

for asm_path, cpp_path, func, fmt_text, push_args, anchor, cpp_trace in entries:
    print(f"\n--- {func} ---")
    process_asm_file(asm_path, func, fmt_text, push_args, anchor)
    instrument_cpp(cpp_path, func, cpp_trace)

# MASK.ASM - Watcom bare proc, args in eax,ebx,ecx,esi
print("\n--- AffMask ---")
fullpath = os.path.join(BASE, 'LIB386/SVGA/MASK.ASM')
content = read_file(fullpath)
if 'debug trace' not in content:
    content = ensure_asm_printf_extrn(content)
    content = add_asm_fmt_strings(content, [
        ('fmt_affmask_1', '[ASM][AffMask][1] num=%d x=%d y=%d bank=%p'),
    ])
    trace = make_trace_block('AffMask', '1', 'params', 'fmt_affmask_1',
                             ['esi', 'ecx', 'ebx', 'eax'])
    # Insert after "AffMask proc" line
    idx = content.find('AffMask\tproc')
    if idx < 0:
        idx = content.find('AffMask proc')
    if idx >= 0:
        eol = content.find('\n', idx)
        content = content[:eol+1] + '\n' + trace + '\n\n' + content[eol+1:]
    write_file(fullpath, content)
    print("  ASM OK: MASK.ASM")

instrument_cpp('LIB386/SVGA/MASK.CPP', 'ClippingMask',
               '"num=%d x=%d y=%d bank=%p", nummask, x, y, (void*)bankmask')

print("\nDone!")
