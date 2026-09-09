#!/usr/bin/env python3
"""Compare actual shared C++ spatial state to bounded original-PC captures."""
from pathlib import Path
import hashlib,json,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'local-data/results/tools-core-cycle-20260909-1900/spatial-readers'
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def main():
    original=OUT/'original-readers.json'
    source=ROOT/'artifacts/native/viewer/Release/ViewerSpatialSerializationChecks.exe'
    rows=[]
    for case in json.loads(original.read_text(encoding='utf-8')):
        result=subprocess.run([str(source),'--capture',case['mode'],case['wire']],cwd=ROOT,
            capture_output=True,text=True,encoding='utf-8',timeout=20)
        if result.returncode:raise RuntimeError(case['mode']+' '+result.stderr)
        actual=json.loads(result.stdout)
        if actual!=case['state']:
            mismatch={'mode':case['mode'],'original':case['state'],'source':actual}
            (OUT/'source-mismatch.json').write_text(json.dumps(mismatch,indent=2)+'\n',encoding='utf-8')
            raise AssertionError(mismatch)
        rows.append({'mode':case['mode'],'matched':True,'wire_sha256':hashlib.sha256(bytes.fromhex(case['wire'])).hexdigest().upper(),'state':actual})
        print('MATCH',case['mode'],flush=True)
    guards=subprocess.run([str(source)],cwd=ROOT,capture_output=True,text=True,encoding='utf-8',timeout=20)
    (OUT/'source-guards.log').write_text(guards.stdout+guards.stderr,encoding='utf-8')
    assert guards.returncode==0,guards.stderr
    report={'status':'passed','scope':'14 actual PC readers vs shared C++ readers; published FAT reference identity, actual factories, state and ownership. No Scene traversal/whole game startup claim.',
        'original_capture_sha256':sha(original),'source_executable_sha256':sha(source),'original_probe_sha256':sha(ROOT/'research/probe_pc_spatial_serializers.py'),
        'source_guards':guards.stdout.strip(),'cases':rows}
    (OUT/'source-comparison.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(guards.stdout.strip());return 0
if __name__=='__main__':raise SystemExit(main())
