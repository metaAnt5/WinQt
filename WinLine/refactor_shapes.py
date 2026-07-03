#!/usr/bin/env python3
"""Refactor Shape struct field names and fix type compatibility for screenToDataCoord calls."""

import re

def refactor_klinewidget_cpp(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Phase 1: Replace all old field references with new (x1,y1,x2,y2)
    replacements = [
        # .candleIdx1 -> x1 (when not followed by more chars)
        (r'\.candleIdx1\b', '.x1'),
        (r'\.candleIdx2\b', '.x2'),
        (r'\.price1\b', '.y1'),
        (r'\.price2\b', '.y2'),
        (r'\.normX\b', '.x1'),
        (r'\.normY\b', '.y1'),
        # ns. variants
        (r'ns\.x1\b', 'ns.x1'),
        (r'ns\.x2\b', 'ns.x2'),
        (r'ns\.y1\b', 'ns.y1'),
        (r'ns\.y2\b', 'ns.y2'),
        (r'fs\.x1\b', 'fs.x1'),
        (r'fs\.y1\b', 'fs.y1'),
    ]
    for old, new in replacements:
        content = re.sub(old, new, content)
    
    # Phase 2: Fix screenToDataCoord calls where x1/x2 (now double) were passed as int&
    # screenToDataCoord(event->pos(), s.x1, s.y1) 
    # needs temp int: int _idx; screenToDataCoord(event->pos(), _idx, s.y1); s.x1 = _idx;
    #
    # Pattern: screenToDataCoord(..., s\.x1, s\.y1)
    # Pattern: screenToDataCoord(..., s\.x2, s\.y2)
    
    # Fix s.candleIdx1 patterns (now s.x1)
    # screenToDataCoord(event->pos(), s.candleIdx1, s.price1) -> already handled above becomes
    # screenToDataCoord(event->pos(), s.x1, s.y1)
    # Need to fix: int tmp; screenToDataCoord(..., tmp, s.y1); s.x1 = tmp;
    
    # Replace pattern: screenToDataCoord(event->pos(), s.x1, s.y1)
    # With: do { int _xi; screenToDataCoord(event->pos(), _xi, s.y1); s.x1 = _xi; } while(0)
    content = re.sub(
        r'screenToDataCoord\(([^,]+),\s*s\.x1,\s*s\.y1\)',
        r'do { int _xi; screenToDataCoord(\1, _xi, s.y1); s.x1 = _xi; } while(0)',
        content
    )
    content = re.sub(
        r'screenToDataCoord\(([^,]+),\s*s\.x2,\s*s\.y2\)',
        r'do { int _xi; screenToDataCoord(\1, _xi, s.y2); s.x2 = _xi; } while(0)',
        content
    )
    
    # Fix ns.candleIdx1 patterns
    content = re.sub(
        r'screenToDataCoord\(([^,]+),\s*ns\.x1,\s*ns\.y1\)',
        r'do { int _xi; screenToDataCoord(\1, _xi, ns.y1); ns.x1 = _xi; } while(0)',
        content
    )
    content = re.sub(
        r'screenToDataCoord\(([^,]+),\s*ns\.x2,\s*ns\.y2\)',
        r'do { int _xi; screenToDataCoord(\1, _xi, ns.y2); ns.x2 = _xi; } while(0)',
        content
    )
    
    # Phase 3: Fix int comparisons that were with candleIdx (int) but now x1/x2 is double
    # e.g., s.candleIdx2 = qMax(s.candleIdx1, ...) -> now s.x2 = qMax((int)s.x1, ...)
    # but qMax works with double too, so it should be fine
    
    # However qBound(0, s.candleIdx1 + dIdx, m_data.size() - 1) needs int
    # s.x1 is now double, and dIdx is int, so s.x1 + dIdx -> double, qBound params need matching types
    # Let's cast: qBound(0.0, s.x1 + dIdx, (double)(m_data.size() - 1))
    # Actually simpler: keep int temp vars for these
    
    content = re.sub(
        r's\.x1 = qBound\(0,\s*s\.x1\s*\+\s*dIdx,\s*m_data\.size\(\)\s*-\s*1\)',
        r's.x1 = (double)qBound(0, (int)s.x1 + dIdx, m_data.size() - 1)',
        content
    )
    content = re.sub(
        r's\.x2 = qBound\(0,\s*s\.x2\s*\+\s*dIdx,\s*m_data\.size\(\)\s*-\s*1\)',
        r's.x2 = (double)qBound(0, (int)s.x2 + dIdx, m_data.size() - 1)',
        content
    )
    content = re.sub(
        r's\.x2 = qMax\(s\.x1,\s*m_data\.size\(\)\s*-\s*1\)',
        r's.x2 = (double)qMax((int)s.x1, m_data.size() - 1)',
        content
    )
    
    # Fix candleCenterXForIndex - was passed candleIdx (int), now x1 (double)
    content = re.sub(
        r'candleCenterXForIndex\(s\.x1\)',
        r'candleCenterXForIndex((int)s.x1)',
        content
    )
    
    # Fix paintEvent dataCoordToScreen casts
    content = re.sub(
        r'dataCoordToScreen\(s\.x1,\s*s\.y1,',
        r'dataCoordToScreen((int)s.x1, s.y1,',
        content
    )
    content = re.sub(
        r'dataCoordToScreen\(s\.x2,\s*s\.y2,',
        r'dataCoordToScreen((int)s.x2, s.y2,',
        content
    )
    
    # Fix ns.x2 = qMax(0, m_data.size() - 1)
    content = re.sub(
        r'ns\.x2 = qMax\(0,\s*m_data\.size\(\)\s*-\s*1\)',
        r'ns.x2 = (double)qMax(0, m_data.size() - 1)',
        content
    )
    
    # Fix ns.x2 = ns.x1
    content = re.sub(
        r'ns\.x2 = ns\.x1;\s*ns\.y2 = ns\.y1',
        r'ns.x2 = ns.x1; ns.y2 = ns.y1',
        content
    )
    
    # Fix normToScreen calls with s.x1/s.y1 (which are now x1/y1)
    content = re.sub(
        r'normToScreen\(s\.x1,\s*s\.y1\)',
        r'normToScreen(s.x1, s.y1)',
        content
    )
    content = re.sub(
        r'normToScreen\(fs\.x1,\s*fs\.y1\)',
        r'normToScreen(fs.x1, fs.y1)',
        content
    )
    
    # Fix saveShapes JSON keys
    # obj["candleIdx1"] = s.x1 -> obj["x1"] = s.x1
    content = re.sub(r'obj\["candleIdx1"\] = s\.x1', 'obj["x1"] = s.x1', content)
    content = re.sub(r'obj\["price1"\] = s\.y1', 'obj["y1"] = s.y1', content)
    content = re.sub(r'obj\["candleIdx2"\] = s\.x2', 'obj["x2"] = s.x2', content)
    content = re.sub(r'obj\["price2"\] = s\.y2', 'obj["y2"] = s.y2', content)
    content = re.sub(r'obj\["normX"\] = s\.x1', 'obj["x1"] = s.x1', content)
    content = re.sub(r'obj\["normY"\] = s\.y1', 'obj["y1"] = s.y1', content)
    
    # Fix loadShapes
    content = re.sub(r's\.x1 = obj\["candleIdx1"\]\.toInt\(\)', 's.x1 = obj["x1"].toDouble()', content)
    content = re.sub(r's\.y1 = obj\["price1"\]\.toDouble\(\)', 's.y1 = obj["y1"].toDouble()', content)
    content = re.sub(r's\.x2 = obj\["candleIdx2"\]\.toInt\(\)', 's.x2 = obj["x2"].toDouble()', content)
    content = re.sub(r's\.y2 = obj\["price2"\]\.toDouble\(\)', 's.y2 = obj["y2"].toDouble()', content)
    content = re.sub(r's\.x1 = obj\["normX"\]\.toDouble\(0\.5\)', 's.x1 = obj["x1"].toDouble(0.5)', content)
    content = re.sub(r's\.y1 = obj\["normY"\]\.toDouble\(0\.5\)', 's.y1 = obj["y1"].toDouble(0.5)', content)
    
    # Handle duplicate assignments from normX/normY conversions
    # Remove duplicate obj["x1"] = s.x1 patterns
    lines = content.split('\n')
    filtered = []
    skip_next = False
    for i, line in enumerate(lines):
        # Check if previous line was obj["y1"] and this is also obj["y1"] (duplicate from normX->x1 and candleIdx1->x1 both mapping to x1)
        if skip_next:
            skip_next = False
            continue
        if i > 0:
            prev = lines[i-1].strip()
            curr = line.strip()
            # Skip duplicate assignments
            if prev == 'obj["y1"] = s.y1;' and curr == 'obj["y1"] = s.y1;':
                skip_next = True
                continue
            if prev == 'obj["x1"] = s.x1;' and curr == 'obj["x1"] = s.x1;':
                skip_next = True
                continue
        filtered.append(line)
    content = '\n'.join(filtered)
    
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"Refactored {path}")


