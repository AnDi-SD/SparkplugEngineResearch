#!/usr/bin/env python3
"""Reproducible structural inventory for spModel in corpus schema v2."""

from __future__ import annotations

import argparse
import collections
import itertools
import sqlite3
import struct
from dataclasses import dataclass
from pathlib import Path


MODEL = 0x763277DB


@dataclass(frozen=True)
class Field:
    section: int
    field_index: int
    field_type: int
    payload_size: int
    preview: bytes
    payload_sha256: str | None
    terminator: bool


@dataclass(frozen=True)
class ObjectInfo:
    index: int
    object_id: int
    name: str
    type_hash: int
    parent_index: int | None


@dataclass(frozen=True)
class Model:
    file_id: int
    corpus: str
    path: str
    entry: ObjectInfo
    fields: tuple[Field, ...]


def u32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def canonical_path(value: str) -> str:
    result = value.replace("\\", "/").lower()
    return result[5:] if result.startswith("data/") else result


def load(
    connection: sqlite3.Connection,
) -> tuple[
    list[Model],
    dict[tuple[int, int], tuple[ObjectInfo, ...]],
    dict[int, str],
]:
    class_names = {
        type_hash: engine_name or f"0x{type_hash:08X}"
        for type_hash, engine_name in connection.execute(
            "SELECT type_hash,engine_name FROM classes"
        )
    }
    objects_by_file_and_id: dict[
        tuple[int, int], list[ObjectInfo]
    ] = collections.defaultdict(list)
    objects_by_key: dict[tuple[int, int], ObjectInfo] = {}
    for row in connection.execute(
        """
        SELECT file_id,object_index,object_id,name,type_hash,parent_index
        FROM objects
        """
    ):
        file_id, index, object_id, name, type_hash, parent = row
        value = ObjectInfo(index, object_id, name, type_hash, parent)
        objects_by_key[(file_id, index)] = value
        objects_by_file_and_id[(file_id, object_id)].append(value)

    rows = connection.execute(
        """
        SELECT c.corpus_key,f.id,f.relative_path,o.object_index,
               d.section_index,d.field_index,d.field_type,d.payload_size,
               d.payload_preview,d.payload_sha256,d.is_section_terminator
        FROM objects o
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        JOIN direct_fields d
          ON d.file_id=o.file_id AND d.object_index=o.object_index
        WHERE o.type_hash=?
        ORDER BY c.id,f.normalized_path,o.object_index,d.field_index
        """,
        (MODEL,),
    )
    grouped: dict[tuple[str, int, str, int], list[Field]] = {}
    for row in rows:
        corpus, file_id, path, object_index = row[:4]
        field = Field(
            section=row[4],
            field_index=row[5],
            field_type=row[6],
            payload_size=row[7],
            preview=bytes(row[8]),
            payload_sha256=row[9],
            terminator=bool(row[10]),
        )
        grouped.setdefault((corpus, file_id, path, object_index), []).append(field)

    models = [
        Model(
            file_id,
            corpus,
            canonical_path(path),
            objects_by_key[(file_id, object_index)],
            tuple(fields),
        )
        for (corpus, file_id, path, object_index), fields in grouped.items()
    ]
    unique = {
        key: tuple(values) for key, values in objects_by_file_and_id.items()
    }
    return models, unique, class_names


def sections(model: Model) -> tuple[tuple[Field, ...], ...]:
    count = max(field.section for field in model.fields) + 1
    return tuple(
        tuple(field for field in model.fields if field.section == index)
        for index in range(count)
    )


def relationship(
    model: Model,
    field: Field | None,
    objects_by_id: dict[tuple[int, int], tuple[ObjectInfo, ...]],
    class_names: dict[int, str],
) -> tuple[str, str, int | None, int | None]:
    if field is None:
        return "absent", "<none>", None, None
    if field.payload_size < 4 or len(field.preview) < 4:
        return "invalid", "<invalid>", None, None
    object_id = u32(field.preview)
    targets = objects_by_id.get((model.file_id, object_id), ())
    target = targets[0] if len(targets) == 1 else None
    target_name = (
        class_names.get(target.type_hash, f"0x{target.type_hash:08X}")
        if target is not None
        else ("<null>" if object_id == 0 else "<unresolved>")
    )
    if field.payload_size == 4:
        storage = "null_id" if object_id == 0 else "id_only"
        return storage, target_name, object_id, None
    if field.payload_size < 8 or len(field.preview) < 8:
        return "invalid", target_name, object_id, None
    inline_size = u32(field.preview, 4)
    if inline_size == 0:
        storage = "null_sized" if object_id == 0 else "reference"
    elif field.payload_size == inline_size + 8:
        storage = "inline"
    else:
        storage = "invalid_size"
    return storage, target_name, object_id, inline_size


