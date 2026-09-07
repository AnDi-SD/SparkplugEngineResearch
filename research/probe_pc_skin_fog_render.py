#!/usr/bin/env python3
"""Whole Skin owns Fog/material/Node; SAN/scene/mesh -> real fog+Skin draw."""
from pathlib import Path
import sys,json,struct
from pc_instruction_emulator import run_bounded
from probe_pc_skin_owned_material_render import OwnedMaterialSceneMesh
from probe_pc_skin_render import main as render
from probe_pc_renderer_fog import inputs,INDICES
from probe_pc_node_serializer import field

MODES=('disabled','exp','exp2','linear','unknown','raw-linear','raw-density','failed-device','post-false')
class FogSkin(OwnedMaterialSceneMesh):
 def __init__(self,mode):
  self.fog_mode=mode;super().__init__('0.25','failed-device' if mode=='failed-device' else 'normal')
 def skin_specimen(self,mode):
  directory,payload=super().skin_specimen(mode);self.fog_input=inputs(self.fog_mode)[0]
  fog=struct.pack('<II',0x7ac95aec,0x4f4f4253)+self.fog_input
  directory=struct.pack('<I',3)+directory[4:]+struct.pack('<IHIII',9,0,0x7ac95aec,0,len(fog))
  return directory,field(1,struct.pack('<II',9,len(fog))+fog)+payload
 def configure_skin_reader(self,fat,manager):
  super().configure_skin_reader(fat,manager);p=self.p;tree=p.uint(0x755378);head=p.uint(tree+0x18);root=p.uint(head+4)
  left=p.uint(root);right=p.uint(root+8);fog=p.allocate(24);self.reader_rtti_inputs.append(fog)
  for entry in (left,right):p.mu.mem_write(entry+20,b'\1\0')
  for off in (0,8):p.put_uint(fog+off,head)
  p.put_uint(fog+4,right);p.put_uint(fog+12,0x7ac95aec);p.put_uint(fog+16,0x75cf48);p.mu.mem_write(fog+20,b'\0\0')
  p.put_uint(right+8,fog);p.put_uint(head+8,fog);p.put_uint(tree+0x1c,4)
  p.put_uint(0x75cf48,0x7ac95aec);p.put_uint(0x75cf48+0x48,0x755310);p.put_uint(0x75cf48+0x4c,0x419e90)
  self.check(self.call(0x4143f0,this=tree,args=(0x7ac95aec,))&255==1,'actual Fog membership in declared four-entry RTTI input')
  serializer=self.call(0x43b830);self.call(0x422d90,this=manager,args=(0x7ac95aec,serializer,0xff,3))
 def on_skin_read_completed(self):
  super().on_skin_read_completed();p=self.p;fog=p.uint(self.loaded_skin+0x24);self.loaded_fog=fog
  self.check(fog not in self.freed and self.allocations[fog]==0x28 and p.uint(fog+8)&65535==1,'same decoded Fog has one owning Skin reference')
  self.fog_capture=bytes(p.mu.mem_read(fog+0x14,20)).hex()
  self.check(self.fog_capture==self.fog_input[2:22].hex(),'whole Skin read preserves all Fog raw bits')
 def on_render_completed(self,skin,bone):
  super().on_render_completed(skin,bone);p=self.p
  self.check(p.visits.get(0x4ad390) and p.uint(self.renderer+0xca20)==self.loaded_fog,'same Skin-owned Fog reaches actual renderer identity cache')
  self.fog_cache=[p.uint(self.renderer+0xe4f4+4*k) for k in INDICES]
  self.check(p.uint(skin+0x24)==self.loaded_fog,'renderer borrows without replacing Skin Fog owner')
def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded owning Fog/Skin graph')
 f=FogSkin(mode);device_mode=mode if mode in ('failed-device','post-false') else 'normal';draw=render(device_mode,True,f)
 f.arena.assert_engine_released();f.check(f.loaded_fog in f.freed and f.loaded_material in f.freed,'actual Skin releases both owning material and Fog references')
 f.check(f.arena.peak_reserved<=65536,'unchanged fixed arena')
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),f.mesh_input.hex(),
          [mode,f.fog_capture,f.fog_cache,[f.material_capture,[f.mesh_capture,[f.animation_capture,draw]]]]]
 if not return_capture:print('SKIN_FOG_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: owning Fog material SAN scene mesh Skin {mode}; read={f.read_instructions}; '
       f'render={f.render_instructions}; peakArena={f.arena.peak_reserved}; owners={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
