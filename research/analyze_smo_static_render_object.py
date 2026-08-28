#!/usr/bin/env python3
"""Reproducible PC/PS2 inventory for spStaticRenderObject in corpus schema v2."""

from __future__ import annotations

import argparse
import collections
import hashlib
import math
import sqlite3
import struct
from dataclasses import dataclass
from pathlib import Path


STATIC_RENDER_OBJECT = 0x56D67170
MODEL = 0x763277DB


@dataclass(frozen=True)
class Location:
    file_id: int
    corpus: str
    source_kind: str
    source_root: str
    relative_path: str
    container_path: str | None
    occurrence_offset: int | None
    byte_size: int


@dataclass(frozen=True)
class Target:
    index: int
    object_id: int
    type_hash: int
    name: str
    serialized_size: int
    parent_index: int | None


@dataclass(frozen=True)
class Field:
    field_index: int
    field_type: int
    payload_size: int
    absolute_payload_offset: int
    preview: bytes
    payload_sha256: str | None
    terminator: bool


@dataclass(frozen=True)
class Observation:
    file_id: int
    corpus: str
    path: str
    ordinal: int
    entry: Target
    parent_type: int | None
    fields: tuple[Field, ...]
    forward: tuple[float, ...]
    inverse: tuple[float, ...]
    renderable: Target


def canonical_path(value: str) -> str:
    result = value.replace("\\", "/").lower()
    return result[5:] if result.startswith("data/") else result


def u32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def matrix_multiply(
    left: tuple[float, ...], right: tuple[float, ...]
) -> tuple[float, ...]:
    return tuple(
        sum(left[row * 4 + k] * right[k * 4 + column] for k in range(4))
        for row in range(4)
        for column in range(4)
    )


