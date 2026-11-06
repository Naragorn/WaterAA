#!/usr/bin/env python3
"""Lists the shaders in the game's shader packages that carry the water's
wobble pair - 0.0002 and 2496.0, each exactly once, in `def` constants - the
way CalmWobble (src/WaterShaders.cpp) tells them.

Usage: tools/check-shaders.py <Oblivion>/Data/Shaders

For Oblivion 1.2.416 it names WATER000, WATER001, WATER007 and WATER015.pso
and nothing else out of 10978 shaders. A shader with only one of the two
numbers, or one of them twice, is listed as "not the pair".

A package: three dwords (magic, the entry count, the bytes of all entries),
then per entry a name of 0x100 bytes, a dword size and the shader.
"""

import os
import re
import struct
import sys

REACH = struct.unpack("<I", struct.pack("<f", 1.0 / 5000.0))[0]
LENGTH = struct.unpack("<I", struct.pack("<f", 2496.0))[0]


def entries(path):
    data = open(path, "rb").read()
    _, count, _ = struct.unpack_from("<III", data, 0)
    offset = 12
    for _ in range(count):
        name = data[offset:offset + 0x100].split(b"\0")[0].decode("latin1")
        size = struct.unpack_from("<I", data, offset + 0x100)[0]
        yield name, data[offset + 0x104:offset + 0x104 + size]
        offset += 0x104 + size


def pair(code):
    """How often the reach and the length stand in `def` constants."""
    tokens = struct.unpack_from(f"<{len(code) // 4}I", code)
    reaches = lengths = 0
    i = 1
    while i < len(tokens) and tokens[i] != 0x0000FFFF:
        token = tokens[i]
        if token & 0xFFFF == 0xFFFE:
            i += 1 + ((token >> 16) & 0x7FFF)
            continue
        length = (token >> 24) & 0xF
        if token & 0xFFFF == 0x51 and length == 5:
            for value in tokens[i + 2:i + 6]:
                reaches += value == REACH
                lengths += value == LENGTH
        i += 1 + length
    return reaches, lengths


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    folder = sys.argv[1]
    total = 0
    found = {}
    for package in sorted(os.listdir(folder)):
        if not re.fullmatch(r"shaderpackage\d+\.sdp", package, re.I):
            continue
        for name, code in entries(os.path.join(folder, package)):
            total += 1
            reaches, lengths = pair(code)
            if reaches or lengths:
                key = (name, "the pair" if (reaches, lengths) == (1, 1) else f"not the pair ({reaches}, {lengths})")
                found[key] = found.get(key, 0) + 1
    print(f"{total} shaders in the packages")
    for (name, what), count in sorted(found.items()):
        print(f"  {name}: {what}, in {count} packages")


if __name__ == "__main__":
    main()
