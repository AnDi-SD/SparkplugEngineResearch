#!/usr/bin/env python3
"""Read-only structural inventory for spMaterialData across corpus databases."""

from __future__ import annotations

import argparse
import collections
import sqlite3
import struct
from dataclasses import dataclass
from pathlib import Path


MATERIAL_DATA_HASH = 0x6160348B
STANDARD_LAYER_HASH = 0x234C576B
TEXTURE_DATA_HASH = 0x78EA082B
ANIM_TEXTURE_CONTROLLER_HASH = 0x16FB0E47
UV_CONTROLLER_HASH = 0x1C0053D6
MATERIAL_COLOR_CONTROLLER_HASH = 0x4C633E85


@dataclass(frozen=True)
class Field:
    type: int
    occurrence: int
    size: int
    payload: bytes


@dataclass(frozen=True)
class Relationship:
    object_id: int
    storage: str
    inline_size: int | None
    inline_type: int | None


@dataclass(frozen=True)
class MaterialPass:
    final_blend: int
    layer_class: int
    texture_states_field: int
    texture_states: tuple[int, ...]
    static_uv: tuple[int, tuple[float, ...]] | None
    texture: Relationship | None
    animation: Relationship | None
    uv_controller: Relationship | None


@dataclass(frozen=True)
class Material:
    render_states: tuple[int, ...]
    vertex_alpha: int
    passes: tuple[MaterialPass, ...]
    colors: tuple[int, int, int, int]
    specular_power_bits: int
    color_controller: Relationship

    def normalized(self) -> tuple:
        """Own material state without platform-specific inline child bytes."""
        return (
            self.render_states,
            self.vertex_alpha,
            tuple(
                (
                    item.final_blend,
                    item.layer_class,
                    item.texture_states,
                    item.static_uv,
                    normalize_relationship(item.texture),
                    normalize_relationship(item.animation),
                    normalize_relationship(item.uv_controller),
                )
                for item in self.passes
            ),
            self.colors,
            self.specular_power_bits,
            normalize_relationship(self.color_controller),
        )

    def differences(self, other: Material) -> tuple[str, ...]:
        result: list[str] = []
        if self.render_states != other.render_states:
            result.append("render-states")
        if self.vertex_alpha != other.vertex_alpha:
            result.append("vertex-alpha")
        if len(self.passes) != len(other.passes):
            result.append("pass-count")
        for left, right in zip(self.passes, other.passes):
            if left.final_blend != right.final_blend:
                result.append("final-blend")
            if left.layer_class != right.layer_class:
                result.append("layer-class")
            if left.texture_states != right.texture_states:
                result.append("texture-states")
            if left.static_uv != right.static_uv:
                result.append("static-uv")
            if normalize_relationship(left.texture) != normalize_relationship(right.texture):
                result.append("texture-relationship")
            if normalize_relationship(left.animation) != normalize_relationship(right.animation):
                result.append("animation-relationship")
            if normalize_relationship(left.uv_controller) != normalize_relationship(
                right.uv_controller
            ):
                result.append("uv-controller-relationship")
        if self.colors != other.colors:
            result.append("material-colors")
        if self.specular_power_bits != other.specular_power_bits:
            result.append("specular-power")
        if normalize_relationship(self.color_controller) != normalize_relationship(
            other.color_controller
        ):
            result.append("color-controller-relationship")
        return tuple(sorted(set(result)))


def normalize_relationship(value: Relationship | None) -> tuple | None:
    if value is None:
        return None
    if value.object_id == 0:
        return "null",
    # Object IDs are file-local and can shift when a PC-only inline controller
    # is absent from the PS2 graph. Presence and target class are the stable
    # semantic properties for a cross-platform comparison.
    return "present", value.inline_type


