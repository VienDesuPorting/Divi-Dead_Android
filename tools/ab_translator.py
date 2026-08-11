#!/usr/bin/env python3
"""Divi-Dead .AB script translator (extract/verify/patch)."""
import argparse, re, sys

def is_translatable(s):
    if len(s) < 3: return False
    if re.match(r'^[A-Za-z0-9_]+\.[A-Za-z0-9]{1,4}$', s): return False
    if re.match(r'^[a-z][a-z0-9_]*$', s) and ' ' not in s: return False
    if re.match(r'^[A-Z][A-Z0-9_]*$', s) and ' ' not in s: return False
    has_space = ' ' in s
    has_lower = any(c.islower() for c in s)
    has_punct = any(c in '.,?!\'":;()[]' for c in s)
    return has_space or has_lower or has_punct

def extract_strings(data):
    pos = 0
    while pos < len(data):
        end = data.find(b'\x00', pos)
        if end == -1: break
        if end > pos:
            chunk = data[pos:end]
            if chunk and all(32 <= b < 127 for b in chunk):
                yield (pos, chunk.decode('ascii'))
        pos = end + 1

def cmd_extract(args):
    with open(args.input, 'rb') as f: data = f.read()
    seen = set(); count = 0
    with open(args.output, 'w', encoding='utf-8') as out:
        out.write(f'# Divi-Dead AB translation template\n# Source: {args.input}\n\n')
        for offset, s in extract_strings(data):
            if not is_translatable(s): continue
            if s in seen and not args.include_duplicates: continue
            seen.add(s)
            out.write(f'# offset: 0x{offset:x}\n< {s}\n> \n\n')
            count += 1
    print(f'Extracted {count} strings to {args.output}')

def parse_patch(path):
    blocks = []
    orig = trans = off = None; has = False
    def flush():
        nonlocal orig, trans, off, has
        if orig is not None and has: blocks.append((orig, trans, off))
        orig = trans = off = None; has = False
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n\r')
            if line.startswith('#'):
                m = re.match(r'^#\s*offset:\s*(0x[0-9a-fA-F]+|\d+)', line)
                if m and orig is not None and off is None: off = int(m.group(1), 0)
                continue
            if line.startswith('@'):
                m = re.match(r'^@\s*(0x[0-9a-fA-F]+|\d+)', line)
                if m: off = int(m.group(1), 0)
                continue
            if line.startswith('< '): flush(); orig = line[2:]
            elif line == '<': flush(); orig = ''
            elif line.startswith('> '): trans = line[2:]; has = True
            elif line == '>': trans = ''; has = True
            elif line.strip() == '': flush()
            elif orig is not None and has:
                trans = (trans + '\n' + line) if trans else line
        flush()
    return blocks

def cmd_patch(args):
    with open(args.input, 'rb') as f: data = bytearray(f.read())
    blocks = parse_patch(args.patch)
    patches = []
    for orig, trans, target in blocks:
        if not trans: continue
        ob = orig.encode('ascii')
        nb = trans.encode('utf-8')
        search = ob + b'\x00'
        occs = []; start = 0
        while True:
            idx = data.find(search, start)
            if idx == -1: break
            occs.append(idx); start = idx + 1
        if not occs: print(f'  WARNING: not found: {orig!r}', file=sys.stderr); continue
        if target is not None and target in occs: occs = [target]
        for o in occs: patches.append((o, o + len(ob), nb))
    patches.sort()
    new = bytearray(); last = 0
    for s, e, r in patches: new.extend(data[last:s]); new.extend(r); last = e
    new.extend(data[last:])
    with open(args.output, 'wb') as f: f.write(new)
    print(f'Patched: {len(data)} -> {len(new)} bytes')

def cmd_verify(args):
    with open(args.input, 'rb') as f: data = f.read()
    blocks = parse_patch(args.patch)
    ok = miss = 0
    for orig, trans, _ in blocks:
        if not trans: continue
        search = orig.encode('ascii') + b'\x00'
        if data.find(search) == -1: miss += 1; print(f'  MISSING: {orig!r}')
        else: ok += 1
    print(f'OK: {ok}, Missing: {miss}')

def main():
    p = argparse.ArgumentParser(description='Divi-Dead .AB translator')
    sub = p.add_subparsers(dest='command', required=True)
    pe = sub.add_parser('extract'); pe.add_argument('input'); pe.add_argument('-o', '--output', required=True); pe.add_argument('--include-duplicates', action='store_true')
    pp = sub.add_parser('patch'); pp.add_argument('input'); pp.add_argument('patch'); pp.add_argument('-o', '--output', required=True)
    pv = sub.add_parser('verify'); pv.add_argument('input'); pv.add_argument('patch')
    args = p.parse_args()
    {'extract': cmd_extract, 'patch': cmd_patch, 'verify': cmd_verify}[args.command](args)

if __name__ == '__main__': main()
