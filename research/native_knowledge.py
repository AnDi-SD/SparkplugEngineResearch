#!/usr/bin/env python3
"""Persist native class evidence and coverage beside the SMO corpus index."""

from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DATABASE = ROOT / "local-data" / "results" / "smo-corpus-v2.sqlite"
DEFAULT_PC = ROOT / "local-data" / "pc-pristine" / "WinxClub.exe"
DEFAULT_PS2 = ROOT / "local-data" / "Winx Club the game PS2" / "SLES_532.19"
DEFAULT_MANIFEST = ROOT / "research" / "native-research-baseline.json"
ARCHITECTURE_SCANNER = ROOT / "research" / "inspect_executable_architecture.py"


DDL = """
PRAGMA foreign_keys=ON;

CREATE TABLE IF NOT EXISTS native_types (
    id INTEGER PRIMARY KEY,
    type_hash INTEGER NOT NULL,
    class_name TEXT NOT NULL UNIQUE,
    owner_scope TEXT NOT NULL CHECK(owner_scope IN ('engine','game')),
    on_pc INTEGER NOT NULL DEFAULT 0 CHECK(on_pc IN (0,1)),
    on_ps2 INTEGER NOT NULL DEFAULT 0 CHECK(on_ps2 IN (0,1)),
    pc_base_hash INTEGER,
    ps2_base_hash INTEGER,
    pc_registration_locator TEXT,
    ps2_registration_locator TEXT,
    updated_utc TEXT NOT NULL,
    CHECK(on_pc=1 OR on_ps2=1)
);

CREATE TABLE IF NOT EXISTS native_type_scopes (
    native_type_id INTEGER NOT NULL REFERENCES native_types(id) ON DELETE CASCADE,
    scope_key TEXT NOT NULL CHECK(scope_key IN ('engine','game','smo','san','smo_san')),
    object_count INTEGER,
    resource_count INTEGER,
    evidence_status TEXT NOT NULL,
    provenance TEXT NOT NULL,
    updated_utc TEXT NOT NULL,
    PRIMARY KEY(native_type_id,scope_key)
);

CREATE TABLE IF NOT EXISTS native_research_progress (
    native_type_id INTEGER PRIMARY KEY REFERENCES native_types(id) ON DELETE CASCADE,
    research_status TEXT NOT NULL CHECK(research_status IN
        ('not_started','identified','scouted','partial','substantial','closed')),
    pc_status TEXT NOT NULL CHECK(pc_status IN
        ('not_started','deferred','identified','scouted','partial','substantial','closed')),
    ps2_status TEXT NOT NULL CHECK(ps2_status IN
        ('not_started','deferred','identified','scouted','partial','substantial','closed')),
    coverage_score REAL NOT NULL CHECK(coverage_score BETWEEN 0.0 AND 100.0),
    lower_bound REAL NOT NULL CHECK(lower_bound BETWEEN 0.0 AND coverage_score),
    upper_bound REAL NOT NULL CHECK(upper_bound BETWEEN coverage_score AND 100.0),
    priority_tier INTEGER NOT NULL DEFAULT 3 CHECK(priority_tier BETWEEN 0 AND 9),
    summary TEXT NOT NULL,
    updated_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS native_research_evidence (
    id INTEGER PRIMARY KEY,
    native_type_id INTEGER REFERENCES native_types(id) ON DELETE CASCADE,
    platform_key TEXT NOT NULL CHECK(platform_key IN ('common','pc','ps2')),
    evidence_kind TEXT NOT NULL,
    source_path TEXT NOT NULL,
    locator TEXT NOT NULL DEFAULT '',
    observation TEXT NOT NULL,
    confidence TEXT NOT NULL CHECK(confidence IN ('confirmed','probable','hypothesis')),
    source_sha256 TEXT,
    created_utc TEXT NOT NULL,
    UNIQUE(native_type_id,platform_key,evidence_kind,source_path,locator)
);

CREATE TABLE IF NOT EXISTS native_research_imports (
    manifest_id TEXT PRIMARY KEY,
    source_path TEXT NOT NULL,
    source_sha256 TEXT NOT NULL,
    import_mode TEXT NOT NULL CHECK(import_mode IN ('baseline','incremental')),
    imported_utc TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS native_coverage_snapshots (
    id INTEGER PRIMARY KEY,
    scope_key TEXT NOT NULL CHECK(scope_key IN ('all','engine','game','smo_san')),
    numerator REAL NOT NULL,
    denominator REAL NOT NULL CHECK(denominator > 0.0),
    coverage_percent REAL NOT NULL CHECK(coverage_percent BETWEEN 0.0 AND 100.0),
    lower_bound REAL NOT NULL CHECK(lower_bound BETWEEN 0.0 AND coverage_percent),
    upper_bound REAL NOT NULL CHECK(upper_bound BETWEEN coverage_percent AND 100.0),
    calculation_method TEXT NOT NULL,
    notes TEXT NOT NULL,
    created_utc TEXT NOT NULL,
    UNIQUE(scope_key,created_utc)
);

CREATE INDEX IF NOT EXISTS ix_native_types_scope
    ON native_types(owner_scope,on_pc,on_ps2);
CREATE INDEX IF NOT EXISTS ix_native_type_scopes_key
    ON native_type_scopes(scope_key,native_type_id);
CREATE INDEX IF NOT EXISTS ix_native_progress_priority
    ON native_research_progress(priority_tier,coverage_score);
CREATE INDEX IF NOT EXISTS ix_native_evidence_type
    ON native_research_evidence(native_type_id,platform_key,evidence_kind);
CREATE INDEX IF NOT EXISTS ix_native_coverage_scope
    ON native_coverage_snapshots(scope_key,created_utc);

CREATE VIEW IF NOT EXISTS latest_native_coverage AS
SELECT snapshot.*
FROM native_coverage_snapshots snapshot
JOIN (
    SELECT scope_key,MAX(created_utc) AS created_utc
    FROM native_coverage_snapshots GROUP BY scope_key
) latest ON latest.scope_key=snapshot.scope_key
         AND latest.created_utc=snapshot.created_utc;

CREATE VIEW IF NOT EXISTS native_smo_san_progress AS
SELECT type.class_name,type.type_hash,scope.scope_key,
       scope.object_count,scope.resource_count,
       COALESCE(progress.research_status,'not_started') AS research_status,
       COALESCE(progress.pc_status,'not_started') AS pc_status,
       COALESCE(progress.ps2_status,'deferred') AS ps2_status,
       COALESCE(progress.coverage_score,0.0) AS coverage_score,
       COALESCE(progress.lower_bound,0.0) AS lower_bound,
       COALESCE(progress.upper_bound,0.0) AS upper_bound,
       COALESCE(progress.priority_tier,9) AS priority_tier,
       COALESCE(progress.summary,'No native research recorded.') AS summary
FROM native_type_scopes scope
JOIN native_types type ON type.id=scope.native_type_id
LEFT JOIN native_research_progress progress ON progress.native_type_id=type.id
WHERE scope.scope_key IN ('smo','san');
"""


