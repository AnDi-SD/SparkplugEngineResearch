#!/usr/bin/env python3
"""Read-only cycle manifest integrity and fixed-scope baseline comparison."""
import hashlib,json,sqlite3
from pathlib import Path
from native_goal_coverage import report
from native_workbench import ROOT,DATABASE

START='2026-09-07T04:48:00Z'
END='2026-09-07T16:00:00Z'
EXE_SHA='3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F'

def main():
 db=sqlite3.connect(DATABASE.resolve().as_uri()+'?mode=ro',uri=True);db.row_factory=sqlite3.Row
 baseline=json.loads((ROOT/'local-data/results/cycle-baseline-20260907-1900.json').read_text(encoding='utf-8'))
 current=report(db,'smo-san-workflow-v2',True)
 assert baseline['scopeId']==current['scopeId'] and baseline['scopeSha256']==current['scopeSha256']
 checked=[];refs=set();hash_mismatches=[];cycle_classes=set()
 for row in db.execute('SELECT * FROM native_platform_imports WHERE created_utc>=? AND created_utc<=? ORDER BY created_utc',(START,END)):
  path=Path(row['source_path']);path=path if path.is_absolute() else ROOT/path
  path=path.resolve();assert path.is_relative_to(ROOT) and path.is_file(),str(path)
  raw=path.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()==row['source_sha256'].upper(),str(path)
  manifest=json.loads(raw);assert (manifest['manifestId'],manifest['createdUtc'])==(row['manifest_id'],row['created_utc'])
  checked.append(str(path.relative_to(ROOT)).replace('\\','/'))
  for assessment in manifest['assessments']:
   cycle_classes.add(assessment['className'])
   assert assessment['platformKey']=='pc','No PS2 evidence may be inferred from this PC cycle'
   for ref in assessment['evidenceRefs']:
    item=(ROOT/ref['sourcePath']).resolve();assert item.is_relative_to(ROOT) and item.is_file(),str(item);refs.add(item)
    if ref.get('sourceSha256') and hashlib.sha256(item.read_bytes()).hexdigest().upper()!=ref['sourceSha256'].upper():
     hash_mismatches.append(dict(manifest=manifest['manifestId'],sourcePath=ref['sourcePath']))
 latest_mismatches=[]
 for row in db.execute('SELECT t.class_name,a.evidence_json FROM native_platform_assessments a JOIN native_types t ON t.id=a.native_type_id WHERE a.platform_key="pc"'):
  if row['class_name'] not in cycle_classes:continue
  for ref in json.loads(row['evidence_json']):
   if ref.get('sourceSha256') and hashlib.sha256((ROOT/ref['sourcePath']).read_bytes()).hexdigest().upper()!=ref['sourceSha256'].upper():
    latest_mismatches.append(dict(className=row['class_name'],sourcePath=ref['sourcePath']))
 assert not latest_mismatches,latest_mismatches
 executable=ROOT/'local-data/pc-pristine/WinxClub.exe'
 assert hashlib.sha256(executable.read_bytes()).hexdigest().upper()==EXE_SHA
 changes=[]
 for platform in ('pc','ps2'):
  before={r['class_name']:r for r in baseline['platforms'][platform]['classes']}
  after={r['class_name']:r for r in current['platforms'][platform]['classes']}
  assert set(before)==set(after)
  for name,row in after.items():
   if before[name]['score']!=row['score']:
    changes.append(dict(platform=platform,className=name,before=before[name]['score'],after=row['score']))
 assert not any(r['platform']=='ps2' for r in changes)
 value=dict(kind='native-cycle-closeout-audit',schemaVersion=1,scopeId=current['scopeId'],scopeSha256=current['scopeSha256'],
   assessmentSha256=current['assessmentSha256'],evidenceAsOfUtc=current['evidenceAsOfUtc'],
   cycleManifestHashesMatched=len(checked),cycleManifests=checked,evidencePathsChecked=len(refs),
   historicalEvidenceHashMismatches=hash_mismatches,latestCycleEvidenceHashMismatches=latest_mismatches,
   cycleAssessedClasses=len(cycle_classes),pcExeSha256=EXE_SHA,changedClassScores=changes,
   baseline={p:{k:baseline['platforms'][p][k] for k in ('catalog','workflow','completion')} for p in ('pc','ps2')},
   current={p:{k:current['platforms'][p][k] for k in ('catalog','workflow','completion')} for p in ('pc','ps2')},
   limitation='Fixed known class scope and manifest/file provenance only; no assertion of full engine readiness, reachable inventory, transitive historic build hashes or behavior coverage.')
 print(json.dumps(value,ensure_ascii=True,indent=2));return 0
if __name__=='__main__':raise SystemExit(main())
