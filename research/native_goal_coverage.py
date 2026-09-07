#!/usr/bin/env python3
"""Versioned SMO/SAN workflow accounting. Reads the ledger, never binaries/assets.

Class-weighted estimates and mandatory completion gates are deliberately separate.
An unregistered helper is a gate obligation, not an invented RTTI class or score.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import sqlite3

from native_platform_knowledge import DATABASE, ROOT, coverage, checked_path

DEFAULT_MANIFEST = ROOT / 'research/native-goal-smo-san-scope-v2.json'
REQUIRED_PC_GATES = {'inventory', 'read', 'write', 'processing', 'lifetime', 'reproducibility', 'display'}
DDL = '''
CREATE TABLE IF NOT EXISTS native_goal_scopes(
 scope_id TEXT PRIMARY KEY, source_path TEXT NOT NULL, source_sha256 TEXT NOT NULL,
 created_utc TEXT NOT NULL, manifest_json TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS native_goal_members(
 scope_id TEXT NOT NULL REFERENCES native_goal_scopes(scope_id),
 native_type_id INTEGER NOT NULL REFERENCES native_types(id), group_id TEXT NOT NULL,
 PRIMARY KEY(scope_id,native_type_id));
CREATE TABLE IF NOT EXISTS native_goal_snapshots(
 id INTEGER PRIMARY KEY, scope_id TEXT NOT NULL REFERENCES native_goal_scopes(scope_id),
 assessment_sha256 TEXT NOT NULL, recorded_utc TEXT NOT NULL, report_json TEXT NOT NULL,
 UNIQUE(scope_id,assessment_sha256));
'''


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False,
                                    separators=(',', ':')).encode()).hexdigest().upper()


def validate_scope(db, manifest):
    if manifest.get('kind') != 'native-goal-scope' or type(manifest.get('schemaVersion')) is not int or manifest['schemaVersion'] != 1:
        raise ValueError('Unsupported goal scope schema')
    if not isinstance(manifest.get('scopeId'), str) or not manifest['scopeId'].strip():
        raise ValueError('Nonempty scopeId required')
    stamp = manifest.get('createdUtc', '')
    if datetime.strptime(stamp, '%Y-%m-%dT%H:%M:%SZ').strftime('%Y-%m-%dT%H:%M:%SZ') != stamp:
        raise ValueError('Canonical UTC required')
    groups = manifest.get('groups', [])
    if not groups:
        raise ValueError('Nonempty groups required')
    names, ids, prepared = set(), set(), []
    for group in groups:
        if group['id'] in ids or not group.get('classes') or not group.get('evidencePaths') or not group.get('basis'):
            raise ValueError('Duplicate/empty group or missing basis/evidence')
        ids.add(group['id'])
        for path in group['evidencePaths']:
            checked_path(path)
        for name in group['classes']:
            if name in names:
                raise ValueError('Class belongs to multiple groups: ' + name)
            names.add(name)
            row = db.execute('SELECT id FROM native_types WHERE class_name=?', (name,)).fetchone()
            if not row:
                raise ValueError('Unknown native class: ' + name)
            prepared.append((row[0], group['id']))
    direct = {r[0] for r in db.execute("SELECT t.class_name FROM native_types t JOIN native_type_scopes s ON s.native_type_id=t.id WHERE s.scope_key='smo_san'")}
    if direct - names:
        raise ValueError('Missing observed SMO/SAN classes: ' + ', '.join(sorted(direct - names)))
    gates = manifest.get('gates', [])
    if not gates:
        raise ValueError('Completion gates required')
    gate_ids = set()
    for gate in gates:
        if gate['id'] in gate_ids or gate['platform'] not in ('pc', 'ps2') or gate['status'] not in ('open', 'partial', 'passed'):
            raise ValueError('Invalid/duplicate completion gate')
        gate_ids.add(gate['id'])
        if not gate.get('acceptance') or not gate.get('current') or not gate.get('evidencePaths'):
            raise ValueError('Gate needs acceptance, status evidence and current result')
        for path in gate['evidencePaths']:
            checked_path(path)
    if not REQUIRED_PC_GATES.issubset({g['id'] for g in gates if g['platform']=='pc'}):
        raise ValueError('All seven PC completion criteria must remain present')
    if not manifest.get('unregisteredObligations'):
        raise ValueError('Non-RTTI obligations must not disappear from the goal')
    for helper in manifest['unregisteredObligations']:
        if not helper.get('gates') or set(helper['gates']) - gate_ids:
            raise ValueError('Unknown helper gate')
        for path in helper['evidencePaths']:
            checked_path(path)
    return prepared


def import_scope(db, path):
    raw = path.read_bytes()
    manifest = json.loads(raw)
    prepared = validate_scope(db, manifest)
    sha = hashlib.sha256(raw).hexdigest().upper()
    prior = db.execute('SELECT source_sha256 FROM native_goal_scopes WHERE scope_id=?', (manifest['scopeId'],)).fetchone()
    if prior:
        if prior[0] != sha:
            raise ValueError('Imported goal scope is immutable; create a new version')
        return False
    latest = db.execute('SELECT MAX(created_utc) FROM native_goal_scopes').fetchone()[0]
    if latest and manifest['createdUtc'] <= latest:
        raise ValueError('Out-of-order scope version')
    if latest:
        old_members = {r[0] for r in db.execute('SELECT m.native_type_id FROM native_goal_members m JOIN native_goal_scopes s ON s.scope_id=m.scope_id WHERE s.created_utc=?', (latest,))}
        if old_members != {r[0] for r in prepared} and not manifest.get('scopeChangeReason', '').strip():
            raise ValueError('Scope membership change requires an explicit reason')
    db.execute('INSERT INTO native_goal_scopes VALUES(?,?,?,?,?)',
               (manifest['scopeId'], str(path), sha, manifest['createdUtc'], raw.decode('utf-8')))
    db.executemany('INSERT INTO native_goal_members VALUES(?,?,?)',
                   [(manifest['scopeId'], identity, group) for identity, group in prepared])
    return True


def aggregate(rows):
    rated = [r for r in rows if r['score'] is not None]
    size = len(rows)
    return dict(denominator=size, assessedCount=len(rated), unratedCount=size-len(rated),
                numerator=sum(r['score'] for r in rated)/100,
                creditedPercent=sum(r['score'] for r in rated)/size if size else None,
                recordedLowerPercent=sum(r['lower_bound'] for r in rated)/size if size else None,
                possibleUpperPercent=(sum(r['upper_bound'] for r in rated)+100*(size-len(rated)))/size if size else None,
                closedCount=sum(r['score'] == 100 and r['research_status'] == 'closed' for r in rated))


def report(db, scope_id=None, details=False):
    if scope_id is None:
        row = db.execute('SELECT scope_id FROM native_goal_scopes ORDER BY created_utc DESC LIMIT 1').fetchone()
        if not row:
            raise ValueError('No imported workflow scope')
        scope_id = row[0]
    row = db.execute('SELECT manifest_json,source_sha256 FROM native_goal_scopes WHERE scope_id=?', (scope_id,)).fetchone()
    if not row:
        raise ValueError('Unknown scope')
    manifest = json.loads(row[0])
    result = dict(scopeId=scope_id, scopeSha256=row[1], method='equal-weight-reviewed-native-classes-v1',
                  note=manifest['notes'], gates=manifest['gates'], unregisteredObligations=manifest['unregisteredObligations'], platforms={})
    all_assessments = [tuple(r) for r in db.execute('SELECT native_type_id,platform_key,score,lower_bound,upper_bound,research_status,unknowns_json,manifest_id,created_utc FROM native_platform_assessments ORDER BY native_type_id,platform_key')]
    result['assessmentLedgerSha256'] = digest(all_assessments)
    # Snapshot key must change for catalog/scope changes even without a new score.
    # The existing SQLite column is named assessment_sha256, but stores this
    # complete accounting-input fingerprint (not just the score ledger hash).
    catalog = [tuple(r) for r in db.execute('SELECT id,class_name,owner_scope,on_pc,on_ps2 FROM native_types ORDER BY id')]
    direct_scopes = [tuple(r) for r in db.execute("SELECT native_type_id,scope_key FROM native_type_scopes WHERE scope_key='smo_san' ORDER BY native_type_id")]
    result['assessmentSha256'] = digest([all_assessments, catalog, direct_scopes, result['scopeSha256']])
    result['evidenceAsOfUtc'] = db.execute('SELECT MAX(created_utc) FROM native_platform_assessments').fetchone()[0]
    for platform in ('pc', 'ps2'):
        rows = [dict(r) for r in db.execute('''SELECT t.class_name,t.owner_scope,m.group_id,a.score,a.lower_bound,a.upper_bound,a.research_status,a.unknowns_json,a.manifest_id
          FROM native_goal_members m JOIN native_types t ON t.id=m.native_type_id
          LEFT JOIN native_platform_assessments a ON a.native_type_id=t.id AND a.platform_key=?
          WHERE m.scope_id=? AND t.on_'''+platform+'=1 ORDER BY m.group_id,t.class_name', (platform, scope_id))]
        data = dict(catalog={s: coverage(db, platform, s) for s in ('all', 'engine', 'game', 'smo_san')}, workflow=aggregate(rows))
        data['groups'] = [dict(id=g['id'], title=g['title'], **aggregate([r for r in rows if r['group_id']==g['id']])) for g in manifest['groups']]
        gates = [g for g in manifest['gates'] if g['platform'] == platform]
        data['completion'] = dict(passed=sum(g['status']=='passed' for g in gates), total=len(gates),
                                  partial=sum(g['status']=='partial' for g in gates),
                                  ready=(bool(rows) and bool(gates) and all(g['status']=='passed' for g in gates)
                                         and data['workflow']['closedCount']==len(rows)))
        if details:
            data['classes'] = rows
        result['platforms'][platform] = data
    return result


def record(db, scope_id=None):
    value = report(db, scope_id, details=True)
    row = db.execute('SELECT id FROM native_goal_snapshots WHERE scope_id=? AND assessment_sha256=?',
                     (value['scopeId'], value['assessmentSha256'])).fetchone()
    if row:
        return row[0], False
    cursor = db.execute('INSERT INTO native_goal_snapshots(scope_id,assessment_sha256,recorded_utc,report_json) VALUES(?,?,?,?)',
                        (value['scopeId'], value['assessmentSha256'], datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),
                         json.dumps(value, ensure_ascii=False, sort_keys=True)))
    return cursor.lastrowid, True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('import', 'record', 'report'))
    parser.add_argument('--database', type=Path, default=DATABASE)
    parser.add_argument('--manifest', type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument('--scope')
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--details', action='store_true')
    args = parser.parse_args()
    uri = args.database.resolve().as_uri() + ('?mode=ro' if args.command=='report' else '?mode=rw')
    with sqlite3.connect(uri, uri=True) as db:
        db.row_factory = sqlite3.Row
        db.execute('PRAGMA foreign_keys=ON')
        if args.command != 'report':
            db.executescript(DDL)
        if args.command == 'import':
            import_scope(db, args.manifest.resolve())
        if args.command != 'report':
            record(db, args.scope)
        value = report(db, args.scope, args.details)
    if args.json:
        print(json.dumps(value, ensure_ascii=True, indent=2))
    else:
        print(value['scopeId'], 'evidence', value['evidenceAsOfUtc'])
        print('Class-weighted evidence estimate; NOT executable bytes, application readiness or remaining time.')
        for platform, data in value['platforms'].items():
            for name, item in data['catalog'].items():
                if item is None:
                    print(f'{platform:3} {name:12} N/A')
                    continue
                print(f"{platform:3} {name:12} {item['creditedPercent']:.2f}% assessed={item['assessedCount']}/{item['denominator']}")
            item = data['workflow']
            credit = f"{item['creditedPercent']:.2f}%" if item['creditedPercent'] is not None else 'N/A'
            print(f"{platform:3} WORKFLOW     {credit} assessed={item['assessedCount']}/{item['denominator']}, unclosed={item['denominator']-item['closedCount']}")
            gate = data['completion']
            print(f"    Mandatory gates={gate['passed']}/{gate['total']}, partial={gate['partial']}, ready={gate['ready']}")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
