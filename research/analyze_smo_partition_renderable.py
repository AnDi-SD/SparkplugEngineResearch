#!/usr/bin/env python3
"""Read-only structural report for spPartitionRenderable in corpus schema v2."""

from __future__ import annotations

import argparse
import collections
import sqlite3
import struct
from pathlib import Path


PARTITION_RENDERABLE = 0x94BBCA2A


def canonical_path(path: str) -> str:
    value = path.replace("\\", "/").lower()
    return value[5:] if value.startswith("data/") else value


def argb(value: int) -> str:
    return (
        f"A={(value >> 24) & 0xFF},R={(value >> 16) & 0xFF},"
        f"G={(value >> 8) & 0xFF},B={value & 0xFF}"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    database = args.database.resolve()
    connection = sqlite3.connect(f"file:{database}?mode=ro", uri=True)

    print("profiles")
    for row in connection.execute(
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
        GROUP BY c.id ORDER BY c.corpus_key
        """,
        (PARTITION_RENDERABLE,),
    ):
        corpus, platform, objects, resources, physical, minimum, maximum = row
        print(
            f"{corpus:13} {platform:3} objects={objects:4} resources={resources:2} "
            f"physical={physical:4} serialized={minimum}..{maximum}"
        )

    class_names = {
        type_hash: name or f"0x{type_hash:08X}"
        for type_hash, name in connection.execute(
            "SELECT type_hash,engine_name FROM classes"
        )
    }
    all_objects: dict[tuple[int, int], list[tuple[int, int, int | None, str]]] = (
        collections.defaultdict(list)
    )
    for file_id, index, object_id, parent, type_hash, name in connection.execute(
        "SELECT file_id,object_index,object_id,parent_index,type_hash,name FROM objects"
    ):
        all_objects[(file_id, object_id)].append((index, type_hash, parent, name))

    object_rows = connection.execute(
        """
        SELECT c.corpus_key,f.id,f.relative_path,o.object_index,o.object_id,
               o.parent_index,parent.type_hash
        FROM objects o
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        LEFT JOIN objects parent
          ON parent.file_id=o.file_id AND parent.object_index=o.parent_index
        WHERE o.type_hash=?
        ORDER BY c.id,f.normalized_path,o.object_index
        """,
        (PARTITION_RENDERABLE,),
    ).fetchall()
    object_keys = {(row[1], row[3]) for row in object_rows}
    fields: dict[
        tuple[int, int], list[tuple[int, int, int, int, bytes]]
    ] = collections.defaultdict(list)
    for file_id, object_index, field_index, terminator, field_type, size, preview in (
        connection.execute(
            """
            SELECT d.file_id,d.object_index,d.field_index,d.is_section_terminator,
                   d.field_type,d.payload_size,d.payload_preview
            FROM direct_fields d
            JOIN objects o
              ON o.file_id=d.file_id AND o.object_index=d.object_index
            WHERE o.type_hash=?
            ORDER BY d.file_id,d.object_index,d.field_index
            """,
            (PARTITION_RENDERABLE,),
        )
    ):
        fields[(file_id, object_index)].append(
            (field_index, terminator, field_type, size, bytes(preview))
        )

    colors: collections.Counter[tuple[str, int]] = collections.Counter()
    renderable_counts: collections.Counter[tuple[str, int]] = collections.Counter()
    encodings: collections.Counter[tuple[str, str]] = collections.Counter()
    targets: collections.Counter[tuple[str, int | None]] = collections.Counter()
    parents: collections.Counter[tuple[str, int | None]] = collections.Counter()
    invalid_shapes = 0
    invalid_relationships = 0
    physical_child_matches = 0
    nonzero_ids = 0
    semantic_by_object: dict[
        tuple[str, str, int], tuple[int, tuple[tuple[str, str], ...]]
    ] = {}
    ordinals: collections.Counter[tuple[str, str]] = collections.Counter()

    for corpus, file_id, path, object_index, _, parent_index, parent_hash in object_rows:
        parents[(corpus, parent_hash)] += 1
        direct = fields[(file_id, object_index)]
        valid_shape = (
            len(direct) >= 3
            and direct[0][1] == 0
            and direct[0][2] == 1
            and direct[0][3] == 4
            and direct[-1][1] == 1
            and direct[-1][2] == 0
            and direct[-1][3] == 0
            and all(
                terminator == 0 and field_type == 0 and size > 0
                for _, terminator, field_type, size, _ in direct[1:-1]
            )
        )
        if not valid_shape:
            invalid_shapes += 1
            continue
        color = struct.unpack_from("<I", direct[0][4])[0]
        colors[(corpus, color)] += 1
        relationships: list[tuple[str, str]] = []
        renderable_counts[(corpus, len(direct) - 2)] += 1
        for _, _, _, payload_size, preview in direct[1:-1]:
            object_id = struct.unpack_from("<I", preview)[0]
            nonzero_ids += object_id != 0
            if payload_size == 4:
                encoding = "id_only"
            elif payload_size >= 8 and len(preview) >= 8:
                inline_size = struct.unpack_from("<I", preview, 4)[0]
                if inline_size != payload_size - 8:
                    invalid_relationships += 1
                    continue
                encoding = "sized_reference" if inline_size == 0 else "inline"
            else:
                invalid_relationships += 1
                continue
            candidates = all_objects.get((file_id, object_id), [])
            target_hash = candidates[0][1] if len(candidates) == 1 else None
            target_name = candidates[0][3].rstrip("\0") if len(candidates) == 1 else ""
            if (
                encoding == "inline"
                and len(candidates) == 1
                and candidates[0][2] == object_index
            ):
                physical_child_matches += 1
            encodings[(corpus, encoding)] += 1
            targets[(corpus, target_hash)] += 1
            relationships.append((encoding, target_name))
        normalized = canonical_path(path)
        ordinal_key = (corpus, normalized)
        ordinal = ordinals[ordinal_key]
        ordinals[ordinal_key] += 1
        semantic_by_object[(corpus, normalized, ordinal)] = (
            color,
            tuple(relationships),
        )

    print("\nshape validation")
    print(f"objects={len(object_keys)} invalid_shapes={invalid_shapes}")
    print(
        f"relationships={sum(encodings.values())} invalid_relationships="
        f"{invalid_relationships} nonzero_ids={nonzero_ids} "
        f"inline_physical_child_matches={physical_child_matches}"
    )

    print("\ndebug colors")
    for (corpus, color), count in sorted(colors.items()):
        print(f"{corpus:13} 0x{color:08X} {argb(color):27} objects={count:4}")

    print("\nrenderables per object")
    for corpus in sorted({row[0] for row in object_rows}):
        counts = {
            count: objects
            for (item_corpus, count), objects in renderable_counts.items()
            if item_corpus == corpus
        }
        total = sum(count * objects for count, objects in counts.items())
        print(
            f"{corpus:13} total={total:5} range={min(counts)}..{max(counts)} "
            f"distribution={dict(sorted(counts.items()))}"
        )

    print("\nrelationship encodings and targets")
    for (corpus, encoding), count in sorted(encodings.items()):
        print(f"{corpus:13} {encoding:16} {count:5}")
    for (corpus, target_hash), count in sorted(
        targets.items(), key=lambda item: (item[0][0], item[0][1] or -1)
    ):
        name = class_names.get(target_hash, "<unresolved>")
        print(f"{corpus:13} target={name:24} {count:5}")

    print("\nphysical parents")
    for (corpus, parent_hash), count in sorted(
        parents.items(), key=lambda item: (item[0][0], item[0][1] or -1)
    ):
        name = class_names.get(parent_hash, "<root>")
        print(f"{corpus:13} {name:24} {count:4}")

    print("\ncross-corpus")
    pc_working = {
        key[1:]: value
        for key, value in semantic_by_object.items()
        if key[0] == "pc-working"
    }
    pc_pristine = {
        key[1:]: value
        for key, value in semantic_by_object.items()
        if key[0] == "pc-pristine"
    }
    ps2 = {
        key[1:]: value
        for key, value in semantic_by_object.items()
        if key[0] == "ps2-pristine"
    }
    common_pc = pc_working.keys() & pc_pristine.keys()
    common_cross = pc_pristine.keys() & ps2.keys()
    print(
        f"PC paired={len(common_pc)} equal={sum(pc_working[key] == pc_pristine[key] for key in common_pc)}"
    )
    print(
        f"PC/PS2 paired={len(common_cross)} "
        f"equal_color={sum(pc_pristine[key][0] == ps2[key][0] for key in common_cross)} "
        f"equal_count={sum(len(pc_pristine[key][1]) == len(ps2[key][1]) for key in common_cross)} "
        f"equal_target_names={sum(pc_pristine[key][1] == ps2[key][1] for key in common_cross)}"
    )
    for key in sorted(common_cross):
        if pc_pristine[key] != ps2[key]:
            print(
                f"  mismatch {key[0]} ordinal={key[1]}: "
                f"pc={pc_pristine[key]} ps2={ps2[key]}"
            )

    print("\nrecorded evidence")
    for kind, observation in connection.execute(
        """
        SELECT evidence_kind,observation FROM evidence
        WHERE type_hash=? AND evidence_kind LIKE 'class_analysis:%'
        ORDER BY evidence_kind
        """,
        (PARTITION_RENDERABLE,),
    ):
        print(f"{kind}: {observation}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
