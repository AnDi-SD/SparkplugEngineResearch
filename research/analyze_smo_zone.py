#!/usr/bin/env python3
"""Read-only structural report for analyzed spZone rows in corpus schema v2."""

from __future__ import annotations

import argparse
import collections
import json
import sqlite3
from pathlib import Path


ZONE = 0x61254AB3


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    connection = sqlite3.connect(f"file:{args.database.resolve()}?mode=ro", uri=True)
    print("profiles")
    for row in connection.execute(
        """
        SELECT c.corpus_key,p.platform_key,COUNT(*),COUNT(DISTINCT o.file_id),
               MIN(o.serialized_size),MAX(o.serialized_size),SUM(o.name <> '')
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id JOIN platforms p ON p.id=c.platform_id
        WHERE o.type_hash=? GROUP BY c.id ORDER BY c.id
        """,
        (ZONE,),
    ):
        print(row)
    print("\nfields")
    for row in connection.execute(
        """
        SELECT c.corpus_key,d.section_index,d.field_type,d.is_section_terminator,
               COUNT(*),MIN(d.payload_size),MAX(d.payload_size),
               COUNT(DISTINCT d.payload_size),SUM(d.is_decoded)
        FROM direct_fields d
        JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
        JOIN files f ON f.id=o.file_id JOIN corpora c ON c.id=f.corpus_id
        WHERE o.type_hash=? GROUP BY c.id,d.section_index,d.field_type,
          d.is_section_terminator ORDER BY c.id,d.section_index,d.field_type
        """,
        (ZONE,),
    ):
        print(row)
    roots: collections.Counter[tuple[str, str]] = collections.Counter()
    multiplicity: collections.Counter[tuple[str, int]] = collections.Counter()
    zones = connection.execute(
        """SELECT c.corpus_key,o.file_id,o.object_index FROM objects o
           JOIN files f ON f.id=o.file_id JOIN corpora c ON c.id=f.corpus_id
           WHERE o.type_hash=? ORDER BY c.id,o.file_id,o.object_index""",
        (ZONE,),
    )
    for corpus, file_id, object_index in zones:
        count = 0
        for (decoded,) in connection.execute(
            """SELECT decoded_value FROM direct_fields
               WHERE file_id=? AND object_index=?
                 AND semantic_key='zone.local_partition_root'""",
            (file_id, object_index),
        ):
            value = json.loads(decoded)
            roots[(corpus, value["TargetClass"])] += 1
            count += 1
        multiplicity[(corpus, count)] += 1
    print("\nroot targets")
    for key, count in sorted(roots.items()):
        print(*key, count)
    print("\nroot multiplicity")
    for key, count in sorted(multiplicity.items()):
        print(*key, count)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
