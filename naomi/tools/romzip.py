#!/usr/bin/env python3
"""Repack a game's ROM zip with the file names this MAME (0.34/0.35) expects.

Modern romsets name the files differently (epr-6916a.ic8 instead of
6916.rom). The driver's ROM_START block lists name and CRC32 of each file;
files are taken from the given zip by CRC and written under the old names.

    romzip.py DRIVER_SOURCE GAME IN.zip OUT.zip
"""
import re
import sys
import zipfile
import zlib


def expected(source, game):
    text = open(source, encoding='latin-1').read()
    m = re.search(r'ROM_START\(\s*%s_rom\s*\)(.*?)ROM_END' % re.escape(game), text, re.S)
    if not m:
        sys.exit('no ROM_START(%s_rom) in %s' % (game, source))
    out = []
    for name, crc in re.findall(r'ROM_LOAD\w*\s*\(\s*"([^"]+)"\s*,[^,]+,[^,]+,\s*(0x[0-9a-fA-F]+)\s*\)', m.group(1)):
        out.append((name, int(crc, 16)))
    return out


def main():
    source, game, src, dst = sys.argv[1:5]
    want = expected(source, game)
    by_crc = {}
    with zipfile.ZipFile(src) as z:
        for info in z.infolist():
            data = z.read(info)
            by_crc[zlib.crc32(data) & 0xffffffff] = data
    missing = []
    with zipfile.ZipFile(dst, 'w', zipfile.ZIP_DEFLATED) as z:
        for name, crc in want:
            if crc not in by_crc:
                missing.append(name)
                continue
            z.writestr(name, by_crc[crc])
    print('%s: %d files' % (dst, len(want) - len(missing)))
    if missing:
        sys.exit('missing: ' + ', '.join(missing))


if __name__ == '__main__':
    main()
