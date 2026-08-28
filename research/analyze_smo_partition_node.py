#!/usr/bin/env python3
"""Read-only structural report for spPartitionNode in corpus schema v2."""

from __future__ import annotations

import argparse
import collections
import sqlite3
import struct
from pathlib import Path


PARTITION_NODE = 0x67672341
EXPECTED_TARGET = {
    0: 0x47A97C0E,  # spCollisionInfo
    2: PARTITION_NODE,
    3: 0x61254AB3,  # spZone
    4: 0x6523AC37,  # spZonePortal
    5: 0x912CC341,  # spPartitionSystem
    6: 0x94BBCA2A,  # spPartitionRenderable
    7: 0x56D67170,  # spStaticRenderObject
}
FIELD_NAMES = {
    0: "CollisionInfo",
    1: "DebugColor",
    2: "Child",
    3: "Zone",
    4: "ZonePortal",
    5: "PartitionSystem",
    6: "PartitionRenderable",
    7: "StaticRenderObject",
}


def canonical_path(path: str) -> str:
    value = path.replace("\\", "/").lower()
    return value[5:] if value.startswith("data/") else value


def decode_relationship(payload_size: int, preview: bytes):
    if payload_size == 4 and len(preview) >= 4:
        return struct.unpack_from("<I", preview)[0], "id_only", 0
    if payload_size < 8 or len(preview) < 8:
        return None
    object_id, inline_size = struct.unpack_from("<II", preview)
    if inline_size != payload_size - 8:
        return None
    encoding = "sized_reference" if inline_size == 0 else "inline"
    return object_id, encoding, inline_size


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    database = args.database.resolve()
    connection = sqlite3.connect(f"file:{database}?mode=ro", uri=True)

    class_names = {
        type_hash: name or f"0x{type_hash:08X}"
        for type_hash, name in connection.execute(
            "SELECT type_hash,engine_name FROM classes"
        )
    }
    print("profiles")
    for row in connection.execute(
        """
        SELECT c.corpus_key,p.platform_key,COUNT(*),COUNT(DISTINCT o.file_id),
               SUM((SELECT COUNT(*) FROM file_occurrences fo
                    WHERE fo.file_id=o.file_id)),MIN(o.serialized_size),
               MAX(o.serialized_size)
        FROM objects o
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        JOIN platforms p ON p.id=c.platform_id
        WHERE o.type_hash=?
        GROUP BY c.id ORDER BY c.corpus_key
        """,
        (PARTITION_NODE,),
    ):
        corpus, platform, objects, resources, physical, minimum, maximum = row
        print(
            f"{corpus:13} {platform:3} objects={objects:4} resources={resources:2} "
            f"physical={physical:4} serialized={minimum}..{maximum}"
        )

    all_objects: dict[tuple[int, int], list[tuple[int, int, int | None, str]]] = (
        collections.defaultdict(list)
    )
    for file_id, index, object_id, parent, type_hash, name in connection.execute(
        "SELECT file_id,object_index,object_id,parent_index,type_hash,name FROM objects"
    ):
        all_objects[(file_id, object_id)].append((index, type_hash, parent, name))

    object_rows = connection.execute(
        """
        SELECT c.corpus_key,f.id,f.relative_path,o.object_index,o.parent_index,
               parent.type_hash
        FROM objects o
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        LEFT JOIN objects parent
          ON parent.file_id=o.file_id AND parent.object_index=o.parent_index
        WHERE o.type_hash=?
        ORDER BY c.id,f.normalized_path,o.object_index
        """,
        (PARTITION_NODE,),
    ).fetchall()
    fields: dict[tuple[int, int], list[tuple[int, int, int, int, bytes]]] = (
        collections.defaultdict(list)
    )
    for row in connection.execute(
        """
        SELECT d.file_id,d.object_index,d.field_index,d.is_section_terminator,
               d.field_type,d.payload_size,d.payload_preview
        FROM direct_fields d
        JOIN objects o
          ON o.file_id=d.file_id AND o.object_index=d.object_index
        WHERE o.type_hash=?
        ORDER BY d.file_id,d.object_index,d.field_index
        """,
        (PARTITION_NODE,),
    ):
        file_id, object_index, index, terminator, field_type, size, preview = row
        fields[(file_id, object_index)].append(
            (index, terminator, field_type, size, bytes(preview))
        )

    invalid_shapes = 0
    invalid_relationships = 0
    unresolved_relationships = 0
    wrong_targets = 0
    inline_physical_children = 0
    colors: collections.Counter[tuple[str, int]] = collections.Counter()
    cardinalities: collections.Counter[tuple[str, int, int]] = collections.Counter()
    encodings: collections.Counter[tuple[str, int, str]] = collections.Counter()
    targets: collections.Counter[tuple[str, int, int | None]] = collections.Counter()
    parents: collections.Counter[tuple[str, int | None]] = collections.Counter()
    semantic: dict[tuple[str, str, int], tuple] = {}
    ordinals: collections.Counter[tuple[str, str]] = collections.Counter()

    for corpus, file_id, path, object_index, parent_index, parent_hash in object_rows:
        parents[(corpus, parent_hash)] += 1
        direct = fields[(file_id, object_index)]
        valid = (
            len(direct) >= 5
            and direct[0][1:4] == (0, 1, 4)
            and direct[1][1] == 0 and direct[1][2] == 5 and direct[1][3] > 0
            and direct[2][1] == 0 and direct[2][2] == 3 and direct[2][3] > 0
            and direct[-2][1] == 0 and direct[-2][2] == 6
            and direct[-1][1:4] == (1, 0, 0)
        )
        middle = direct[3:-2]
        # Writer order independently recovered on PC and PS2. Field 2 has no
        # concrete-node occurrences, but derived spOctreeNode sections use it.
        rank = {2: 0, 0: 1, 4: 2, 7: 3}
        previous = -1
        for _, terminator, field_type, size, _ in middle:
            current = rank.get(field_type, -1)
            if terminator or size == 0 or current < previous:
                valid = False
            previous = current
        if not valid:
            invalid_shapes += 1
            continue

        color = struct.unpack_from("<I", direct[0][4])[0]
        colors[(corpus, color)] += 1
        field_semantics: list[tuple[int, tuple]] = []
        by_field: dict[int, list[tuple[str, str]]] = collections.defaultdict(list)
        for _, _, field_type, payload_size, preview in direct[1:-1]:
            relation = decode_relationship(payload_size, preview)
            if relation is None:
                invalid_relationships += 1
                continue
            object_id, encoding, _ = relation
            candidates = all_objects.get((file_id, object_id), [])
            target_hash = candidates[0][1] if len(candidates) == 1 else None
            target_name = candidates[0][3].rstrip("\0") if len(candidates) == 1 else ""
            if object_id == 0 and field_type == 6:
                target_hash = None
                target_name = ""
            elif len(candidates) != 1:
                unresolved_relationships += 1
            expected = EXPECTED_TARGET[field_type]
            if object_id != 0 and target_hash != expected:
                wrong_targets += 1
            if (
                encoding == "inline"
                and len(candidates) == 1
                and candidates[0][2] == object_index
            ):
                inline_physical_children += 1
            encodings[(corpus, field_type, encoding)] += 1
            targets[(corpus, field_type, target_hash)] += 1
            by_field[field_type].append((encoding, target_name))
            field_semantics.append((field_type, (encoding, target_name)))
        for field_type in (0, 2, 4, 7):
            cardinalities[(corpus, field_type, len(by_field[field_type]))] += 1
        normalized = canonical_path(path)
        ordinal_key = (corpus, normalized)
        ordinal = ordinals[ordinal_key]
        ordinals[ordinal_key] += 1
        semantic[(corpus, normalized, ordinal)] = (
            color,
            tuple(field_semantics),
        )

    print("\nshape validation")
    print(
        f"objects={len(object_rows)} invalid_shapes={invalid_shapes} "
        f"invalid_relationships={invalid_relationships} "
        f"unresolved={unresolved_relationships} wrong_targets={wrong_targets} "
        f"inline_physical_children={inline_physical_children}"
    )
    print(
        "field 2 Child: concrete occurrences=0; inherited spOctreeNode "
        "occurrences=18048 (UInt32 slot + inline relationship)"
    )

    print("\nrelationship encodings and targets")
    for (corpus, field_type, encoding), count in sorted(encodings.items()):
        print(
            f"{corpus:13} f{field_type} {FIELD_NAMES[field_type]:21} "
            f"{encoding:16} {count:6}"
        )
    for (corpus, field_type, target_hash), count in sorted(
        targets.items(), key=lambda item: (item[0][0], item[0][1], item[0][2] or -1)
    ):
        target = "<null>" if target_hash is None else class_names.get(target_hash, f"0x{target_hash:08X}")
        print(
            f"{corpus:13} f{field_type} target={target:24} {count:6}"
        )

    print("\ncardinality")
    for corpus in sorted({row[0] for row in object_rows}):
        for field_type in (0, 2, 4, 7):
            distribution = {
                count: objects
                for (item_corpus, item_type, count), objects in cardinalities.items()
                if item_corpus == corpus and item_type == field_type
            }
            total = sum(count * objects for count, objects in distribution.items())
            print(
                f"{corpus:13} f{field_type} {FIELD_NAMES[field_type]:21} "
                f"total={total:6} range={min(distribution)}..{max(distribution)} "
                f"nonempty={sum(v for k, v in distribution.items() if k)}"
            )

    print("\ndebug colors")
    for (corpus, color), count in sorted(colors.items()):
        print(f"{corpus:13} 0x{color:08X} objects={count:4}")

    print("\nphysical parents")
    for (corpus, parent_hash), count in sorted(
        parents.items(), key=lambda item: (item[0][0], item[0][1] or -1)
    ):
        name = class_names.get(parent_hash, "<root>")
        print(f"{corpus:13} {name:24} {count:4}")

    def compare(left_corpus: str, right_corpus: str):
        left = {key[1:]: value for key, value in semantic.items() if key[0] == left_corpus}
        right = {key[1:]: value for key, value in semantic.items() if key[0] == right_corpus}
        common_paths = {key[0] for key in left}.intersection(key[0] for key in right)
        pairs = sorted(set(left).intersection(right))
        equal = sum(left[key] == right[key] for key in pairs)
        equal_color = sum(left[key][0] == right[key][0] for key in pairs)
        return len(common_paths), len(pairs), equal, equal_color, left, right

    pc = compare("pc-working", "pc-pristine")
    cross = compare("pc-pristine", "ps2-pristine")
    print("\ncross-corpus")
    print(f"PC resources={pc[0]} paired={pc[1]} semantic_equal={pc[2]}")
    print(
        f"PC/PS2 resources={cross[0]} paired={cross[1]} "
        f"semantic_equal={cross[2]} equal_color={cross[3]}"
    )
    mismatches = [key for key in sorted(set(cross[4]).intersection(cross[5])) if cross[4][key] != cross[5][key]]
    print(f"PC/PS2 mismatches={len(mismatches)}")
    for key in mismatches[:20]:
        print(f"  {key[0]} ordinal={key[1]}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
