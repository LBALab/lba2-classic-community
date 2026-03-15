#!/usr/bin/env python3
"""Add scanline trace to Filler_TextureZFogSmoothZBuf."""
import os

filepath = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'LIB386/pol_work/POLYTZF.CPP')

with open(filepath, 'r') as f:
    content = f.read()

zbuf_start = content.find('S32 Filler_TextureZFogSmoothZBuf')
if zbuf_start < 0:
    print('ERROR: function not found')
    exit(1)

first_while = content.find('while (1)', zbuf_start)
second_while = content.find('while (1)', first_while + 10)

if 'TextureZFogSmoothZBuf", "2"' in content[second_while:second_while+500]:
    print('SKIP: already present')
    exit(0)

anchor = content.find('if (xEnd > xStart)', second_while)
if anchor < 0:
    print('ERROR: anchor not found')
    exit(1)

trace = '    LIB386_TRACE_CPP("Filler_TextureZFogSmoothZBuf", "2", "scanline xS=%u xE=%u u=%d v=%d w=%d zb=%u",\n        xStart, xEnd, u, v, w, Fill_CurZBufMin);\n    '
content = content[:anchor] + trace + content[anchor:]

with open(filepath, 'w') as f:
    f.write(content)
print('OK: Added ZBuf scanline trace')
