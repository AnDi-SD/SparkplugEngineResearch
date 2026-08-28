#!/usr/bin/env python3
"""Read-only structural report for spOctreeNode in corpus schema v2."""

from __future__ import annotations

import argparse
import collections
import sqlite3
import struct
from pathlib import Path


OCTREE_NODE = 0x21A70829


def canonical_path(path: str) -> str:
    value = path.replace("\\", "/").lower()
    return value[5:] if value.startswith("data/") else value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    connection = sqlite3.connect(f"file:{args.database.resolve()}?mode=ro", uri=True)
    class_names = {
        value: name or f"0x{value:08X}"
        for value, name in connection.execute(
            "SELECT type_hash,engine_name FROM classes"
        )
    }

    print("profiles")
    for row in connection.execute(
        """
        SELECT c.corpus_key,p.platform_key,COUNT(*),COUNT(DISTINCT o.file_id),
               SUM((SELECT COUNT(*) FROM file_occurrences fo
                    WHERE fo.file_id=o.file_id)),MIN(o.serialized_size),
               MAX(o.serialized_size),SUM(o.name <> '')
        FROM objects o
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        JOIN platforms p ON p.id=c.platform_id
        WHERE o.type_hash=? GROUP BY c.id ORDER BY c.corpus_key
        """,
        (OCTREE_NODE,),
    ):
        print(row)

    print("\nphysical parents")
    for corpus, parent_hash, count in connection.execute(
        """
        SELECT c.corpus_key,parent.type_hash,COUNT(*)
        FROM objects o
        JOIN files f ON f.id=o.file_id JOIN corpora c ON c.id=f.corpus_id
        LEFT JOIN objects parent ON parent.file_id=o.file_id
          AND parent.object_index=o.parent_index
        WHERE o.type_hash=? GROUP BY c.id,parent.type_hash ORDER BY c.id,count(*) DESC
        """,
        (OCTREE_NODE,),
    ):
        print(corpus, class_names.get(parent_hash, "<root>"), count)

    print("\nphysical children")
    for corpus, child_hash, count in connection.execute(
        """
        SELECT c.corpus_key,child.type_hash,COUNT(*)
        FROM objects parent
        JOIN files f ON f.id=parent.file_id JOIN corpora c ON c.id=f.corpus_id
        JOIN objects child ON child.file_id=parent.file_id
          AND child.parent_index=parent.object_index
        WHERE parent.type_hash=? GROUP BY c.id,child.type_hash
        ORDER BY c.id,count(*) DESC
        """,
        (OCTREE_NODE,),
    ):
        print(corpus, class_names.get(child_hash, f"0x{child_hash:08X}"), count)

    print("\nsection/field profiles")
    for row in connection.execute(
        """
        SELECT c.corpus_key,d.section_index,d.field_type,d.is_section_terminator,
               COUNT(*),MIN(d.payload_size),MAX(d.payload_size),
               COUNT(DISTINCT d.payload_size)
        FROM direct_fields d
        JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
        JOIN files f ON f.id=o.file_id JOIN corpora c ON c.id=f.corpus_id
        WHERE o.type_hash=?
        GROUP BY c.id,d.section_index,d.field_type,d.is_section_terminator
        ORDER BY c.id,d.section_index,d.field_type,d.is_section_terminator
        """,
        (OCTREE_NODE,),
    ):
        print(row)

    shapes = connection.execute(
        """
        SELECT c.corpus_key,o.field_shape,COUNT(*)
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        WHERE o.type_hash=? GROUP BY c.id,o.field_shape
        ORDER BY c.id,COUNT(*) DESC,o.field_shape
        """,
        (OCTREE_NODE,),
    ).fetchall()
    print("\nraw shapes", len(shapes), "(inline child sizes create this diversity)")
    print("field 2 prefix samples")
    for payload_size, preview in connection.execute(
        """SELECT d.payload_size,d.payload_preview FROM direct_fields d
           JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
           WHERE o.type_hash=? AND d.section_index=0 AND d.field_type=2
           ORDER BY d.file_id,d.object_index,d.field_index LIMIT 8""",
        (OCTREE_NODE,),
    ):
        print(payload_size, bytes(preview[:16]).hex())

    octree_file_ids = [row[0] for row in connection.execute(
        "SELECT DISTINCT file_id FROM objects WHERE type_hash=?", (OCTREE_NODE,)
    )]
    placeholders = ",".join("?" for _ in octree_file_ids)
    all_objects: dict[tuple[int, int], list[tuple[int, int, int | None]]] = (
        collections.defaultdict(list)
    )
    octree_by_index: dict[tuple[int, int], tuple[int, int | None]] = {}
    for file_id, index, object_id, parent, type_hash in connection.execute(
        f"""SELECT file_id,object_index,object_id,parent_index,type_hash
            FROM objects WHERE file_id IN ({placeholders})""",
        octree_file_ids,
    ):
        all_objects[(file_id, object_id)].append((index, type_hash, parent))
        if type_hash == OCTREE_NODE:
            octree_by_index[(file_id, index)] = (object_id, parent)
    relationship_counts = collections.Counter()
    target_counts = collections.Counter()
    invalid_relationships = 0
    inline_physical = 0
    child_slots: dict[tuple[int, int, int], int] = {}
    for corpus, file_id, object_index, field_type, payload_size, preview in connection.execute(
        """
        SELECT c.corpus_key,d.file_id,d.object_index,d.field_type,d.payload_size,
               d.payload_preview
        FROM direct_fields d
        JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
        JOIN files f ON f.id=o.file_id JOIN corpora c ON c.id=f.corpus_id
        WHERE o.type_hash=? AND d.section_index=0 AND d.field_type IN (2,3,5,6)
          AND d.is_section_terminator=0 ORDER BY c.id,d.file_id,d.object_index,d.field_index
        """,
        (OCTREE_NODE,),
    ):
        data = bytes(preview)
        slot = None
        if field_type == 2:
            if payload_size <= 4 or len(data) <= 4:
                invalid_relationships += 1
                continue
            slot = struct.unpack_from("<I", data)[0]
            data = data[4:]
            payload_size -= 4
        if payload_size == 4 and len(data) >= 4:
            object_id = struct.unpack_from("<I", data)[0]
            encoding = "id_only"
        elif payload_size >= 8 and len(data) >= 8:
            object_id, inline_size = struct.unpack_from("<II", data)
            if inline_size != payload_size - 8:
                invalid_relationships += 1
                continue
            encoding = "sized_reference" if inline_size == 0 else "inline"
        else:
            invalid_relationships += 1
            continue
        candidates = all_objects.get((file_id, object_id), [])
        if field_type == 2 and slot is not None:
            child_slots[(file_id, object_index, object_id)] = slot
        target_hash = candidates[0][1] if len(candidates) == 1 else None
        if object_id == 0:
            target_hash = None
        relationship_counts[(corpus, field_type, encoding, slot)] += 1
        target_counts[(corpus, field_type, target_hash)] += 1
        if encoding == "inline" and len(candidates) == 1 and candidates[0][2] == object_index:
            inline_physical += 1
    print("\nrelationships invalid", invalid_relationships, "inline physical", inline_physical)
    for key, count in sorted(
        relationship_counts.items(),
        key=lambda item: tuple(-1 if value is None else value for value in item[0]),
    ):
        print("encoding", key, count)
    for (corpus, field_type, target_hash), count in sorted(
        target_counts.items(), key=lambda item: (item[0][0], item[0][1], item[0][2] or -1)
    ):
        print("target", corpus, field_type, class_names.get(target_hash, "<null>"), count)

    print("\ndebug colors")
    for corpus, color_blob, count in connection.execute(
        """
        SELECT c.corpus_key,d.payload_preview,COUNT(*) FROM direct_fields d
        JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
        JOIN files f ON f.id=o.file_id JOIN corpora c ON c.id=f.corpus_id
        WHERE o.type_hash=? AND d.section_index=0 AND d.field_type=1
        GROUP BY c.id,d.payload_preview ORDER BY c.id,d.payload_preview
        """,
        (OCTREE_NODE,),
    ):
        print(corpus, f"0x{struct.unpack_from('<I', bytes(color_blob))[0]:08X}", count)

    print("\nvector extrema and distinct counts")
    vector_rows = connection.execute(
        """
        SELECT c.corpus_key,d.file_id,d.object_index,d.section_index,
               d.field_type,d.payload_preview
        FROM direct_fields d
        JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
        JOIN files f ON f.id=o.file_id JOIN corpora c ON c.id=f.corpus_id
        WHERE o.type_hash=? AND d.payload_size=12 AND d.is_section_terminator=0
        ORDER BY c.id,d.section_index,d.field_type
        """,
        (OCTREE_NODE,),
    ).fetchall()
    grouped: dict[tuple[str, int, int], list[tuple[float, float, float]]] = (
        collections.defaultdict(list)
    )
    object_vectors: dict[tuple[str, int, int], list[tuple[float, float, float]]] = (
        collections.defaultdict(list)
    )
    for corpus, file_id, object_index, section, field_type, preview in vector_rows:
        vector = struct.unpack_from("<fff", bytes(preview))
        grouped[(corpus, section, field_type)].append(vector)
        if section == 1:
            object_vectors[(corpus, file_id, object_index)].append(vector)
    for key, items in grouped.items():
        minimum = tuple(min(value[axis] for value in items) for axis in range(3))
        maximum = tuple(max(value[axis] for value in items) for axis in range(3))
        print(key, "count", len(items), "distinct", len(set(items)), "min", minimum, "max", maximum)

    geometry = collections.Counter()
    maximum_midpoint_error = 0.0
    for (corpus, file_id, object_index), items in object_vectors.items():
        if len(items) != 3:
            geometry[(corpus, "wrong_field_count")] += 1
            continue
        pivot, mins, maxs = items
        ordered = all(mins[axis] <= pivot[axis] <= maxs[axis] for axis in range(3))
        positive = all(mins[axis] <= maxs[axis] for axis in range(3))
        midpoint_error = max(
            abs(pivot[axis] - (mins[axis] + maxs[axis]) * 0.5)
            for axis in range(3)
        )
        maximum_midpoint_error = max(maximum_midpoint_error, midpoint_error)
        geometry[(corpus, "ordered" if ordered else "pivot_outside")] += 1
        geometry[(corpus, "positive" if positive else "inverted")] += 1
        geometry[(corpus, "midpoint_exact" if midpoint_error <= 1e-5 else "midpoint_other")] += 1
    print("\ngeometry", dict(geometry), "maximum_midpoint_error", maximum_midpoint_error)

    octant_patterns: dict[int, set[str]] = collections.defaultdict(set)
    corpus_by_file = {file_id: corpus for corpus, file_id, _ in object_vectors}
    invalid_octant_axes = 0
    for (file_id, child_index), (child_id, parent_index) in octree_by_index.items():
        if parent_index is None or (file_id, parent_index) not in octree_by_index:
            continue
        corpus = corpus_by_file[file_id]
        parent_vectors = object_vectors[(corpus, file_id, parent_index)]
        child_vectors = object_vectors[(corpus, file_id, child_index)]
        parent_pivot, parent_mins, parent_maxs = parent_vectors
        _, child_mins, child_maxs = child_vectors
        slot = child_slots[(file_id, parent_index, child_id)]
        pattern = []
        for axis in range(3):
            if (child_mins[axis], child_maxs[axis]) == (
                parent_mins[axis], parent_pivot[axis]
            ):
                pattern.append("L")
            elif (child_mins[axis], child_maxs[axis]) == (
                parent_pivot[axis], parent_maxs[axis]
            ):
                pattern.append("H")
            else:
                pattern.append("?")
                invalid_octant_axes += 1
        octant_patterns[slot].add("".join(pattern))
    print(
        "octants",
        {slot: sorted(values) for slot, values in sorted(octant_patterns.items())},
        "invalid_axes",
        invalid_octant_axes,
    )

    objects = connection.execute(
        """
        SELECT c.corpus_key,f.id,f.relative_path,o.object_index
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id WHERE o.type_hash=?
        ORDER BY c.id,f.normalized_path,o.object_index
        """,
        (OCTREE_NODE,),
    ).fetchall()
    vectors: dict[tuple[str, str, int], tuple[tuple[float, float, float], ...]] = {}
    ordinals: collections.Counter[tuple[str, str]] = collections.Counter()
    for corpus, file_id, path, object_index in objects:
        values = object_vectors[(corpus, file_id, object_index)]
        normalized = canonical_path(path)
        key = (corpus, normalized)
        ordinal = ordinals[key]
        ordinals[key] += 1
        vectors[(corpus, normalized, ordinal)] = tuple(values)

    def compare(left_corpus: str, right_corpus: str):
        left = {key[1:]: value for key, value in vectors.items() if key[0] == left_corpus}
        right = {key[1:]: value for key, value in vectors.items() if key[0] == right_corpus}
        pairs = sorted(set(left).intersection(right))
        resources = {key[0] for key in pairs}
        equal = sum(left[key] == right[key] for key in pairs)
        return len(resources), len(pairs), equal, left, right

    pc = compare("pc-working", "pc-pristine")
    cross = compare("pc-pristine", "ps2-pristine")
    print("\ncross-corpus")
    print("PC", pc[:3])
    print("PC/PS2", cross[:3])
    for key in sorted(set(cross[3]).intersection(cross[4])):
        if cross[3][key] != cross[4][key]:
            print("mismatch", key, cross[3][key], cross[4][key])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
