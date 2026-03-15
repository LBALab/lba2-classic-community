#!/usr/bin/env python3
"""
Instrument LIB386 ASM and CPP files with debug printf traces.

This script adds:
- ASM: pushad/popad-wrapped printf traces at function entry
- CPP: LIB386_TRACE_CPP() macro calls at function entry

Usage: python3 tools/add_traces.py
"""

import re
import os
import sys

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read_file(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()


def write_file(path, content):
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)


# ─── ASM instrumentation ───

def add_asm_printf_extrn(content):
    """Add extrn C printf:NEAR to .DATA section if not already present."""
    if 'printf' in content:
        return content
    # Find last extrn or end of .DATA declarations
    # Insert before .CODE section
    code_match = re.search(r'(\n\t*\.CODE)', content)
    if code_match:
        insert_pos = code_match.start()
        return content[:insert_pos] + '\n\t\tEXTRN\tC\tprintf\t:NEAR\n' + content[insert_pos:]
    return content


def add_asm_fmt_string(content, label, text):
    """Add a format string to .DATA section."""
    if label in content:
        return content
    # Find .CODE section and insert before it
    code_match = re.search(r'(\n;?\*?[\u2500\u2550].*\n\t*\.CODE)', content)
    if not code_match:
        code_match = re.search(r'(\n\t*\.CODE)', content)
    if code_match:
        insert_pos = code_match.start()
        escaped_text = text.replace('"', '\\"')
        fmt_line = f'\n\n{label}\tDB\t"{text}", 10, 0\n'
        return content[:insert_pos] + fmt_line + content[insert_pos:]
    return content


def make_asm_trace_block(func_name, trace_id, desc, fmt_label, push_args):
    """Generate an ASM debug trace block.
    push_args: list of strings like 'eax', 'ebx', 'dword ptr [X0]'
    """
    lines = [
        f'\t\t; --- debug trace [ASM][{func_name}][{trace_id}] {desc} ---',
        '\t\tpushad',
    ]
    for arg in reversed(push_args):
        lines.append(f'\t\tpush\t{arg}')
    lines.append(f'\t\tpush\toffset {fmt_label}')
    lines.append('\t\tcall\tprintf')
    cleanup = 4 * (len(push_args) + 1)
    lines.append(f'\t\tadd\tesp, {cleanup}')
    lines.append('\t\tpopad')
    lines.append(f'\t\t; --- end debug trace ---')
    return '\n'.join(lines)


# ─── CPP instrumentation ───

def ensure_cpp_include(content, include_path):
    """Add #include if not present."""
    if include_path in content:
        return content
    # Find first #include and add after the includes block
    lines = content.split('\n')
    last_include_idx = -1
    for i, line in enumerate(lines):
        if line.strip().startswith('#include'):
            last_include_idx = i
    if last_include_idx >= 0:
        lines.insert(last_include_idx + 1, f'#include {include_path}')
    return '\n'.join(lines)


def add_cpp_entry_trace(content, func_name, cpp_func_sig_pattern, trace_args):
    """Add LIB386_TRACE_CPP at function entry after opening brace.
    trace_args: format string like '"x=%d z=%d", x, z'
    """
    if f'LIB386_TRACE_CPP("{func_name}"' in content:
        return content  # Already instrumented
    
    # Find function definition and its opening brace
    # Look for the function signature followed by {
    pattern = re.compile(
        rf'({re.escape(cpp_func_sig_pattern)}.*?\{{)',
        re.DOTALL
    )
    match = pattern.search(content)
    if match:
        insert_pos = match.end()
        trace_line = f'\n    LIB386_TRACE_CPP("{func_name}", "1", {trace_args});'
        return content[:insert_pos] + trace_line + content[insert_pos:]
    return content


# ─── File definitions ───

# Each entry: (asm_file, cpp_file, func_name, asm_fmt_label, 
#              asm_fmt_text, asm_push_args, 
#              cpp_func_sig_start, cpp_trace_args)

