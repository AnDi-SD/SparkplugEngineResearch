#!/usr/bin/env python3
"""Read-only corpus report for spOcclusionVolume in schema v2."""

from __future__ import annotations

import argparse
import collections
import hashlib
import math
import sqlite3
import struct
from dataclasses import dataclass
from pathlib import Path


OCCLUSION_VOLUME = 0x43D24430
NODE = 0x695C0F65


@dataclass(frozen=True)
class Volume:
    corpus: str
    file_id: int
    path: str
    object_index: int
    name: str
    parent_type: int | None
    parent_name: str | None
    serialized_size: int
    position: tuple[float, float, float]
    rotation: tuple[float, float, float, float] | None
    animated: bool
    primitive_type: int
    index_format: int
    triangles: tuple[tuple[int, int, int], ...]
    vertex_declaration: int
    vertex_flags: int
    vertices: tuple[tuple[float, float, float], ...]
    serialized_hash: str


def canonical_path(path: str) -> str:
    value = path.replace("\\", "/").lower()
    return value[5:] if value.startswith("data/") else value


def read_resource(
    source_kind: str,
    source_root: str,
    relative_path: str,
    container_path: str | None,
    occurrence_offset: int | None,
    byte_size: int,
) -> bytes:
    root = Path(source_root)
    if source_kind == "directory":
        return (root / Path(relative_path.replace("/", "\\"))).read_bytes()
    if container_path is None or occurrence_offset is None:
        raise ValueError(f"incomplete PCK location for {relative_path}")
    archive = root / Path(container_path.replace("/", "\\"))
    with archive.open("rb") as stream:
        stream.seek(occurrence_offset)
        result = stream.read(byte_size)
    if len(result) != byte_size:
        raise ValueError(f"short PCK read for {relative_path}")
    return result


def decode_volumes(connection: sqlite3.Connection) -> list[Volume]:
    rows = connection.execute(
        """
        SELECT c.corpus_key,o.file_id,f.relative_path,o.object_index,o.name,
               p.type_hash,p.name,o.serialized_size,c.source_kind,c.source_root,
               ct.relative_path,fo.byte_offset,f.byte_size
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        LEFT JOIN objects p ON p.file_id=o.file_id AND p.object_index=o.parent_index
        LEFT JOIN file_occurrences fo ON fo.id=(
            SELECT MIN(inner_fo.id) FROM file_occurrences inner_fo
            WHERE inner_fo.file_id=f.id)
        LEFT JOIN containers ct ON ct.id=fo.container_id
        WHERE o.type_hash=? ORDER BY c.id,o.file_id,o.object_index
        """,
        (OCCLUSION_VOLUME,),
    ).fetchall()
    result: list[Volume] = []
    resources: dict[int, bytes] = {}
    for (
        corpus, file_id, path, index, name, parent_type, parent_name, size,
        source_kind, source_root, container_path, occurrence_offset, byte_size,
    ) in rows:
        if file_id not in resources:
            resources[file_id] = read_resource(
                source_kind, source_root, path, container_path,
                occurrence_offset, byte_size,
            )
        resource = resources[file_id]
        fields = connection.execute(
            """
            SELECT section_index,field_type,payload_size,payload_preview,
                   absolute_payload_offset
            FROM direct_fields WHERE file_id=? AND object_index=?
              AND is_section_terminator=0 ORDER BY field_index
            """,
            (file_id, index),
        ).fetchall()
        position = rotation = index_buffer = vertex_buffer = None
        animated = None
        serialized = bytearray()
        for section, field_type, payload_size, preview, payload_offset in fields:
            payload = resource[payload_offset : payload_offset + payload_size]
            if len(payload) != payload_size:
                raise ValueError("occlusion payload exceeds the source resource")
            if payload[: len(preview)] != bytes(preview):
                raise ValueError("source payload no longer matches the database preview")
            serialized.extend(struct.pack("<II", field_type, payload_size))
            serialized.extend(payload)
            if section == 0 and field_type == 0:
                position = struct.unpack("<3f", payload)
            elif section == 0 and field_type == 1:
                rotation = struct.unpack("<4f", payload)
            elif section == 0 and field_type == 8:
                animated = bool(payload[0])
            elif section == 1 and field_type == 0:
                index_buffer = payload
            elif section == 1 and field_type == 1:
                vertex_buffer = payload
        if position is None or animated is None or index_buffer is None or vertex_buffer is None:
            raise ValueError(f"incomplete spOcclusionVolume {corpus}:{path} [{index}]")

        primitive_type, triangle_count, index_format = struct.unpack_from(
            "<III", index_buffer
        )
        if len(index_buffer) != 12 + triangle_count * 6:
            raise ValueError("invalid occlusion index buffer length")
        indices = struct.unpack_from(f"<{triangle_count * 3}H", index_buffer, 12)
        triangles = tuple(
            tuple(indices[offset : offset + 3])
            for offset in range(0, len(indices), 3)
        )
        vertex_declaration, vertex_count, vertex_flags = struct.unpack_from(
            "<III", vertex_buffer
        )
        if len(vertex_buffer) != 12 + vertex_count * 12:
            raise ValueError("invalid occlusion vertex buffer length")
        vertices = tuple(
            struct.unpack_from("<3f", vertex_buffer, 12 + vertex * 12)
            for vertex in range(vertex_count)
        )
        if any(value >= vertex_count for triangle in triangles for value in triangle):
            raise ValueError("occlusion index outside vertex buffer")
        result.append(
            Volume(
                corpus,
                file_id,
                path,
                index,
                name,
                parent_type,
                parent_name,
                size,
                position,
                rotation,
                animated,
                primitive_type,
                index_format,
                triangles,
                vertex_declaration,
                vertex_flags,
                vertices,
                hashlib.sha256(serialized).hexdigest(),
            )
        )
    return result


