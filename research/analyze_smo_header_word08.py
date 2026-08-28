from __future__ import annotations

import argparse
import collections
import math
import sqlite3
import zlib
from pathlib import Path


def normalize_pair_path(path: str) -> str:
    normalized = path.replace("\\", "/").lower()
    if normalized.startswith("data/"):
        normalized = normalized[5:]
    return normalized


def pearson(left: list[int], right: list[int]) -> float:
    if len(left) != len(right) or len(left) < 2:
        return float("nan")
    left_mean = sum(left) / len(left)
    right_mean = sum(right) / len(right)
    numerator = sum(
        (a - left_mean) * (b - right_mean) for a, b in zip(left, right)
    )
    left_square = sum((item - left_mean) ** 2 for item in left)
    right_square = sum((item - right_mean) ** 2 for item in right)
    denominator = math.sqrt(left_square * right_square)
    return numerator / denominator if denominator else float("nan")


def fnv1a(data: bytes) -> int:
    value = 0x811C9DC5
    for item in data:
        value ^= item
        value = (value * 0x01000193) & 0xFFFFFFFF
    return value


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Analyze the still-unknown UInt32 at FFPS header offset 0x08."
    )
    parser.add_argument("database", type=Path)
    args = parser.parse_args()

    database = args.database.resolve()
    connection = sqlite3.connect(f"file:{database.as_posix()}?mode=ro", uri=True)
    rows = connection.execute(
        """
        SELECT c.corpus_key,f.relative_path,f.byte_size,f.sha256,
               f.ffps_unknown08,f.platform_mask,f.data_start,f.data_size,
               f.object_count
        FROM files f JOIN corpora c ON c.id=f.corpus_id
        WHERE f.extension='.smo' AND f.parse_status='ok'
          AND f.ffps_unknown08 IS NOT NULL
        ORDER BY c.id,f.normalized_path
        """
    ).fetchall()
    connection.close()

    canonical = [row for row in rows if row[0] in ("pc-pristine", "ps2-pristine")]
    values = [int(row[4]) for row in canonical]
    counts = collections.Counter(values)
    print(
        f"canonical resources={len(canonical)} distinct={len(counts)} "
        f"range=0x{min(values):04X}..0x{max(values):04X} "
        f"all_15_bit={all(0 <= value <= 0x7FFF for value in values)}"
    )
    print("top values:")
    for value, count in counts.most_common(16):
        print(f"  0x{value:04X}: {count}")

    metrics = {
        "byte_size": [int(row[2]) for row in canonical],
        "data_start": [int(row[6]) for row in canonical],
        "data_size": [int(row[7]) for row in canonical],
        "object_count": [int(row[8]) for row in canonical],
        "platform_mask": [int(row[5]) for row in canonical],
    }
    print("pearson correlations:")
    for name, metric in metrics.items():
        print(f"  word08 vs {name}: {pearson(values, metric):+.6f}")

    candidates: dict[str, list[int]] = {
        "byte_size low15": [int(row[2]) & 0x7FFF for row in canonical],
        "data_start low15": [int(row[6]) & 0x7FFF for row in canonical],
        "data_size low15": [int(row[7]) & 0x7FFF for row in canonical],
        "object_count low15": [int(row[8]) & 0x7FFF for row in canonical],
    }
    for source_kind, selector in (
        ("path", lambda row: normalize_pair_path(str(row[1]))),
        ("file_name", lambda row: Path(str(row[1])).name.lower()),
    ):
        encoded = [selector(row).encode("utf-8") for row in canonical]
        candidates[f"crc32({source_kind}) low15"] = [
            zlib.crc32(item) & 0x7FFF for item in encoded
        ]
        candidates[f"adler32({source_kind}) low15"] = [
            zlib.adler32(item) & 0x7FFF for item in encoded
        ]
        candidates[f"fnv1a({source_kind}) low15"] = [
            fnv1a(item) & 0x7FFF for item in encoded
        ]
    print("exact candidate matches:")
    for name, candidate in candidates.items():
        matches = sum(a == b for a, b in zip(values, candidate))
        print(f"  {name}: {matches}/{len(values)}")

    by_sha: dict[str, set[int]] = collections.defaultdict(set)
    for row in rows:
        by_sha[str(row[3])].add(int(row[4]))
    inconsistent_sha = sum(len(items) > 1 for items in by_sha.values())
    print(
        f"same SHA-256 with different word08: {inconsistent_sha}/"
        f"{len(by_sha)} unique contents"
    )

    pc = {
        normalize_pair_path(str(row[1])): row
        for row in rows
        if row[0] == "pc-pristine"
    }
    ps2 = {
        normalize_pair_path(str(row[1])): row
        for row in rows
        if row[0] == "ps2-pristine"
    }
    paired_paths = sorted(pc.keys() & ps2.keys())
    same_word = sum(int(pc[path][4]) == int(ps2[path][4]) for path in paired_paths)
    same_content = sum(str(pc[path][3]) == str(ps2[path][3]) for path in paired_paths)
    same_word_and_content = sum(
        int(pc[path][4]) == int(ps2[path][4]) and
        str(pc[path][3]) == str(ps2[path][3])
        for path in paired_paths
    )
    print(
        f"PC/PS2 same-path pairs={len(paired_paths)} "
        f"same_word08={same_word} same_content={same_content} "
        f"same_word08_and_content={same_word_and_content}"
    )

    pc_pristine = {
        normalize_pair_path(str(row[1])): row
        for row in rows
        if row[0] == "pc-pristine"
    }
    pc_working = {
        normalize_pair_path(str(row[1])): row
        for row in rows
        if row[0] == "pc-working"
    }
    working_pairs = sorted(pc_pristine.keys() & pc_working.keys())
    changed_content = [
        path for path in working_pairs
        if str(pc_pristine[path][3]) != str(pc_working[path][3])
    ]
    changed_word = sum(
        int(pc_pristine[path][4]) != int(pc_working[path][4])
        for path in changed_content
    )
    print(
        f"PC pristine/working SMO pairs={len(working_pairs)} "
        f"changed_content={len(changed_content)} "
        f"changed_word08_among_changed={changed_word}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