ANIM_FILES = [
    {
        'asm': 'LIB386/ANIM/FRAME.ASM',
        'cpp': 'LIB386/ANIM/FRAME.CPP',
        'func': 'ObjectSetFrame',
        'fmt_label': 'fmt_frame_1',
        'fmt_text': '[ASM][ObjectSetFrame][1] obj=%p frame=%u',
        'asm_push_args': ['ebx', 'edx'],  # After mov ebx,obj; mov edx,frame
        'asm_anchor_before': 'mov\tecx, [ebx].OBJ_3D.Status',
        'asm_anchor_after': '\t\t\tmov edx, frame',
        'cpp_sig': 'void ObjectSetFrame(T_OBJ_3D *obj, U32 frame)',
        'cpp_trace': '"obj=%p frame=%u", (void*)obj, frame',
    },
    {
        'asm': 'LIB386/ANIM/INTANIM.ASM',
        'cpp': 'LIB386/ANIM/INTANIM.CPP',
        'func': 'ObjectSetInterAnim',
        'fmt_label': 'fmt_intanim_1',
        'fmt_text': '[ASM][ObjectSetInterAnim][1] obj=%p',
        'asm_push_args': ['ebx'],
        'asm_anchor_after': '\t\t\tmov ebx, obj',
        'cpp_sig': 'void ObjectSetInterAnim(T_OBJ_3D *obj)',
        'cpp_trace': '"obj=%p", (void*)obj',
    },
    {
        'asm': 'LIB386/ANIM/INTERDEP.ASM',
        'cpp': 'LIB386/ANIM/INTERDEP.CPP',
        'func': 'ObjectSetInterDep',
        'fmt_label': 'fmt_interdep_1',
        'fmt_text': '[ASM][ObjectSetInterDep][1] obj=%p',
        'asm_push_args': ['ebx'],
        'asm_anchor_after': '\t\t\tmov ebx, obj',
        'cpp_sig': 'void ObjectSetInterDep(T_OBJ_3D *obj)',
        'cpp_trace': '"obj=%p", (void*)obj',
    },
    {
        'asm': 'LIB386/ANIM/INTFRAME.ASM',
        'cpp': 'LIB386/ANIM/INTFRAME.CPP',
        'func': 'ObjectSetInterFrame',
        'fmt_label': 'fmt_intframe_1',
        'fmt_text': '[ASM][ObjectSetInterFrame][1] obj=%p',
        'asm_push_args': ['ebx'],
        'asm_anchor_after': '\t\t\tmov ebx, obj',
        'cpp_sig': 'void ObjectSetInterFrame(T_OBJ_3D *obj)',
        'cpp_trace': '"obj=%p", (void*)obj',
    },
    {
        'asm': 'LIB386/ANIM/STOFRAME.ASM',
        'cpp': 'LIB386/ANIM/STOFRAME.CPP',
        'func': 'ObjectStoreFrame',
        'fmt_label': 'fmt_stoframe_1',
        'fmt_text': '[ASM][ObjectStoreFrame][1] obj=%p',
        'asm_push_args': ['ebx'],
        'asm_anchor_after': None,  # Bare proc, EBX is the param
        'cpp_sig': 'void ObjectStoreFrame(T_OBJ_3D *obj)',
        'cpp_trace': '"obj=%p", (void*)obj',
    },
    {
        'asm': 'LIB386/ANIM/TEXTURE.ASM',
        'cpp': 'LIB386/ANIM/TEXTURE.CPP',
        'func': 'ObjectInitTexture',
        'fmt_label': 'fmt_texture_1',
        'fmt_text': '[ASM][ObjectInitTexture][1] obj=%p tex=%p',
        'asm_push_args': ['ebx', 'eax'],
        'asm_anchor_after': None,  # Bare proc
        'cpp_sig': 'void ObjectInitTexture(T_OBJ_3D *obj, void *texture)',
        'cpp_trace': '"obj=%p tex=%p", (void*)obj, texture',
    },
]


def instrument_asm_file(filepath, func, fmt_label, fmt_text, push_args, anchor_after=None, anchor_before=None):
    """Add printf trace to an ASM file."""
    fullpath = os.path.join(BASE, filepath)
    if not os.path.exists(fullpath):
        print(f"  SKIP (not found): {filepath}")
        return False
    
    content = read_file(fullpath)
    
    if 'debug trace' in content:
        print(f"  SKIP (already instrumented): {filepath}")
        return True
    
    # Add printf extrn
    content = add_asm_printf_extrn(content)
    
    # Add format string
    content = add_asm_fmt_string(content, fmt_label, fmt_text)
    
    # Add trace block
    trace = make_asm_trace_block(func, '1', 'params', fmt_label, push_args)
    
    if anchor_after:
        # Insert trace after the anchor line
        idx = content.find(anchor_after)
        if idx >= 0:
            end_of_line = content.find('\n', idx + len(anchor_after))
            if end_of_line >= 0:
                content = content[:end_of_line+1] + '\n' + trace + '\n\n' + content[end_of_line+1:]
            else:
                print(f"  WARN: couldn't find end of anchor line in {filepath}")
                return False
        else:
            print(f"  WARN: anchor_after not found in {filepath}: {anchor_after!r}")
            return False
    elif anchor_before:
        idx = content.find(anchor_before)
        if idx >= 0:
            content = content[:idx] + trace + '\n\n\t\t' + content[idx:]
        else:
            print(f"  WARN: anchor_before not found in {filepath}: {anchor_before!r}")
            return False
    else:
        print(f"  WARN: no anchor specified for {filepath}")
        return False
    
    write_file(fullpath, content)
    print(f"  OK: {filepath}")
    return True


def instrument_cpp_file(filepath, func, trace_args):
    """Add LIB386_TRACE_CPP to a CPP file."""
    fullpath = os.path.join(BASE, filepath)
    if not os.path.exists(fullpath):
        print(f"  SKIP (not found): {filepath}")
        return False
    
    content = read_file(fullpath)
    
    if f'LIB386_TRACE_CPP("{func}"' in content:
        print(f"  SKIP (already instrumented): {filepath}")
        return True
    
    # Add include
    content = ensure_cpp_include(content, '<SYSTEM/DEBUG_TRACES.H>')
    
    # Find function and add trace after opening brace
    # Pattern: function signature followed by {
    # We search for the function name followed by ( ... ) ... {
    func_pattern = re.compile(
        rf'({re.escape(func)}\s*\([^)]*\)\s*\{{)',
        re.DOTALL
    )
    match = func_pattern.search(content)
    if match:
        insert_pos = match.end()
        trace_line = f'\n    LIB386_TRACE_CPP("{func}", "1", {trace_args});'
        content = content[:insert_pos] + trace_line + content[insert_pos:]
        write_file(fullpath, content)
        print(f"  OK: {filepath}")
        return True
    else:
        print(f"  WARN: function {func} not found in {filepath}")
        return False


def main():
    print("=== Instrumenting ANIM files ===")
    for entry in ANIM_FILES:
        print(f"\n--- {entry['func']} ---")
        instrument_asm_file(
            entry['asm'], entry['func'], entry['fmt_label'], entry['fmt_text'],
            entry['asm_push_args'],
            anchor_after=entry.get('asm_anchor_after'),
            anchor_before=entry.get('asm_anchor_before'),
        )
        instrument_cpp_file(entry['cpp'], entry['func'], entry['cpp_trace'])
    
    print("\n=== Done ===")


if __name__ == '__main__':
    main()
