#!/usr/bin/env python3
"""Whole lit Skin/SAN/scene/mesh calls real DXLight producer and submission."""
from pathlib import Path
import sys,json,struct
from pc_instruction_emulator import run_bounded
from probe_pc_skin_owned_material_render import OwnedMaterialSceneMesh
from probe_pc_skin_render import main as render
from probe_pc_node_serializer import field

MODES=('null','empty','directional','point','spot','disabled','ambient','failed-device','post-false','mode-zero','unlit-list','mode-three','mode-four','mode-five','mode-six','mode-seven')
class LitSkin(OwnedMaterialSceneMesh):
 arena_alignment=8
 arena_best_fit=True
 use_material_as_fallback=True
 def __init__(self,mode):
  self.light_mode=mode;self.light_kind={'point':1,'spot':2,'ambient':3}.get(mode,0)
  self.ordinary=mode not in ('null','empty','ambient');self.raw_light_mode={'mode-zero':0,'unlit-list':2,'mode-three':3,'mode-four':4,'mode-five':5,'mode-six':6,'mode-seven':7}.get(mode,1)
  self.cached_shader_key=0x1000011|(self.raw_light_mode<<16)|(int(self.ordinary)<<20)
  self.cached_shader_types=self.light_kind if self.ordinary else 0
  super().__init__('0.25',mode if mode in ('failed-device','post-false') else 'normal')
 def skin_specimen(self,mode):
  directory,payload=super().skin_specimen(mode)
  old=field(0,struct.pack('<11I',0,0,1,2,1,1,3,0,2,1,6))
  new=field(0,struct.pack('<11I',0,0,1,2,1,1,3,0,self.raw_light_mode,1,6))
  if payload.count(old)!=1:raise AssertionError('one exact encoded material state set')
  return directory,payload.replace(old,new,1)
 def after_mesh_read(self):
  p=self.p;self.call(0x4ae140,this=self.renderer+0xf358)
  self.check(p.uint(self.renderer+0xf35c)==0 and self.mesh_declaration_refs==1,'completed declaration map retains mesh owner')
 def create_light_for_render(self):
  p=self.p;obj=self.call(0x4ac000)
  p.put_uint(obj+0xc0,self.light_kind);p.put_floats(obj+0xc4,(.25,.75,1.5,.5))
  p.mu.mem_write(obj+0xd4,b'\1');p.put_floats(obj+0xd8,(2.,));p.put_floats(obj+0xe0,(200.,.5,1.))
  p.put_floats(obj+0x74,(4.,5.,6.));p.put_floats(obj+0xa4,(1.,2.,3.));p.mu.mem_write(obj+0xed,bytes([self.light_mode!='disabled']))
  p.mu.mem_write(obj+0xf0,b'\xcc'*104)
  return obj
 def build_light_payload(self,obj):self.call(0x4b53c0,this=obj)
 def prepare_light_list(self,obj):
  p=self.p;light_list=self.arena.allocate_raw_high(0x28) if getattr(self,'light_list_high',False) else p.allocate(0x28)
  if self.ordinary:p.put_uint(light_list,obj);p.put_uint(light_list+0x24,1)
  if self.light_mode=='ambient':p.put_uint(light_list+0x20,obj)
  return light_list
 def before_render(self,skin,bone):
  p=self.p;r=self.renderer;self.light=self.create_light_for_render();obj=self.light
  if self.light_mode=='mode-six':p.put_uint(skin+0x28,0x80402010)
  self.check(self.allocations[obj]==0x158 and p.uint(obj)==0x6f0c88,'actual DXLight factory')
  p.put_uint(0x73fe98,0x7f234567);p.put_floats(0x7600e0,(.125,.25,.5))
  if hasattr(self,'prepare_light_world'):self.prepare_light_world(obj)
  self.build_light_payload(obj);self.light_words=[p.uint(obj+0xf0+4*i) for i in range(26)]
  self.check(p.visits.get(0x4b53c0),'whole native light device-payload producer')
  self.light_list=self.prepare_light_list(obj)
  p.put_uint(r+0xc190,0 if self.light_mode=='null' else self.light_list);p.put_uint(0x764340,3)
  p.put_floats(r+0xc178,(.25,.25,.25,.25))
  for i in range(24):p.put_uint(r+0xf2f8+4*i,obj)
  for i,(slot,name,argc) in enumerate(((0xcc,'light',3),(0xd4,'enable',3))):
   address=0x34090c00+16*i;p.put_uint(p.uint(self.device)+slot,address)
   p.seams[address]=lambda m,n=name,c=argc:self.observe(m,n,c)
 def observe(self,p,name,argc):
  if name in ('light','enable'):
   args=[p.uint(p.reg('ESP')+4+4*i) for i in range(argc)];self.check(args[0]==self.device,'original light COM receiver')
   event=[name,args[1],[p.uint(args[2]+4*i) for i in range(26)]] if name=='light' else [name,*args[1:]]
   self.events.append([*event,self.state()]);p.fixture_return(argc*4,eax=0x80004005 if self.fail else 0);return
  super().observe(p,name,argc)
 def on_render_completed(self,skin,bone):
  p=self.p;r=self.renderer;self.render_instructions=sum(p.visits.values())
  self.check(bool(p.visits.get(0x4bde50))==(self.raw_light_mode!=2),'whole geometry selects real lighting consumer unless raw state2')
  self.check(not p.visits.get(0x4c89eb) and not self.sdk.calls and not self.shader_events,'actual cached key includes every ordinary light, even disabled/unlit')
  self.check(p.uint(skin+0x20)==self.loaded_material==p.uint(r+0xc18c),'same decoded lit material selected')
  self.light_state=[p.uint(0x764340),0 if not p.uint(r+0xc190) else 9,[7 if p.uint(r+0xf2f8+4*i)==self.light else 0 for i in range(24)],
                    [p.uint(r+0xc178+4*i) for i in range(4)],p.uint(r+0xe4f4+139*4)]
 def on_render_teardown(self):
  self.call(self.p.uint(self.p.uint(self.light)),this=self.light,args=(1,))
  self.check(not self.mesh_declaration_refs and all(b['refs']==0 and b['locks']==0 for b in self.mesh_buffers.values()),'mesh COM teardown after completed map')

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded whole lit Skin')
 f=LitSkin(mode);device_mode=mode if mode in ('failed-device','post-false') else 'normal';draw=render(device_mode,True,f);f.arena.assert_engine_released()
 f.check(f.light in f.freed and f.loaded_material in f.freed,'actual owning light/Skin/material teardown')
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),f.mesh_input.hex(),
          [mode,f.light_words,f.light_state,[f.material_capture,[f.mesh_capture,[f.animation_capture,draw]]]]]
 if not return_capture:print('LIT_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: whole lit Skin {mode}; read={f.read_instructions}; render={f.render_instructions}; peakArena={f.arena.peak_reserved}; owners={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
