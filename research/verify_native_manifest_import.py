#!/usr/bin/env python3
"""Read-only applied-row/SHA verification confined to the native ledger.

Foreign-key checks target native tables only. The unrelated asset corpus may
be large; inspecting it does not strengthen a native assessment import check.
"""
from pathlib import Path
import argparse,hashlib,json,sqlite3,time
from native_platform_knowledge import ROOT,DATABASE


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args();path=args.manifest.resolve();output=args.output.resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Local evidence output required')
    if output.exists():raise ValueError('Do not overwrite prior verification')
    start=time.perf_counter();m=json.loads(path.read_text(encoding='utf-8'));hashes={}
    with sqlite3.connect(DATABASE.resolve().as_uri()+'?mode=ro',uri=True) as db:
        db.row_factory=sqlite3.Row
        for a in m['assessments']:
            r=db.execute('select a.* from native_platform_assessments a join native_types t on t.id=a.native_type_id where t.class_name=? and a.platform_key=?',
                         (a['className'],a['platformKey'])).fetchone()
            assert r is not None,(a['className'],a['platformKey'])
            for key,value in [('score',a['coverageScore']),('lower_bound',a['lowerBound']),('upper_bound',a['upperBound']),
                              ('research_status',a['researchStatus']),('assessment_origin',a['assessmentOrigin']),('manifest_id',m['manifestId']),('summary',a['summary'])]:
                assert r[key]==value,(a['className'],a['platformKey'],key,r[key],value)
            assert json.loads(r['unknowns_json'])==a['unknowns']
            for e in a['evidenceRefs']:
                source=e['sourcePath']
                if source not in hashes:hashes[source]=hashlib.sha256((ROOT/source).read_bytes()).hexdigest().upper()
                assert hashes[source]==e['sourceSha256'],source
        tables=[r[0] for r in db.execute("select name from sqlite_master where type='table' and name like 'native_%'")]
        for table in tables:
            assert table.replace('_','').isalnum()
            assert not db.execute('pragma foreign_key_check("'+table+'")').fetchall(),table
    report=dict(status='passed',appliedRows=len(m['assessments']),uniqueEvidenceSources=len(hashes),manifestId=m['manifestId'],
        manifestSha256=hashlib.sha256(path.read_bytes()).hexdigest().upper(),sourceHashes=hashes,nativeForeignKeyTables=tables,
        readOnly=True,assetCorpusChecked=False,seconds=time.perf_counter()-start,verifierSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper())
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:report[k] for k in ('status','appliedRows','uniqueEvidenceSources','seconds','assetCorpusChecked')}))


if __name__=='__main__':main()
