#!/usr/bin/env python3
"""Reproducible structural inventory for spMeshData in corpus schema v2."""

from __future__ import annotations

import argparse
import collections
import math
import sqlite3
import struct
from dataclasses import dataclass
from pathlib import Path


MESH_DATA = 0x33C34CF0


@dataclass(frozen=True)
class Field:
    field_index: int
    field_type: int
    payload_size: int
    preview: bytes


@dataclass(frozen=True)
class Mesh:
    corpus: str
    platform: str
    path: str
    object_index: int
    ordinal: int
    fields: tuple[Field, ...]


def canonical_path(value: str) -> str:
    result = value.replace("\\", "/").lower()
    return result[5:] if result.startswith("data/") else result


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def f32(data: bytes, offset: int) -> float:
    return struct.unpack_from("<f", data, offset)[0]


def load_meshes(connection: sqlite3.Connection) -> list[Mesh]:
    rows = connection.execute(
        """
        SELECT c.corpus_key,p.platform_key,f.relative_path,o.object_index,
               d.field_index,d.field_type,d.payload_size,d.payload_preview
        FROM objects o
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        JOIN platforms p ON p.id=c.platform_id
        JOIN direct_fields d
          ON d.file_id=o.file_id AND d.object_index=o.object_index
        WHERE o.type_hash=?
        ORDER BY c.id,f.normalized_path,o.object_index,d.field_index
        """,
        (MESH_DATA,),
    )
    grouped: dict[tuple[str, str, str, int], list[Field]] = {}
    for corpus, platform, path, object_index, field_index, field_type, size, preview in rows:
        key = (corpus, platform, canonical_path(path), object_index)
        grouped.setdefault(key, []).append(
            Field(field_index, field_type, size, bytes(preview))
        )

    ordinals: collections.Counter[tuple[str, str]] = collections.Counter()
    result: list[Mesh] = []
    for (corpus, platform, path, object_index), fields in grouped.items():
        ordinal_key = (corpus, path)
        ordinal = ordinals[ordinal_key]
        ordinals[ordinal_key] += 1
        result.append(
            Mesh(corpus, platform, path, object_index, ordinal, tuple(fields))
        )
    return result


def nonterminal(mesh: Mesh) -> tuple[Field, ...]:
    return tuple(
        field
        for field in mesh.fields
        if not (field.field_type == 0 and field.payload_size == 0)
    )


def parse_pc_e1(field: Field) -> tuple[int, int, int, int, int, int] | None:
    data = field.preview
    if field.field_type != 1 or len(data) < 29 or data[16] != 0:
        return None
    vertex_format = u32(data, 0)
    vertex_count = u32(data, 4)
    runtime_bytes = u32(data, 8)
    index_bytes = u32(data, 12)
    primitive = u32(data, 17)
    stored_count = u32(data, 21)
    if u32(data, 25) != 0 or primitive not in (2, 3):
        return None
    expected_index_bytes = (
        stored_count * 3 * 2 if primitive == 2 else (stored_count + 2) * 2
    )
    if index_bytes != expected_index_bytes or field.payload_size < 41 + index_bytes:
        return None
    return vertex_format, vertex_count, runtime_bytes, index_bytes, primitive, stored_count


