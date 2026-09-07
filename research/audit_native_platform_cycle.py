#!/usr/bin/env python3
"""Read-only platform ledger replay, provenance and bounded native-table FK audit."""
import hashlib
import json
import math
from pathlib import Path
import re
import sqlite3
from native_platform_knowledge import ROOT, DATABASE, coverage

CYCLE_START = '2026-09-06T07:24:40Z'
CYCLE_END = '2026-09-06T09:00:00Z'


def main():
    db = sqlite3.connect(DATABASE.as_uri() + '?mode=ro', uri=True)
    db.row_factory = sqlite3.Row
    catalog = {row['class_name']: dict(row) for row in db.execute('SELECT * FROM native_types')}
    history = []; hashes = {}; scores = {}; research_delta = {'pc': 0., 'ps2': 0.}
    def fingerprint(path):
        path = Path(path)
        if not path.is_absolute():
            path = ROOT / path
        path = path.resolve()
        assert path.is_relative_to(ROOT) and path.is_file(), path
        if path not in hashes:
            hashes[path] = hashlib.sha256(path.read_bytes()).hexdigest().upper()
        return hashes[path]
    for row in db.execute('SELECT * FROM native_platform_imports ORDER BY created_utc'):
        path = Path(row['source_path'])
        assert fingerprint(path) == row['source_sha256'], path
        manifest = json.loads(path.read_text(encoding='utf-8'))
        assert (manifest['manifestId'], manifest['createdUtc']) == (row['manifest_id'], row['created_utc'])
        history.append(manifest)
        for record in manifest['assessments']:
            key = (record['className'], record['platformKey'])
            assert catalog[key[0]]['on_' + key[1]] == 1, key
            if record['assessmentOrigin'] == 'new_research' and CYCLE_START <= manifest['createdUtc'] < CYCLE_END:
                # This pilot only advances previously independently rated classes.
                assert key in scores, ('unknown baseline for new research', key)
                research_delta[key[1]] += (record['coverageScore'] - scores[key]['coverageScore']) / 100
            scores[key] = record
            for evidence in record['evidenceRefs']:
                assert evidence['platformKey'] == key[1]
                path = (ROOT / evidence['sourcePath']).resolve()
                assert path.is_relative_to(ROOT) and path.is_file(), path
                if evidence.get('sourceSha256'):
                    assert fingerprint(path) == evidence['sourceSha256'].upper(), path
    assert len(scores) == db.execute('SELECT COUNT(*) FROM native_platform_assessments').fetchone()[0]
    for (name, platform), record in scores.items():
        row = db.execute('SELECT * FROM native_platform_assessments WHERE native_type_id=? AND platform_key=?',
                         (catalog[name]['id'], platform)).fetchone()
        assert (row['score'], row['lower_bound'], row['upper_bound'], row['summary']) == (
            record['coverageScore'], record['lowerBound'], record['upperBound'], record['summary']), (name, platform)
        assert json.loads(row['evidence_json']) == record['evidenceRefs']
    direct = {r[0] for r in db.execute("SELECT native_type_id FROM native_type_scopes WHERE scope_key='smo_san'")}
    for row in db.execute('SELECT * FROM latest_native_platform_coverage ORDER BY id'):
        platform, scope = row['platform_key'], row['scope_key']
        names = [name for name, item in catalog.items() if item['on_' + platform] and
                 (scope == 'all' or scope == item['owner_scope'] or scope == 'smo_san' and item['id'] in direct)]
        rated = [scores[(name, platform)] for name in names if (name, platform) in scores]
        assert row['denominator'] == len(names)
        assert row['assessed_count'] == len(rated) and row['unrated_count'] == len(names) - len(rated)
        assert math.isclose(row['credited_percent'], sum(r['coverageScore'] for r in rated) / len(names), abs_tol=1e-12)
        actual = coverage(db, platform, scope)
        assert math.isclose(actual['numerator'], row['numerator'], abs_tol=1e-12)
        print(f"{platform}/{scope}: {row['credited_percent']:.8f}% credited, assessed{row['assessed_count']}/{row['denominator']}, unrated{row['unrated_count']}")
    # Full corpus FK checks are deliberately avoided; clone only small native tables.
    memory = sqlite3.connect(':memory:')
    for obj in db.execute("SELECT name,sql FROM sqlite_master WHERE type='table' AND name LIKE 'native_%'"):
        memory.execute(obj['sql'])
        rows = db.execute('SELECT * FROM "' + obj['name'] + '"').fetchall()
        if rows:
            memory.executemany('INSERT INTO "' + obj['name'] + '" VALUES(' + ','.join('?' for _ in rows[0]) + ')', rows)
    assert memory.execute('PRAGMA foreign_key_check').fetchall() == []
    memory.close()
    assert db.execute("SELECT value FROM schema_info WHERE key='schema_version'").fetchone()[0] == '5'
    # Legacy row counts are reported, not fixed forever: later authorized cycles
    # may extend the historical mixed ledger without invalidating platform replay.
    print('Legacy imports/snapshots:', db.execute('SELECT COUNT(*) FROM native_research_imports').fetchone()[0],
          db.execute('SELECT COUNT(*) FROM native_coverage_snapshots').fetchone()[0])
    links = 0
    for name in ('docs/research/native-research-workbench.md', 'docs/research/native-pc-visibility-plane-storage.md',
                 'journal/2026/2026-09-06-pc-acceleration-until-1200.md', 'docs/README.md', 'Sparkplug/README.md'):
        path = ROOT / name
        for target in re.findall(r'\]\(([^\n)]+)\)', path.read_text(encoding='utf-8')):
            target = target.strip('<>').split('#', 1)[0]
            if not target or '://' in target:
                continue
            assert (path.parent / target).resolve().exists(), (name, target)
            links += 1
    print(f'PASS {len(history)} immutable platform imports; {len(scores)} latest independent assessments; native-only FK clean; {links} links')
    print('This cycle new research score-units ONLY (migration excluded):', research_delta)
    assert math.isclose(research_delta['pc'], .06, abs_tol=1e-12) and research_delta['ps2'] == 0
    db.close()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