def connect(path: Path) -> sqlite3.Connection:
    connection = sqlite3.connect(path)
    connection.row_factory = sqlite3.Row
    connection.execute("PRAGMA foreign_keys=ON")
    return connection


def ensure_schema(connection: sqlite3.Connection) -> None:
    row = connection.execute(
        "SELECT value FROM schema_info WHERE key='schema_version'").fetchone()
    if row is None or row[0] not in ("4", "5"):
        raise RuntimeError(f"Expected research schema 4 or 5, got {row[0] if row else None}")
    connection.executescript(DDL)
    connection.execute(
        "UPDATE schema_info SET value='5' WHERE key='schema_version'")


def architecture(pc: Path, ps2: Path) -> dict[str, Any]:
    result = subprocess.run(
        [sys.executable, "-B", str(ARCHITECTURE_SCANNER), str(pc), str(ps2), "--json"],
        check=True, capture_output=True, text=True)
    return json.loads(result.stdout)


def upsert_catalog(
    connection: sqlite3.Connection, catalog: dict[str, Any], updated_utc: str
) -> tuple[int, int, int]:
    merged: dict[str, dict[str, Any]] = {}
    executable_hashes: dict[str, str] = {}
    for executable in catalog["executables"]:
        platform = "pc" if executable["format"] == "PE" else "ps2"
        executable_hashes[platform] = executable["sha256"]
        for item in executable["registered_types"]:
            type_hash = int(item["class_hash"])
            class_name = item["class_name"]
            entry = merged.setdefault(class_name, {
                "type_hash": type_hash, "class_name": class_name,
                "on_pc": 0, "on_ps2": 0,
                "pc_base_hash": None, "ps2_base_hash": None,
                "pc_registration_locator": None, "ps2_registration_locator": None,
            })
            if entry["type_hash"] != type_hash:
                raise RuntimeError(f"Class name {class_name} has inconsistent hashes")
            entry[f"on_{platform}"] = 1
            entry[f"{platform}_base_hash"] = int(item["base_class_hash"])
            address_key = "registration_rva" if platform == "pc" else "registration_va"
            prefix = "RVA" if platform == "pc" else "VA"
            entry[f"{platform}_registration_locator"] = (
                f"{prefix} 0x{int(item[address_key]):08X}")

    for name, item in merged.items():
        type_hash = item["type_hash"]
        owner = "engine" if name.startswith("sp") else "game"
        connection.execute("""
            INSERT INTO native_types(
                type_hash,class_name,owner_scope,on_pc,on_ps2,
                pc_base_hash,ps2_base_hash,pc_registration_locator,
                ps2_registration_locator,updated_utc)
            VALUES(?,?,?,?,?,?,?,?,?,?)
            ON CONFLICT(class_name) DO UPDATE SET
                type_hash=excluded.type_hash,owner_scope=excluded.owner_scope,
                on_pc=excluded.on_pc,on_ps2=excluded.on_ps2,
                pc_base_hash=excluded.pc_base_hash,ps2_base_hash=excluded.ps2_base_hash,
                pc_registration_locator=excluded.pc_registration_locator,
                ps2_registration_locator=excluded.ps2_registration_locator,
                updated_utc=excluded.updated_utc
            """, (type_hash, name, owner, item["on_pc"], item["on_ps2"],
                  item["pc_base_hash"], item["ps2_base_hash"],
                  item["pc_registration_locator"], item["ps2_registration_locator"],
                  updated_utc))
        native_type_id = connection.execute(
            "SELECT id FROM native_types WHERE class_name=?", (name,)).fetchone()[0]
        connection.execute("""
            INSERT INTO native_type_scopes(
                native_type_id,scope_key,evidence_status,provenance,updated_utc)
            VALUES(?,?,?,?,?) ON CONFLICT(native_type_id,scope_key) DO UPDATE SET
                evidence_status=excluded.evidence_status,
                provenance=excluded.provenance,updated_utc=excluded.updated_utc
            """, (native_type_id, owner, "confirmed_registration",
                  "PC/PS2 registration graph", updated_utc))
        for platform in ("pc", "ps2"):
            if not item[f"on_{platform}"]:
                continue
            base_hash = item[f"{platform}_base_hash"]
            observation = (
                f"{name} has class ID 0x{type_hash:08X} and direct base ID "
                f"0x{base_hash:08X} in the {platform.upper()} registration graph.")
            connection.execute("""
                INSERT INTO native_research_evidence(
                    native_type_id,platform_key,evidence_kind,source_path,locator,
                    observation,confidence,source_sha256,created_utc)
                VALUES(?,?,?,?,?,?,?,?,?)
                ON CONFLICT(native_type_id,platform_key,evidence_kind,source_path,locator)
                DO UPDATE SET observation=excluded.observation,
                    confidence=excluded.confidence,
                    source_sha256=excluded.source_sha256
                """, (native_type_id, platform, "class_registration",
                      str(DEFAULT_PC.relative_to(ROOT) if platform == "pc" else
                          DEFAULT_PS2.relative_to(ROOT)),
                      item[f"{platform}_registration_locator"], observation,
                      "confirmed", executable_hashes[platform], updated_utc))

    engine = sum(item["class_name"].startswith("sp") for item in merged.values())
    return len(merged), engine, len(merged) - engine