def identity_error(value: tuple[float, ...]) -> float:
    return max(
        abs(cell - (1.0 if index // 4 == index % 4 else 0.0))
        for index, cell in enumerate(value)
    )


def determinant3(value: tuple[float, ...]) -> float:
    a, b, c = value[0], value[1], value[2]
    d, e, f = value[4], value[5], value[6]
    g, h, i = value[8], value[9], value[10]
    return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)


def is_affine(value: tuple[float, ...], tolerance: float = 1e-6) -> bool:
    return (
        abs(value[3]) <= tolerance
        and abs(value[7]) <= tolerance
        and abs(value[11]) <= tolerance
        and abs(value[15] - 1.0) <= tolerance
    )


def matrix_equal(
    left: tuple[float, ...], right: tuple[float, ...], tolerance: float = 1e-5
) -> bool:
    return max(abs(a - b) for a, b in zip(left, right, strict=True)) <= tolerance


def engine_world_inverse(value: tuple[float, ...]) -> tuple[float, ...]:
    """Sparkplug's stored rigid/orthogonal inverse, including authored scale."""
    transposed = (
        value[0], value[4], value[8], 0.0,
        value[1], value[5], value[9], 0.0,
        value[2], value[6], value[10], 0.0,
    )
    tx, ty, tz = value[12], value[13], value[14]
    inverse_translation = tuple(
        -(tx * transposed[column] +
          ty * transposed[4 + column] +
          tz * transposed[8 + column])
        for column in range(3)
    )
    return transposed + inverse_translation + (1.0,)


def axis_lengths(value: tuple[float, ...]) -> tuple[float, float, float]:
    return tuple(
        math.sqrt(sum(value[row * 4 + column] ** 2 for column in range(3)))
        for row in range(3)
    )


def orthogonality_error(value: tuple[float, ...]) -> float:
    rows = [value[0:3], value[4:7], value[8:11]]
    return max(
        abs(sum(a * b for a, b in zip(rows[left], rows[right], strict=True)))
        for left, right in ((0, 1), (0, 2), (1, 2))
    )


def read_resource(location: Location) -> bytes:
    root = Path(location.source_root)
    if location.source_kind == "directory":
        return (root / Path(location.relative_path.replace("/", "\\"))).read_bytes()
    if location.container_path is None or location.occurrence_offset is None:
        raise ValueError(f"incomplete PCK location for {location.relative_path}")
    archive = root / Path(location.container_path.replace("/", "\\"))
    with archive.open("rb") as stream:
        stream.seek(location.occurrence_offset)
        # The caller only slices bounded payload offsets; reading the SMO entry
        # once keeps this script independent from PCK extraction.
        return stream.read(location.byte_size)


def load(connection: sqlite3.Connection) -> list[Observation]:
    locations: dict[int, Location] = {}
    rows = connection.execute(
        """
        SELECT DISTINCT f.id,c.corpus_key,c.source_kind,c.source_root,
               f.relative_path,ct.relative_path,fo.byte_offset,f.byte_size
        FROM files f
        JOIN corpora c ON c.id=f.corpus_id
        JOIN objects o ON o.file_id=f.id AND o.type_hash=?
        LEFT JOIN file_occurrences fo ON fo.id=(
            SELECT MIN(inner_fo.id) FROM file_occurrences inner_fo
            WHERE inner_fo.file_id=f.id)
        LEFT JOIN containers ct ON ct.id=fo.container_id
        """,
        (STATIC_RENDER_OBJECT,),
    )
    for row in rows:
        locations[row[0]] = Location(*row)

    objects: dict[tuple[int, int], Target] = {}
    by_id: dict[tuple[int, int], list[Target]] = collections.defaultdict(list)
    for row in connection.execute(
        """
        SELECT file_id,object_index,object_id,type_hash,name,serialized_size,
               parent_index
        FROM objects
        """
    ):
        target = Target(row[1], row[2], row[3], row[4], row[5], row[6])
        objects[(row[0], row[1])] = target
        by_id[(row[0], row[2])].append(target)

    fields_by_object: dict[tuple[int, int], list[Field]] = collections.defaultdict(list)
    for row in connection.execute(
        """
        SELECT d.file_id,d.object_index,d.field_index,d.field_type,d.payload_size,
               d.absolute_payload_offset,d.payload_preview,d.payload_sha256,
               d.is_section_terminator
        FROM direct_fields d
        JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
        WHERE o.type_hash=?
        ORDER BY d.file_id,d.object_index,d.field_index
        """,
        (STATIC_RENDER_OBJECT,),
    ):
        fields_by_object[(row[0], row[1])].append(
            Field(row[2], row[3], row[4], row[5], bytes(row[6]), row[7], bool(row[8]))
        )

    parent_types = {
        (file_id, index): objects[(file_id, target.parent_index)].type_hash
        if target.parent_index is not None
        else None
        for (file_id, index), target in objects.items()
        if target.type_hash == STATIC_RENDER_OBJECT
    }
    observations: list[Observation] = []
    entries_by_file: dict[int, list[Target]] = collections.defaultdict(list)
    for (file_id, _), target in objects.items():
        if target.type_hash == STATIC_RENDER_OBJECT:
            entries_by_file[file_id].append(target)

    for file_id, entries in entries_by_file.items():
        location = locations[file_id]
        resource = read_resource(location)
        base = location.occurrence_offset or 0
        if location.source_kind != "directory":
            # read_resource starts at the SMO's PCK byte offset.
            base = 0
        for ordinal, entry in enumerate(sorted(entries, key=lambda item: item.index)):
            fields = tuple(fields_by_object[(file_id, entry.index)])
            shape = tuple(
                field.field_type for field in fields if not field.terminator
            )
            if shape != (1, 2, 0) or len(fields) != 4 or not fields[-1].terminator:
                raise ValueError(
                    f"unexpected field shape {shape} in {location.relative_path} "
                    f"[{entry.index}]"
                )
            matrices = []
            for field in fields[:2]:
                start = base + field.absolute_payload_offset
                payload = resource[start : start + field.payload_size]
                if len(payload) != 64:
                    raise ValueError("truncated matrix payload")
                matrices.append(struct.unpack("<16f", payload))
            relationship = fields[2]
            if relationship.payload_size < 8 or len(relationship.preview) < 12:
                raise ValueError("truncated renderable relationship prefix")
            object_id = u32(relationship.preview)
            inline_size = u32(relationship.preview, 4)
            candidates = by_id[(file_id, object_id)]
            if len(candidates) != 1:
                raise ValueError("renderable relationship does not resolve uniquely")
            renderable = candidates[0]
            if (
                inline_size == 0
                or relationship.payload_size != inline_size + 8
                or renderable.serialized_size != inline_size
                or renderable.type_hash != MODEL
                or u32(relationship.preview, 8) != MODEL
                or renderable.parent_index != entry.index
            ):
                raise ValueError(
                    f"invalid inline model relationship in {location.relative_path} "
                    f"[{entry.index}]"
                )
            observations.append(
                Observation(
                    file_id,
                    location.corpus,
                    canonical_path(location.relative_path),
                    ordinal,
                    entry,
                    parent_types[(file_id, entry.index)],
                    fields,
                    matrices[0],
                    matrices[1],
                    renderable,
                )
            )
    return observations


def resource_groups(
    observations: list[Observation], corpus: str
) -> dict[str, list[Observation]]:
    result: dict[str, list[Observation]] = collections.defaultdict(list)
    for item in observations:
        if item.corpus == corpus:
            result[item.path].append(item)
    for values in result.values():
        values.sort(key=lambda item: item.ordinal)
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    connection = sqlite3.connect(args.database)
    observations = load(connection)

    print("profiles")
    for corpus, values in sorted(
        (corpus, [item for item in observations if item.corpus == corpus])
        for corpus in {item.corpus for item in observations}
    ):
        print(
            f"{corpus:13} objects={len(values):6} "
            f"resources={len({item.file_id for item in values}):2}"
        )

    print("\nstructure")
    for corpus in sorted({item.corpus for item in observations}):
        values = [item for item in observations if item.corpus == corpus]
        inline_sizes = [item.fields[2].payload_size - 8 for item in values]
        print(
            f"{corpus:13} fields=(1:64,2:64,0:inline spModel) "
            f"inline_model={min(inline_sizes)}..{max(inline_sizes)} "
            f"distinct_sizes={len(set(inline_sizes))}"
        )

    print("\nmatrix validation")
    for corpus in sorted({item.corpus for item in observations}):
        values = [item for item in observations if item.corpus == corpus]
        affine_forward = sum(is_affine(item.forward) for item in values)
        affine_inverse = sum(is_affine(item.inverse) for item in values)
        singular = sum(abs(determinant3(item.forward)) <= 1e-8 for item in values)
        mirrored = sum(determinant3(item.forward) < 0 for item in values)
        scale_values = [axis_lengths(item.forward) for item in values]
        unit_scale = sum(
            max(abs(axis - 1.0) for axis in scale) <= 1e-5
            for scale in scale_values
        )
        uniform_scale = sum(
            max(scale) - min(scale) <= 1e-5 for scale in scale_values
        )
        zero_translation = sum(
            max(abs(item.forward[index]) for index in (12, 13, 14)) <= 1e-6
            for item in values
        )
        inverse_ok = sum(
            identity_error(matrix_multiply(item.forward, item.inverse)) <= 1e-4
            for item in values
        )
        engine_errors = [
            max(abs(a - b) for a, b in zip(
                engine_world_inverse(item.forward), item.inverse, strict=True))
            for item in values
        ]
        engine_inverse_ok = sum(error <= 0.01 for error in engine_errors)
        identity = sum(identity_error(item.forward) <= 1e-6 for item in values)
        max_residual = max(
            identity_error(matrix_multiply(item.forward, item.inverse))
            for item in values
        )
        print(
            f"{corpus:13} affine={affine_forward}/{affine_inverse} "
            f"singular={singular} mathematical_inverse={inverse_ok} "
            f"engine_transpose_inverse={engine_inverse_ok} identity={identity} "
            f"max_product_error={max_residual:.9g} "
            f"max_engine_error={max(engine_errors):.9g}"
        )
        print(
            f"{'':13} unit_scale={unit_scale} uniform_scale={uniform_scale} "
            f"mirrored={mirrored} zero_translation={zero_translation} "
            f"scale_range={min(min(item) for item in scale_values):.9g}.."
            f"{max(max(item) for item in scale_values):.9g} "
            f"max_axis_dot={max(orthogonality_error(item.forward) for item in values):.9g}"
        )

    pc_working = resource_groups(observations, "pc-working")
    pc_pristine = resource_groups(observations, "pc-pristine")
    common_pc = sorted(pc_working.keys() & pc_pristine.keys())
    equal_pc = 0
    for path in common_pc:
        left = pc_working[path]
        right = pc_pristine[path]
        if len(left) != len(right):
            continue
        if all(
            a.entry.name == b.entry.name
            and all(
                x.payload_sha256 == y.payload_sha256
                for x, y in zip(a.fields, b.fields, strict=True)
            )
            for a, b in zip(left, right, strict=True)
        ):
            equal_pc += 1
    print("\nPC working/pristine")
    print(f"common resources={len(common_pc)} exactly_equal={equal_pc}")

    ps2 = resource_groups(observations, "ps2-pristine")
    common = sorted(pc_pristine.keys() & ps2.keys())
    equal_count = [
        path for path in common if len(pc_pristine[path]) == len(ps2[path])
    ]
    pairs = [
        (left, right)
        for path in equal_count
        for left, right in zip(pc_pristine[path], ps2[path], strict=True)
    ]
    checks = {
        "object_name": lambda a, b: a.entry.name == b.entry.name,
        "model_name": lambda a, b: a.renderable.name == b.renderable.name,
        "forward_matrix": lambda a, b: matrix_equal(a.forward, b.forward),
        "inverse_matrix": lambda a, b: matrix_equal(a.inverse, b.inverse),
        "translation": lambda a, b: matrix_equal(
            (a.forward[12], a.forward[13], a.forward[14]),
            (b.forward[12], b.forward[13], b.forward[14]),
        ),
    }
    print("\nPC/PS2 ordinal pairing")
    print(
        f"common resources={len(common)} equal_count_resources={len(equal_count)} "
        f"paired_objects={len(pairs)}"
    )
    for name, check in checks.items():
        equal = sum(check(left, right) for left, right in pairs)
        print(f"{name:20} equal={equal:6} different={len(pairs) - equal:6}")

    hashes = collections.Counter(
        (item.corpus, hashlib.sha256(struct.pack("<16f", *item.forward)).hexdigest())
        for item in observations
    )
    print("\nunique forward matrices")
    for corpus in sorted({item.corpus for item in observations}):
        print(f"{corpus:13} {sum(key[0] == corpus for key in hashes)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
