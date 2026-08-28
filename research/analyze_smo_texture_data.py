#!/usr/bin/env python3
"""Read-only structural inventory for embedded spTextureData payloads."""

from __future__ import annotations

import argparse
import collections
import sqlite3
import struct
from dataclasses import dataclass
from pathlib import Path


TEXTURE_DATA_HASH = 0x78EA082B


@dataclass(frozen=True)
class Field:
    section: int
    type: int
    payload: bytes


def read_header(data: bytes, offset: int) -> tuple[int, int, int]:
    if offset >= len(data):
        raise ValueError("field header outside payload")
    raw = data[offset]
    if raw == 0:
        return 0, 0, 1
    field_type = raw & 0x1F
    size_code = raw >> 5
    header_size = 1
    if field_type == 0x1F:
        if offset + header_size >= len(data):
            raise ValueError("extended field type outside payload")
        field_type = data[offset + header_size]
        header_size += 1
    if size_code <= 4:
        size = (0, 1, 2, 4, 8)[size_code]
    elif size_code == 5:
        size = data[offset + header_size]
        header_size += 1
    elif size_code == 6:
        size = struct.unpack_from("<H", data, offset + header_size)[0]
        header_size += 2
    else:
        size = struct.unpack_from("<I", data, offset + header_size)[0]
        header_size += 4
    if offset + header_size + size > len(data):
        raise ValueError("field payload crosses containing block")
    return field_type, size, header_size


def parse_sections(data: bytes) -> list[Field]:
    fields: list[Field] = []
    offset = 0
    section = 0
    while offset < len(data):
        field_type, size, header_size = read_header(data, offset)
        offset += header_size
        if field_type == 0 and size == 0:
            section += 1
            continue
        fields.append(Field(section, field_type, data[offset : offset + size]))
        offset += size
    return fields


def load_payload(row: sqlite3.Row, handles: dict[Path, object]) -> bytes:
    source_root = Path(row["source_root"])
    if row["source_kind"] == "directory":
        path = source_root / Path(row["relative_path"])
        absolute = row["physical_offset"] + row["relative_payload_offset"]
    else:
        path = source_root / Path(row["container_path"])
        absolute = (
            row["occurrence_offset"]
            + row["physical_offset"]
            + row["relative_payload_offset"]
        )
    stream = handles.get(path)
    if stream is None:
        stream = path.open("rb")
        handles[path] = stream
    stream.seek(absolute)
    payload = stream.read(row["payload_size"])
    if len(payload) != row["payload_size"]:
        raise ValueError(f"short source read: {path} @ 0x{absolute:X}")
    return payload


def describe_cross_platform(payload: bytes) -> tuple:
    fields = parse_sections(payload)
    if len(fields) != 1 or fields[0].type != 5 or len(fields[0].payload) < 16:
        return ("invalid", tuple((f.section, f.type, len(f.payload)) for f in fields))
    raw = fields[0].payload
    width, height, flags, bytes_per_pixel = struct.unpack_from("<IIII", raw)
    return (
        "cross",
        width,
        height,
        flags,
        bytes_per_pixel,
        len(raw) - 16,
        len(fields[0].payload),
    )


