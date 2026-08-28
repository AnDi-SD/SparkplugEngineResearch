#!/usr/bin/env python3
"""Read-only full-corpus report for spNavigationPortal."""

from __future__ import annotations

import argparse
import collections
import hashlib
import sqlite3
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_smo_mesh_navigation_set import (  # noqa: E402
    MESH_NAVIGATION_SET,
    NAVIGATION_GRAPH,
    canonical_path,
    decode_relationship,
    read_resource,
)


NAVIGATION_PORTAL = 0x385662AA


@dataclass(frozen=True)
class Portal:
    corpus: str
    file_id: int
    path: str
    object_index: int
    object_id: int
    name: str
    parent_index: int | None
    position: tuple[float, float, float]
    animated: bool
    graph: object
    sets: tuple[object, object]
    node_pairs: tuple[tuple[int, int], ...]
    paths: tuple[tuple[int, int, int], ...]
    serialized_hash: str


def relationship_size(payload: bytes, offset: int) -> int:
    if len(payload) - offset < 4:
        raise ValueError("truncated relationship")
    if len(payload) - offset == 4:
        return 4
    inline_size = struct.unpack_from("<I", payload, offset + 4)[0]
    size = 8 + inline_size
    if size > len(payload) - offset:
        raise ValueError("relationship exceeds its field")
    return size


def decode_two_relationships(payload: bytes, object_map):
    first_size = relationship_size(payload, 0)
    if first_size >= len(payload):
        raise ValueError("portal set field does not contain two relationships")
    first = decode_relationship(payload[:first_size], object_map)
    second = decode_relationship(payload[first_size:], object_map)
    return first, second


def load(connection: sqlite3.Connection) -> list[Portal]:
    rows = connection.execute(
        """
        SELECT c.corpus_key,o.file_id,f.relative_path,o.object_index,o.object_id,
               o.name,o.parent_index,c.source_kind,c.source_root,ct.relative_path,
               fo.byte_offset,f.byte_size
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        LEFT JOIN file_occurrences fo ON fo.id=(
            SELECT MIN(inner_fo.id) FROM file_occurrences inner_fo
            WHERE inner_fo.file_id=f.id)
        LEFT JOIN containers ct ON ct.id=fo.container_id
        WHERE o.type_hash=? ORDER BY c.id,o.file_id,o.object_index
        """,
        (NAVIGATION_PORTAL,),
    ).fetchall()
    resources: dict[int, bytes] = {}
    object_maps: dict[int, dict[int, tuple[int, int, str]]] = {}
    result: list[Portal] = []
    for (
        corpus, file_id, path, index, object_id, name, parent_index,
        source_kind, source_root, container_path, occurrence_offset, byte_size,
    ) in rows:
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
            """SELECT field_index,section_index,field_type,payload_size,
                      payload_preview,absolute_payload_offset
               FROM direct_fields WHERE file_id=? AND object_index=?
                 AND is_section_terminator=0 ORDER BY field_index""",
            (file_id, index),
        ).fetchall()
        fields: dict[tuple[int, int, int], bytes] = {}
        occurrences: collections.Counter[tuple[int, int]] = collections.Counter()
        serialized = bytearray()
        for _, section, field_type, size, preview, offset in rows_fields:
            payload = resource[offset:offset + size]
            if len(payload) != size or payload[:len(preview)] != bytes(preview):
                raise ValueError("source portal field does not match database")
            key = (section, field_type)
            occurrence = occurrences[key]
            occurrences[key] += 1
            fields[(section, field_type, occurrence)] = payload
            serialized += struct.pack("<III", section, field_type, size) + payload
        required = {(0, 0, 0), (1, 0, 0), (1, 1, 0)}
        if not required.issubset(fields):
            raise ValueError(f"incomplete portal {corpus}:{path}[{index}]")
        unexpected = [
            key for key in fields
            if key not in required and
            not (key[0] == 0 and key[1] in (1, 2, 8)) and
            not (key[0] == 1 and key[1] in (2, 3))
        ]
        if unexpected:
            raise ValueError(f"unexpected portal fields: {unexpected}")
        animated = fields.get((0, 8, 0), b"\0")
        if len(fields[(0, 0, 0)]) != 12 or animated not in (b"\0", b"\1"):
            raise ValueError("invalid portal node section")
        graph = decode_relationship(fields[(1, 0, 0)], object_maps[file_id])
        sets = decode_two_relationships(fields[(1, 1, 0)], object_maps[file_id])
        node_pairs = tuple(
            tuple(fields[(1, 2, occurrence)])
            for occurrence in range(occurrences[(1, 2)])
        )
        paths = tuple(
            tuple(fields[(1, 3, occurrence)])
            for occurrence in range(occurrences[(1, 3)])
        )
        if graph.target_type != NAVIGATION_GRAPH:
            raise ValueError("portal graph relationship has wrong target")
        if any(value.target_type != MESH_NAVIGATION_SET for value in sets):
            raise ValueError("portal navigation-set relationship has wrong target")
        if not node_pairs or not paths:
            raise ValueError("portal has no node pair or path record")
        result.append(Portal(
            corpus, file_id, path, index, object_id, name, parent_index,
            struct.unpack("<3f", fields[(0, 0, 0)]), bool(animated[0]), graph, sets,
            node_pairs, paths, hashlib.sha256(serialized).hexdigest(),
        ))
    return result


