"""Qualify one bounded original report into a compact native regression fixture."""
from pathlib import Path
import hashlib,json,struct,sys
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/sys.argv[1];report=json.loads((folder/'report.json').read_text())
assert report['status']=='passed-scoped-runtime' and report['objects_released']
assert report['pc']==hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper()
for row in report['dependencies']:
    assert hashlib.sha256((ROOT/row['path']).read_bytes()).hexdigest().upper()==row['sha256'],row['path']
result=bytearray(b'TXT1')
def u(*values):result.extend(struct.pack('<'+'I'*len(values),*values))
def blob(value):u(len(value));result.extend(value)
def state(value):
    words=value['words'];offsets=[0x64,0x68,0x80,0x84,0x6c,0x70,0x74,0x78,0x8c,0x90,0x94,0x98,0x9c,0xa0]
    u(*(words[f'{i:02X}'] for i in offsets));u(value['manager_color'])
blob((folder/'font-input.bin').read_bytes())
u(len(report['measure']))
for row in report['measure']:
    u(row['text'] is not None,row['wrap'],row['width'],row['height']);blob(bytes.fromhex(row['text'] or ''))
u(len(report['reader']))
for row in report['reader']:
    blob((folder/(row['name']+'-input.bin')).read_bytes());state(row['state'])
u(len(report['layout']))
for row in report['layout']:
    u(row['wrap'],row['alignment']);blob(bytes.fromhex(row['text']));state(row['state'])
u(len(report['node']))
for i,row in enumerate(report['node']):
    u(row['count'],0 if not row['cached'] else 1 if i<2 else 2,*row['refs'],*row['sphere'])
path=ROOT/'Sparkplug/Tests/Fixtures/text-runtime-pc.dat'
if path.exists():assert path.read_bytes()==result,'Existing qualified fixture must remain byte-exact'
else:path.write_bytes(result)
print(json.dumps(dict(bytes=len(result),sha256=hashlib.sha256(result).hexdigest().upper(),measure=len(report['measure']),reader=len(report['reader']),layout=len(report['layout']),node=len(report['node']))))