def decode_relationship(payload: bytes, payload_size: int) -> Relationship:
    if payload_size == 4:
        object_id = struct.unpack_from("<I", payload)[0]
        storage = "null-id" if object_id == 0 else "legacy-id-only"
        return Relationship(object_id, storage, None, None)
    if payload_size < 8 or len(payload) < 8:
        raise ValueError(f"short relationship payload: {payload_size}")
    object_id, inline_size = struct.unpack_from("<II", payload)
    if inline_size == 0:
        if payload_size != 8:
            raise ValueError("reference relationship has trailing bytes")
        return Relationship(object_id, "reference", 0, None)
    if payload_size != 8 + inline_size:
        raise ValueError(
            f"inline relationship size mismatch: {inline_size} != {payload_size - 8}"
        )
    if inline_size < 8:
        raise ValueError("inline SBOO is too short")
    if len(payload) < 16:
        raise ValueError("inline SBOO preview is too short")
    inline_type = struct.unpack_from("<I", payload, 8)[0]
    if payload[12:16] != b"SBOO":
        raise ValueError("inline relationship lacks SBOO signature")
    return Relationship(object_id, "inline", inline_size, inline_type)


def decode_material(fields: list[Field]) -> Material:
    allowed = {0, 1, 2, 3, 4, 6, 8, 9, 10, 11, 12, 17}
    unknown = sorted({field.type for field in fields} - allowed)
    if unknown:
        raise ValueError(f"unknown direct material fields: {unknown}")

    render_fields = [field for field in fields if field.type == 0]
    color_fields = [field for field in fields if field.type == 2]
    controller_fields = [field for field in fields if field.type == 6]
    alpha_fields = [field for field in fields if field.type == 1]
    if len(render_fields) != 1 or render_fields[0].size != 44:
        raise ValueError("material requires one 44-byte render-state field")
    if len(color_fields) != 1 or color_fields[0].size != 20:
        raise ValueError("material requires one 20-byte color field")
    if len(controller_fields) != 1:
        raise ValueError("material requires one color-controller relationship")
    if len(alpha_fields) > 1 or any(field.payload != b"\x01" for field in alpha_fields):
        raise ValueError("vertex-alpha field is not an optional true byte")

    render_states = struct.unpack("<11I", render_fields[0].payload)
    ambient, diffuse, specular, emissive, power_bits = struct.unpack(
        "<5I", color_fields[0].payload
    )

    passes: list[MaterialPass] = []
    index = 0
    while index < len(fields):
        if fields[index].type != 3:
            index += 1
            continue
        pass_fields = [fields[index]]
        index += 1
        while index < len(fields) and fields[index].type not in (2, 3, 6):
            pass_fields.append(fields[index])
            index += 1
        by_type: dict[int, list[Field]] = collections.defaultdict(list)
        for field in pass_fields:
            by_type[field.type].append(field)
        if len(by_type[3]) != 1 or by_type[3][0].size != 4:
            raise ValueError("pass requires one UInt32 FinalBlendOp")
        if len(by_type[4]) != 1 or by_type[4][0].size != 4:
            raise ValueError("pass requires one UInt32 layer class")
        state_fields = by_type[8] + by_type[17]
        if len(state_fields) != 1 or state_fields[0].size != 36:
            raise ValueError("pass requires one 9-DWORD texture-state block")
        for relationship_type in (10, 11, 12):
            if len(by_type[relationship_type]) > 1:
                raise ValueError(f"duplicate pass relationship field {relationship_type}")
        if len(by_type[9]) > 1:
            raise ValueError("duplicate static UV transform")
        static_uv = None
        if by_type[9]:
            payload = by_type[9][0].payload
            if by_type[9][0].size != 40:
                raise ValueError("static UV transform must contain flag + Matrix3x3")
            enabled = struct.unpack_from("<I", payload)[0]
            matrix = struct.unpack_from("<9f", payload, 4)
            static_uv = enabled, matrix
        passes.append(
            MaterialPass(
                struct.unpack("<I", by_type[3][0].payload)[0],
                struct.unpack("<I", by_type[4][0].payload)[0],
                state_fields[0].type,
                struct.unpack("<9I", state_fields[0].payload),
                static_uv,
                decode_relationship(by_type[10][0].payload, by_type[10][0].size)
                if by_type[10] else None,
                decode_relationship(by_type[11][0].payload, by_type[11][0].size)
                if by_type[11] else None,
                decode_relationship(by_type[12][0].payload, by_type[12][0].size)
                if by_type[12] else None,
            )
        )
    if not passes:
        raise ValueError("material has no passes")
    return Material(
        render_states,
        1 if alpha_fields else 0,
        tuple(passes),
        (ambient, diffuse, specular, emissive),
        power_bits,
        decode_relationship(controller_fields[0].payload, controller_fields[0].size),
    )


