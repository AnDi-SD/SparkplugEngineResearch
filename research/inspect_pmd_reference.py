"""Inspect PMD 1.0 skeleton/IK/physics using only Python's standard library.

Usage: python research/inspect_pmd_reference.py model.pmd [another.pmd ...]
The JSON output describes the supplied files; no model or motion is written.
Binary layout cross-check: MMD-Blender/blender_mmd_tools, core/pmd/__init__.py.
Geometry is skipped because SAN -> VMD research only needs the skeleton.
"""

import hashlib
import json
from pathlib import Path
import struct
import sys


class Reader:
    def __init__(self, data):
        self.data = data
        self.offset = 0

    def take(self, size):
        if size < 0 or size > len(self.data) - self.offset:
            raise ValueError(f"Truncated PMD at byte {self.offset}, need {size}")
        start = self.offset
        self.offset += size
        return self.data[start:self.offset]

    def unpack(self, fmt):
        return struct.unpack("<" + fmt, self.take(struct.calcsize("<" + fmt)))

    def number(self, fmt):
        return self.unpack(fmt)[0]

    def text(self, size):
        return self.take(size).split(b"\0", 1)[0].decode("cp932")


def inspect(path):
    raw = path.read_bytes()
    r = Reader(raw)
    if r.take(3) != b"Pmd" or r.number("f") != 1.0:
        raise ValueError("Expected PMD 1.0")
    report = {"path": str(path), "sha256": hashlib.sha256(raw).hexdigest(),
              "name": r.text(20)}
    r.take(256)  # Model comment.
    report["vertices"] = r.number("I")
    r.take(report["vertices"] * 38)
    r.take(r.number("I") * 2)  # Triangle vertex indices.
    r.take(r.number("I") * 70)  # Materials and texture names.

    bones = []
    for index in range(r.number("H")):
        name = r.text(20)
        parent, tail, kind, influence, x, y, z = r.unpack("HHBH3f")
        bones.append({"index": index, "name": name,
                      "parent": None if parent == 65535 else parent,
                      "tail": tail, "type": kind, "influence": influence,
                      "position": [x, y, z]})
    for bone in bones:
        parent = bone["parent"]
        if parent is not None and parent >= len(bones):
            raise ValueError("PMD bone parent is outside bone table")
        bone["parent_name"] = bones[parent]["name"] if parent is not None else None
    report["bones"] = bones

    iks = []
    for _ in range(r.number("H")):
        controller, target, chain_length, iterations, weight = r.unpack("HHBHf")
        chain = list(r.unpack("H" * chain_length))
        if any(i >= len(bones) for i in [controller, target, *chain]):
            raise ValueError("PMD IK reference is outside bone table")
        iks.append({"controller": bones[controller]["name"],
                    "target": bones[target]["name"],
                    "chain": [bones[i]["name"] for i in chain],
                    "iterations": iterations, "weight": weight})
    report["iks"] = iks

    morph_count = r.number("H")
    for _ in range(morph_count):
        r.take(20)
        count = r.number("I")
        r.take(1 + count * 16)
    r.take(r.number("B") * 2)  # Morph display indices.
    display_count = r.number("B")
    r.take(display_count * 50)
    r.take(r.number("I") * 3)  # Bone display rows.

    report["rigid_bodies"] = []
    report["joints"] = 0
    # Old PMD files may end before the English names, toon or physics extension.
    if r.offset == len(raw):
        return report
    if r.number("B"):
        report["name_english"] = r.text(20)
        r.take(256)
        for bone in bones:
            bone["name_english"] = r.text(20)
        r.take(max(0, morph_count - 1) * 20 + display_count * 50)
    r.take(1000)  # Ten toon texture filenames.
    if r.offset == len(raw):
        return report
    for _ in range(r.number("I")):
        # PMD rigid body: 20-byte name, bone index, group/mask, shape, five
        # physical coefficients and transform; final byte selects physics mode.
        name = r.text(20)
        bone_index = r.number("H")
        r.take(60)
        mode = r.number("B")
        if bone_index != 65535 and bone_index >= len(bones):
            raise ValueError("PMD rigid body bone is outside bone table")
        report["rigid_bodies"].append({
            "name": name, "mode": mode,
            "bone": None if bone_index == 65535 else bones[bone_index]["name"],
        })
    report["joints"] = r.number("I")
    r.take(report["joints"] * 124)
    if r.offset != len(raw):
        raise ValueError(f"Unaccounted PMD tail: {len(raw) - r.offset} bytes")
    return report


if __name__ == "__main__":
    for name in sys.argv[1:]:
        print(json.dumps(inspect(Path(name)), ensure_ascii=True))
