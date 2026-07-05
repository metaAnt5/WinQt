#!/usr/bin/env python3
"""Remove all debugLog/debugLog2 lines from main.cpp"""
with open('WinLine/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

lines = content.splitlines()
new_lines = []
removed = 0
for line in lines:
    s = line.strip()
    if s.startswith('debugLog(') and s.endswith(');'):
        removed += 1
        continue
    if s.startswith('debugLog2(') and s.endswith(');'):
        removed += 1
        continue
    new_lines.append(line)

result = '\n'.join(new_lines)
# Clean up empty #else\n#endif case
result = result.replace('#else\n#endif', '')

print(f'Removed {removed} debugLog/debugLog2 lines')

with open('WinLine/main.cpp', 'w', encoding='utf-8') as f:
    f.write(result)

print('Done!')
