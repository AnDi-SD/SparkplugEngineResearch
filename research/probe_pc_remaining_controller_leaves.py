import sys,struct,json,time,hashlib
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,'research')
from capture_native_ranges import ROOT,EXPECTED
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
import probe_pc_animation_lifecycle as lifetime
p=ROOT/'local-data/results/native-cycle-20260910-1900/ps2-remainder';out=p/'pc-counterpart-leaves-run1.json';assert not out.exists();cases=[];t=time.perf_counter()
for kind,inputs in [('finite-add',[(-2,.5),(0,0),(1,2),(2,-4)]),('word14',[(x,) for x in (0,1,0x21000200,0xffffffff)]),('nested-buffer',[(0,0),(1,0),(1,1)])]:
 for values in inputs:
  with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
  q=f.p;o=q.allocate(0x400);data=bytearray(b'\xa5'*0x400);expected=bytearray(data)
  if kind=='finite-add':
   struct.pack_into('<f',data,0x20,values[0]);expected=bytearray(data);struct.pack_into('<f',expected,0x20,sum(values));address=0x423190;args=(struct.unpack('<I',struct.pack('<f',values[1]))[0],)
  elif kind=='word14':struct.pack_into('<I',data,0x14,values[0]);expected=bytearray(data);address=0x5ff4c0;args=()
  else:
   first=o+0x100 if values[0] else 0;second=o+0x200 if values[1] else 0;struct.pack_into('<I',data,0x10,first);struct.pack_into('<I',data,0x110,second);expected=bytearray(data);address=0x5ff3e0;args=()
  q.mu.mem_write(o,bytes(data));value=f.call(address,o,args);assert bytes(q.mu.mem_read(o,0x400))==expected
  if kind=='word14':assert value==values[0]
  if kind=='nested-buffer':assert value==(second+9 if first and second else 0)
  cases.append(dict(kind=kind,inputs=values,address=f'{address:08X}',result=value,guardVerified=True))
r=dict(kind='pc-counterparts-of-independent-ps2-leaves',status='passed',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),cases=cases,seconds=time.perf_counter()-t,scope='Original PC leaves on declared guarded records;no claimed full factory/lifetime. Existing PC scores retained. Four finite exact dyadic additions;no NaN,denormal,overflow or hardware FPU equivalence.')
out.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(dict(status=r['status'],cases=len(cases),seconds=r['seconds'])))