def sync_asset_scopes(connection: sqlite3.Connection, updated_utc: str) -> int:
    rows = connection.execute("""
        SELECT o.type_hash,c.engine_name,COUNT(*) AS objects,
               COUNT(DISTINCT o.file_id) AS resources
        FROM objects o JOIN classes c ON c.type_hash=o.type_hash
        GROUP BY o.type_hash,c.engine_name
        """).fetchall()
    for row in rows:
        native = connection.execute(
            "SELECT id,type_hash FROM native_types WHERE class_name=?",
            (row["engine_name"],)).fetchone()
        if native is None or int(native["type_hash"]) != int(row["type_hash"]):
            raise RuntimeError(
                f"SMO class {row['engine_name']} does not match native catalog")
        native_type_id = int(native["id"])
        connection.execute("""
            INSERT INTO native_type_scopes(
                native_type_id,scope_key,object_count,resource_count,
                evidence_status,provenance,updated_utc)
            VALUES(?,?,?,?,?,?,?) ON CONFLICT(native_type_id,scope_key) DO UPDATE SET
                object_count=excluded.object_count,
                resource_count=excluded.resource_count,
                evidence_status=excluded.evidence_status,
                provenance=excluded.provenance,updated_utc=excluded.updated_utc
            """, (native_type_id, "smo", row["objects"], row["resources"],
                  "confirmed_corpus", "objects table across indexed SMO corpora",
                  updated_utc))
        connection.execute("""
            INSERT INTO native_type_scopes(
                native_type_id,scope_key,object_count,resource_count,
                evidence_status,provenance,updated_utc)
            VALUES(?,?,?,?,?,?,?) ON CONFLICT(native_type_id,scope_key) DO UPDATE SET
                object_count=excluded.object_count,
                resource_count=excluded.resource_count,
                evidence_status=excluded.evidence_status,
                provenance=excluded.provenance,updated_utc=excluded.updated_utc
            """, (native_type_id, "smo_san", row["objects"], row["resources"],
                  "confirmed_corpus", "direct SMO/SAN asset scope", updated_utc))

    animation = connection.execute(
        "SELECT id FROM native_types WHERE class_name='spAnimation'").fetchone()
    if animation is None:
        raise RuntimeError("spAnimation is absent from native registration catalog")
    for scope in ("san", "smo_san"):
        connection.execute("""
            INSERT INTO native_type_scopes(
                native_type_id,scope_key,evidence_status,provenance,updated_utc)
            VALUES(?,?,?,?,?) ON CONFLICT(native_type_id,scope_key) DO UPDATE SET
                evidence_status=excluded.evidence_status,
                provenance=excluded.provenance,updated_utc=excluded.updated_utc
            """, (animation[0], scope, "confirmed_corpus",
                  "spAnimation object observed in SAN FFPS corpus", updated_utc))
    return len(rows) + 1


