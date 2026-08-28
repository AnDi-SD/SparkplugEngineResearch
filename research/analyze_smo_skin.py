#!/usr/bin/env python3
"""Read-only report for the analyzed spSkin rows in corpus schema v2."""

from __future__ import annotations

import argparse
import collections
import json
import sqlite3
from pathlib import Path


SKIN = 0x681F2043


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    database = args.database.resolve()
    connection = sqlite3.connect(f"file:{database}?mode=ro", uri=True)

    profiles = connection.execute(
        """
        SELECT c.corpus_key,COUNT(*),COUNT(DISTINCT o.file_id),
               MIN(o.serialized_size),MAX(o.serialized_size)
        FROM objects o
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        WHERE o.type_hash=?
        GROUP BY c.corpus_key
        ORDER BY c.corpus_key
        """,
        (SKIN,),
    ).fetchall()
    print("profiles")
    for corpus, objects, resources, minimum, maximum in profiles:
        print(
            f"{corpus:13} objects={objects:4} resources={resources:3} "
            f"serialized={minimum}..{maximum}"
        )

    fields = connection.execute(
        """
        SELECT c.corpus_key,d.section_index,d.semantic_key,COUNT(*)
        FROM direct_fields d
        JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        WHERE o.type_hash=? AND d.is_section_terminator=0
        GROUP BY c.corpus_key,d.section_index,d.semantic_key
        ORDER BY c.corpus_key,d.section_index,d.semantic_key
        """,
        (SKIN,),
    ).fetchall()
    print("\nsemantic fields")
    for corpus, section, semantic, count in fields:
        print(f"{corpus:13} section={section} {semantic or '<unanalyzed>':28} {count:4}")

    variants = connection.execute(
        """
        SELECT c.corpus_key,v.variant_key,COUNT(*)
        FROM object_variant_assignments a
        JOIN class_variants v ON v.id=a.variant_id
        JOIN objects o ON o.file_id=a.file_id AND o.object_index=a.object_index
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        WHERE v.type_hash=? AND v.variant_key LIKE 'skin_%'
        GROUP BY c.corpus_key,v.variant_key
        ORDER BY c.corpus_key,v.variant_key
        """,
        (SKIN,),
    ).fetchall()
    print("\nfield-presence variants")
    for corpus, variant, count in variants:
        print(f"{corpus:13} {variant:24} {count:4}")

    palettes = connection.execute(
        """
        SELECT c.corpus_key,d.decoded_value
        FROM direct_fields d
        JOIN objects o ON o.file_id=d.file_id AND o.object_index=d.object_index
        JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        WHERE o.type_hash=? AND d.semantic_key='skin.palette'
        ORDER BY c.corpus_key,f.normalized_path,o.object_index
        """,
        (SKIN,),
    ).fetchall()
    header_counts: collections.Counter[tuple[str, int]] = collections.Counter()
    slot_counts: collections.Counter[tuple[str, int]] = collections.Counter()
    storage_counts: collections.Counter[tuple[str, str]] = collections.Counter()
    affine = 0
    invertible = 0
    total_slots = 0
    for corpus, encoded in palettes:
        value = json.loads(encoded)
        header_counts[(corpus, value["blendInfluenceCountHint"])] += 1
        slot_counts[(corpus, value["slotCount"])] += 1
        storage_counts[(corpus, "inline")] += value["inlineNodes"]
        storage_counts[(corpus, "reference")] += value["sizedReferences"]
        affine += value["affineMatrices"]
        invertible += value["invertibleMatrices"]
        total_slots += value["slotCount"]

    print("\npalette slot counts")
    for (corpus, slots), count in sorted(slot_counts.items()):
        print(f"{corpus:13} slots={slots:2} objects={count:4}")
    print("\nblend-influence count hints")
    for (corpus, value), count in sorted(header_counts.items()):
        print(f"{corpus:13} value={value} objects={count:4}")
    print("\npalette node storage")
    for (corpus, storage), count in sorted(storage_counts.items()):
        print(f"{corpus:13} {storage:10} {count:6}")
    print(
        f"matrix totals: slots={total_slots}, affine={affine}, "
        f"invertible={invertible}"
    )

    print("\nrecorded evidence")
    for kind, observation in connection.execute(
        """
        SELECT evidence_kind,observation
        FROM evidence
        WHERE type_hash=? AND evidence_kind LIKE 'class_analysis:%'
        ORDER BY evidence_kind
        """,
        (SKIN,),
    ):
        print(f"{kind}: {observation}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