def material_variant(material: Material) -> str:
    generations = {item.texture_states_field for item in material.passes}
    generation = (
        "legacy" if generations == {8}
        else "current" if generations == {17}
        else "mixed"
    )
    pass_kind = "single" if len(material.passes) == 1 else "multi"
    return f"{generation}-{pass_kind}-pass"


def describe_relationship(value: Relationship | None) -> tuple:
    if value is None:
        return "absent",
    return value.storage, value.inline_type


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    parser.add_argument("--corpus")
    parser.add_argument("--examples", type=int, default=3)
    parser.add_argument("--summary", action="store_true")
    args = parser.parse_args()

    database = args.database.resolve()
    connection = sqlite3.connect(f"file:{database}?mode=ro", uri=True)
    connection.row_factory = sqlite3.Row
    file_metadata = {
        row["file_id"]: row
        for row in connection.execute(
            """
            SELECT f.id AS file_id,co.corpus_key,pl.platform_key,f.relative_path
            FROM files f
            JOIN corpora co ON co.id=f.corpus_id
            JOIN platforms pl ON pl.id=co.platform_id;
            """
        )
    }
    object_metadata = {
        (row["file_id"], row["object_index"]): row
        for row in connection.execute(
            """
            SELECT o.file_id,o.object_index,o.object_id,
                   parent.type_hash AS parent_type_hash
            FROM objects o
            LEFT JOIN objects parent ON parent.file_id=o.file_id
                AND parent.object_index=o.parent_index
            WHERE o.type_hash=?;
            """,
            (MATERIAL_DATA_HASH,),
        )
    }
    query = """
        SELECT d.file_id,d.object_index,d.field_index,d.field_type,d.occurrence,
               d.payload_size,d.payload_preview
        FROM direct_fields d
        JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
        WHERE o.type_hash=? AND d.is_section_terminator=0
        ORDER BY d.file_id,d.object_index,d.field_index;
    """
    counts: collections.Counter[tuple] = collections.Counter()
    examples: dict[tuple, list[str]] = collections.defaultdict(list)
    materials: dict[tuple[int, int], Material] = {}
    current_key: tuple[int, int] | None = None
    current_rows: list[sqlite3.Row] = []

    def add(key: tuple, example: str) -> None:
        counts[key] += 1
        if len(examples[key]) < args.examples:
            examples[key].append(example)

    def finish(rows: list[sqlite3.Row]) -> None:
        if not rows:
            return
        fields = [
            Field(
                row["field_type"],
                row["occurrence"],
                row["payload_size"],
                bytes(row["payload_preview"]),
            )
            for row in rows
        ]
        material = decode_material(fields)
        row = rows[0]
        object_key = row["file_id"], row["object_index"]
        file_meta = file_metadata[row["file_id"]]
        object_meta = object_metadata[object_key]
        materials[object_key] = material
        corpus = file_meta["corpus_key"]
        example = f"{file_meta['relative_path']} [{row['object_index']}]"
        add(("objects", corpus), example)
        add(("variant", corpus, material_variant(material)), example)
        add(("pass-count", corpus, len(material.passes)), example)
        add(("vertex-alpha", corpus, material.vertex_alpha), example)
        add(("render-states", corpus, material.render_states), example)
        add(("material-colors", corpus, material.colors, material.specular_power_bits), example)
        add(("color-controller", corpus, *describe_relationship(material.color_controller)), example)
        add(("parent", corpus, object_meta["parent_type_hash"]), example)
        for item in material.passes:
            add(("passes", corpus), example)
            add(("final-blend", corpus, item.final_blend), example)
            add(("layer-class", corpus, item.layer_class), example)
            add(("texture-state-field", corpus, item.texture_states_field), example)
            add(("texture-states", corpus, item.texture_states), example)
            add(("static-uv", corpus, "present" if item.static_uv else "absent"), example)
            add(("texture", corpus, *describe_relationship(item.texture)), example)
            add(("animation", corpus, *describe_relationship(item.animation)), example)
            add(("uv-controller", corpus, *describe_relationship(item.uv_controller)), example)

    try:
        for row in connection.execute(query, (MATERIAL_DATA_HASH,)):
            if args.corpus is not None and (
                file_metadata[row["file_id"]]["corpus_key"] != args.corpus
            ):
                continue
            key = row["file_id"], row["object_index"]
            if current_key is not None and key != current_key:
                finish(current_rows)
                current_rows = []
            current_key = key
            current_rows.append(row)
        finish(current_rows)

        if args.corpus is None:
            material_indices: dict[int, list[int]] = collections.defaultdict(list)
            for file_id, object_index in materials:
                material_indices[file_id].append(object_index)
            for indices in material_indices.values():
                indices.sort()
            pc_files = {
                row["relative_path"].replace("\\", "/").lower(): file_id
                for file_id, row in file_metadata.items()
                if row["corpus_key"] == "pc-pristine" and file_id in material_indices
            }
            ps2_files = {
                row["relative_path"].replace("\\", "/").lower()
                    .removeprefix("data/"): file_id
                for file_id, row in file_metadata.items()
                if row["corpus_key"] == "ps2-pristine" and file_id in material_indices
            }
            common_paths = sorted(pc_files.keys() & ps2_files.keys())
            counts[("pc-ps2-resource-pairs", "common-path")] = len(common_paths)
            for path in common_paths:
                pc_file = pc_files[path]
                ps2_file = ps2_files[path]
                pc_indices = material_indices[pc_file]
                ps2_indices = material_indices[ps2_file]
                if len(pc_indices) != len(ps2_indices):
                    counts[("pc-ps2-resource-pairs", "material-count-different")] += 1
                    continue
                counts[("pc-ps2-resource-pairs", "material-count-equal")] += 1
                for pc_index, ps2_index in zip(pc_indices, ps2_indices):
                    pc = materials[(pc_file, pc_index)]
                    ps2 = materials[(ps2_file, ps2_index)]
                    result = (
                        "equal" if pc.normalized() == ps2.normalized() else "different"
                    )
                    counts[("pc-ps2-normalized-pairs", result)] += 1
                    if result == "different":
                        for difference in pc.differences(ps2):
                            counts[("pc-ps2-difference", difference)] += 1
    finally:
        connection.close()

    if args.summary:
        for category in ("render-states", "material-colors", "texture-states"):
            corpora = {key[1] for key in counts if key[0] == category}
            for corpus in corpora:
                counts[("distinct-values", corpus, category)] = sum(
                    1 for key in counts if key[0] == category and key[1] == corpus
                )
        visible = {
            "animation",
            "color-controller",
            "distinct-values",
            "final-blend",
            "layer-class",
            "objects",
            "parent",
            "pass-count",
            "passes",
            "pc-ps2-normalized-pairs",
            "pc-ps2-difference",
            "pc-ps2-resource-pairs",
            "static-uv",
            "texture",
            "texture-state-field",
            "uv-controller",
            "variant",
            "vertex-alpha",
        }
    else:
        visible = {key[0] for key in counts}
    for key, count in sorted(counts.items(), key=lambda item: repr(item[0])):
        if key[0] not in visible:
            continue
        print(f"{count:6d} {key}")
        for example in examples[key]:
            print(f"       {example}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
