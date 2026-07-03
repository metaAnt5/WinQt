import sys, os

pairs = [
    ('.candleIdx1', '.x1'),
    ('.candleIdx2', '.x2'),
    ('.price1', '.y1'),
    ('.price2', '.y2'),
    ('.normX', '.x1'),
    ('.normY', '.y1'),
]

files = ['klinewidget.cpp','luascriptengine.cpp','main.cpp']
for fn in files:
    path = os.path.join(os.path.dirname(__file__), fn)
    with open(path, 'r', encoding='utf-8') as f:
        c = f.read()
    for old, new in pairs:
        c = c.replace(old, new)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(c)
    # verify no old fields left
    remaining = sum(c.count(o) for o,_ in pairs)
    print(f'{fn}: done, remaining old refs: {remaining}')
