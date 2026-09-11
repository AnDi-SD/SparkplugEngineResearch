"""Package a successful original custom Text material/atlas capture, without recomputing geometry."""
from pathlib import Path
import hashlib,json,struct,sys
ROOT=Path(__file__).resolve().parents[1]
folder=(ROOT/sys.argv[1]).resolve();target=(ROOT/sys.argv[2]).resolve()
assert folder.is_relative_to(ROOT/'local-data/results') and target.is_relative_to(ROOT/'Sparkplug/Tests/Fixtures')
report=json.loads((folder/'report.json').read_text())
assert report['status']=='passed-scoped-geometry' and report['remainingAllocations']==[] and len(report['cases'])==4
out=bytearray(b'FTM1')
def word(n):out.extend(struct.pack('<I',n))
def blob(data):word(len(data));out.extend(data)
blob((folder/'font-input.bin').read_bytes());word(len(report['cases']))
for case in report['cases']:
    assert case['origin']=='TextRenderable' and case['returned']==1
    word(case['alignment']);word(case['color']);blob(bytes.fromhex(case['text']))
    blob(bytes.fromhex(case['vertexBytes']));blob(bytes.fromhex(case['indexBytes']))
target.write_bytes(out)
print(json.dumps(dict(bytes=len(out),sha256=hashlib.sha256(out).hexdigest().upper(),cases=len(report['cases']))))