def refactor_luascriptengine_cpp(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    replacements = [
        (r'\.candleIdx1\b', '.x1'),
        (r'\.candleIdx2\b', '.x2'),
        (r'\.price1\b', '.y1'),
        (r'\.price2\b', '.y2'),
        (r'\.normX\b', '.x1'),
        (r'\.normY\b', '.y1'),
    ]
    for old, new in replacements:
        content = re.sub(old, new, content)
    
    # Fix type casts for dataCoordToScreen calls
    content = re.sub(
        r'dataCoordToScreen\(s\.x1,\s*s\.y1,',
        r'dataCoordToScreen((int)s.x1, s.y1,',
        content
    )
    
    # Fix Trend ray calculations that used candleIdx as int
    # s.x2 = s.x1 -> double assignments
    # dx = s.x2 - s.x1, dx == 0 comparisons all work with doubles, ok
    
    # However int dx = s.x2 - s.x1; dx == 0 where dx is int might need care
    # Actually x1,x2 are doubles now, casting to int for dx
    content = re.sub(
        r'int dx = s\.x2 - s\.x1;',
        r'int dx = (int)s.x2 - (int)s.x1;',
        content
    )
    
    # candleIdx - s.x1 where candleIdx is int and s.x1 is double
    content = re.sub(
        r'double t = double\(candleIdx - s\.x1\) / double\(dx\);',
        r'double t = double(candleIdx - (int)s.x1) / double(dx);',
        content
    )
    
    # s.y1 + (s.y2 - s.y1) * t; - ok, doubles
    
    # Fix lua_shape_add_child assignment
    content = re.sub(
        r's\.x1 = candleIdx1;',
        r's.x1 = (double)candleIdx1;',
        content
    )
    content = re.sub(
        r's\.x2 = candleIdx2;',
        r's.x2 = (double)candleIdx2;',
        content
    )
    
    # Fix s.normX = normX; s.normY = normY; -> s.x1 = normX; s.y1 = normY;
    content = re.sub(
        r's\.x1 = qBound\(0\.0,\s*([^,]+),\s*1\.0\);',
        r's.x1 = qBound(0.0, \1, 1.0);',
        content
    )
    content = re.sub(
        r's\.y1 = qBound\(0\.0,\s*([^,]+),\s*1\.0\);',
        r's.y1 = qBound(0.0, \1, 1.0);',
        content
    )
    
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"Refactored {path}")


def refactor_main_cpp(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    replacements = [
        (r'\.candleIdx1\b', '.x1'),
        (r'\.candleIdx2\b', '.x2'),
        (r'\.price1\b', '.y1'),
        (r'\.price2\b', '.y2'),
    ]
    for old, new in replacements:
        content = re.sub(old, new, content)
    
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"Refactored {path}")


if __name__ == '__main__':
    import sys
    base = r'f:\stock\WinLineQt\WinQt\WinLine'
    refactor_klinewidget_cpp(f'{base}\\klinewidget.cpp')
    refactor_luascriptengine_cpp(f'{base}\\luascriptengine.cpp')
    refactor_main_cpp(f'{base}\\main.cpp')
