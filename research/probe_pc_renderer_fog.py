#!/usr/bin/env python3
"""Actual Fog codec -> PC4AD390 identity cache -> original device state cache."""
from pathlib import Path
import sys,struct,json
from pc_instruction_emulator import run_bounded
from probe_pc_material_apply import ApplyFixture
from probe_pc_node_serializer import field
from probe_pc_function_eval import cleanup

MODES=('disabled','exp','exp2','linear','unknown','raw-linear','raw-density','failed-device','cache')
INDICES=(0x1c,0x22,0x23,0x24,0x25,0x26)
def inputs(mode):
 kind={'disabled':0,'exp':1,'exp2':2,'unknown':4,'raw-density':2}.get(mode,3)
 words=(kind,0x12345678,0xc0000000,0x42f60000,0x3e800000)
 if mode=='raw-linear':words=(3,0x12345678,0x80000000,0x7fc12345,0x7f800000)
 if mode=='raw-density':words=(2,0x12345678,0,0x3f800000,0x7fc12345)
 return [field(0,struct.pack('<5I',*v))+b'\0' for v in (words,(1,0x89abcdef,0,0x3f800000,0x3f000000),(0,0xff000000,0,0x3f800000,0x3f800000))]
class FogFixture(ApplyFixture):
 def observe(self,p,name,argc):
  if name!='render':raise AssertionError('Fog only calls external SetRenderState')
  device,index,value=[p.uint(p.reg('ESP')+4+4*i) for i in range(3)]
  if device!=self.device or index not in INDICES:raise AssertionError('bounded original fog device contract')
  self.events.append([index,value,self.fog_tokens[p.uint(self.renderer+0xca20)],p.uint(self.renderer+0xe4f4+4*index)])
  p.fixture_return(12,eax=0x80004005 if self.fail else 0)
def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded Fog renderer cases')
 f=FogFixture(mode=='failed-device');p=f.p;r=f.renderer;checks=0;maximum=0
 def check(ok,label):
  nonlocal checks
  checks+=1
  if not ok:raise AssertionError(label)
 serializer=f.call(0x43b830);objects=[];wire=inputs(mode)
 for data in wire:
  fog=f.call(0x419e90);f.data=data;f.position=0
  check(f.call(0x43b910,this=serializer+0x10,args=(f.stream,fog))&255==1,'actual whole Fog field codec')
  check(f.position==len(data) and bytes(p.mu.mem_read(fog+0x14,20))==data[2:22],'all raw Fog payload bits retained')
  objects.append(fog)
 f.fog_tokens={0:0,**{obj:i+1 for i,obj in enumerate(objects)}};p.put_uint(r+0xe4a0,objects[2])
 p.mu.mem_write(r+0xe4f4,b'\xa5'*1024)
 calls=[objects[0],objects[0]]+([objects[0],objects[1],0,0,objects[0]] if mode=='cache' else [0,0]);captures=[]
 for i,obj in enumerate(calls):
  if mode=='cache' and i==2:p.put_uint(objects[0]+0x18,0xa1b2c3d4);p.put_uint(objects[0]+0x1c,0x80000000)
  f.events.clear();result=f.call(0x4ad390,this=r+0x18,args=(obj,))&255;maximum=max(maximum,sum(p.visits.values()))
  check(result==int(mode!='unknown' or i!=0),'invalid type caches identity before false; same-pointer retry returns true')
  check(p.uint(r+0xca20)==(obj or objects[2]),'selected/fallback identity stored before device')
  if i==1 or (mode=='cache' and i in (2,5)) or (mode!='cache' and i==3):check(not f.events,'same object identity suppresses all payload changes and device calls')
  captures.append([result,f.fog_tokens[p.uint(r+0xca20)],[p.uint(r+0xe4f4+4*k) for k in INDICES],list(f.events)])
 cleanup(f,tuple([serializer,*objects]));check(set(f.allocations)==set(f.freed),'all actual Fog and serializer owners freed')
 capture=[mode,[data.hex() for data in wire],captures]
 if not return_capture:print('RENDERER_FOG_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {checks}/{checks}: original decoded Fog renderer {mode}; maxInstructions={maximum}; arena={p.allocated}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