def describe_pc_platform(payload: bytes) -> tuple:
    fields = parse_sections(payload)
    shapes = tuple((field.type, len(field.payload)) for field in fields)
    if not fields or fields[0].type != 0 or any(
        field.type != 1 for field in fields[1:]
    ):
        return ("invalid-pc-platform", shapes)
    first = fields[0].payload
    if len(first) < 26:
        return ("short-pc-platform", shapes, first.hex())
    present = first[0]
    width, height, pixel_format = struct.unpack_from("<III", first, 1)
    mip_data_present = first[13]
    first_width, first_stride, first_height = struct.unpack_from("<III", first, 14)
    mip_shapes = [(first_width, first_stride, first_height, len(first) - 26)]
    for field in fields[1:]:
        if len(field.payload) < 12:
            return ("short-pc-mip", shapes, len(field.payload))
        mip_width, mip_stride, mip_height = struct.unpack_from(
            "<III", field.payload
        )
        mip_shapes.append(
            (mip_width, mip_stride, mip_height, len(field.payload) - 12)
        )
    valid = (
        present in (0, 1)
        and width == first_width
        and height == first_height
        and mip_data_present in (0, 1)
        and all(
            stride * mip_height == data_size
            for _, stride, mip_height, data_size in mip_shapes
        )
    )
    if not valid:
        return (
            "invalid-pc-platform", shapes
        )
    return (
        "pc-platform",
        present,
        pixel_format,
        width,
        height,
        mip_data_present,
        len(mip_shapes),
        tuple(mip_shapes),
    )


def describe_ps2_platform(payload: bytes) -> tuple:
    fields = parse_sections(payload)
    shapes = tuple((field.type, len(field.payload)) for field in fields)
    if len(fields) != 1 or fields[0].type != 0 or len(fields[0].payload) < 21:
        return ("invalid-ps2-platform", shapes)
    raw = fields[0].payload
    present = raw[0]
    pixel_format, width, height, flags, mip_count = struct.unpack_from(
        "<IIIII", raw, 1
    )
    palette_size = {0: 64, 1: 1024, 3: 0}.get(pixel_format)
    if palette_size is None:
        return ("invalid-ps2-format", pixel_format, shapes)
    offset = 21 + palette_size
    mip_shapes = []
    for _ in range(mip_count):
        if offset + 16 > len(raw):
            return ("short-ps2-mip-header", pixel_format, width, height, mip_count)
        mip_width, mip_height, mip_stride, data_size = struct.unpack_from(
            "<IIII", raw, offset
        )
        offset += 16
        if offset + data_size > len(raw):
            return (
                "short-ps2-mip-data",
                pixel_format,
                width,
                height,
                mip_count,
                (mip_width, mip_height, mip_stride, data_size),
                len(raw) - offset,
            )
        mip_shapes.append((mip_width, mip_height, mip_stride, data_size))
        offset += data_size
    if offset != len(raw):
        return (
            "trailing-ps2-platform-data",
            pixel_format,
            width,
            height,
            mip_count,
            len(raw) - offset,
        )
    return (
        "ps2-platform",
        present,
        pixel_format,
        width,
        height,
        mip_count,
        palette_size,
        tuple(mip_shapes),
    )


def describe_platform_specific(payload: bytes) -> tuple:
    pc = describe_pc_platform(payload)
    if pc[0] == "pc-platform":
        return pc
    ps2 = describe_ps2_platform(payload)
    if ps2[0] == "ps2-platform":
        return ps2
    return ("invalid-platform-specific", pc, ps2)