def parse_ps2_e1(field: Field) -> tuple[tuple[float, ...], int, int, int, int, int, int] | None:
    data = field.preview
    if field.field_type != 1 or field.payload_size < 40 or len(data) < 40:
        return None
    if (field.payload_size - 40) % 16:
        return None
    dma_qwords = (field.payload_size - 40) // 16
    if u32(data, 28) != dma_qwords:
        return None
    sphere = tuple(f32(data, offset) for offset in range(0, 16, 4))
    vertex_format = u32(data, 24)
    additional_uv = u32(data, 32)
    blend_weights = u32(data, 36)
    if (
        not all(math.isfinite(value) for value in sphere)
        or sphere[3] < 0
        or additional_uv not in (0, 1)
        or blend_weights not in (0, 4)
        or (additional_uv == 1) != bool(vertex_format & 0x1000)
        or (blend_weights == 4) != bool(vertex_format & 0x003E)
    ):
        return None
    return (
        sphere,
        u32(data, 16),
        u32(data, 20),
        vertex_format,
        dma_qwords,
        additional_uv,
        blend_weights,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    connection = sqlite3.connect(args.database)
    meshes = load_meshes(connection)

    shapes: collections.Counter[tuple[str, tuple[int, ...]]] = collections.Counter()
    for mesh in meshes:
        shapes[(mesh.corpus, tuple(field.field_type for field in nonterminal(mesh)))] += 1
    print("structural variants")
    for (corpus, shape), count in sorted(shapes.items()):
        print(f"{corpus:13} {str(shape):12} {count:6}")

    print("\nPC field-1 representation kinds")
    pc_metadata: dict[tuple[str, int], tuple[int, int, int, int, int, int]] = {}
    pc_counts: collections.Counter[tuple[str, str, int]] = collections.Counter()
    pc_native_counts: collections.Counter[tuple[str, int]] = collections.Counter()
    for mesh in meshes:
        if mesh.corpus != "pc-pristine":
            continue
        field = next((value for value in nonterminal(mesh) if value.field_type == 1), None)
        native = parse_ps2_e1(field) if field else None
        if native is not None:
            pc_native_counts[(mesh.path, native[3])] += 1
            continue
        parsed = parse_pc_e1(field) if field else None
        if parsed is None:
            continue
        pc_metadata[(mesh.path, mesh.ordinal)] = parsed
        fmt, vertices, runtime_bytes, index_bytes, primitive, stored_count = parsed
        pc_counts[("format", f"0x{fmt:04X}", primitive)] += 1
        runtime_stride = runtime_bytes // vertices if vertices and runtime_bytes % vertices == 0 else -1
        pc_counts[("runtime-stride", str(runtime_stride), primitive)] += 1
    for key, count in sorted(pc_counts.items()):
        print(f"{count:6} {key}")
    print(
        f"Direct3D={sum(pc_counts[key] for key in pc_counts if key[0] == 'format')}; "
        f"PS2-native={sum(pc_native_counts.values())} in "
        f"{len({path for path, _ in pc_native_counts})} resources"
    )
    for (path, vertex_format), count in sorted(pc_native_counts.items()):
        print(f"  {count:4} {path} format=0x{vertex_format:04X}")

    print("\nPS2 native header metadata")
    ps2_metadata: dict[tuple[str, int], tuple[tuple[float, ...], int, int, int, int, int, int]] = {}
    ps2_counts: collections.Counter[tuple[str, int]] = collections.Counter()
    ps2_flag_pairs: collections.Counter[tuple[int, int]] = collections.Counter()
    ps2_relations: collections.Counter[str] = collections.Counter()
    malformed: list[tuple[str, int, int]] = []
    for mesh in meshes:
        if mesh.corpus != "ps2-pristine":
            continue
        field = next((value for value in nonterminal(mesh) if value.field_type == 1), None)
        parsed = parse_ps2_e1(field) if field else None
        if parsed is None:
            malformed.append((mesh.path, mesh.object_index, field.payload_size if field else -1))
            continue
        ps2_metadata[(mesh.path, mesh.ordinal)] = parsed
        sphere, word4, word5, vertex_format, dma_qwords, word8, word9 = parsed
        ps2_counts[("format", vertex_format)] += 1
        ps2_counts[("word8", word8)] += 1
        ps2_counts[("word9", word9)] += 1
        ps2_flag_pairs[(word8, word9)] += 1
        ps2_relations["word4>=word5"] += word4 >= word5
        ps2_relations["word4<word5"] += word4 < word5
    for key, count in sorted(ps2_counts.items()):
        label, value = key
        rendered = f"0x{value:04X}" if label == "format" else str(value)
        print(f"{count:6} {label:7} {rendered}")
    print(f"malformed={len(malformed)}")
    for item in malformed[:20]:
        print("  ", item)
    print("flag pairs", sorted(ps2_flag_pairs.items()))
    print("count relations", dict(ps2_relations))

    missing_bounds = [
        mesh
        for mesh in meshes
        if mesh.corpus == "ps2-pristine"
        and not any(field.field_type == 2 for field in nonterminal(mesh))
    ]
    print(f"missing bounding boxes={len(missing_bounds)}")
    for mesh in missing_bounds:
        native = ps2_metadata[(mesh.path, mesh.ordinal)]
        print(
            f"  {mesh.path} [{mesh.object_index}] size={next(field.payload_size for field in nonterminal(mesh) if field.field_type == 1)} "
            f"format=0x{native[3]:04X} counts={native[1]}/{native[2]} flags={native[5]}/{native[6]}"
        )

    bounds_valid = 0
    sphere_contains_bounds = 0
    bounds_count = 0
    for mesh in meshes:
        if mesh.corpus != "ps2-pristine":
            continue
        bounds = next((field for field in nonterminal(mesh) if field.field_type == 2), None)
        native = ps2_metadata.get((mesh.path, mesh.ordinal))
        if bounds is None or native is None or len(bounds.preview) < 24:
            continue
        values = tuple(f32(bounds.preview, offset) for offset in range(0, 24, 4))
        bounds_count += 1
        if all(math.isfinite(value) for value in values) and all(
            values[index] <= values[index + 3] for index in range(3)
        ):
            bounds_valid += 1
        sphere = native[0]
        corners = (
            (x, y, z)
            for x in (values[0], values[3])
            for y in (values[1], values[4])
            for z in (values[2], values[5])
        )
        if all(
            math.dist((sphere[0], sphere[1], sphere[2]), corner)
            <= sphere[3] + max(1e-3, abs(sphere[3]) * 1e-4)
            for corner in corners
        ):
            sphere_contains_bounds += 1
    print(
        f"bounding boxes finite/ordered={bounds_valid}/{bounds_count}; "
        f"native sphere contains all box corners={sphere_contains_bounds}/{bounds_count}"
    )

    by_corpus_path: dict[tuple[str, str], list[Mesh]] = collections.defaultdict(list)
    for mesh in meshes:
        by_corpus_path[(mesh.corpus, mesh.path)].append(mesh)
    common = sorted(
        path
        for corpus, path in by_corpus_path
        if corpus == "pc-pristine" and ("ps2-pristine", path) in by_corpus_path
    )
    equal_count = [
        path
        for path in common
        if len(by_corpus_path[("pc-pristine", path)])
        == len(by_corpus_path[("ps2-pristine", path)])
    ]
    print("\nPC/PS2 metadata correlation")
    paired_meshes = sum(len(by_corpus_path[("pc-pristine", path)]) for path in equal_count)
    print(
        f"common resources={len(common)}, equal mesh counts={len(equal_count)}, "
        f"paired meshes={paired_meshes}"
    )
    correlations: collections.Counter[str] = collections.Counter()
    pairs = 0
    for path in equal_count:
        count = len(by_corpus_path[("pc-pristine", path)])
        for ordinal in range(count):
            pc = pc_metadata.get((path, ordinal))
            ps2 = ps2_metadata.get((path, ordinal))
            if pc is None or ps2 is None:
                continue
            pairs += 1
            _, pc_vertices, _, _, pc_primitive, pc_stored = pc
            _, word4, word5, _, _, word8, word9 = ps2
            correlations["word4=pc-stored"] += word4 == pc_stored
            correlations["word4=pc-vertices"] += word4 == pc_vertices
            correlations["word5=pc-stored"] += word5 == pc_stored
            correlations["word5=pc-vertices"] += word5 == pc_vertices
            correlations["word8=pc-primitive"] += word8 == pc_primitive
            correlations["word9=pc-stored"] += word9 == pc_stored
    print(f"comparable platform-specific pairs={pairs}")
    for key, count in correlations.items():
        print(f"{key:24} {count:6}/{pairs}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
