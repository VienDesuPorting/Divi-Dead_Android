#!/usr/bin/env python3
"""Repack a modified .AB file back into SG.DL1."""
import struct, sys, os

def lz_compress(data):
    ring = bytearray(0x1000); rinp = 0xFEE
    out = bytearray(); pos = 0; ln = len(data); ops = []
    while pos < ln:
        best_len = 0; best_pos = 0
        if pos + 2 < ln:
            for off in range(1, 0x1000):
                p = (rinp - off) & 0xFFF; ml = 0
                while ml < 18 and pos+ml < ln and ring[p] == data[pos+ml]:
                    ml += 1; p = (p+1) & 0xFFF
                if ml > best_len: best_len = ml; best_pos = (rinp-off) & 0xFFF
                    if best_len >= 18: break
        if best_len >= 3:
            lc = best_len - 3
            d = (best_pos & 0xFF) | ((best_pos & 0xF00) << 4) | (lc << 8)
            ops.append((False, d))
            for i in range(best_len): ring[rinp] = data[pos+i]; rinp = (rinp+1) & 0xFFF
            pos += best_len
        else:
            ops.append((True, data[pos])); ring[rinp] = data[pos]; rinp = (rinp+1) & 0xFFF; pos += 1
        if len(ops) == 8:
            ctrl = sum(1 << i for i, (l, _) in enumerate(ops) if l)
            out.append(ctrl)
            for l, v in ops: out.append(v & 0xFF) if l else out.extend([v & 0xFF, (v >> 8) & 0xFF])
            ops = []
    if ops:
        ctrl = sum(1 << i for i, (l, _) in enumerate(ops) if l)
        out.append(ctrl)
        for l, v in ops: out.append(v & 0xFF) if l else out.extend([v & 0xFF, (v >> 8) & 0xFF])
    return bytes(out)

def main():
    import argparse
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('sg_dl1'); p.add_argument('ab_name'); p.add_argument('new_ab'); p.add_argument('-o', '--output', required=True)
    args = p.parse_args()

    with open(args.sg_dl1, 'rb') as f: data = f.read()
    count = struct.unpack('<H', data[8:10])[0]
    table_off = struct.unpack('<I', data[10:14])[0]
    files = []; cursor = table_off; pos = 0x10
    for i in range(count):
        name = data[cursor:cursor+12].split(b'\x00')[0].decode('ascii', errors='replace').strip()
        cursor += 12; length = struct.unpack('<I', data[cursor:cursor+4])[0]; cursor += 4
        files.append({'name': name, 'length': length, 'pos': pos}); pos += length
    for fi in files:
        raw = data[fi['pos']:fi['pos']+fi['length']]
        fi['is_lz'] = raw[:2] == b'LZ'
        if fi['is_lz']:
            fi['compressed_data'] = raw[10:]
            fi['uncompressed_size'] = struct.unpack('<I', raw[6:10])[0]
        else: fi['raw_data'] = raw

    with open(args.new_ab, 'rb') as f: new_data = f.read()
    target = next((f for f in files if f['name'].upper() == args.ab_name.upper()), None)
    if not target: print(f'ERROR: {args.ab_name} not found', file=sys.stderr); sys.exit(1)

    if target['is_lz']:
        comp = lz_compress(new_data)
        target['new_data'] = b'LZ' + struct.pack('<I', len(comp)) + struct.pack('<I', len(new_data)) + comp
    else: target['new_data'] = new_data

    out = bytearray(0x10); out[:8] = b'DL1.0\x1a\x00\x00'
    for fi in files:
        fi['new_pos'] = len(out)
        raw = fi.get('new_data') or (b'LZ' + struct.pack('<I', len(fi['compressed_data'])) + struct.pack('<I', fi['uncompressed_size']) + fi['compressed_data'] if fi['is_lz'] else fi['raw_data'])
        fi['new_length'] = len(raw); out.extend(raw)
    tbl = len(out)
    for fi in files:
        nb = fi['name'].encode('ascii')[:12]; out.extend(nb + b'\x00'*(12-len(nb))); out.extend(struct.pack('<I', fi['new_length']))
    struct.pack_into('<H', out, 8, len(files)); struct.pack_into('<I', out, 10, tbl)
    with open(args.output, 'wb') as f: f.write(out)
    print(f'Written {args.output} ({len(out)} bytes)')

if __name__ == '__main__': main()
