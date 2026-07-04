import sys
content = open('WinLine/main.cpp', 'r', encoding='utf-8').read()

# Fix 1: *sp -> sp (QSharedPointer should not be dereferenced)
content = content.replace('shapes.append(*sp)', 'shapes.append(sp)')

# Fix 2: shape. -> shape-> (QSharedPointer needs -> not .)
content = content.replace('if (!shape.scriptName.isEmpty())', 'if (!shape->scriptName.isEmpty())')
content = content.replace('QString scriptFile = shape.scriptName;', 'QString scriptFile = shape->scriptName;')
content = content.replace("luaEngine->loadScript(scriptFile, shape.scriptParams, binding);", "luaEngine->loadScript(scriptFile, shape->scriptParams, binding);")

open('WinLine/main.cpp', 'w', encoding='utf-8').write(content)
print('Fixed successfully')
