#!/usr/bin/env python3
"""Fix filler traces: remove references to undeclared globals.
Each filler ASM file only has EXTRN for the globals it actually uses.
The trace blocks must only push globals that are declared in that file."""
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
]

for relpath in files:
    filepath = os.path.join(BASE, relpath)
    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()
    
    # Find all EXTRN declarations to know which globals are available
    extrns = set()
    for m in re.finditer(r'EXTRN\s+C\s+(\w+)', content, re.IGNORECASE):
        extrns.add(m.group(1))
    for m in re.finditer(r'extrn\s+C\s+(\w+)', content, re.IGNORECASE):
        extrns.add(m.group(1))
    
    # Find all push instructions in trace blocks that reference globals
    modified = False
    lines = content.split('\n')
    new_lines = []
    in_trace = False
    trace_lines = []
    
    for line in lines:
        if '; --- debug trace' in line:
            in_trace = True
            trace_lines = [line]
            continue
        elif '; --- end debug trace ---' in line:
            in_trace = False
            trace_lines.append(line)
            
            # Process trace block: remove pushes of undeclared globals
            filtered = []
            push_count = 0
            removed_pushes = 0
            for tl in trace_lines:
                # Check if it's a push of a global
                m = re.match(r'\s+push\s+dword ptr \[(\w+)\]', tl)
                if m:
                    global_name = m.group(1)
                    if global_name not in extrns and global_name != 'printf':
                        # Remove this push and adjust esp cleanup
                        removed_pushes += 1
                        continue
                    push_count += 1
                elif re.match(r'\s+push\s+', tl) and 'offset' not in tl:
                    push_count += 1
                
                # Fix esp cleanup if pushes were removed
                cleanup_m = re.match(r'(\s+add\s+esp,\s+)(\d+)', tl)
                if cleanup_m and removed_pushes > 0:
                    old_cleanup = int(cleanup_m.group(2))
                    new_cleanup = old_cleanup - (removed_pushes * 4)
                    if new_cleanup > 0:
                        tl = f'{cleanup_m.group(1)}{new_cleanup}'
                    else:
                        continue  # Skip the add esp entirely
                
                filtered.append(tl)
            
            if removed_pushes > 0:
                modified = True
                # Also need to fix the format string to remove corresponding %d
                # Find the format label in this trace
                fmt_match = re.search(r'push\s+offset\s+(\w+)', '\n'.join(filtered))
                if fmt_match:
                    fmt_label = fmt_match.group(1)
                    # We can't easily fix the format string from here,
                    # so let's just skip complex traces for files that
                    # don't have the globals. Replace the whole trace
                    # with a simplified version.
                    pass
            
            new_lines.extend(filtered)
            trace_lines = []
            continue
        
        if in_trace:
            trace_lines.append(line)
        else:
            new_lines.append(line)
    
    if modified:
        content = '\n'.join(new_lines)
        # Now fix format strings that have too many %d args
        # The simplest approach: for files that don't have certain globals,
        # replace the detailed trace blocks with simpler ones that only use
        # available globals.
    
    # Simpler approach: just remove ALL the [1b]-[1e] traces from files that
    # don't have the right globals, and replace with a single trace using
    # only available globals.
    
    # Actually, the CLEANEST fix: remove all [1b]-[1e] traces, and add
    # per-file custom traces based on what's available.
    
    # For now, just remove the broken trace blocks entirely
    # (the [1] entry trace with nbLines/xmin/xmax is fine since those are params)
    
    lines = content.split('\n')
    new_lines = []
    skip_until_end = False
    
    for line in lines:
        if skip_until_end:
            if '; --- end debug trace ---' in line:
                skip_until_end = False
            continue
        
        if '; --- debug trace' in line and any(f'[{t}]' in line for t in ['1b', '1c', '1d', '1e']):
            skip_until_end = True
            continue
        
        new_lines.append(line)
    
    content = '\n'.join(new_lines)
    
    # Now add back CORRECT traces based on available globals
    # Build the available globals list
    available_filler_globals = {
        'Fill_LeftSlope': 'Fill_LeftSlope' in extrns,
        'Fill_RightSlope': 'Fill_RightSlope' in extrns,
        'Fill_CurY': 'Fill_CurY' in extrns,
        'Fill_MapU_XSlope': 'Fill_MapU_XSlope' in extrns,
        'Fill_MapV_XSlope': 'Fill_MapV_XSlope' in extrns,
        'Fill_W_XSlope': 'Fill_W_XSlope' in extrns,
        'Fill_ZBuf_XSlope': 'Fill_ZBuf_XSlope' in extrns,
        'Fill_Gouraud_XSlope': 'Fill_Gouraud_XSlope' in extrns,
        'Fill_MapU_LeftSlope': 'Fill_MapU_LeftSlope' in extrns,
        'Fill_MapV_LeftSlope': 'Fill_MapV_LeftSlope' in extrns,
        'Fill_W_LeftSlope': 'Fill_W_LeftSlope' in extrns,
        'Fill_ZBuf_LeftSlope': 'Fill_ZBuf_LeftSlope' in extrns,
        'Fill_Gouraud_LeftSlope': 'Fill_Gouraud_LeftSlope' in extrns,
        'Fill_CurMapUMin': 'Fill_CurMapUMin' in extrns,
        'Fill_CurMapVMin': 'Fill_CurMapVMin' in extrns,
        'Fill_CurWMin': 'Fill_CurWMin' in extrns,
        'Fill_CurZBufMin': 'Fill_CurZBufMin' in extrns,
        'Fill_CurGouraudMin': 'Fill_CurGouraudMin' in extrns,
        'Fill_Patch': 'Fill_Patch' in extrns,
    }
    
    # Build trace args based on available globals
    trace1b_args = []
    trace1b_fmt_parts = []
    for g in ['Fill_LeftSlope', 'Fill_RightSlope', 'Fill_CurY']:
        if available_filler_globals.get(g):
            trace1b_args.append(f'dword ptr [{g}]')
            short = g.replace('Fill_', '').replace('Slope', 'S')
            trace1b_fmt_parts.append(f'{short}=%d')
    
    trace1c_args = []
    trace1c_fmt_parts = []
    for g in ['Fill_MapU_XSlope', 'Fill_MapV_XSlope', 'Fill_W_XSlope', 'Fill_ZBuf_XSlope', 'Fill_Gouraud_XSlope']:
        if available_filler_globals.get(g):
            trace1c_args.append(f'dword ptr [{g}]')
            short = g.replace('Fill_', '').replace('_XSlope', '_XS')
            trace1c_fmt_parts.append(f'{short}=%d')
    
    trace1e_args = []
    trace1e_fmt_parts = []
    for g in ['Fill_CurMapUMin', 'Fill_CurMapVMin', 'Fill_CurWMin', 'Fill_CurZBufMin', 'Fill_CurGouraudMin', 'Fill_Patch']:
        if available_filler_globals.get(g):
            trace1e_args.append(f'dword ptr [{g}]')
            short = g.replace('Fill_Cur', 'cur').replace('Min', '').replace('Fill_', '')
            trace1e_fmt_parts.append(f'{short}=%d')
    
    # For each filler function, find its [1] entry trace end and add detail traces
    for func_match in re.finditer(r'\[ASM\]\[(\w+)\]\[1\] entry', content):
        func_name = func_match.group(1)
        end_trace = content.find('; --- end debug trace ---', func_match.end())
        if end_trace < 0:
            continue
        end_trace += len('; --- end debug trace ---')
        
        # Create trace blocks
        new_traces = []
        basename = os.path.basename(relpath).lower().replace('.asm', '')
        
        if trace1b_args:
            fmt_text = ' '.join(trace1b_fmt_parts)
            fmt_label = f'fmt_{func_name.lower()[:12]}_1b'
            # Add the format string if not present
            if fmt_label not in content:
                # Find insertion point (before _DATA ENDS or .CODE)
                for marker in ['_DATA\t\t\tENDS', '_DATA ENDS', '.CODE']:
                    mi = content.find(marker)
                    if mi >= 0:
                        content = content[:mi] + f'{fmt_label}\tDB\t"[ASM][{func_name}][1b] {fmt_text}", 10, 0\n' + content[mi:]
                        end_trace += len(f'{fmt_label}\tDB\t"[ASM][{func_name}][1b] {fmt_text}", 10, 0\n')
                        break
            
            block = [f'\t\t; --- debug trace [ASM][{func_name}][1b] slopes ---', '\t\tpushad']
            for arg in reversed(trace1b_args):
                block.append(f'\t\tpush\t{arg}')
            block.append(f'\t\tpush\toffset {fmt_label}')
            block.append('\t\tcall\tprintf')
            block.append(f'\t\tadd\tesp, {4 * (len(trace1b_args) + 1)}')
            block.append('\t\tpopad')
            block.append('\t\t; --- end debug trace ---')
            new_traces.append('\n'.join(block))
        
        if trace1e_args:
            fmt_text = ' '.join(trace1e_fmt_parts)
            fmt_label = f'fmt_{func_name.lower()[:12]}_1e'
            if fmt_label not in content:
                for marker in ['_DATA\t\t\tENDS', '_DATA ENDS', '.CODE']:
                    mi = content.find(marker)
                    if mi >= 0:
                        content = content[:mi] + f'{fmt_label}\tDB\t"[ASM][{func_name}][1e] {fmt_text}", 10, 0\n' + content[mi:]
                        end_trace += len(f'{fmt_label}\tDB\t"[ASM][{func_name}][1e] {fmt_text}", 10, 0\n')
                        break
            
            block = [f'\t\t; --- debug trace [ASM][{func_name}][1e] curvals ---', '\t\tpushad']
            for arg in reversed(trace1e_args):
                block.append(f'\t\tpush\t{arg}')
            block.append(f'\t\tpush\toffset {fmt_label}')
            block.append('\t\tcall\tprintf')
            block.append(f'\t\tadd\tesp, {4 * (len(trace1e_args) + 1)}')
            block.append('\t\tpopad')
            block.append('\t\t; --- end debug trace ---')
            new_traces.append('\n'.join(block))
        
        if new_traces:
            insert_text = '\n' + '\n'.join(new_traces)
            # Re-find end_trace position since content may have changed
            et = content.find('; --- end debug trace ---', content.find(f'[ASM][{func_name}][1] entry'))
            if et >= 0:
                et += len('; --- end debug trace ---')
                content = content[:et] + insert_text + content[et:]
    
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)
    
    fcount = content.count('debug trace')
    print(f"  OK: {relpath} ({fcount} trace markers)")

print("\nDone!")
