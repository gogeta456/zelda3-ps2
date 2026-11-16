#!/usr/bin/env python3
"""Build and verify an ISO9660 PS2 homebrew disc. Requires: pip install pycdlib."""
import argparse
import hashlib
import io
from pathlib import Path
import struct
import pycdlib


def build(elf, assets, output):
    executable = elf.read_bytes()
    if executable[:6] != b'\x7fELF\x01\x01' or struct.unpack_from('<H', executable, 18)[0] != 8:
        raise ValueError('Expected a 32-bit little-endian MIPS ELF for PS2')
    cnf = b'BOOT2 = cdrom0:\\ZELDA3.ELF;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n'
    entries = {'/SYSTEM.CNF;1': cnf, '/ZELDA3.ELF;1': executable,
               '/Z3ASSETS.DAT;1': assets.read_bytes(),
               '/LEIAME.TXT;1': Path(__file__).with_name('LEIA-ME.txt').read_bytes()}
    if not entries['/Z3ASSETS.DAT;1']:
        raise ValueError('Empty assets file')
    output.parent.mkdir(parents=True, exist_ok=True)
    iso = pycdlib.PyCdlib()
    iso.new(interchange_level=1, sys_ident='PLAYSTATION', vol_ident='ZELDA3', pub_ident_str='ZELDA3 PS2 PORT')
    streams = []
    try:
        for name, data in entries.items():
            stream = io.BytesIO(data)
            streams.append(stream)
            iso.add_fp(stream, len(data), iso_path=name)
        iso.write(str(output))
    finally:
        iso.close()
    check = pycdlib.PyCdlib()
    check.open(str(output))
    try:
        for name, data in entries.items():
            recovered = io.BytesIO()
            check.get_file_from_iso_fp(recovered, iso_path=name)
            if recovered.getvalue() != data:
                raise RuntimeError('ISO verification failed: ' + name)
            print(name, len(data), hashlib.sha256(data).hexdigest())
    finally:
        check.close()
    print('Verified ISO:', output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', type=Path)
    parser.add_argument('assets', type=Path)
    parser.add_argument('output', nargs='?', type=Path, default=Path('zelda3-ps2.iso'))
    args = parser.parse_args()
    build(args.elf, args.assets, args.output)
