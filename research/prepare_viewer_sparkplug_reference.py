"""Reuse the directed GUI fixtures; replace Icy PRS with measured original PC outputs."""
from pathlib import Path
import hashlib
import json
ROOT=Path(__file__).resolve().parents[1]
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest().upper()

def main():
    previous=ROOT/'local-data/results/tool-cycle-20260908-1900/shared-san-export-v4/input.json'
    base=ROOT/'local-data/results/viewer-sparkplug-core-20260908'
    native_path=base/'icy-original-prs-all.json'
    native=json.loads(native_path.read_text(encoding='utf-8'))
    report=json.loads(previous.read_text(encoding='utf-8'))
    assert sha(ROOT/native['source']['path'])==native['source']['sha256']
    for case in report['cases']:
        reference=(previous.parent/case['reference']).resolve()
        assert sha(reference)==case['referenceSha256']
        assert sha(Path(case['san']))==case['sanSha256']
        case['reference']=str(reference)
        if case['name']!='icy-walk':continue
        assert sha(Path(case['san']))==native['source']['sha256']
        case['nativePrsReference']=True
        for sample in case['samples']:
            rows=[row for row in native['rows'] if abs(row['seconds']-sample['seconds'])<1e-8]
            assert len(rows)==native['source']['tracks']
            sample['tracks']={row['name']:{str(role+2):row['prs'][role] for role in range(3) if row['validity'][role]} for row in rows}
    report['previousInput']={'path':str(previous),'sha256':sha(previous)}
    report['icyOriginalPrs']={'path':str(native_path),'sha256':sha(native_path)}
    report['referenceChange']='Icy uses original PC PRS instead of the previous C# sampler. The independent FK, geometry inputs and tolerances are unchanged.'
    output=base/'reference-input.json'
    output.write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(output)

if __name__=='__main__':main()