def subtract(a: tuple[float, float, float], b: tuple[float, float, float]):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a: tuple[float, float, float], b: tuple[float, float, float]):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def dot(a: tuple[float, float, float], b: tuple[float, float, float]):
    return sum(left * right for left, right in zip(a, b))


def length(value: tuple[float, float, float]):
    return math.sqrt(dot(value, value))


def geometry(volume: Volume) -> tuple[float, float, int, int, bool, bool]:
    normals: list[tuple[float, float, float]] = []
    area = 0.0
    edges: collections.Counter[tuple[int, int]] = collections.Counter()
    for triangle in volume.triangles:
        a, b, c = (volume.vertices[index] for index in triangle)
        normal = cross(subtract(b, a), subtract(c, a))
        magnitude = length(normal)
        if magnitude <= 1e-6:
            raise ValueError(f"degenerate occlusion triangle in {volume.name}")
        normals.append(tuple(value / magnitude for value in normal))
        area += magnitude * 0.5
        for start, end in zip(triangle, triangle[1:] + triangle[:1]):
            edges[tuple(sorted((start, end)))] += 1
    reference = normals[0]
    if any(dot(reference, normal) < 0.9999 for normal in normals[1:]):
        raise ValueError(f"inconsistent triangle winding in {volume.name}")
    origin = volume.vertices[volume.triangles[0][0]]
    maximum_distance = max(
        abs(dot(reference, subtract(vertex, origin))) for vertex in volume.vertices
    )
    boundary_edges = sum(count == 1 for count in edges.values())
    internal_edges = sum(count == 2 for count in edges.values())
    manifold = all(count in (1, 2) for count in edges.values())
    boundary: dict[int, list[int]] = collections.defaultdict(list)
    for (start, end), count in edges.items():
        if count == 1:
            boundary[start].append(end)
            boundary[end].append(start)
    strictly_convex = len(boundary) == len(volume.vertices) and all(
        len(neighbours) == 2 for neighbours in boundary.values()
    )
    cycle: list[int] = []
    if strictly_convex:
        start = min(boundary)
        previous, current = -1, start
        while current != start or not cycle:
            if current in cycle:
                strictly_convex = False
                break
            cycle.append(current)
            neighbours = boundary[current]
            following = neighbours[0] if neighbours[0] != previous else neighbours[1]
            previous, current = current, following
        strictly_convex = strictly_convex and len(cycle) == len(volume.vertices)
    if strictly_convex:
        drop_axis = max(range(3), key=lambda axis: abs(reference[axis]))
        kept = [axis for axis in range(3) if axis != drop_axis]
        turn_sign = 0
        for index in range(len(cycle)):
            a, b, c = (
                volume.vertices[cycle[(index + offset) % len(cycle)]]
                for offset in range(3)
            )
            turn = (
                (b[kept[0]] - a[kept[0]]) * (c[kept[1]] - b[kept[1]])
                - (b[kept[1]] - a[kept[1]]) * (c[kept[0]] - b[kept[0]])
            )
            sign = 1 if turn > 0.0001 else -1 if turn < -0.0001 else 0
            if sign == 0 or (turn_sign and sign != turn_sign):
                strictly_convex = False
                break
            turn_sign = sign
    return (
        maximum_distance, area, boundary_edges, internal_edges, manifold,
        strictly_convex,
    )