def default_progress(connection: sqlite3.Connection, updated_utc: str) -> None:
    connection.execute("""
        INSERT INTO native_research_progress(
            native_type_id,research_status,pc_status,ps2_status,coverage_score,
            lower_bound,upper_bound,priority_tier,summary,updated_utc)
        SELECT scope.native_type_id,'partial','identified','deferred',25.0,20.0,30.0,5,
               'Observed wire layout is decoded; native runtime behavior is not yet directly reconstructed.',?
        FROM native_type_scopes scope
        WHERE scope.scope_key='smo'
        ON CONFLICT(native_type_id) DO NOTHING
        """, (updated_utc,))
    animation = connection.execute(
        "SELECT id FROM native_types WHERE class_name='spAnimation'").fetchone()
    connection.execute("""
        INSERT INTO native_research_progress(
            native_type_id,research_status,pc_status,ps2_status,coverage_score,
            lower_bound,upper_bound,priority_tier,summary,updated_utc)
        VALUES(?,'partial','partial','deferred',55.0,48.0,62.0,0,
               'PC SAN timing, interpolation and name binding are substantial; PS2 playback is deferred.',?)
        ON CONFLICT(native_type_id) DO NOTHING
        """, (animation[0], updated_utc))
    priorities = {
        0: ("spSkin", "spAnimation"),
        1: ("spUVController", "spAnimTexController",
            "spMaterialColorController", "spModel", "spMeshData",
            "spMaterialData", "spTextureData"),
        2: ("spStaticRenderObject",),
        3: ("spParticleSystem", "spSkyBox", "spLensFlare",
            "spTextNode", "spTextRenderable", "spFont"),
        4: ("spPartitionSystem", "spPartitionNode", "spPartitionRenderable",
            "spOctreeNode", "spBSPNode", "spZone", "spZonePortal",
            "spZonePortalNode", "spOcclusionVolume", "spNavigationGraph",
            "spMeshNavigationSet", "spNavigationPortal"),
        5: ("spCollisionInfo", "spMeshBV", "spOBBBV", "spBoxBV",
            "spSphereBV", "spFog", "spLightData", "spRenderNode", "spNode"),
    }
    for tier, names in priorities.items():
        placeholders = ",".join("?" for _ in names)
        connection.execute(
            f"UPDATE native_research_progress SET priority_tier=? "
            f"WHERE native_type_id IN (SELECT id FROM native_types "
            f"WHERE class_name IN ({placeholders}))",
            (tier, *names))


