#!/usr/bin/env python3
"""Read-only corpus report for spZonePortalNode in schema v2."""

from __future__ import annotations

import argparse
import collections
import json
import math
import sqlite3
import struct
from pathlib import Path


ZONE_PORTAL_NODE = 0xABB5AB2C


def direction(name: str) -> str:
    lowered = name.lower()
    if lowered.endswith("fronttoback"):
        return "front_to_back"
    if lowered.endswith("backtofront"):
        return "back_to_front"
    return "unmarked"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    connection = sqlite3.connect(f"file:{args.database.resolve()}?mode=ro", uri=True)

    print("profiles")
    for row in connection.execute(
        """
        SELECT c.corpus_key,p.platform_key,COUNT(*),COUNT(DISTINCT o.file_id),
               MIN(o.serialized_size),MAX(o.serialized_size),SUM(o.name <> '')
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id JOIN platforms p ON p.id=c.platform_id
        WHERE o.type_hash=? GROUP BY c.id ORDER BY c.id
        """,
        (ZONE_PORTAL_NODE,),
    ):
        print(row)

    print("\nnon-default shape resources")
    for row in connection.execute(
        """
        SELECT c.corpus_key,f.relative_path,
               CASE WHEN o.field_shape LIKE '%s0:f1:16%' THEN 'rotation'
                    WHEN o.field_shape LIKE '%s0:f4:1%' THEN 'static'
                    ELSE 'position_only' END AS variant,
               COUNT(*),GROUP_CONCAT(o.name, ',')
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id WHERE o.type_hash=?
          AND (o.field_shape LIKE '%s0:f1:16%' OR o.field_shape LIKE '%s0:f4:1%')
        GROUP BY c.id,f.id,variant ORDER BY c.id,f.relative_path
        """,
        (ZONE_PORTAL_NODE,),
    ):
        print(row)

    print("\nshapes")
    for row in connection.execute(
        """
        SELECT c.corpus_key,o.field_shape,COUNT(*),COUNT(DISTINCT o.file_id)
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id WHERE o.type_hash=?
        GROUP BY c.id,o.field_shape ORDER BY c.id,COUNT(*) DESC
        """,
        (ZONE_PORTAL_NODE,),
    ):
        print(row)

    node_values: collections.Counter[tuple[str, int, str]] = collections.Counter()
    positions: collections.defaultdict[str, list[tuple[float, float, float]]] = (
        collections.defaultdict(list)
    )
    pair_order: collections.Counter[tuple[str, str, str]] = collections.Counter()
    center_errors: collections.Counter[tuple[str, str]] = collections.Counter()
    examples: list[tuple[object, ...]] = []
    rows = connection.execute(
        """
        SELECT c.corpus_key,o.file_id,f.relative_path,o.object_index,o.name,d.field_index,
               d.section_index,d.field_type,d.payload_size,d.payload_preview
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        JOIN direct_fields d ON d.file_id=o.file_id AND d.object_index=o.object_index
        WHERE o.type_hash=? AND d.is_section_terminator=0
        ORDER BY c.id,o.file_id,o.object_index,d.field_index
        """,
        (ZONE_PORTAL_NODE,),
    )
    grouped: dict[tuple[str, int, str, int, str], list[tuple[int, int, int, int, bytes]]] = {}
    for corpus, file_id, path, index, name, field_index, section, field_type, size, preview in rows:
        grouped.setdefault((corpus, file_id, path, index, name), []).append(
            (field_index, section, field_type, size, preview)
        )

    for (corpus, file_id, path, index, name), fields in grouped.items():
        relations: list[tuple[int, str]] = []
        position: tuple[float, float, float] | None = None
        for _field_index, section, field_type, size, preview in fields:
            if section == 0:
                if field_type == 0 and size == 12:
                    position = struct.unpack("<3f", preview[:12])
                    positions[corpus].append(position)
                    continue
                elif field_type == 1 and size == 16:
                    value = ",".join(
                        f"{item:.6g}" for item in struct.unpack("<4f", preview[:16])
                    )
                elif size == 1:
                    value = str(preview[0])
                else:
                    value = preview.hex().upper()
                node_values[(corpus, field_type, value)] += 1
            elif section == 1 and field_type == 0 and size == 8:
                object_id = struct.unpack("<I", preview[:4])[0]
                target = connection.execute(
                    "SELECT name,object_index FROM objects WHERE file_id=? AND object_id=?",
                    (file_id, object_id),
                ).fetchone()
                if target is None:
                    raise RuntimeError(f"unresolved relationship {path} [{index}] -> {object_id}")
                relations.append((target[1], target[0]))

        if len(relations) != 2:
            raise RuntimeError(f"expected two portal relationships in {path} [{index}]")
        pair_order[(corpus, direction(relations[0][1]), direction(relations[1][1]))] += 1

        portal_field = connection.execute(
            """
            SELECT d.payload_preview,d.decoded_value FROM direct_fields d
            WHERE d.file_id=? AND d.object_index=? AND d.section_index=0
              AND d.field_type=1 AND d.is_section_terminator=0
            """,
            (file_id, relations[0][0]),
        ).fetchone()
        if position is None or portal_field is None:
            center_errors[(corpus, "missing") ] += 1
            continue
        payload, decoded = portal_field
        if decoded:
            vertices = json.loads(decoded)["Vertices"]
            count = len(vertices)
        else:
            count = struct.unpack("<I", payload[:4])[0]
            if len(payload) < 4 + count * 12:
                center_errors[(corpus, "truncated_preview")] += 1
                continue
            vertices = [
                struct.unpack_from("<3f", payload, 4 + vertex * 12)
                for vertex in range(count)
            ]
        center = tuple(sum(vertex[axis] for vertex in vertices) / count for axis in range(3))
        error = math.sqrt(sum((position[axis] - center[axis]) ** 2 for axis in range(3)))
        center_errors[(corpus, "centroid" if error <= 1e-3 else "different")] += 1
        if error > 1e-3 and len(examples) < 12:
            examples.append((corpus, path, index, name, position, center, error))

    print("\nnode field values")
    for corpus, values in sorted(positions.items()):
        bounds = tuple(
            (min(value[axis] for value in values), max(value[axis] for value in values))
            for axis in range(3)
        )
        print(corpus, "position", len(values), "distinct", len(set(values)), "bounds", bounds)
    for key, count in sorted(node_values.items()):
        print(*key, count)
    print("\nportal relationship order")
    for key, count in sorted(pair_order.items()):
        print(*key, count)
    print("\nposition versus first-polygon centroid")
    for key, count in sorted(center_errors.items()):
        print(*key, count)
    if examples:
        print("\ncentroid differences")
        for item in examples:
            print(item)

    print("\nanalyzed annotations")
    for row in connection.execute(
        """
        SELECT c.corpus_key,d.semantic_key,COUNT(*),SUM(d.is_decoded)
        FROM direct_fields d
        JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
        JOIN files f ON f.id=o.file_id JOIN corpora c ON c.id=f.corpus_id
        WHERE o.type_hash=? AND d.is_section_terminator=0
        GROUP BY c.id,d.semantic_key ORDER BY c.id,d.semantic_key
        """,
        (ZONE_PORTAL_NODE,),
    ):
        print(row)
    print("\nanalysis rows")
    print(
        "field_definitions",
        connection.execute(
            "SELECT COUNT(*) FROM field_definitions WHERE type_hash=?",
            (ZONE_PORTAL_NODE,),
        ).fetchone()[0],
    )
    print(
        "variants",
        connection.execute(
            "SELECT COUNT(*) FROM class_variants WHERE type_hash=?",
            (ZONE_PORTAL_NODE,),
        ).fetchone()[0],
    )
    print(
        "assignments",
        connection.execute(
            """SELECT COUNT(*) FROM object_variant_assignments a
               JOIN class_variants v ON v.id=a.variant_id WHERE v.type_hash=?""",
            (ZONE_PORTAL_NODE,),
        ).fetchone()[0],
    )
    print(
        "evidence",
        connection.execute(
            "SELECT COUNT(*) FROM evidence WHERE type_hash=?",
            (ZONE_PORTAL_NODE,),
        ).fetchone()[0],
    )
    for row in connection.execute(
        """SELECT evidence_kind,locator,observation FROM evidence
           WHERE type_hash=? ORDER BY id""",
        (ZONE_PORTAL_NODE,),
    ):
        print(row)
    connection.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
