#!/usr/bin/env python3
"""Whole queue flush -> actual RenderNode world setter -> decoded Skin pre refusal."""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from probe_pc_skin_alpha_queue import AlphaSkin,IDENTITY
from probe_pc_function_eval import cleanup

MODES=('three','failed-device','preserve-lights','append-during-pre','already-active','empty')
class AlphaFlush(AlphaSkin):
 def __init__(self,mode):
  super().__init__('queued');self.mode=mode;p=self.p
  p.put_uint(0x755378,0)
  for address in self.reader_rtti_inputs:self.arena.release_raw(address)
  self.arena.retire_initial_stream_inputs()
  self.prepare_device(mode=='failed-device',0xf2f8,0x1b8,True,self.arena.allocate_raw_high(0xf2f8))
  p.put_uint(0x75e150,0x603625d0);p.put_uint(0x75e150+0x48,0x75dd88)
  self.supports=[self.call(0x425520) for _ in range(2)];self.camera=p.allocate(0x238)
  mesh=self.call(0x4a9e80);p.put_floats(mesh+0x18,(1.,2.,3.,1.));self.call(0x479e20,this=self.loaded_skin,args=(mesh,))
  for node,position in zip(self.supports,((10.,20.,30.),(-5.,4.,0.))):
   p.put_floats(node+0x138,(*IDENTITY[:12],*position,1.));self.call(0x469ed0,this=node+0xb4,args=(self.loaded_skin,))
  p.put_floats(self.camera+0xcc,(*IDENTITY[:12],-1.,-2.,-3.,1.))
  p.mu.mem_write(self.renderer+0x45,b'\1');p.mu.mem_write(self.renderer+0xc9d8,b'\1')
  p.mu.mem_write(self.renderer+0xc9c4,bytes([mode=='preserve-lights']));p.put_uint(self.renderer+0xc190,0x11223344 if mode=='preserve-lights' else 0)
  self.pre_count=0;self.setup_count=0;p.put_uint(self.loaded_skin+0x2c,0x34090e00);p.seams[0x34090e00]=self.pre
  p.put_uint(0x6d9360,0x34090e10);p.seams[0x34090e10]=self.qsort
  self.before_refs=p.uint(self.loaded_skin+8)&65535
  for support,priority in (() if mode=='empty' else ((self.supports[0],30),(self.supports[0],20),(self.supports[1],10))):
   p.put_uint(self.loaded_skin+0x1c,priority)
   self.check(self.call(0x46a240,this=self.loaded_skin,args=(self.camera,support+0xb4))&255==0,'whole Skin enqueues before pre callback')
  self.check(not self.events and p.uint(self.renderer+0x4c)==(0 if mode=='empty' else 3),'borrowed queue prepared without callbacks/device')
  p.mu.mem_write(self.renderer+0x44,bytes([mode=='already-active']))
 def lights(self):
  ptr=self.p.uint(self.renderer+0xc190)
  return self.supports.index(ptr-0xf0)+1 if ptr-0xf0 in self.supports else 9 if ptr==0x11223344 else 0
 def sphere(self):return [self.p.uint(self.renderer+0xc9c8+4*i) for i in range(4)]
 def observe(self,p,name,argc):
  args=[p.uint(p.reg('ESP')+4+4*i) for i in range(argc)]
  self.check(name=='transform' and args[:2]==[self.device,256],'actual RenderNode reaches original world matrix setter and external COM')
  self.setup_count+=1;self.events.append(['matrix',[p.uint(args[2]+4*i) for i in range(16)],p.uint(self.renderer+0x4c),p.uint(self.renderer+0x44)&255,self.lights(),self.sphere()])
  p.fixture_return(argc*4,eax=0x80004005 if self.fail else 0)
 def qsort(self,p):
  args=[p.uint(p.reg('ESP')+4+4*i) for i in range(4)]
  self.check(args==[self.renderer+0x50,p.uint(self.renderer+0x4c),24,0x454800] and args[1]<=3,'declared qsort record size/original comparator and bounded already ordered input')
  priorities=[p.uint(self.renderer+0x60+24*i) for i in range(args[1])]
  self.check(priorities==sorted(priorities,reverse=True) and len(set(priorities))==len(priorities),'no-op external sorting only for distinct ordered priorities')
  self.events.append(['qsort',args[1],p.uint(self.renderer+0x44)&255]);p.fixture_return()
 def pre(self,p):
  skin,camera,support=[p.uint(p.reg('ESP')+4+4*i) for i in range(3)]
  self.check(skin==self.loaded_skin and camera==self.camera and support-0xb4 in self.supports,'actual queued Skin borrowed callback arguments')
  self.pre_count+=1;self.events.append(['pre',self.supports.index(support-0xb4)+1,p.uint(self.renderer+0x4c),p.uint(self.renderer+0x44)&255])
  if self.mode=='append-during-pre' and self.pre_count==1:
   # Explicit user callback mutation of its borrowed queue, no native method
   # replaced. Tests45489E's live count reread and absence of a second sort.
   p.mu.mem_write(self.renderer+0x50+72,bytes(p.mu.mem_read(self.renderer+0x50+48,24)));p.put_uint(self.renderer+0x4c,4)
  p.fixture_return(eax=0)

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded Skin alpha flush')
 f=AlphaFlush(mode);p=f.p;r=f.renderer
 result=f.call(0x454850,this=r)&255;instructions=sum(p.visits.values())
 f.check(result==1 and p.uint(r+0x4c)==0 and p.uint(r+0x44)&255==0,'flush ignores all Skin refusals and clears count/active')
 f.check(f.pre_count==(0 if mode=='empty' else 4 if mode=='append-during-pre' else 3) and f.setup_count==(0 if mode=='empty' else 2),'live count read and consecutive support reuse')
 f.check(not p.visits.get(0x454c30) and not p.visits.get(0x461d70),'active flag prevents re-enqueue and false pre prevents palette/draw')
 f.check((p.uint(f.loaded_skin+8)&65535)==f.before_refs==2,'two actual RenderNodes own same Skin; queue/flush retain no refs')
 records=[]
 for i in range(0 if mode=='empty' else 4 if mode=='append-during-pre' else 3):
  at=r+0x50+i*24;records.append([1,f.supports.index(p.uint(at+4)-0xb4)+1,3,p.uint(at+12),p.uint(at+16),p.uint(at+20)&255])
 f.check(f.call(0x454850,this=r)&255==1,'empty second flush still calls external qsort')
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),[mode,result,list(f.events),[p.uint(r+0x4c),p.uint(r+0x44)&255,f.lights(),f.sphere(),p.uint(r+0xf2f4)&255],records]]
 cleanup(f,(*reversed(f.supports),f.loaded_bone,*tuple(p.uint(a) for a in (0x75db78,) if p.uint(a))));f.arena.assert_engine_released()
 f.check(f.loaded_skin in f.freed and f.loaded_material in f.freed and f.arena.peak_reserved<=65536,'actual support/Skin/material/mesh destruction and fixed arena')
 if not return_capture:print('SKIN_ALPHA_FLUSH_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: whole Skin alpha flush {mode}; read={f.read_instructions}; flush={instructions}; peakArena={f.arena.peak_reserved}; owners={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