def key(items: list[Portal], corpus: str):
    ordinals: collections.Counter[tuple[str, str]] = collections.Counter()
    result = {}
    for item in (value for value in items if value.corpus == corpus):
        base = canonical_path(item.path), item.name.lower()
        ordinal = ordinals[base]
        ordinals[base] += 1
        result[(*base, ordinal)] = item
    return result


def core(item: Portal):
    return (
        item.position, item.animated, item.node_pairs, item.paths,
        tuple(value.target_type for value in item.sets), item.graph.target_type,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    connection = sqlite3.connect(args.database)
    items = load(connection)
    print("objects", len(items))
    for corpus, group_iterator in __import__("itertools").groupby(
        items, lambda item: item.corpus
    ):
        group = list(group_iterator)
        print(
            corpus, "objects", len(group),
            "node_pairs", (min(len(x.node_pairs) for x in group),
                           max(len(x.node_pairs) for x in group)),
            collections.Counter(len(x.node_pairs) for x in group),
            "paths", (min(len(x.paths) for x in group),
                      max(len(x.paths) for x in group)),
            "set_encodings", collections.Counter(
                tuple(value.encoding for value in x.sets) for x in group),
            "graph_encodings", collections.Counter(x.graph.encoding for x in group),
        )
        print(
            " node values", collections.Counter(
                value for x in group for pair in x.node_pairs for value in pair),
            "path src/dst/index ranges",
            (min(v[0] for x in group for v in x.paths),
             max(v[0] for x in group for v in x.paths)),
            (min(v[1] for x in group for v in x.paths),
             max(v[1] for x in group for v in x.paths)),
            (min(v[2] for x in group for v in x.paths),
             max(v[2] for x in group for v in x.paths)),
        )
    for left_name, right_name in (
        ("pc-working", "pc-pristine"),
        ("pc-pristine", "ps2-pristine"),
    ):
        left, right = key(items, left_name), key(items, right_name)
        keys = left.keys() & right.keys()
        print(
            "compare", left_name, right_name, "paired", len(keys),
            "core", sum(core(left[k]) == core(right[k]) for k in keys),
            "bytes", sum(left[k].serialized_hash == right[k].serialized_hash
                         for k in keys),
            "left_only", len(left.keys() - right.keys()),
            "right_only", len(right.keys() - left.keys()),
        )
    print("parents")
    for row in connection.execute(
        """SELECT c.corpus_key,pc.engine_name,COUNT(*) FROM objects o
           JOIN files f ON f.id=o.file_id JOIN corpora c ON c.id=f.corpus_id
           LEFT JOIN objects p ON p.file_id=o.file_id AND p.object_index=o.parent_index
           LEFT JOIN classes pc ON pc.type_hash=p.type_hash
           WHERE o.type_hash=? GROUP BY c.id,p.type_hash ORDER BY c.id""",
        (NAVIGATION_PORTAL,),
    ):
        print(*row)
    connection.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
