#!/usr/bin/env python3
"""Unchanged tiny PC SMO LightData slices through whole native header/read/write."""
from pathlib import Path
import sys,json,hashlib
from pc_instruction_emulator import ROOT,run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_light_serializer import snapshot
from probe_pc_function_eval import cleanup

def specimen(case):
 rows=json.loads((ROOT/'research/pc-light-corpus-fixtures.json').read_text(encoding='utf-8'))['cases']
 row=next((x for x in rows if x['case']==case),None)
 if row is None:raise ValueError('pinned compact corpus case')
 raw=(ROOT/'local-data/pc-pristine/Media'/row['relative_path']).read_bytes()
 if hashlib.sha256(raw).hexdigest().upper()!=row['sha256']:raise AssertionError('unchanged whole SMO hash')
 data=raw[row['physical_offset']:row['physical_offset']+row['serialized_size']]
 if len(data)>100 or hashlib.sha256(data).hexdigest().upper()!=row['sliceSha256']:raise AssertionError('unchanged bounded SMO light slice')
 return row,data
def state(p,obj):
 node=b''.join(bytes(p.mu.mem_read(obj+o,n*4)) for o,n in ((0x20,3),(0x30,3),(0x40,9),(0x74,3),(0x80,3),(0x8c,9)))
 return [snapshot(p,obj),node.hex()]
def main(case,return_capture=False):
 row,data=specimen(case);f=PCWriteBytesFixture(data);p=f.p;checks=0
 def check(value,label):
  nonlocal checks
  checks+=1
  if not value:raise AssertionError(label)
 f.call(0x6d38e0);serializer=f.call(0x43ffd0);light=f.call(0x4400b0,this=serializer,args=(f.stream,))
 check(f.position==8 and f.allocations[light]==0x158,'same actual Data header creates DXLight')
 p.put_uint(light+0xb0,0);p.put_uint(light+0xdc,0xa1b2c3d4)
 check(f.call(0x440640,this=serializer+0x10,args=(f.stream,light))&255==1,'whole original unchanged light reader')
 instructions=sum(p.visits.values());position=f.position-8;before=state(p,light)
 check(f.position==len(data) and not f.errors,'entire original light slice consumed')
 check(p.visits.get(0x463a70) and p.visits.get(0x4b58d0),'actual base Node reader calls DXLight virtual world')
 f.data=b'';f.position=0
 check(f.call(0x440110,this=serializer+0x10,args=(f.stream,light))&255==1,'whole writer includes converted Node quaternion')
 writeinstructions=sum(p.visits.values());written=f.data
 check(state(p,light)==before,'writer preserves complete light/Node state')
 nextlight=f.call(0x4ac000);p.put_uint(nextlight+0xb0,0);p.put_uint(nextlight+0xdc,0xa1b2c3d4);f.position=0
 check(f.call(0x440640,this=serializer+0x10,args=(f.stream,nextlight))&255==1,'actual fresh DXLight reads complete output')
 after=state(p,nextlight)
 cleanup(f,(light,nextlight,serializer));check(set(f.allocations)==set(f.freed),'all actual owners released')
 captured=['data',case,data[8:].hex(),1,position,before,written.hex(),after]
 if not return_capture:print('LIGHT_CORPUS_CAPTURE',json.dumps(captured),flush=True)
 print(f'PASS {checks}/{checks}: actual light corpus {case}; read={instructions}; write={writeinstructions}; bytes={sum(f.allocations.values())}',flush=True)
 return captured if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
