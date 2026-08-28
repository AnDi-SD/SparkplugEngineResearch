#!/usr/bin/env python3
"""Read-only report for analyzed spZonePortal rows in corpus schema v2."""

from __future__ import annotations

import argparse
import collections
import json
import sqlite3
from pathlib import Path


ZONE_PORTAL = 0x6523AC37


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
        (ZONE_PORTAL,),
    ):
        print(row)
    print("\nfields")
    for row in connection.execute(
        """
        SELECT c.corpus_key,d.field_type,d.is_section_terminator,COUNT(*),
               MIN(d.payload_size),MAX(d.payload_size),
               COUNT(DISTINCT d.payload_size),SUM(d.is_decoded)
        FROM direct_fields d
        JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
        JOIN files f ON f.id=o.file_id JOIN corpora c ON c.id=f.corpus_id
        WHERE o.type_hash=? GROUP BY c.id,d.field_type,d.is_section_terminator
        ORDER BY c.id,d.field_type,d.is_section_terminator
        """,
        (ZONE_PORTAL,),
    ):
        print(row)

    destinations: collections.Counter[tuple[str, str]] = collections.Counter()
    vertices: collections.Counter[tuple[str, int]] = collections.Counter()
    open_values: collections.Counter[tuple[str, bool]] = collections.Counter()
    rows = connection.execute(
        """SELECT c.corpus_key,d.semantic_key,d.decoded_value
           FROM direct_fields d
           JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
           JOIN files f ON f.id=o.file_id JOIN corpora c ON c.id=f.corpus_id
           WHERE o.type_hash=? AND d.is_decoded=1
           ORDER BY c.id,o.file_id,o.object_index,d.field_index""",
        (ZONE_PORTAL,),
    )
    for corpus, semantic, decoded in rows:
        value = json.loads(decoded)
        if semantic == "zone_portal.destination_zone":
            destinations[(corpus, value["Encoding"])] += 1
        elif semantic == "zone_portal.polygon":
            vertices[(corpus, value["VertexCount"])] += 1
        elif semantic == "zone_portal.open":
            open_values[(corpus, value)] += 1
    print("\ndestination encodings")
    for key, count in sorted(destinations.items()):
        print(*key, count)
    print("\npolygon vertex counts")
    for key, count in sorted(vertices.items()):
        print(*key, count)
    print("\nopen values")
    for key, count in sorted(open_values.items()):
        print(*key, count)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
