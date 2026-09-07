#!/usr/bin/env python3
"""Whole loaded Skin/SAN/scene/mesh -> alpha enqueue/flush -> complete Skin draw."""
from pathlib import Path
import sys,json,struct
from pc_instruction_emulator import run_bounded
from probe_pc_skin_owned_material_render import OwnedMaterialSceneMesh
from probe_pc_skin_render import main as render
from probe_pc_skin_alpha_queue import IDENTITY
from probe_pc_node_serializer import field

MODES=('normal','failed-device','post-false')
class QueuedMesh(OwnedMaterialSceneMesh):
 arena_alignment=8
 arena_best_fit=True
 use_material_as_fallback=True
 def __init__(self,mode):super().__init__('0.25',mode)
 def skin_specimen(self,mode):
  directory,payload=super().skin_specimen(mode)
  old=field(3,struct.pack('<I',0))
  if payload.count(old)!=1:raise AssertionError('one exact material pass blend wire field')
  return directory,payload.replace(old,field(3,struct.pack('<I',1)),1)
 def after_mesh_read(self):
  p=self.p;self.call(0x4ae140,this=self.renderer+0xf358)
  self.check(p.uint(self.renderer+0xf35c)==0 and self.mesh_declaration_refs==1,'completed lookup map releases scratch but retains declaration')
  p.put_uint(0x75e150,0x603625d0);p.put_uint(0x75e150+0x48,0x75dd88)
  self.queue_support=self.call(0x425520);self.queue_camera=p.allocate(0x238);p.put_floats(self.queue_camera+0xcc,IDENTITY)
 def before_render(self,skin,bone):
  p=self.p;r=self.renderer;p.mu.mem_write(r+0x45,b'\1');p.mu.mem_write(r+0xc9d8,b'\1');p.put_uint(skin+0x1c,13)
  self.check(self.call(0x46a240,this=skin,args=(self.queue_camera,self.queue_support+0xb4))&255==0 and p.uint(r+0x4c)==1 and not self.events,'same loaded Skin enqueues without callback/device work')
  at=r+0x50;self.enqueue_record=[1,2,3,p.uint(at+12),p.uint(at+16),p.uint(at+20)&255]
  p.put_uint(0x6d9360,0x34090e20)
  def qsort(m):
   args=[m.uint(m.reg('ESP')+4+4*i) for i in range(4)]
   self.check(args==[r+0x50,1,24,0x454800],'one-record external qsort boundary with original comparator')
   self.events.append(['qsort',1,p.uint(r+0x44)&255]);m.fixture_return()
  p.seams[0x34090e20]=qsort
 def invoke_render(self,skin,mode):
  p=self.p;self.inner_results=[]
  # Read-only instruction observer after the actual queued virtual call.
  # No engine body or register/return value is replaced.
  hook=p.mu.hook_add(p.uc.UC_HOOK_CODE,lambda mu,a,n,u:self.inner_results.append(p.reg('EAX')&255),begin=0x45489e,end=0x45489e)
  try:self.flush_result=self.call(0x454850,this=self.renderer)&255
  finally:p.mu.hook_del(hook)
  self.check(len(self.inner_results)==1 and self.flush_result==1,'whole alpha flush ignores actual inner Skin result')
  return self.inner_results[0]
 def on_render_completed(self,skin,bone):
  p=self.p;r=self.renderer;self.render_instructions=sum(p.visits.values())
  self.check(all(p.visits.get(a) for a in (0x454850,0x4248d0,0x46a240,0x4be210)) and not p.visits.get(0x454c30),'actual full queued support/Skin/mesh draw, no re-enqueue')
  self.check(not p.visits.get(0x4c89eb) and not self.sdk.calls and not self.shader_events,'declared actual cached shader draw')
  self.check(p.uint(r+0xc18c)==self.loaded_material==p.uint(r+0xc9c0),'same decoded owning alpha material selected')
  self.flush_state=[p.uint(r+0x4c),p.uint(r+0x44)&255,2 if p.uint(r+0xc190)==self.queue_support+0xf0 else 0,[p.uint(r+0xc9c8+4*i) for i in range(4)]]
  self.check(self.flush_state[:3]==[0,0,2],'queue cleared and support light cache published')
 def on_render_teardown(self):
  self.call(0x4255d0,this=self.queue_support,args=(1,))
  self.check(not self.mesh_declaration_refs and all(b['refs']==0 and b['locks']==0 for b in self.mesh_buffers.values()),'all mesh COM owners freed after ended lookup map')

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded whole queued mesh Skin')
 f=QueuedMesh(mode);draw=render(mode,True,f);f.arena.assert_engine_released()
 f.check(f.loaded_material in f.freed and f.arena.peak_reserved<=65536,'same decoded owning graph freed in unchanged arena')
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),f.mesh_input.hex(),
          [mode,f.enqueue_record,f.flush_result,f.inner_results,f.flush_state,[f.material_capture,[f.mesh_capture,[f.animation_capture,draw]]]]]
 if not return_capture:print('QUEUED_MESH_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: whole queued mesh Skin {mode}; read={f.read_instructions}; render={f.render_instructions}; peakArena={f.arena.peak_reserved}; owners={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