def apply_manifest(
    connection: sqlite3.Connection, path: Path
) -> tuple[int, bool]:
    raw = path.read_bytes()
    manifest = json.loads(raw)
    manifest_id = manifest["manifestId"]
    digest = hashlib.sha256(raw).hexdigest().upper()
    previous = connection.execute(
        "SELECT source_sha256 FROM native_research_imports WHERE manifest_id=?",
        (manifest_id,)).fetchone()
    if previous is not None:
        if previous[0] != digest:
            raise RuntimeError(
                f"Manifest {manifest_id} was already imported with a different hash")
        return 0, False

    updated_utc = manifest["createdUtc"]
    changed: list[tuple[int, float, float]] = []
    for record in manifest["classes"]:
        row = connection.execute(
            "SELECT id FROM native_types WHERE class_name=?",
            (record["className"],)).fetchone()
        if row is None:
            raise RuntimeError(f"Unknown native class {record['className']}")
        native_type_id = int(row[0])
        old = connection.execute(
            "SELECT coverage_score FROM native_research_progress WHERE native_type_id=?",
            (native_type_id,)).fetchone()
        old_score = float(old[0]) if old else 0.0
        score = float(record["coverageScore"])
        connection.execute("""
            INSERT INTO native_research_progress(
                native_type_id,research_status,pc_status,ps2_status,coverage_score,
                lower_bound,upper_bound,priority_tier,summary,updated_utc)
            VALUES(?,?,?,?,?,?,?,?,?,?)
            ON CONFLICT(native_type_id) DO UPDATE SET
                research_status=excluded.research_status,
                pc_status=excluded.pc_status,ps2_status=excluded.ps2_status,
                coverage_score=excluded.coverage_score,
                lower_bound=excluded.lower_bound,upper_bound=excluded.upper_bound,
                priority_tier=excluded.priority_tier,summary=excluded.summary,
                updated_utc=excluded.updated_utc
            """, (native_type_id, record["researchStatus"], record["pcStatus"],
                  record["ps2Status"], score, record["lowerBound"],
                  record["upperBound"], record["priorityTier"],
                  record["summary"], updated_utc))
        changed.append((native_type_id, old_score, score))
        for evidence in record.get("evidence", []):
            connection.execute("""
                INSERT INTO native_research_evidence(
                    native_type_id,platform_key,evidence_kind,source_path,locator,
                    observation,confidence,source_sha256,created_utc)
                VALUES(?,?,?,?,?,?,?,?,?)
                ON CONFLICT(native_type_id,platform_key,evidence_kind,source_path,locator)
                DO UPDATE SET observation=excluded.observation,
                    confidence=excluded.confidence,
                    source_sha256=excluded.source_sha256,
                    created_utc=excluded.created_utc
                """, (native_type_id, evidence["platformKey"], evidence["evidenceKind"],
                      evidence["sourcePath"], evidence.get("locator", ""),
                      evidence["observation"], evidence["confidence"],
                      evidence.get("sourceSha256"), updated_utc))

    if manifest["mode"] == "baseline":
        for snapshot in manifest["coverageSnapshots"]:
            connection.execute("""
                INSERT INTO native_coverage_snapshots(
                    scope_key,numerator,denominator,coverage_percent,
                    lower_bound,upper_bound,calculation_method,notes,created_utc)
                VALUES(?,?,?,?,?,?,?,?,?)
                """, (snapshot["scopeKey"], snapshot["numerator"],
                      snapshot["denominator"], snapshot["coveragePercent"],
                      snapshot["lowerBound"], snapshot["upperBound"],
                      snapshot["calculationMethod"], snapshot["notes"], updated_utc))
    else:
        advance_snapshots(connection, changed, updated_utc, manifest.get("notes", ""))

    connection.execute("""
        INSERT INTO native_research_imports(
            manifest_id,source_path,source_sha256,import_mode,imported_utc)
        VALUES(?,?,?,?,?)
        """, (manifest_id, str(path), digest, manifest["mode"], updated_utc))
    return len(changed), True


