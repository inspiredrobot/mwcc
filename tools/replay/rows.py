#!/usr/bin/env python3
"""Print objdiff row range with full context: rows.py out.json fn start end"""
import json, sys

path, fn, start, end = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
d = json.load(open(path))

def find(side):
    for s in d[side]['symbols']:
        if s['name'] == fn:
            return s
    raise SystemExit(f'{fn} not on {side}')

def text(row):
    if row is None or 'instruction' not in row or row['instruction'] is None:
        return '-'
    i = row['instruction']
    s = i.get('formatted', '?')
    r = i.get('relocation')
    if r and r.get('target'):
        pass  # formatted already includes reloc name
    return s

L, R = find('left'), find('right')
lrows, rrows = L['instructions'], R['instructions']
n = max(len(lrows), len(rrows))
for idx in range(start, min(end, n)):
    lrow = lrows[idx] if idx < len(lrows) else None
    rrow = rrows[idx] if idx < len(rrows) else None
    lt, rt = text(lrow), text(rrow)
    lk = (lrow or {}).get('diff_kind', '') or (rrow or {}).get('diff_kind', '')
    mark = '*' if lk else ' '
    print(f"[{idx:4d}]{mark} T: {lt:60s} C: {rt}")
