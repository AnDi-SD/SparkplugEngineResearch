#!/usr/bin/env python3
"""Safely inspect Winx/Sparkplug PCK indexes without reading payload data.

The parser mirrors spPCKManager's executable-backed layout: u32 string bytes,
the string block, u32 file count, then 0x14-byte file records.  It deliberately
caps metadata and validates every offset before decoding it.
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


RECORD = struct.Struct("<IIIII")
MAX_STRING_BYTES = 128 * 1024 * 1024
MAX_RECORDS = 4_000_000


@dataclass(frozen=True)
class FileRecord:
    directory: str
    file_name: str
    logical_sector: int
    byte_offset: int
    byte_count: int


@dataclass(frozen=True)
class PackageIndex:
    path: Path
    physical_size: int
    string_bytes: int
    files: tuple[FileRecord, ...]


def read_exact(stream, count: int) -> bytes:
    data = stream.read(count)
    if len(data) != count:
        raise ValueError(f"truncated metadata: requested {count}, got {len(data)}")
    return data


def block_string(block: bytes, offset: int) -> str:
    if offset >= len(block):
        raise ValueError(f"string offset 0x{offset:X} is outside 0x{len(block):X}")
    end = block.find(b"\0", offset)
    if end < 0:
        raise ValueError(f"string at 0x{offset:X} has no terminator")
    return block[offset:end].decode("ascii", errors="strict")


def read_index(path: Path) -> PackageIndex:
    physical_size = path.stat().st_size
    with path.open("rb") as stream:
        string_bytes = struct.unpack("<I", read_exact(stream, 4))[0]
        if string_bytes > MAX_STRING_BYTES or string_bytes > physical_size - 8:
            raise ValueError(f"unsafe string-block size 0x{string_bytes:X}")
        strings = read_exact(stream, string_bytes)
        file_count = struct.unpack("<I", read_exact(stream, 4))[0]
        if file_count > MAX_RECORDS:
            raise ValueError(f"unsafe file count {file_count}")
        metadata_end = 8 + string_bytes + file_count * RECORD.size
        if metadata_end > physical_size:
            raise ValueError(
                f"record table ends at 0x{metadata_end:X}, file ends at 0x{physical_size:X}"
            )
        raw_records = read_exact(stream, file_count * RECORD.size)

    records: list[FileRecord] = []
    previous_name = ""
    for index, values in enumerate(RECORD.iter_unpack(raw_records)):
        directory_offset, file_name_offset, lsn, byte_offset, byte_count = values
        directory = block_string(strings, directory_offset)
        file_name = block_string(strings, file_name_offset)
        if index and previous_name > file_name:
            raise ValueError(
                f"record {index} breaks basename ordering: {previous_name!r} > {file_name!r}"
            )
        if lsn * 0x800 != byte_offset:
            raise ValueError(
                f"record {index} LSN mismatch: 0x{lsn:X} * 0x800 != 0x{byte_offset:X}"
            )
        if byte_offset > physical_size or byte_count > physical_size - byte_offset:
            raise ValueError(
                f"record {index} payload 0x{byte_offset:X}+0x{byte_count:X} is out of range"
            )
        records.append(FileRecord(directory, file_name, lsn, byte_offset, byte_count))
        previous_name = file_name

    return PackageIndex(path, physical_size, string_bytes, tuple(records))


def normalize_resource_name(value: str) -> tuple[str, str]:
    if value.startswith(("./", ".\\")):
        value = value[2:]
    value = value.replace("\\", "/").lower()
    directory, separator, file_name = value.rpartition("/")
    return (directory, file_name) if separator else ("", value)


def find_record(index: PackageIndex, resource_name: str) -> FileRecord | None:
    directory, file_name = normalize_resource_name(resource_name)
    # Keep the implementation simple here; the native manager's binary search
    # and equal-basename scan are reconstructed and tested in C++.
    return next(
        (
            record
            for record in index.files
            if record.file_name == file_name and record.directory == directory
        ),
        None,
    )


def collect_paths(arguments: list[Path]) -> list[Path]:
    result: list[Path] = []
    for path in arguments:
        if path.is_dir():
            result.extend(sorted(path.rglob("*.PCK")))
            result.extend(sorted(path.rglob("*.pck")))
        else:
            result.append(path)
    # A case-insensitive filesystem can produce the same file twice.
    return list(dict.fromkeys(path.resolve() for path in result))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--lookup", action="append", default=[])
    parser.add_argument("--show", type=int, default=0, help="show first N records")
    args = parser.parse_args()

    paths = collect_paths(args.paths)
    if not paths:
        parser.error("no PCK files found")

    failures = 0
    total_files = 0
    for path in paths:
        try:
            index = read_index(path)
        except (OSError, UnicodeError, ValueError) as error:
            failures += 1
            print(f"ERROR {path}: {error}")
            continue

        total_files += len(index.files)
        print(
            f"PCK {path}: size=0x{index.physical_size:X} "
            f"strings=0x{index.string_bytes:X} files={len(index.files)}"
        )
        for record in index.files[: args.show]:
            print(
                f"  {record.directory}/{record.file_name}: "
                f"lsn=0x{record.logical_sector:X} offset=0x{record.byte_offset:X} "
                f"size=0x{record.byte_count:X}"
            )
        for query in args.lookup:
            record = find_record(index, query)
            if record is not None:
                print(
                    f"  FOUND {query}: lsn=0x{record.logical_sector:X} "
                    f"offset=0x{record.byte_offset:X} size=0x{record.byte_count:X}"
                )

    print(f"SUMMARY packages={len(paths)} valid={len(paths) - failures} files={total_files}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