def describe_embedded(payload: bytes, platform: str) -> tuple:
    fields = parse_sections(payload)
    base = tuple(
        (field.type, len(field.payload), field.payload.hex())
        for field in fields
        if field.section == 0
    )
    derived = [field for field in fields if field.section == 1]
    platform_type = next(
        (
            struct.unpack_from("<I", field.payload)[0]
            for field in derived
            if field.type == 6 and len(field.payload) == 4
        ),
        None,
    )
    cross = next((field for field in derived if field.type == 0), None)
    specific = next((field for field in derived if field.type == 1), None)
    return (
        "embedded",
        platform,
        base,
        platform_type,
        describe_cross_platform(cross.payload) if cross else None,
        (
            describe_platform_specific(specific.payload) if specific else None
        ),
        tuple((field.type, len(field.payload)) for field in derived),
    )


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
    query = """
        SELECT co.corpus_key,p.platform_key,co.source_kind,co.source_root,
               f.relative_path,o.object_index,o.name,o.physical_offset,
               d.field_type,d.relative_payload_offset,d.payload_size,
               ct.relative_path AS container_path,
               fo.byte_offset AS occurrence_offset
        FROM direct_fields d
        JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
        JOIN files f ON f.id=o.file_id
        JOIN corpora co ON co.id=f.corpus_id
        JOIN platforms p ON p.id=co.platform_id
        LEFT JOIN file_occurrences fo ON fo.id=(
            SELECT MIN(inside_fo.id) FROM file_occurrences inside_fo
            WHERE inside_fo.file_id=f.id)
        LEFT JOIN containers ct ON ct.id=fo.container_id
        WHERE o.type_hash=? AND d.is_section_terminator=0
          AND (? IS NULL OR co.corpus_key=?)
        ORDER BY co.id,f.normalized_path,o.object_index,d.field_index;
    """
    counts: collections.Counter[tuple] = collections.Counter()
    examples: dict[tuple, list[str]] = collections.defaultdict(list)
    summary_counts: collections.Counter[tuple] = collections.Counter()
    handles: dict[Path, object] = {}
    try:
        for row in connection.execute(
            query, (TEXTURE_DATA_HASH, args.corpus, args.corpus)
        ):
            payload = load_payload(row, handles)
            field_type = row["field_type"]
            if field_type == 0:
                description = describe_cross_platform(payload)
            elif field_type == 3:
                description = describe_embedded(payload, row["platform_key"])
            elif field_type == 6 and len(payload) == 4:
                description = ("platform-type", struct.unpack_from("<I", payload)[0])
            else:
                description = ("unknown", field_type, len(payload), payload[:32].hex())
            key = (row["corpus_key"], field_type, description)
            counts[key] += 1
            corpus = row["corpus_key"]
            summary_counts[("direct-field", corpus, field_type)] += 1
            if field_type == 0:
                summary_counts[("variant", corpus, "legacy-cross")] += 1
            elif field_type == 3 and description[0] == "embedded":
                platform_type = description[3]
                cross = description[4]
                specific = description[5]
                summary_counts[("platform-type", corpus, platform_type)] += 1
                if specific and specific[0] == "pc-platform":
                    kind = "direct3d-bgra32"
                    mip_count = specific[6]
                    width, height, texture_format = specific[3], specific[4], specific[2]
                elif specific and specific[0] == "ps2-platform":
                    kind = {0: "ps2-indexed4", 1: "ps2-indexed8", 3: "ps2-rgba32"}[
                        specific[2]
                    ]
                    mip_count = specific[5]
                    width, height, texture_format = specific[3], specific[4], specific[2]
                elif cross:
                    kind = "cross-bgra32"
                    mip_count = 1
                    width, height, texture_format = cross[1], cross[2], cross[3]
                else:
                    kind = "invalid"
                    mip_count = width = height = texture_format = -1
                summary_counts[("representation", corpus, kind)] += 1
                summary_counts[("format", corpus, kind, texture_format)] += 1
                summary_counts[("mips", corpus, kind, mip_count)] += 1
                summary_counts[("dimensions", corpus, kind, width, height)] += 1
                variant = (
                    "direct3d-embedded" if kind == "direct3d-bgra32"
                    else "ps2-native-with-cross" if kind.startswith("ps2-") and cross
                    else "ps2-native" if kind.startswith("ps2-")
                    else "embedded-cross-only"
                )
                summary_counts[("variant", corpus, variant)] += 1
            if len(examples[key]) < args.examples:
                examples[key].append(
                    f"{row['relative_path']} [{row['object_index']}] {row['name']}"
                )
    finally:
        for stream in handles.values():
            stream.close()
        connection.close()

    selected_counts = summary_counts if args.summary else counts
    for key, count in sorted(selected_counts.items(), key=lambda item: repr(item[0])):
        print(f"{count:5d} {key}")
        if not args.summary:
            for example in examples[key]:
                print(f"      {example}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