def signature(volume: Volume) -> tuple[object, ...]:
    return (
        volume.name,
        volume.position,
        volume.rotation,
        volume.animated,
        volume.primitive_type,
        volume.index_format,
        volume.triangles,
        volume.vertex_declaration,
        volume.vertex_flags,
        volume.vertices,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    connection = sqlite3.connect(f"file:{args.database.resolve()}?mode=ro", uri=True)
    volumes = decode_volumes(connection)

    print("profiles")
    for row in connection.execute(
        """
        SELECT c.corpus_key,p.platform_key,COUNT(*),COUNT(DISTINCT o.file_id),
               MIN(o.serialized_size),MAX(o.serialized_size),SUM(o.name<>'')
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id JOIN platforms p ON p.id=c.platform_id
        WHERE o.type_hash=? GROUP BY c.id ORDER BY c.id
        """,
        (OCCLUSION_VOLUME,),
    ):
        print(row)

    print("\ngeometry")
    for corpus, items in sorted(
        (key, list(group))
        for key, group in __import__("itertools").groupby(
            sorted(volumes, key=lambda item: item.corpus), lambda item: item.corpus
        )
    ):
        metrics = [geometry(item) for item in items]
        print(
            corpus,
            "cardinality",
            dict(collections.Counter((len(item.vertices), len(item.triangles)) for item in items)),
            "rotation_present",
            sum(item.rotation is not None for item in items),
            "headers",
            collections.Counter(
                (item.primitive_type, item.index_format, item.vertex_declaration, item.vertex_flags)
                for item in items
            ),
            "max_planar_error",
            max(item[0] for item in metrics),
            "area_range",
            (min(item[1] for item in metrics), max(item[1] for item in metrics)),
            "all_manifold_disks",
            all(item[4] and item[2] == len(volume.vertices) for item, volume in zip(metrics, items)),
            "strictly_convex",
            sum(item[5] for item in metrics),
        )

    print("\nobjects (pc-pristine)")
    for volume in volumes:
        if volume.corpus != "pc-pristine":
            continue
        planar_error, area, boundary, internal, _, strictly_convex = geometry(volume)
        print(
            canonical_path(volume.path),
            "object", volume.object_index,
            volume.name,
            "vertices", len(volume.vertices),
            "triangles", len(volume.triangles),
            "boundary", boundary,
            "internal", internal,
            "area", area,
            "planar_error", planar_error,
            "strictly_convex", strictly_convex,
            "position", volume.position,
            "rotation", volume.rotation,
        )

    print("\ncross corpus")
    def keyed(corpus: str):
        ordinals: collections.Counter[tuple[str, str]] = collections.Counter()
        result = {}
        for item in (value for value in volumes if value.corpus == corpus):
            base = (canonical_path(item.path), item.name.lower())
            ordinal = ordinals[base]
            ordinals[base] += 1
            result[(base[0], base[1], ordinal)] = item
        return result

    for left_name, right_name in (
        ("pc-working", "pc-pristine"),
        ("pc-pristine", "ps2-pristine"),
    ):
        left, right = keyed(left_name), keyed(right_name)
        keys = left.keys() & right.keys()
        print(
            left_name, right_name,
            "paired", len(keys),
            "semantic", sum(signature(left[key]) == signature(right[key]) for key in keys),
            "field_bytes", sum(left[key].serialized_hash == right[key].serialized_hash for key in keys),
        )

    print("\nanalysis rows")
    for label, sql in (
        ("field_definitions", "SELECT COUNT(*) FROM field_definitions WHERE type_hash=?"),
        ("variants", "SELECT COUNT(*) FROM class_variants WHERE type_hash=?"),
        ("evidence", "SELECT COUNT(*) FROM evidence WHERE type_hash=?"),
        ("annotated_fields", """
            SELECT COUNT(*) FROM direct_fields d JOIN objects o
              ON o.file_id=d.file_id AND o.object_index=d.object_index
            WHERE o.type_hash=? AND d.semantic_key IS NOT NULL
            """),
        ("variant_assignments", """
            SELECT COUNT(*) FROM object_variant_assignments a
            JOIN class_variants v ON v.id=a.variant_id WHERE v.type_hash=?
            """),
    ):
        print(label, connection.execute(sql, (OCCLUSION_VOLUME,)).fetchone()[0])
    connection.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
