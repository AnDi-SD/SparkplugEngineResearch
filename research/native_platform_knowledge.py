#!/usr/bin/env python3
"""Independent PC/PS2 reviewed score ledger, no executable or asset scanning.

Unrated classes stay unrated. Credited percentage divides recorded scores by
the complete platform catalog; it is not a claim that unreviewed history is 0.
Legacy mixed-platform estimates remain intact and are not copied implicitly.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
import json
import math
from pathlib import Path
import sqlite3

ROOT=Path(__file__).resolve().parents[1]
DATABASE=ROOT/'local-data/results/smo-corpus-v2.sqlite'
PLATFORMS=('pc','ps2')
SCOPES=('all','engine','game','smo_san')
METHOD='explicit-platform-class-scores-v1'
DDL="""
CREATE TABLE IF NOT EXISTS native_platform_imports(
 manifest_id TEXT PRIMARY KEY,source_path TEXT NOT NULL,source_sha256 TEXT NOT NULL,
 created_utc TEXT NOT NULL,notes TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS native_platform_assessments(
 native_type_id INTEGER NOT NULL REFERENCES native_types(id),
 platform_key TEXT NOT NULL CHECK(platform_key IN ('pc','ps2')),
 score REAL NOT NULL CHECK(score BETWEEN 0 AND 100),
 lower_bound REAL NOT NULL CHECK(lower_bound BETWEEN 0 AND score),
 upper_bound REAL NOT NULL CHECK(upper_bound BETWEEN score AND 100),
 research_status TEXT NOT NULL,assessment_origin TEXT NOT NULL,
 summary TEXT NOT NULL,evidence_json TEXT NOT NULL,unknowns_json TEXT NOT NULL,
 manifest_id TEXT NOT NULL REFERENCES native_platform_imports(manifest_id),
 created_utc TEXT NOT NULL,PRIMARY KEY(native_type_id,platform_key));
CREATE TABLE IF NOT EXISTS native_platform_snapshots(
 id INTEGER PRIMARY KEY,platform_key TEXT NOT NULL CHECK(platform_key IN ('pc','ps2')),
 scope_key TEXT NOT NULL CHECK(scope_key IN ('all','engine','game','smo_san')),
 numerator REAL NOT NULL,denominator INTEGER NOT NULL CHECK(denominator>0),
 credited_percent REAL NOT NULL,assessed_count INTEGER NOT NULL,unrated_count INTEGER NOT NULL,
 recorded_lower_percent REAL NOT NULL,possible_upper_percent REAL NOT NULL,
 method TEXT NOT NULL,manifest_id TEXT NOT NULL REFERENCES native_platform_imports(manifest_id),
 created_utc TEXT NOT NULL,UNIQUE(platform_key,scope_key,created_utc));
CREATE VIEW IF NOT EXISTS latest_native_platform_coverage AS
 SELECT s.* FROM native_platform_snapshots s JOIN
 (SELECT platform_key,scope_key,MAX(created_utc) AS stamp FROM native_platform_snapshots GROUP BY platform_key,scope_key) x
 ON x.platform_key=s.platform_key AND x.scope_key=s.scope_key AND x.stamp=s.created_utc;
"""


def ensure_schema(db):
    # Additive companion schema; never rewrite corpus schema version/tables.
    db.executescript(DDL)


def checked_path(value):
    path=Path(value)
    if not path.is_absolute():path=ROOT/path
    path=path.resolve()
    if not path.is_relative_to(ROOT) or not path.is_file():raise ValueError(f'Invalid evidence path: {value}')
    return path


def coverage(db,platform,scope):
    if platform not in PLATFORMS or scope not in SCOPES:raise ValueError('Unknown platform/scope')
    condition=''
    if scope in ('engine','game'):condition=" AND t.owner_scope='"+scope+"'"
    elif scope=='smo_san':condition=" AND EXISTS(SELECT 1 FROM native_type_scopes s WHERE s.native_type_id=t.id AND s.scope_key='smo_san')"
    rows=db.execute('SELECT a.score,a.lower_bound,a.upper_bound FROM native_types t LEFT JOIN native_platform_assessments a ON a.native_type_id=t.id AND a.platform_key=? WHERE t.on_'+platform+'=1'+condition,(platform,)).fetchall()
    size=len(rows)
    if not size:return None
    rated=[r for r in rows if r[0] is not None];unrated=size-len(rated)
    total=sum(r[0] for r in rated)
    return dict(platformKey=platform,scopeKey=scope,numerator=total/100.,denominator=size,
                creditedPercent=total/size,assessedCount=len(rated),unratedCount=unrated,
                recordedLowerPercent=sum(r[1] for r in rated)/size,
                possibleUpperPercent=(sum(r[2] for r in rated)+100*unrated)/size,method=METHOD)


def apply_manifest(db,path):
    raw=path.read_bytes();manifest=json.loads(raw)
    digest=hashlib.sha256(raw).hexdigest().upper()
    if manifest.get('kind')!='native-platform-research':raise ValueError('Expected native-platform-research manifest')
    if type(manifest.get('schemaVersion')) is not int or manifest['schemaVersion']!=1:raise ValueError('Unsupported platform manifest schema')
    identity=manifest.get('manifestId')
    if not isinstance(identity,str) or not identity.strip():raise ValueError('Nonempty manifest ID required')
    old=db.execute('SELECT source_sha256 FROM native_platform_imports WHERE manifest_id=?',(identity,)).fetchone()
    if old:
        if old[0]!=digest:raise ValueError('Imported platform manifest is immutable')
        return 0,False
    stamp=manifest['createdUtc']
    parsed=datetime.strptime(stamp,'%Y-%m-%dT%H:%M:%SZ')
    if parsed.strftime('%Y-%m-%dT%H:%M:%SZ')!=stamp:raise ValueError('Canonical UTC timestamp required')
    latest=db.execute('SELECT MAX(created_utc) FROM native_platform_imports').fetchone()[0]
    if latest and stamp<=latest:raise ValueError('Out-of-order platform manifest')
    if not manifest.get('assessments'):raise ValueError('Empty assessment manifest')
    # Validate everything before writes; surrounding transaction provides rollback.
    prepared=[];seen=set()
    for item in manifest['assessments']:
        platform=item['platformKey'];name=item['className']
        if platform not in PLATFORMS:raise ValueError('Assessments require explicit pc or ps2, never common')
        if (name,platform) in seen:raise ValueError('Duplicate platform class in manifest')
        seen.add((name,platform))
        native=db.execute('SELECT id,on_pc,on_ps2 FROM native_types WHERE class_name=?',(name,)).fetchone()
        if not native or not native[1 if platform=='pc' else 2]:raise ValueError(f'{name} absent from {platform}')
        score,lower,upper=(float(item[k]) for k in ('coverageScore','lowerBound','upperBound'))
        if not all(math.isfinite(n) for n in (score,lower,upper)) or not 0<=lower<=score<=upper<=100:raise ValueError('Invalid score/bounds')
        if item['assessmentOrigin'] not in ('reviewed_existing','new_research','wire_baseline'):raise ValueError('Unknown assessment origin')
        if item['researchStatus'] not in ('identified','scouted','partial','substantial','closed'):raise ValueError('Invalid status')
        if not item.get('summary','').strip() or not item.get('evidenceRefs'):raise ValueError('Assessment needs evidence and summary')
        for evidence in item['evidenceRefs']:
            evidence_path=checked_path(evidence['sourcePath'])
            if evidence['platformKey']!=platform:raise ValueError('Cross-platform evidence cannot silently confer score')
            if not evidence.get('locator','').strip() or not evidence.get('observation','').strip():raise ValueError('Evidence needs locator and observation')
            if evidence.get('sourceSha256') and hashlib.sha256(evidence_path.read_bytes()).hexdigest().upper()!=evidence['sourceSha256'].upper():raise ValueError('Evidence hash mismatch')
        prior=db.execute('SELECT created_utc FROM native_platform_assessments WHERE native_type_id=? AND platform_key=?',(native[0],platform)).fetchone()
        if prior and prior[0]>=manifest['createdUtc']:raise ValueError('Out-of-order platform assessment')
        prepared.append((native[0],platform,score,lower,upper,item['researchStatus'],item['assessmentOrigin'],item['summary'],
                         json.dumps(item['evidenceRefs'],ensure_ascii=False),json.dumps(item.get('unknowns',[]),ensure_ascii=False),identity,manifest['createdUtc']))
    db.execute('INSERT INTO native_platform_imports VALUES(?,?,?,?,?)',(identity,str(path),digest,manifest['createdUtc'],manifest.get('notes','')))
    for row in prepared:
        db.execute('''INSERT INTO native_platform_assessments VALUES(?,?,?,?,?,?,?,?,?,?,?,?)
          ON CONFLICT(native_type_id,platform_key) DO UPDATE SET score=excluded.score,
          lower_bound=excluded.lower_bound,upper_bound=excluded.upper_bound,research_status=excluded.research_status,
          assessment_origin=excluded.assessment_origin,summary=excluded.summary,evidence_json=excluded.evidence_json,
          unknowns_json=excluded.unknowns_json,manifest_id=excluded.manifest_id,created_utc=excluded.created_utc''',row)
    for platform in PLATFORMS:
        for scope in SCOPES:
            value=coverage(db,platform,scope)
            if value:
                db.execute('''INSERT INTO native_platform_snapshots(platform_key,scope_key,numerator,denominator,
                credited_percent,assessed_count,unrated_count,recorded_lower_percent,possible_upper_percent,method,manifest_id,created_utc)
                VALUES(?,?,?,?,?,?,?,?,?,?,?,?)''',tuple(value[k] for k in ('platformKey','scopeKey','numerator','denominator','creditedPercent','assessedCount','unratedCount','recordedLowerPercent','possibleUpperPercent','method'))+(identity,manifest['createdUtc']))
    return len(prepared),True


def report(db):
    return [dict(row) for row in db.execute('SELECT * FROM latest_native_platform_coverage ORDER BY platform_key,id')]


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('command',choices=('import','report'))
    parser.add_argument('--database',type=Path,default=DATABASE);parser.add_argument('--manifest',type=Path)
    parser.add_argument('--json',action='store_true');args=parser.parse_args()
    if args.command=='report':db=sqlite3.connect(args.database.resolve().as_uri()+'?mode=ro',uri=True)
    else:db=sqlite3.connect(args.database)
    db.row_factory=sqlite3.Row;db.execute('PRAGMA foreign_keys=ON')
    if args.command=='import':
        if not args.manifest:parser.error('--manifest is required')
        ensure_schema(db)
        with db:print('IMPORT',apply_manifest(db,args.manifest.resolve()))
    result=report(db)
    if args.json:print(json.dumps(result,ensure_ascii=False,indent=2))
    else:
        print('Explicit platform ledger: credited percentages exclude unrated history; NOT legacy overall estimates.')
        print('For the expanded SMO/SAN workflow and completion gates: python research/native_goal_coverage.py report')
        for row in result:
            credit=f"{row['credited_percent']:.2f}%" if row['assessed_count'] else 'UNRATED'
            print(f"{row['platform_key']:3} {row['scope_key']:7} credited={credit} assessed={row['assessed_count']}/{row['denominator']} unrated={row['unrated_count']}")
    db.close();return 0


if __name__=='__main__':raise SystemExit(main())
