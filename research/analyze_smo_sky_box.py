#!/usr/bin/env python3
"""Read-only full-corpus report for spSkyBox and its inline models."""

from __future__ import annotations

import argparse
import collections
import hashlib
import itertools
import sqlite3
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_smo_mesh_navigation_set import (  # noqa: E402
    canonical_path,
    decode_relationship,
    read_resource,
)

SKY_BOX = 0x7A7124AF
MODEL = 0x763277DB


@dataclass(frozen=True)
class SkyBox:
    corpus: str
    file_id: int
    path: str
    object_index: int
    object_id: int
    name: str
    position: tuple[float, float, float]
    rotation: tuple[float, float, float, float]
    scale: tuple[float, float, float]
    static: bool
    animated: bool
    models: tuple[object, ...]
    serialized_hash: str


def load(connection: sqlite3.Connection) -> list[SkyBox]:
    rows = connection.execute(
        """
        SELECT c.corpus_key,o.file_id,f.relative_path,o.object_index,o.object_id,
               o.name,c.source_kind,c.source_root,ct.relative_path,fo.byte_offset,
               f.byte_size
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        LEFT JOIN file_occurrences fo ON fo.id=(
            SELECT MIN(inner_fo.id) FROM file_occurrences inner_fo
            WHERE inner_fo.file_id=f.id)
        LEFT JOIN containers ct ON ct.id=fo.container_id
        WHERE o.type_hash=? ORDER BY c.id,o.file_id,o.object_index
        """, (SKY_BOX,),
    ).fetchall()
    resources: dict[int, bytes] = {}
    object_maps: dict[int, dict[int, tuple[int, int, str]]] = {}
    result: list[SkyBox] = []
    for (corpus, file_id, path, index, object_id, name, source_kind,
         source_root, container_path, occurrence_offset, byte_size) in rows:
        if file_id not in resources:
            resources[file_id] = read_resource(
                source_kind, source_root, path, container_path,
                occurrence_offset, byte_size,
            )
            object_maps[file_id] = {
                row_id: (row_index, row_type, row_name)
                for row_index, row_id, row_type, row_name in connection.execute(
                    """SELECT object_index,object_id,type_hash,name FROM objects
                       WHERE file_id=?""", (file_id,)
                )
            }
        resource = resources[file_id]
        rows_fields = connection.execute(
            """SELECT section_index,field_type,payload_size,payload_preview,
                      absolute_payload_offset
               FROM direct_fields WHERE file_id=? AND object_index=?
                 AND is_section_terminator=0 ORDER BY field_index""",
            (file_id, index),
        ).fetchall()
        fields: dict[tuple[int, int, int], bytes] = {}
        occurrences: collections.Counter[tuple[int, int]] = collections.Counter()
        serialized = bytearray()
        for section, field_type, size, preview, offset in rows_fields:
            payload = resource[offset:offset + size]
            if len(payload) != size or payload[:len(preview)] != bytes(preview):
                raise ValueError("source sky-box field does not match database")
            key = section, field_type
            occurrence = occurrences[key]
            occurrences[key] += 1
            fields[(section, field_type, occurrence)] = payload
            serialized += struct.pack("<III", section, field_type, size) + payload
        if any(key[0] == 1 and key[1] != 0 for key in fields) or \
                any(key[0] == 0 and key[1] not in range(9) for key in fields):
            raise ValueError("unexpected spSkyBox field")
        models = tuple(
            decode_relationship(fields[(1, 0, occurrence)], object_maps[file_id])
            for occurrence in range(occurrences[(1, 0)])
        )
        if not models or any(model.target_type != MODEL for model in models):
            raise ValueError("spSkyBox model relationship is missing or has wrong type")
        position = struct.unpack("<3f", fields.get((0, 0, 0), bytes(12)))
        rotation = struct.unpack("<4f", fields.get(
            (0, 1, 0), struct.pack("<4f", 0, 0, 0, 1)))
        scale = struct.unpack("<3f", fields.get(
            (0, 2, 0), struct.pack("<3f", 1, 1, 1)))
        static = fields.get((0, 4, 0), b"\0")
        animated = fields.get((0, 8, 0), b"\0")
        if static not in (b"\0", b"\1") or animated not in (b"\0", b"\1"):
            raise ValueError("invalid node Boolean")
        result.append(SkyBox(
            corpus, file_id, path, index, object_id, name, position, rotation,
            scale, bool(static[0]), bool(animated[0]), models,
            hashlib.sha256(serialized).hexdigest(),
        ))
    return result


def pairing(items: list[SkyBox], corpus: str):
    ordinals: collections.Counter[tuple[str, str]] = collections.Counter()
    result = {}
    for item in (value for value in items if value.corpus == corpus):
        base = canonical_path(item.path), item.name.lower()
        ordinal = ordinals[base]
        ordinals[base] += 1
        result[(*base, ordinal)] = item
    return result


def core(item: SkyBox):
    return (
        item.position, item.rotation, item.scale, item.static, item.animated,
        len(item.models), tuple(model.target_type for model in item.models),
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    connection = sqlite3.connect(args.database)
    items = load(connection)
    print("objects", len(items))
    for corpus, iterator in itertools.groupby(items, lambda item: item.corpus):
        group = list(iterator)
        print(corpus, "objects", len(group),
              "models", collections.Counter(len(item.models) for item in group),
              "encodings", collections.Counter(
                  model.encoding for item in group for model in item.models),
              "static", collections.Counter(item.static for item in group),
              "animated", collections.Counter(item.animated for item in group),
              "positions", len({item.position for item in group}),
              "rotations", len({item.rotation for item in group}),
              "scales", len({item.scale for item in group}))
    for left_name, right_name in (
        ("pc-working", "pc-pristine"),
        ("pc-pristine", "ps2-pristine"),
    ):
        left, right = pairing(items, left_name), pairing(items, right_name)
        common = left.keys() & right.keys()
        print("compare", left_name, right_name, "paired", len(common),
              "core", sum(core(left[key]) == core(right[key]) for key in common),
              "bytes", sum(left[key].serialized_hash == right[key].serialized_hash
                           for key in common),
              "left_only", len(left.keys() - right.keys()),
              "right_only", len(right.keys() - left.keys()))
        if left_name == "pc-pristine":
            for key in common:
                if core(left[key]) != core(right[key]):
                    print(" difference", key, core(left[key]), core(right[key]))
    print("names", collections.Counter(item.name.lower() for item in items))
    connection.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
