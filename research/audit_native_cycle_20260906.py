#!/usr/bin/env python3
"""Read-only audit of this PC cycle: immutable imports, links and fixed scores.

Never scans executable/assets or checks the huge corpus tables. FK validation
uses only native tables cloned into memory; the canonical DB opens mode=ro.
"""
from pathlib import Path
import hashlib
import json
import math
import re
import sqlite3

ROOT=Path(__file__).resolve().parents[1]
START='2026-09-05T18:22:54Z'


def main():
    source=sqlite3.connect((ROOT/'local-data/results/smo-corpus-v2.sqlite').as_uri()+'?mode=ro',uri=True)
    source.row_factory=sqlite3.Row
    imports=list(source.execute('SELECT * FROM native_research_imports'));history=[]
    for item in imports:
        path=Path(item['source_path'])
        if not path.is_absolute():path=ROOT/path
        assert path.is_relative_to(ROOT),path
        assert hashlib.sha256(path.read_bytes()).hexdigest().upper()==item['source_sha256'],path
        history.append(json.loads(path.read_text(encoding='utf-8')))
    files=sorted((ROOT/'research').glob('native-research-2026-09-06-pc-*.json'))
    manifests=sorted([(json.loads(path.read_text(encoding='utf-8')),path) for path in files],
                     key=lambda pair:pair[0]['createdUtc'])
    assert len(manifests)==20,len(manifests)
    imported={row['manifest_id'] for row in imports};latest={};evidence_paths=set();documents=set()
    for manifest,path in manifests:
        assert manifest['manifestId'] in imported,path
        assert manifest['mode']=='incremental' and manifest['createdUtc']>=START,path
        for record in manifest['classes']:
            latest[record['className']]=record
            assert record.get('ps2Status')=='deferred',(path,record['className'])
            for evidence in record.get('evidence',[]):
                assert evidence['platformKey']!='ps2',(path,record['className'])
                target=ROOT/evidence['sourcePath'];assert target.is_file(),target
                evidence_paths.add(target)
                if target.suffix=='.md':documents.add(target)
    for name,record in latest.items():
        row=source.execute('SELECT p.* FROM native_research_progress p JOIN native_types t ON t.id=p.native_type_id WHERE t.class_name=?',(name,)).fetchone()
        assert row is not None,name
        assert (row['coverage_score'],row['lower_bound'],row['upper_bound'])==(
            record['coverageScore'],record['lowerBound'],record['upperBound']),name
        assert row['summary']==record['summary'],name
    native=list(source.execute("SELECT type,name,sql FROM sqlite_master WHERE name LIKE 'native_%' OR name='latest_native_coverage' ORDER BY type='view'"))
    memory=sqlite3.connect(':memory:')
    for obj in native:
        if obj['type']!='table':continue
        memory.execute(obj['sql']);rows=source.execute('SELECT * FROM "'+obj['name']+'"').fetchall()
        if rows:memory.executemany('INSERT INTO "'+obj['name']+'" VALUES ('+','.join('?' for _ in rows[0])+')',rows)
    assert memory.execute('PRAGMA foreign_key_check').fetchall()==[]
    counts=source.execute('SELECT COUNT(*),SUM(owner_scope="engine"),SUM(owner_scope="game"),SUM(on_pc),SUM(on_ps2) FROM native_types').fetchone()
    assert tuple(counts)==(784,373,411,733,681),tuple(counts)
    assert source.execute("SELECT COUNT(*) FROM native_type_scopes WHERE scope_key='smo_san'").fetchone()[0]==37
    # Independent score ledger, not a call to native_knowledge.advance_snapshots.
    # Initial wire defaults are25; SAN Animation starts55 before baseline import.
    owner={r['class_name']:r['owner_scope'] for r in source.execute('SELECT class_name,owner_scope FROM native_types')}
    wire={r[0] for r in source.execute("SELECT t.class_name FROM native_types t JOIN native_type_scopes s ON s.native_type_id=t.id WHERE s.scope_key='smo'")}
    direct={r[0] for r in source.execute("SELECT t.class_name FROM native_types t JOIN native_type_scopes s ON s.native_type_id=t.id WHERE s.scope_key='smo_san'")}
    scores={name:25. for name in wire};scores['spAnimation']=55.
    delta=dict.fromkeys(('all','engine','game','smo_san'),0.)
    backfills=0
    for manifest in sorted(history,key=lambda m:m['createdUtc']):
        for record in manifest['classes']:
            name=record['className'];new=record['coverageScore'];old=scores.get(name,0.)
            if manifest['createdUtc']>=START:
                assert manifest['mode']=='incremental'
                if record.get('coverageAccounting')=='baseline_backfill':
                    assert record.get('baselineBackfillReason') and name not in scores,name
                    change=0.;backfills+=1
                else:change=(new-old)/100.
                delta['all']+=change;delta[owner[name]]+=change
                if name in direct:delta['smo_san']+=change
            scores[name]=new
    print(f'PASS immutable imports {len(imports)}, cycle manifests {len(manifests)}, latest classes {len(latest)}, evidence files {len(evidence_paths)}, native-only FK clean')
    for row in source.execute('SELECT * FROM latest_native_coverage ORDER BY id'):
        old=source.execute('SELECT * FROM native_coverage_snapshots WHERE scope_key=? AND created_utc<? ORDER BY created_utc DESC LIMIT 1',(row['scope_key'],START)).fetchone()
        assert old is not None
        assert row['denominator']==old['denominator']
        assert math.isclose(row['numerator']-old['numerator'],delta[row['scope_key']],abs_tol=1e-10),(row['scope_key'],delta)
        assert math.isclose(row['coverage_percent'],100*row['numerator']/row['denominator'],abs_tol=1e-10)
        print(f"{row['scope_key']}: {old['coverage_percent']:.8f}% -> {row['coverage_percent']:.8f}%; delta {row['coverage_percent']-old['coverage_percent']:.8f} pp; {row['numerator']:.3f}/{row['denominator']:.0f}")
    print(f'PASS independent class-score ledger {delta}; {backfills} explicit zero-credit backfills')
    documents.update((ROOT/'docs/research').glob('native-pc-*.md'))
    documents.update(ROOT/path for path in ('docs/README.md','Sparkplug/README.md',
        'docs/research/native-open-questions.md','docs/research/native-reconstruction-plan.md',
        'docs/research/game-resource-database.md','journal/2026/2026-09-06-pc-reconstruction-until-1000.md',
        'research/open-questions.md','docs/engine/runtime-resource-pipeline.md',
        'docs/research/smo-runtime-validation-plan.md'))
    links=0
    for path in documents:
        value=path.read_text(encoding='utf-8')
        for target in re.findall(r'\]\(([^\n)]+)\)',value):
            target=target.strip('<>').split('#',1)[0]
            if not target or '://' in target or target.startswith('mailto:'):continue
            # These repository research documents use plain relative links.
            linked=(path.parent/target).resolve()
            assert linked.exists(),(path.relative_to(ROOT),target)
            links+=1
    print(f'PASS {links} local links in {len(documents)} research/index/journal documents')
    for name in ('native_research_imports','native_research_evidence','native_coverage_snapshots'):
        print(name,source.execute('SELECT COUNT(*) FROM '+name).fetchone()[0])
    memory.close();source.close();return 0


if __name__=='__main__':raise SystemExit(main())
