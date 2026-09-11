"""Pack retained original PC material/geometry outputs, without synthesizing expected values."""
from pathlib import Path
import json,struct,sys,hashlib
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def main():
 folder=ROOT/sys.argv[1];row=json.loads((folder/'report.json').read_text())
 assert row['status']=='passed-scoped-geometry' and row['pc']==sha(ROOT/'local-data/pc-pristine/WinxClub.exe')
 for dependency in row['dependencies']:assert sha(ROOT/dependency['path'])==dependency['sha256'],dependency['path']
 u=lambda *values:struct.pack('<'+'I'*len(values),*values)
 blob=lambda value:u(len(value))+value
 mat=row['material'];data=b'FTG1'+blob((folder/'font-input.bin').read_bytes())
 words=mat['states']+list(struct.unpack('<17I',bytes.fromhex(mat['colors'])))+mat['flags']+[mat['passBlend'],mat['layers']]+mat['textureStates']
 data+=u(len(words),*words)+u(len(row['cases']))
 for case in row['cases']:
  mode=int(case.get('origin')=='TextRenderable');text=None if case['text'] is None else bytes.fromhex(case['text'])
  data+=u(mode,int(text is not None),case['wrap'],case.get('alignment',0),case.get('color',0xff804020))
  data+=struct.pack('<3f',*case.get('position',(0,0,0)))+blob(text or b'')
  vb=bytes.fromhex(case['vertexBytes']);ib=bytes.fromhex(case['indexBytes'])
  assert len(vb)==case['vertices']*24 and len(ib)==case['indices']*2
  data+=blob(vb)+blob(ib)
 target=ROOT/'Sparkplug/Tests/Fixtures/text-geometry-pc.dat'
 if target.exists():assert target.read_bytes()==data,'Existing golden fixture differs'
 else:target.write_bytes(data)
 print(json.dumps(dict(path=target.relative_to(ROOT).as_posix(),bytes=len(data),sha256=sha(target),cases=len(row['cases']))))
if __name__=='__main__':main()