def semantic_fields(model: Model) -> dict[str, Field | None]:
    value = sections(model)
    if len(value) != 2:
        return {}
    renderable = [field for field in value[0] if not field.terminator]
    own = [field for field in value[1] if not field.terminator]
    return {
        "material": next((f for f in renderable if f.field_type == 0), None),
        "fog": next((f for f in renderable if f.field_type == 1), None),
        "alpha_sort": next((f for f in renderable if f.field_type == 2), None),
        "priority": next((f for f in renderable if f.field_type == 3), None),
        "base": next((f for f in own if f.field_type == 0), None),
        "projection_group": next((f for f in own if f.field_type == 1), None),
    }


def uint_value(field: Field | None) -> int | None:
    if field is None or field.payload_size != 4 or len(field.preview) < 4:
        return None
    return u32(field.preview)


def presence_shape(model: Model) -> tuple[tuple[int, ...], ...]:
    return tuple(
        tuple(field.field_type for field in section if not field.terminator)
        for section in sections(model)
    )


VARIANT_NAMES = {
    ((0, 1), (0,)): "model_legacy_compact",
    ((0, 1, 2, 3), (0,)): "model_no_projection",
    ((0, 1, 2, 3), (0, 1)): "model_full",
    ((0, 2, 3), (0,)): "model_no_fog_no_projection",
    ((0, 2, 3), (0, 1)): "model_no_fog",
    ((1, 2, 3), (0,)): "model_no_material_no_projection",
    ((1, 2, 3), (0, 1)): "model_no_material",
}


def target_object_name(
    model: Model,
    field: Field | None,
    objects_by_id: dict[tuple[int, int], tuple[ObjectInfo, ...]],
) -> str | None:
    if field is None or field.payload_size < 4 or len(field.preview) < 4:
        return None
    targets = objects_by_id.get((model.file_id, u32(field.preview)), ())
    return targets[0].name if len(targets) == 1 else None


