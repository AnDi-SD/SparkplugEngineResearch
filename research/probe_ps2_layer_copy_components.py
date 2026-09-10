import sys,json,struct,hashlib,time
from pathlib import Path
sys.path.insert(0,'research')
from capture_native_ranges import ROOT,EXPECTED
from ps2_scalar_prefix import Ps2ScalarPrefix
p=ROOT/'local-data/results/native-cycle-20260910-1900/layer-clone-ps2';out=p/'copy-components-run1.json';assert not out.exists();cases=[];t=time.perf_counter()
def create(ranges):
 q=Ps2ScalarPrefix(ranges);q.map(0x21000000,4096);q.write(0x21000000,b'\xa5'*1024);q.map(0x22000000,4096);q.reg('SP',0x22000800);q.map(0x49f000,4096);q.put_uint(0x49f810,0x21000300);q.reg('GP',0x4a4170);return q
q=create([(0x1722d0,8)]);q.reg('A0',0x21000000);q.reg('V0',0x12345678);ex=q.run(0x1722d0,[q.RETURN]);clone_result=q.reg('V0');assert clone_result==0 and q.read(0x21000000,1024)==b'\xa5'*1024;cases.append(dict(kind='original-material-movie-texture-null-clone',execution=ex,result=clone_result))
q=create([(0x170714,0x3c),(0x104f00,8)]);q.reg('S0',0x21000000);q.reg('V0',clone_result);ex=q.run(0x170714,[0x170750]);expected=bytearray(b'\xa5'*1024);struct.pack_into('<I',expected,0x10,0);assert q.reg('V0')==1 and q.read(0x21000000,1024)==expected;cases.append(dict(kind='layer-copy-continuation-after-observed-null-child-clone',execution=ex,result=1,scope='Separate original continuation consumes observed original child return;not whole nested PS2 Clone transaction'))
for ptr in (0,0x21000200):
 q=create([(0x1706e0,0x34)]);q.put_uint(0x21000010,ptr);q.put_uint(0x21000110,0);before=q.read(0x21000000,1024);q.reg('A0',0x21000000);q.reg('A1',0x21000100);ex=q.run(0x1706e0,[0x173290]);assert q.reg('A0')==ptr and q.read(0x21000000,1024)==before;cases.append(dict(kind='empty-destination-source-payload-forward',execution=ex,payload=ptr))
for word in (0,4,0xffffffff,0x80000000):
 q=create([(0x1705a4,0x40),(0x104f00,8)]);q.put_uint(0x21000014,word);q.reg('S1',0x21000000);q.reg('S0',0x21000100);expected=bytearray(q.read(0x21000000,1024));struct.pack_into('<I',expected,0x114,word);ex=q.run(0x1705a4,[0x1705e4]);assert q.reg('V0')==1 and q.read(0x21000000,1024)==expected;cases.append(dict(kind='environment-layer-copy-own-tail',execution=ex,word14=word,scope='Original tail after successful base Copy;not full parent clone'))
r=dict(kind='independent-ps2-layer-copy-components',status='passed',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),cases=cases,seconds=time.perf_counter()-t);out.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(dict(status='passed',cases=len(cases),seconds=r['seconds'])))
