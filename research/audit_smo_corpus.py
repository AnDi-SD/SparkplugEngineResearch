#!/usr/bin/env python3
"""Read-only completion audit for the PC/PS2 SMO research database."""

from __future__ import annotations

import argparse
import sqlite3
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class ExpectedClass:
    type_hash: int
    name: str
    objects: int
    analysis_variants: int


FINAL_CLASSES = (
    ExpectedClass(0x385662AA, "spNavigationPortal", 209, 3),
    ExpectedClass(0x188A161F, "spNavigationGraph", 80, 2),
    ExpectedClass(0x7A7124AF, "spSkyBox", 126, 3),
    ExpectedClass(0x5AFA1A4F, "spParticleSystem", 1745, 7),
    ExpectedClass(0x16FB0E47, "spAnimTexController", 28, 3),
    ExpectedClass(0x435370B5, "spLensFlare", 6, 1),
    ExpectedClass(0x4693490A, "spFont", 20, 2),
    ExpectedClass(0x19A745D7, "spTextRenderable", 20, 2),
    ExpectedClass(0x52E86EFE, "spTextNode", 20, 2),
)


def scalar(connection: sqlite3.Connection, sql: str, parameters=()) -> int:
    row = connection.execute(sql, parameters).fetchone()
    if row is None:
        raise RuntimeError("Audit query returned no row")
    return int(row[0])


def audit(database: Path) -> None:
    uri = database.resolve().as_uri() + "?mode=ro"
    connection = sqlite3.connect(uri, uri=True)
    failures: list[str] = []

    integrity = connection.execute("PRAGMA quick_check").fetchone()[0]
    if integrity != "ok":
        failures.append(f"SQLite quick_check: {integrity}")

    registered_class_count = scalar(connection, "SELECT COUNT(*) FROM classes")
    class_count = scalar(
        connection,
        "SELECT COUNT(DISTINCT type_hash) FROM objects",
    )
    known_count = scalar(
        connection,
        """
        SELECT COUNT(DISTINCT class.type_hash)
        FROM classes AS class
        JOIN objects AS object ON object.type_hash=class.type_hash
        WHERE class.is_known=1
        """,
    )
    decoded_count = scalar(
        connection,
        """
        SELECT COUNT(DISTINCT class.type_hash)
        FROM classes AS class
        JOIN objects AS object ON object.type_hash=class.type_hash
        WHERE class.decode_status='read_only_decode'
        """,
    )
    smo_files = scalar(
        connection,
        "SELECT COUNT(*) FROM files WHERE lower(extension)='.smo'",
    )
    failed_files = scalar(
        connection,
        """
        SELECT COUNT(*) FROM files
        WHERE lower(extension)='.smo' AND parse_status<>'ok'
        """,
    )
    field_errors = scalar(
        connection,
        "SELECT COUNT(*) FROM objects WHERE field_parse_error IS NOT NULL",
    )
    print(
        f"registered_classes={registered_class_count}; observed_classes={class_count}; "
        f"known_observed={known_count}; "
        f"read_only_decode={decoded_count}; failed_files={failed_files}; "
        f"smo_files={smo_files}; field_parse_errors={field_errors}"
    )
    if class_count != 36 or known_count != 36 or decoded_count != 36:
        failures.append(
            "Expected exactly 36 known classes with read_only_decode status"
        )
    if failed_files or field_errors:
        failures.append("Database contains parse or direct-field errors")

    for expected in FINAL_CLASSES:
        parameters = (expected.type_hash,)
        objects = scalar(
            connection, "SELECT COUNT(*) FROM objects WHERE type_hash=?", parameters
        )
        definitions = scalar(
            connection,
            "SELECT COUNT(*) FROM field_definitions WHERE type_hash=?",
            parameters,
        )
        variants = scalar(
            connection,
            """
            SELECT COUNT(*) FROM class_variants
            WHERE type_hash=? AND status='confirmed'
            """,
            parameters,
        )
        assignments = scalar(
            connection,
            """
            SELECT COUNT(*)
            FROM object_variant_assignments AS assignment
            JOIN class_variants AS variant ON variant.id=assignment.variant_id
            WHERE variant.type_hash=? AND assignment.confidence='confirmed'
            """,
            parameters,
        )
        evidence = scalar(
            connection,
            """
            SELECT COUNT(*) FROM evidence
            WHERE type_hash=? AND evidence_kind LIKE 'class_analysis:%'
            """,
            parameters,
        )
        annotations = scalar(
            connection,
            """
            SELECT COUNT(*)
            FROM direct_fields AS field
            JOIN objects AS object
              ON object.file_id=field.file_id
             AND object.object_index=field.object_index
            WHERE object.type_hash=? AND field.is_section_terminator=0
              AND field.semantic_key IS NOT NULL AND field.is_decoded=1
            """,
            parameters,
        )
        unannotated = scalar(
            connection,
            """
            SELECT COUNT(*)
            FROM direct_fields AS field
            JOIN objects AS object
              ON object.file_id=field.file_id
             AND object.object_index=field.object_index
            WHERE object.type_hash=? AND field.is_section_terminator=0
              AND (field.semantic_key IS NULL OR field.is_decoded<>1)
            """,
            parameters,
        )
        print(
            f"0x{expected.type_hash:08X} {expected.name}: objects={objects}; "
            f"definitions={definitions}; variants={variants}; "
            f"assignments={assignments}; evidence={evidence}; "
            f"annotations={annotations}; unannotated={unannotated}"
        )
        if objects != expected.objects:
            failures.append(
                f"{expected.name}: expected {expected.objects} objects, got {objects}"
            )
        if definitions == 0:
            failures.append(f"{expected.name}: no field definitions")
        if variants != expected.analysis_variants:
            failures.append(
                f"{expected.name}: expected {expected.analysis_variants} confirmed "
                f"variants, got {variants}"
            )
        if assignments != objects:
            failures.append(
                f"{expected.name}: {assignments} assignments for {objects} objects"
            )
        if evidence != 4:
            failures.append(f"{expected.name}: expected 4 analysis evidence rows")
        if unannotated:
            failures.append(
                f"{expected.name}: {unannotated} content fields are not decoded"
            )

    connection.close()
    if failures:
        raise SystemExit("AUDIT FAILED\n- " + "\n- ".join(failures))
    print("AUDIT PASS")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    arguments = parser.parse_args()
    if not arguments.database.is_file():
        parser.error(f"database does not exist: {arguments.database}")
    audit(arguments.database)


if __name__ == "__main__":
    main()
