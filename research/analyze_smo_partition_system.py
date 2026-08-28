#!/usr/bin/env python3
"""Read-only structural report for spPartitionSystem in corpus schema v2."""

from __future__ import annotations

import argparse
import collections
import json
import sqlite3
from pathlib import Path


PARTITION_SYSTEM = 0x912CC341


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
        (PARTITION_SYSTEM,),
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
        (PARTITION_SYSTEM,),
    ):
        print(row)
    print("\nrelationships")
    relationships: collections.Counter[tuple[str, str, str, str]] = (
        collections.Counter()
    )
    roots: collections.Counter[tuple[str, int]] = collections.Counter()
    systems = connection.execute(
        """SELECT c.corpus_key,o.file_id,o.object_index FROM objects o
           JOIN files f ON f.id=o.file_id JOIN corpora c ON c.id=f.corpus_id
           WHERE o.type_hash=? ORDER BY c.id,o.file_id""",
        (PARTITION_SYSTEM,),
    )
    for corpus, file_id, object_index in systems:
        for semantic, payload_size, decoded in connection.execute(
            """SELECT semantic_key,payload_size,decoded_value FROM direct_fields
               WHERE file_id=? AND object_index=? AND is_section_terminator=0""",
            (file_id, object_index),
        ):
            if semantic not in (
                "node.child", "node.collision", "partition_system.partition_root"
            ):
                continue
            value = json.loads(decoded)
            relationships[(
                corpus, semantic, value["Encoding"], value["TargetTypeHash"]
            )] += 1
            if semantic == "partition_system.partition_root":
                roots[(corpus, payload_size)] += 1
    for key, count in sorted(relationships.items()):
        print(*key, count)
    print("\nroot payload sizes")
    for (corpus, size), count in sorted(roots.items()):
        print(corpus, size, count)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
