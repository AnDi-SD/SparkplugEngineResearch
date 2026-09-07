#!/usr/bin/env python3
"""Same decoded DXLight through complete virtual world/dirty/device producer."""
from pathlib import Path
import sys,json,struct
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_node_serializer import field
from probe_pc_light_corpus import state
from probe_pc_function_eval import cleanup
MODES=('directional','point','spot','disabled','ambient','parent')
def specimen(mode):
 kind={'point':1,'spot':2,'ambient':3,'parent':2}.get(mode,0)
 node=field(0,struct.pack('<3f',4,5,6))+field(1,struct.pack('<4I',0x3f3504f3,0,0,0x3f3504f3))+b'\0'
 light=field(0,struct.pack('<I',kind))+field(2,struct.pack('<I',0x80402010))+field(3,b'\1')+field(4,struct.pack('<f',2.5))+field(5,struct.pack('<f',123.))+field(6,struct.pack('<f',.5))+field(7,struct.pack('<f',1.))+field(8,bytes([mode!='disabled']))+b'\0'
 return struct.pack('<II',0x5e6402df,0x4f4f4253)+node+light
def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded decoded light world')
 data=specimen(mode);f=PCWriteBytesFixture(data);p=f.p;checks=0
 def check(value,label):
  nonlocal checks
  checks+=1
  if not value:raise AssertionError(label)
 f.call(0x6d38e0);p.put_uint(0x73fe98,0x7f234567);p.put_floats(0x7600e0,(.125,.25,.5))
 serializer=f.call(0x43ffd0);light=f.call(0x4400b0,this=serializer,args=(f.stream,));p.put_uint(light+0xdc,0xa1b2c3d4)
 check(f.call(0x440640,this=serializer+0x10,args=(f.stream,light))&255==1,'whole same DXLight reader')
 read=sum(p.visits.values());check(f.position==len(data) and not f.errors,'whole light wire consumed')
 check(p.visits.get(0x4b58d0) and p.visits.get(0x4b53c0),'Node field reader dispatches actual DX world before own fields')
 def capture(label):return [label,state(p,light),[p.uint(light+0xf0+4*i) for i in range(26)]]
 rows=[capture('read')];parent=0;maximum=0
 if mode=='parent':
  parent=f.call(0x421e20);p.put_floats(parent+0x20,(10.,-20.,30.));p.put_uint(parent+0xb0,p.uint(parent+0xb0)|1)
  f.call(0x421a60,this=parent,args=(light,));check(p.uint(light+0x2c)==parent and p.uint(light+8)&65535==1,'actual owning parent attach')
 for label in ('world','raw-change','dirty','inherited','disabled-position'):
  inherited=8 if label=='inherited' else 0
  if label=='raw-change':p.put_floats(light+0xd8,(3.,))
  if label=='dirty':p.put_uint(light+0xb0,p.uint(light+0xb0)|8)
  if label=='disabled-position':p.mu.mem_write(light+0xed,b'\0');p.put_floats(light+0x20,(7.,8.,9.));p.put_uint(light+0xb0,p.uint(light+0xb0)|1)
  if parent and label=='world':f.call(0x421420,this=parent,args=(1,))
  else:f.call(0x4b58d0,this=light,args=(inherited,))
  maximum=max(maximum,sum(p.visits.values()))
  check(bool(p.visits.get(0x4b53c0))==(mode!='disabled' and label not in ('raw-change','disabled-position')),'captured flags and post-base Enabled govern refresh')
  check(not p.uint(light+0xb0)&15,'base Node clears1..4, Light clears8 even while disabled')
  rows.append(capture(label))
 if parent:check(p.floats(light+0x74,3)==(17.,-12.,39.),'same inherited world after local change')
 for obj in (parent or light,serializer):f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
 cleanup(f,tuple(p.uint(a) for a in (0x75db90,0x75db78) if p.uint(a)));check(set(f.allocations)==set(f.freed),'complete parent/child/light/serializer ownership teardown')
 captured=[mode,data.hex(),rows]
 if not return_capture:print('LIGHT_WORLD_CAPTURE',json.dumps(captured),flush=True)
 print(f'PASS {checks}/{checks}: decoded light virtual world {mode}; read={read}; worldMax={maximum}; bytes={sum(f.allocations.values())}',flush=True)
 return captured if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