def advance_snapshots(
    connection: sqlite3.Connection,
    changed: list[tuple[int, float, float]],
    created_utc: str,
    notes: str,
) -> None:
    deltas = {scope: 0.0 for scope in ("all", "engine", "game", "smo_san")}
    for native_type_id, old, new in changed:
        delta = (new - old) / 100.0
        deltas["all"] += delta
        owner = connection.execute(
            "SELECT owner_scope FROM native_types WHERE id=?", (native_type_id,)
        ).fetchone()[0]
        deltas[owner] += delta
        in_assets = connection.execute("""
            SELECT 1 FROM native_type_scopes
            WHERE native_type_id=? AND scope_key='smo_san'
            """, (native_type_id,)).fetchone()
        if in_assets:
            deltas["smo_san"] += delta

    for scope, delta in deltas.items():
        previous = connection.execute("""
            SELECT * FROM latest_native_coverage WHERE scope_key=?
            """, (scope,)).fetchone()
        if previous is None:
            raise RuntimeError(f"No baseline coverage snapshot for {scope}")
        numerator = float(previous["numerator"]) + delta
        denominator = float(previous["denominator"])
        percent = numerator / denominator * 100.0
        bound_delta = percent - float(previous["coverage_percent"])
        connection.execute("""
            INSERT INTO native_coverage_snapshots(
                scope_key,numerator,denominator,coverage_percent,
                lower_bound,upper_bound,calculation_method,notes,created_utc)
            VALUES(?,?,?,?,?,?,?,?,?)
            """, (scope, numerator, denominator, percent,
                  max(0.0, float(previous["lower_bound"]) + bound_delta),
                  min(100.0, float(previous["upper_bound"]) + bound_delta),
                  "incremental class-score delta from audited baseline", notes,
                  created_utc))


