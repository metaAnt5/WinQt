#!/usr/bin/env python3
"""Remove all core.log() debug calls from Lua script files."""
import os

scripts_dir = 'WinLine/data/scripts'
total_removed = 0

for fname in sorted(os.listdir(scripts_dir)):
    if not fname.endswith('.lua'):
        continue
    fpath = os.path.join(scripts_dir, fname)
    with open(fpath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Remove all core.log(...) blocks, including multi-line with nested parens
    result = []
    i = 0
    removed = 0
    while i < len(content):
        # Look for "core.log(" pattern
        pos = content.find('core.log(', i)
        if pos == -1:
            result.append(content[i:])
            break
        
        # Check it's not part of another word
        if pos > 0 and (content[pos-1].isalnum() or content[pos-1] == '_'):
            result.append(content[i:pos+9])
            i = pos + 9
            continue
        
        # Append text before core.log(
        result.append(content[i:pos])
        
        # Find matching closing paren with nesting
        depth = 0
        j = pos
        while j < len(content):
            if content[j] == '(':
                depth += 1
            elif content[j] == ')':
                depth -= 1
                if depth == 0:
                    removed += 1
                    i = j + 1
                    break
            j += 1
        else:
            # Not found, keep original
            result.append(content[pos:])
            i = len(content)
    
    new_content = ''.join(result)
    if removed > 0:
        with open(fpath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f'{fname}: removed {removed} core.log() call(s)')
        total_removed += removed

print(f'\nTotal: removed {total_removed} core.log() calls')
