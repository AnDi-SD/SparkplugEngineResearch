#!/usr/bin/env python3
"""Decoded material + real SAN/scene/mesh -> same Skin generated draw."""
from pathlib import Path
import sys,json,struct
from pc_instruction_emulator import run_bounded
from probe_pc_skin_scene_mesh_render import SceneMeshGenerated,MODES
from probe_pc_skin_loaded_render import specimen
from probe_pc_skin_render import main as render
from probe_pc_node_serializer import field

def material_specimen():
 states=(0,0,1,2,1,1,3,0,2,1,6)
 fields=field(0,struct.pack('<11I',*states))+field(2,struct.pack('<4If',0,0xff804020,0xff102030,0x00112233,3.5))
 fields+=field(3,struct.pack('<I',0))+field(4,struct.pack('<I',0x234c576b))
 fields+=field(17,struct.pack('<9I',0,2,1,0,0,0xff112233,1,0,0))+field(4,struct.pack('<I',0x234c576b))+b'\0'
 return struct.pack('<II',0x6160348b,0x4f4f4253)+fields

class MaterialSceneMesh(SceneMeshGenerated):
 def on_skin_read_completed(self):
  p=self.p;tree=p.uint(0x755378);entry=p.uint(p.uint(tree+0x18)+4)
  saved=bytes(p.mu.mem_read(entry+12,8));p.put_uint(entry+12,0x234c576b);p.put_uint(entry+16,0x75ffa8)
  p.put_uint(0x75ffa8+0x4c,0x460e50)
  for record,identity,parent in ((0x75ffa8,0x234c576b,0x75df70),(0x75df70,0x7f577c6d,0x755310),
                                 (0x7630e8,0x797b39ec,0x75dfd0),(0x75dfd0,0x5c0314c5,0x755310)):
   p.put_uint(record,identity);p.put_uint(record+0x48,parent)
  self.material_input=material_specimen();self.data=self.material_input;self.position=0
  serializer=self.call(0x42f690);self.loaded_material=self.call(0x42f4c0,this=serializer,args=(self.stream,));material=self.loaded_material
  self.check(p.uint(material)==0x6ef264 and self.allocations[material]==0xbc,'specialized material header creates actual DXMaterial')
  self.check(self.call(0x42f670,this=serializer+0x10,args=(self.stream,material))&255==1,'whole common material/layer reader')
  self.material_read_instructions=sum(p.visits.values())
  self.check(self.position==len(self.data) and not self.errors and p.uint(material+0x48)==1,'all material fields consumed and one owned pass')
  owner=p.uint(material+0x4c);self.check(p.uint(owner+0x14)==2,'two actual Std layers from runtime RTTI factory')
  layers=[]
  for i in range(2):
   layer=p.uint(owner+0x18+4*i);texture=p.uint(layer+0x10)
   self.check(self.allocations[layer]==0x14 and self.allocations[texture]==0x68,'actual nested Std/MaterialTexture owners')
   layers.append(bytes(p.mu.mem_read(texture+0x10,36)).hex())
  self.material_capture=[[p.uint(material+0x18+4*i) for i in range(11)],
                         [p.uint(material+0x78+4*i) for i in range(17)],p.uint(owner+0x10),layers]
  self.call(p.uint(p.uint(serializer)),this=serializer,args=(1,));p.mu.mem_write(entry+12,saved)
  self.check(material not in self.freed,'same decoded material survives serializer teardown')

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded material/SAN/scene/mesh draws')
 delta,device_mode=MODES[mode];f=MaterialSceneMesh(delta,device_mode);draw=render(device_mode,True,f);f.arena.assert_engine_released()
 f.check(set(f.sdk.released)=={f.sdk.buffer,f.sdk.table} and f.shader_events==[['create',7],['release',7]],'all SDK and shader handle lifetimes')
 f.check(f.arena.peak_reserved<=65536,'same fixed64KiB arena')
 directory,payload=specimen('normal')
 capture=[directory.hex(),payload.hex(),f.mesh_input.hex(),f.material_input.hex(),
          [f.material_capture,[f.mesh_capture,[f.animation_capture,[f.sdk.calls,draw,f.shader_events]]]]]
 if not return_capture:print('MATERIAL_MESH_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: material/SAN/scene/mesh Skin {mode}; materialRead={f.material_read_instructions}; '
       f'meshRead={f.mesh_read_instructions}; render={f.render_instructions}; peakArena={f.arena.peak_reserved}; '
       f'ownerGenerations={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