def report(connection: sqlite3.Connection, as_json: bool) -> None:
    coverage = [dict(row) for row in connection.execute("""
        SELECT scope_key,numerator,denominator,coverage_percent,
               lower_bound,upper_bound,calculation_method,created_utc
        FROM latest_native_coverage
        ORDER BY CASE scope_key WHEN 'all' THEN 0 WHEN 'engine' THEN 1
                 WHEN 'game' THEN 2 ELSE 3 END
        """)]
    queue = [dict(row) for row in connection.execute("""
        SELECT class_name,scope_key,object_count,resource_count,research_status,
               pc_status,ps2_status,coverage_score,lower_bound,upper_bound,
               priority_tier,summary
        FROM native_smo_san_progress
        ORDER BY priority_tier,coverage_score,
                 COALESCE(object_count,0) DESC,class_name
        """)]
    counts = dict(connection.execute("""
        SELECT COUNT(*) AS total,
               SUM(owner_scope='engine') AS engine,
               SUM(owner_scope='game') AS game,
               SUM(on_pc=1) AS pc,SUM(on_ps2=1) AS ps2
        FROM native_types
        """).fetchone())
    result = {"nativeTypes": counts, "coverage": coverage, "assetQueue": queue}
    if as_json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return
    print("Native catalog: total={total} engine={engine} game={game} pc={pc} ps2={ps2}".format(**counts))
    for item in coverage:
        print(f"{item['scope_key']:7} {item['coverage_percent']:6.2f}% "
              f"[{item['lower_bound']:.2f}, {item['upper_bound']:.2f}] "
              f"units={item['numerator']:.3f}/{item['denominator']:.0f}")
    print("PC-first SMO/SAN queue:")
    for item in queue:
        print(f"  P{item['priority_tier']} {item['class_name']:<30} "
              f"{item['coverage_score']:5.1f}% pc={item['pc_status']:<11} "
              f"ps2={item['ps2_status']:<11} objects={item['object_count'] or 0}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    sync = subparsers.add_parser("sync", help="sync catalog, scopes and a manifest")
    sync.add_argument("--database", type=Path, default=DEFAULT_DATABASE)
    sync.add_argument("--pc", type=Path, default=DEFAULT_PC)
    sync.add_argument("--ps2", type=Path, default=DEFAULT_PS2)
    sync.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    import_manifest = subparsers.add_parser(
        "import-manifest",
        help="import new research evidence without rescanning either executable",
    )
    import_manifest.add_argument("--database", type=Path, default=DEFAULT_DATABASE)
    import_manifest.add_argument("--manifest", type=Path, required=True)
    show = subparsers.add_parser("report", help="show stored coverage and PC-first queue")
    show.add_argument("--database", type=Path, default=DEFAULT_DATABASE)
    show.add_argument("--json", action="store_true")
    args = parser.parse_args()

    with connect(args.database.resolve()) as connection:
        if args.command == "sync":
            ensure_schema(connection)
            manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
            catalog_counts = upsert_catalog(
                connection, architecture(args.pc.resolve(), args.ps2.resolve()),
                manifest["createdUtc"])
            asset_count = sync_asset_scopes(connection, manifest["createdUtc"])
            default_progress(connection, manifest["createdUtc"])
            changed, imported = apply_manifest(connection, args.manifest.resolve())
            connection.commit()
            print(f"SYNC OK catalog={catalog_counts[0]} engine={catalog_counts[1]} "
                  f"game={catalog_counts[2]} assets={asset_count} "
                  f"progress={changed} imported={imported}")
        elif args.command == "import-manifest":
            ensure_schema(connection)
            changed, imported = apply_manifest(connection, args.manifest.resolve())
            connection.commit()
            print(f"IMPORT OK progress={changed} imported={imported} executable_scan=False")
        report(connection, getattr(args, "json", False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
