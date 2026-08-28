#!/usr/bin/env python3
"""Read-only structural report for spCollisionInfo in corpus schema v2."""

from __future__ import annotations

import argparse
import collections
import math
import sqlite3
import struct
from pathlib import Path


COLLISION_INFO = 0x47A97C0E


def class_name(connection: sqlite3.Connection, type_hash: int | None) -> str:
    if type_hash is None:
        return "<unresolved>"
    row = connection.execute(
        "SELECT engine_name FROM classes WHERE type_hash=?", (type_hash,)
    ).fetchone()
    return row[0] if row and row[0] else f"0x{type_hash:08X}"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    database = args.database.resolve()
    connection = sqlite3.connect(f"file:{database}?mode=ro", uri=True)

    profiles = connection.execute(
        """
        SELECT c.corpus_key,p.platform_key,COUNT(*),COUNT(DISTINCT o.file_id),
               MIN(o.serialized_size),MAX(o.serialized_size)
        FROM objects o
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        JOIN platforms p ON p.id=c.platform_id
        WHERE o.type_hash=?
        GROUP BY c.corpus_key,p.platform_key
        ORDER BY c.corpus_key
        """,
        (COLLISION_INFO,),
    ).fetchall()
    print("profiles")
    for corpus, platform, objects, resources, minimum, maximum in profiles:
        print(
            f"{corpus:13} {platform:3} objects={objects:4} resources={resources:3} "
            f"serialized={minimum}..{maximum}"
        )

    collision_rows = connection.execute(
        """
        SELECT c.corpus_key,p.platform_key,o.file_id,o.object_index,o.object_id,
               o.parent_index,parent.type_hash
        FROM objects o
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        JOIN platforms p ON p.id=c.platform_id
        LEFT JOIN objects parent
          ON parent.file_id=o.file_id AND parent.object_index=o.parent_index
        WHERE o.type_hash=?
        ORDER BY c.corpus_key,o.file_id,o.object_index
        """,
        (COLLISION_INFO,),
    ).fetchall()
    collision_keys = {(row[2], row[3]) for row in collision_rows}

    fields_by_object: dict[tuple[int, int], list[tuple[int, int, int, bytes]]] = (
        collections.defaultdict(list)
    )
    for file_id, object_index, field_index, field_type, payload_size, preview in (
        connection.execute(
            """
            SELECT d.file_id,d.object_index,d.field_index,d.field_type,
                   d.payload_size,d.payload_preview
            FROM direct_fields d
            JOIN objects o
              ON o.file_id=d.file_id AND o.object_index=d.object_index
            WHERE o.type_hash=?
            ORDER BY d.file_id,d.object_index,d.field_index
            """,
            (COLLISION_INFO,),
        )
    ):
        fields_by_object[(file_id, object_index)].append(
            (field_index, field_type, payload_size, bytes(preview))
        )

    print("\nfield-presence variants")
    shapes: collections.Counter[tuple[str, tuple[int, ...]]] = collections.Counter()
    for corpus, _, file_id, object_index, *_ in collision_rows:
        field_types = tuple(
            field_type
            for _, field_type, payload_size, _ in fields_by_object[(file_id, object_index)]
            if not (field_type == 0 and payload_size == 0)
        )
        shapes[(corpus, field_types)] += 1
    for (corpus, field_types), count in sorted(shapes.items()):
        shape = ",".join(map(str, field_types)) or "<empty>"
        print(f"{corpus:13} fields={shape:7} objects={count:4}")

    print("\ncollision groups")
    groups: collections.Counter[tuple[str, str]] = collections.Counter()
    for corpus, _, file_id, object_index, *_ in collision_rows:
        field = next(
            (
                item
                for item in fields_by_object[(file_id, object_index)]
                if item[1] == 1 and item[2] != 0
            ),
            None,
        )
        if field is None:
            groups[(corpus, "<omitted>")] += 1
        elif field[2] == 4 and len(field[3]) >= 4:
            groups[(corpus, str(struct.unpack_from("<I", field[3])[0]))] += 1
        else:
            groups[(corpus, f"<invalid:{field[2]}>")] += 1
    for (corpus, value), count in sorted(groups.items()):
        print(f"{corpus:13} group={value:9} objects={count:4}")

    object_ids: dict[tuple[int, int], list[tuple[int, int, str]]] = (
        collections.defaultdict(list)
    )
    for file_id, object_index, object_id, type_hash, name in connection.execute(
        "SELECT file_id,object_index,object_id,type_hash,name FROM objects"
    ):
        object_ids[(file_id, object_id)].append((object_index, type_hash, name))

    def relationship_profile(
        file_id: int, object_index: int
    ) -> tuple[str, int | None]:
        primitive_fields = [
            item
            for item in fields_by_object[(file_id, object_index)]
            if item[1] == 0 and item[2] != 0
        ]
        if len(primitive_fields) != 1:
            return "invalid", None
        _, _, payload_size, prefix = primitive_fields[0]
        if payload_size == 4 and len(prefix) >= 4:
            object_id = struct.unpack_from("<I", prefix)[0]
            encoding = "id_only"
        elif payload_size >= 8 and len(prefix) >= 8:
            object_id, inline_size = struct.unpack_from("<II", prefix)
            if inline_size != payload_size - 8:
                return "invalid", None
            encoding = "sized_reference" if inline_size == 0 else "inline"
        else:
            return "invalid", None
        targets = object_ids.get((file_id, object_id), [])
        return encoding, targets[0][1] if len(targets) == 1 else None

    print("\nsemantic variants")
    semantic_variants: collections.Counter[
        tuple[str, tuple[int, ...], str, str]
    ] = collections.Counter()
    for corpus, _, file_id, object_index, _, parent_index, parent_hash in collision_rows:
        field_types = tuple(
            field_type
            for _, field_type, payload_size, _ in fields_by_object[(file_id, object_index)]
            if not (field_type == 0 and payload_size == 0)
        )
        encoding, target_hash = relationship_profile(file_id, object_index)
        parent = "<root>" if parent_index is None else class_name(connection, parent_hash)
        semantic_variants[(corpus, field_types, parent, f"{encoding}:{class_name(connection, target_hash)}")] += 1
    for (corpus, field_types, parent, primitive), count in sorted(
        semantic_variants.items()
    ):
        shape = ",".join(map(str, field_types))
        print(
            f"{corpus:13} fields={shape:7} parent={parent:18} "
            f"primitive={primitive:25} {count:4}"
        )

    print("\nprimitive relationships")
    relationships: collections.Counter[tuple[str, str, str]] = collections.Counter()
    invalid_relationships = 0
    for corpus, _, file_id, object_index, *_ in collision_rows:
        encoding, target_hash = relationship_profile(file_id, object_index)
        if encoding == "invalid":
            invalid_relationships += 1
            continue
        relationships[(corpus, encoding, class_name(connection, target_hash))] += 1
    for (corpus, encoding, target), count in sorted(relationships.items()):
        print(f"{corpus:13} {encoding:15} -> {target:12} {count:4}")
    print(f"invalid relationships: {invalid_relationships}")

    print("\nphysical parents")
    parents: collections.Counter[tuple[str, str]] = collections.Counter()
    for corpus, _, _, _, _, parent_index, parent_hash in collision_rows:
        label = "<root>" if parent_index is None else class_name(connection, parent_hash)
        parents[(corpus, label)] += 1
    for (corpus, parent), count in sorted(parents.items()):
        print(f"{corpus:13} {parent:24} {count:4}")

    print("\nphysical children")
    children: collections.Counter[tuple[str, str]] = collections.Counter()
    for corpus, _, file_id, object_index, *_ in collision_rows:
        rows = connection.execute(
            "SELECT type_hash FROM objects WHERE file_id=? AND parent_index=?",
            (file_id, object_index),
        ).fetchall()
        if not rows:
            children[(corpus, "<none>")] += 1
        for (child_hash,) in rows:
            children[(corpus, class_name(connection, child_hash))] += 1
    for (corpus, child), count in sorted(children.items()):
        print(f"{corpus:13} {child:24} {count:4}")

    print("\ntransforms")
    transforms: collections.Counter[tuple[str, str]] = collections.Counter()
    scale_values: collections.Counter[tuple[str, tuple[float, float, float]]] = (
        collections.Counter()
    )
    quaternion_norms: list[float] = []
    invalid_transforms = 0
    for corpus, _, file_id, object_index, *_ in collision_rows:
        transform_fields = [
            item
            for item in fields_by_object[(file_id, object_index)]
            if item[1] == 2 and item[2] != 0
        ]
        if not transform_fields:
            transforms[(corpus, "omitted")] += 1
            continue
        if len(transform_fields) != 1 or transform_fields[0][2] != 40:
            invalid_transforms += 1
            continue
        values = struct.unpack_from("<10f", transform_fields[0][3])
        if not all(math.isfinite(value) for value in values):
            invalid_transforms += 1
            continue
        position = values[0:3]
        quaternion = values[3:7]
        scale = values[7:10]
        norm = math.sqrt(sum(value * value for value in quaternion))
        quaternion_norms.append(norm)
        scale_values[(corpus, scale)] += 1
        identity_position = all(abs(value) <= 1e-6 for value in position)
        identity_rotation = (
            all(abs(value) <= 1e-6 for value in quaternion[:3])
            and abs(abs(quaternion[3]) - 1.0) <= 1e-6
        )
        unit_scale = all(abs(value - 1.0) <= 1e-6 for value in scale)
        kind = "identity" if identity_position and identity_rotation and unit_scale else "non_identity"
        transforms[(corpus, kind)] += 1
        if norm < 1e-6 or any(abs(value) < 1e-8 for value in scale):
            invalid_transforms += 1
    for (corpus, kind), count in sorted(transforms.items()):
        print(f"{corpus:13} {kind:12} {count:4}")
    print(f"invalid transforms: {invalid_transforms}")
    if quaternion_norms:
        print(
            "quaternion norm range: "
            f"{min(quaternion_norms):.9g}..{max(quaternion_norms):.9g}"
        )
    print("most common scales")
    for (corpus, scale), count in sorted(
        scale_values.items(), key=lambda item: (-item[1], item[0])
    )[:20]:
        print(f"{corpus:13} scale={scale!s:36} {count:4}")

    print("\nrecorded evidence")
    for kind, observation in connection.execute(
        """
        SELECT evidence_kind,observation
        FROM evidence
        WHERE type_hash=? AND evidence_kind LIKE 'class_analysis:%'
        ORDER BY evidence_kind
        """,
        (COLLISION_INFO,),
    ):
        print(f"{kind}: {observation}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
