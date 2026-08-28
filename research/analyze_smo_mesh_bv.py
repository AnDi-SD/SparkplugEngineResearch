#!/usr/bin/env python3
"""Read-only semantic report for decoded spMeshBV rows in corpus schema v2."""

from __future__ import annotations

import argparse
import collections
import json
import sqlite3
from pathlib import Path


MESH_BV = 0x3F453DE7
SURFACE_NAMES = {
    0: "unspecified",
    1: "stone",
    2: "dirt",
    3: "grass",
    4: "water",
    5: "snow",
    6: "swamp",
    7: "mud",
    8: "deepwater",
    9: "carpet",
}


def add_histogram(
    target: collections.Counter[int], values: dict[str, int]
) -> None:
    target.update({int(key): count for key, count in values.items()})


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    database = args.database.resolve()
    connection = sqlite3.connect(f"file:{database}?mode=ro", uri=True)

    print("profiles")
    profiles = connection.execute(
        """
        SELECT c.corpus_key,p.platform_key,COUNT(*),COUNT(DISTINCT o.file_id),
               SUM((SELECT COUNT(*) FROM file_occurrences fo
                    WHERE fo.file_id=o.file_id)),
               MIN(o.serialized_size),MAX(o.serialized_size)
        FROM objects o
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        JOIN platforms p ON p.id=c.platform_id
        WHERE o.type_hash=?
        GROUP BY c.id
        ORDER BY c.corpus_key
        """,
        (MESH_BV,),
    ).fetchall()
    for corpus, platform, objects, resources, physical, minimum, maximum in profiles:
        print(
            f"{corpus:13} {platform:3} objects={objects:4} resources={resources:3} "
            f"physical={physical:4} serialized={minimum}..{maximum}"
        )

    print("\nconfirmed variants")
    for corpus, variant, count in connection.execute(
        """
        SELECT c.corpus_key,v.variant_key,COUNT(*)
        FROM object_variant_assignments a
        JOIN class_variants v ON v.id=a.variant_id
        JOIN objects o ON o.file_id=a.file_id AND o.object_index=a.object_index
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        WHERE v.type_hash=? AND v.variant_key LIKE 'mesh_bv_%'
        GROUP BY c.id,v.id
        ORDER BY c.corpus_key,v.variant_key
        """,
        (MESH_BV,),
    ):
        print(f"{corpus:13} {variant:26} {count:4}")

    geometry: dict[str, collections.Counter[str]] = collections.defaultdict(
        collections.Counter
    )
    face_records: collections.Counter[str] = collections.Counter()
    face_masks: dict[str, collections.Counter[int]] = collections.defaultdict(
        collections.Counter
    )
    surface_types: dict[str, collections.Counter[int]] = collections.defaultdict(
        collections.Counter
    )
    flags: dict[str, collections.Counter[int]] = collections.defaultdict(
        collections.Counter
    )
    surface_ids: dict[str, collections.Counter[int]] = collections.defaultdict(
        collections.Counter
    )
    decoded_rows = connection.execute(
        """
        SELECT c.corpus_key,d.field_type,d.decoded_value
        FROM direct_fields d INDEXED BY ix_fields_semantic
        JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        WHERE o.type_hash=? AND d.semantic_key IN
              ('mesh_bv.geometry','mesh_bv.face_data')
        ORDER BY c.id,d.file_id,d.object_index,d.field_index
        """,
        (MESH_BV,),
    )
    for corpus, field_type, encoded in decoded_rows:
        value = json.loads(encoded)
        if field_type == 0:
            geometry[corpus]["objects"] += 1
            geometry[corpus]["triangles"] += value["TriangleCount"]
            geometry[corpus]["vertices"] += value["VertexCount"]
            geometry[corpus]["degenerate"] += value["DegenerateTriangles"]
            continue
        face_records[corpus] += value["FaceCount"]
        add_histogram(face_masks[corpus], value["FieldMasks"])
        add_histogram(surface_types[corpus], value["SurfaceTypes"])
        add_histogram(flags[corpus], value["Flags"])
        add_histogram(surface_ids[corpus], value["SurfaceIds"])

    print("\ngeometry")
    for corpus in sorted(geometry):
        values = geometry[corpus]
        print(
            f"{corpus:13} triangles={values['triangles']:6} "
            f"vertices={values['vertices']:6} degenerate={values['degenerate']:2}"
        )

    print("\nsurface types")
    corpora = sorted(surface_types)
    for surface in range(10):
        counts = " ".join(
            f"{corpus}={surface_types[corpus][surface]}" for corpus in corpora
        )
        print(f"{surface:2} {SURFACE_NAMES[surface]:11} {counts}")

    print("\nface metadata")
    for corpus in corpora:
        nonzero_flags = sum(
            count for value, count in flags[corpus].items() if value != 0
        )
        nonzero_ids = sum(
            count for value, count in surface_ids[corpus].items() if value != 0
        )
        print(
            f"{corpus:13} faces={face_records[corpus]:5} "
            f"field_masks={dict(sorted(face_masks[corpus].items()))} "
            f"nonzero_flags={nonzero_flags:5} nonzero_surface_ids={nonzero_ids:4}"
        )
        common_flags = flags[corpus].most_common(12)
        print(f"  common flags: {common_flags}")

    print("\nphysical parents")
    for corpus, parent_name, count in connection.execute(
        """
        SELECT c.corpus_key,
               COALESCE(parent_class.engine_name,'<root>') AS parent_name,
               COUNT(*)
        FROM objects o
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        LEFT JOIN objects parent
          ON parent.file_id=o.file_id AND parent.object_index=o.parent_index
        LEFT JOIN classes parent_class ON parent_class.type_hash=parent.type_hash
        WHERE o.type_hash=?
        GROUP BY c.id,parent.type_hash
        ORDER BY c.corpus_key,parent_name
        """,
        (MESH_BV,),
    ):
        print(f"{corpus:13} {parent_name:24} {count:4}")

    print("\nrecorded evidence")
    for kind, observation in connection.execute(
        """
        SELECT evidence_kind,observation
        FROM evidence
        WHERE type_hash=? AND evidence_kind LIKE 'class_analysis:%'
        ORDER BY evidence_kind
        """,
        (MESH_BV,),
    ):
        print(f"{kind}: {observation}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
