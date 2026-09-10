#!/usr/bin/env python3
"""Execute each selected original PS2 getter once, plus five null Clone leaves."""
import hashlib,json,sys,time
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED
from ps2_scalar_prefix import Ps2ScalarPrefix


def main():
    p=ROOT/'local-data/results/native-cycle-20260910-1900/game-remainder';out=Path(sys.argv[1]).resolve()
    if not out.is_relative_to(ROOT/'local-data/results') or out.exists():raise ValueError('Fresh local output required')
    started=time.perf_counter();report=dict(kind='game-remainder-ps2-getter-and-null-clone',inputs=EXPECTED,status='running',sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),getters=[],nullClones=[],scope='One bounded original constant getter per selected PS2 class and five null Clone leaves. Factory/constructor tables independently captured;this is not full PS2 object lifetime.')
    for r in json.loads((p/'getter-candidates-run1.json').read_text())['classes']:
        if 'ps2' not in r:continue
        c=r['ps2'];assert len(c['getters'])==1;entry=c['getters'][0]['address'];q=Ps2ScalarPrefix([(entry,12)]);execution=q.run(entry,[q.RETURN]);assert q.reg('V0')==c['registration']
        report['getters'].append(dict(className=r['className'],entry=entry,record=c['registration'],execution=execution))
    for r in json.loads((p/'ps2-null-clones/capture.json').read_text())['ranges']:
        q=Ps2ScalarPrefix([(r['address'],8)]);execution=q.run(r['address'],[q.RETURN]);assert q.reg('V0')==0
        report['nullClones'].append(dict(className=r['name'],entry=r['address'],execution=execution))
    report.update(status='passed',seconds=time.perf_counter()-started);out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(status='passed',getters=len(report['getters']),nullClones=len(report['nullClones']),seconds=report['seconds'])))


if __name__=='__main__':main()