def serialized_signature(model: Model) -> tuple[object, ...]:
    return (
        model.entry.name,
        tuple(
            (
                field.section,
                field.field_type,
                field.payload_size,
                field.payload_sha256,
                field.terminator,
            )
            for field in model.fields
        ),
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    connection = sqlite3.connect(args.database)
    models, objects_by_id, class_names = load(connection)

    print("profiles")
    for corpus, values in sorted(
        (key, tuple(group))
        for key, group in itertools.groupby(
            sorted(models, key=lambda item: item.corpus), key=lambda item: item.corpus
        )
    ):
        print(
            f"{corpus:13} objects={len(values):6} "
            f"resources={len({item.file_id for item in values}):3}"
        )

    shape_counts: collections.Counter[tuple[str, tuple[tuple[int, ...], ...]]] = (
        collections.Counter()
    )
    for model in models:
        shape = presence_shape(model)
        shape_counts[(model.corpus, shape)] += 1
    print("\nsection field-presence shapes")
    for key, count in sorted(shape_counts.items()):
        print(f"{key[0]:13} {str(key[1]):28} {count:6}")

    relationships: collections.Counter[tuple[str, str, str, str]] = (
        collections.Counter()
    )
    numeric: collections.Counter[tuple[str, str, int | None]] = (
        collections.Counter()
    )
    variants: collections.Counter[tuple[str, str]] = collections.Counter()
    malformed: list[tuple[str, str, int]] = []
    for model in models:
        fields = semantic_fields(model)
        if not fields:
            malformed.append((model.corpus, model.path, model.entry.index))
            continue
        relation_values = {}
        for semantic in ("material", "fog", "base"):
            decoded = relationship(
                model, fields[semantic], objects_by_id, class_names
            )
            relation_values[semantic] = decoded
            relationships[(model.corpus, semantic, decoded[0], decoded[1])] += 1
        number_values = {
            semantic: uint_value(fields[semantic])
            for semantic in ("alpha_sort", "priority", "projection_group")
        }
        for semantic, value in number_values.items():
            numeric[(model.corpus, semantic, value)] += 1
        variant = VARIANT_NAMES.get(presence_shape(model),"unknown")
        variants[(model.corpus,variant)] += 1

    print("\nrelationship encodings and targets")
    for key, count in sorted(relationships.items()):
        print(f"{key[0]:13} {key[1]:9} {key[2]:12} {key[3]:24} {count:6}")

    print("\nnumeric values")
    for key, count in sorted(
        numeric.items(), key=lambda item: (item[0][0], item[0][1], str(item[0][2]))
    ):
        print(f"{key[0]:13} {key[1]:16} {str(key[2]):>6} {count:6}")

    print("\nnormalized variants")
    for (corpus,variant),count in sorted(variants.items()):
        print(f"{corpus:13} {variant:34} {count:6}")
    print(f"malformed section sets={len(malformed)}")
    for item in malformed[:20]:
        print("  ", item)

    parent_counts: collections.Counter[tuple[str, str]] = collections.Counter()
    by_index = {
        (model.file_id, model.entry.index): model.entry for model in models
    }
    all_objects = {
        (file_id, target.index): target
        for (file_id, _), targets in objects_by_id.items()
        for target in targets
    }
    for model in models:
        parent = (
            all_objects.get((model.file_id, model.entry.parent_index))
            if model.entry.parent_index is not None
            else None
        )
        parent_counts[
            (
                model.corpus,
                class_names.get(parent.type_hash, f"0x{parent.type_hash:08X}")
                if parent is not None
                else "<root>",
            )
        ] += 1
    print("\nphysical parent classes")
    for key, count in sorted(parent_counts.items()):
        print(f"{key[0]:13} {key[1]:28} {count:6}")

    grouped: dict[str,dict[str,list[Model]]] = collections.defaultdict(
        lambda: collections.defaultdict(list)
    )
    for model in models:
        grouped[model.corpus][model.path].append(model)
    for paths in grouped.values():
        for values in paths.values():
            values.sort(key=lambda item: item.entry.index)

    working = grouped["pc-working"]
    pristine = grouped["pc-pristine"]
    common_pc = sorted(working.keys() & pristine.keys())
    equal_pc = sum(
        [serialized_signature(item) for item in working[path]] ==
        [serialized_signature(item) for item in pristine[path]]
        for path in common_pc
    )
    print("\nPC working/pristine serialized comparison")
    print(f"common resources={len(common_pc)} exactly equal model sets={equal_pc}")

    ps2 = grouped["ps2-pristine"]
    common_cross = sorted(pristine.keys() & ps2.keys())
    equal_count = [
        path for path in common_cross if len(pristine[path]) == len(ps2[path])
    ]
    paired = [
        (pc,console)
        for path in equal_count
        for pc,console in zip(pristine[path],ps2[path],strict=True)
    ]
    comparison_counts: collections.Counter[str] = collections.Counter()
    for pc,console in paired:
        pc_fields = semantic_fields(pc)
        ps2_fields = semantic_fields(console)
        checks = {
            "object_name": pc.entry.name == console.entry.name,
            "presence_shape": presence_shape(pc) == presence_shape(console),
            "alpha_sort": uint_value(pc_fields["alpha_sort"]) ==
                          uint_value(ps2_fields["alpha_sort"]),
            "priority": uint_value(pc_fields["priority"]) ==
                        uint_value(ps2_fields["priority"]),
            "projection_effective_zero":
                (uint_value(pc_fields["projection_group"]) or 0) ==
                (uint_value(ps2_fields["projection_group"]) or 0),
            "material_target_name": target_object_name(
                pc,pc_fields["material"],objects_by_id) == target_object_name(
                console,ps2_fields["material"],objects_by_id),
            "fog_target_name": target_object_name(
                pc,pc_fields["fog"],objects_by_id) == target_object_name(
                console,ps2_fields["fog"],objects_by_id),
            "base_mesh_target_name": target_object_name(
                pc,pc_fields["base"],objects_by_id) == target_object_name(
                console,ps2_fields["base"],objects_by_id),
        }
        for name,matched in checks.items():
            comparison_counts[name + ("_equal" if matched else "_different")] += 1
    print("\nPC/PS2 paired comparison")
    print(
        f"common resources={len(common_cross)} equal model-count resources="
        f"{len(equal_count)} paired models={len(paired)}"
    )
    for key,count in sorted(comparison_counts.items()):
        print(f"{key:40} {count:6}")

    print("\nlegacy compact examples")
    legacy = [
        model for model in models
        if model.corpus == "pc-pristine" and
        presence_shape(model) == ((0,1),(0,))
    ]
    for model in legacy:
        print(f"{model.path} [{model.entry.index}] {model.entry.name!r}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
