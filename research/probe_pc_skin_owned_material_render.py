#!/usr/bin/env python3
"""Whole Skin reader owns material and bone; SAN/scene/decoded mesh -> draw."""
from pathlib import Path
import sys,json,struct
from pc_instruction_emulator import run_bounded
from probe_pc_skin_scene_mesh_render import SceneMeshGenerated,MODES
from probe_pc_skin_loaded_render import specimen
from probe_pc_skin_material_mesh_render import material_specimen
from probe_pc_skin_render import main as render
from probe_pc_node_serializer import field

class OwnedMaterialSceneMesh(SceneMeshGenerated):
 generation_expected=False
 use_material_as_fallback=False
 def skin_specimen(self,mode):
  directory,payload=specimen(mode);material=material_specimen()
  directory=struct.pack('<I',2)+directory[4:]+struct.pack('<IHIII',8,0,0x6160348b,0,len(material))
  return directory,field(0,struct.pack('<II',8,len(material))+material)+payload
 def configure_skin_reader(self,fat,manager):
  p=self.p;tree=p.uint(0x755378);head=p.uint(tree+0x18);root=p.uint(head+4)
  left=p.allocate(24);right=p.allocate(24);self.reader_rtti_inputs.extend((left,right))
  p.put_uint(root,left);p.put_uint(root+8,right);p.put_uint(root+12,0x6160348b);p.put_uint(root+16,0x75d548)
  p.put_uint(head,left);p.put_uint(head+8,right);p.put_uint(tree+0x1c,3)
  for entry,identity,record in ((left,0x234c576b,0x75ffa8),(right,0x695c0f65,0x75dd88)):
   for off in (0,8):p.put_uint(entry+off,head)
   p.put_uint(entry+4,root);p.put_uint(entry+12,identity);p.put_uint(entry+16,record);p.mu.mem_write(entry+20,b'\0\0')
  p.put_uint(0x75d548+0x4c,0x41a390);p.put_uint(0x75ffa8+0x4c,0x460e50)
  for record,identity,parent in ((0x75d548,0x6160348b,0x75dfd0),(0x75dfd0,0x5c0314c5,0x755310),
                                (0x75ffa8,0x234c576b,0x75df70),(0x75df70,0x7f577c6d,0x755310),
                                (0x7630e8,0x797b39ec,0x75dfd0)):
   p.put_uint(record,identity);p.put_uint(record+0x48,parent)
  for identity in (0x234c576b,0x6160348b,0x695c0f65):
   self.check(self.call(0x4143f0,this=tree,args=(identity,))&255==1,'actual membership in declared three-entry RTTI tree')
  serializer=self.call(0x42f690);self.call(0x422d90,this=manager,args=(0x6160348b,serializer,0xff,3))
 def on_skin_read_completed(self):
  p=self.p;material=p.uint(self.loaded_skin+0x20);self.loaded_material=material
  self.check(material not in self.freed and self.allocations[material]==0xbc and p.uint(material)==0x6ef264,
             'whole Skin reader creates and retains canonical DXMaterial')
  self.check(p.uint(material+8)&65535==1 and p.uint(material+0x48)==1,'one owning Skin reference and decoded pass')
  owner=p.uint(material+0x4c);self.check(p.uint(owner+0x14)==2,'both decoded Std layer owners')
  layers=[]
  for i in range(2):
   layer=p.uint(owner+0x18+4*i);texture=p.uint(layer+0x10)
   self.check(self.allocations[layer]==0x14 and self.allocations[texture]==0x68,'actual nested layer and texture')
   layers.append(bytes(p.mu.mem_read(texture+0x10,36)).hex())
  self.material_capture=[[p.uint(material+0x18+4*i) for i in range(11)],
                         [p.uint(material+0x78+4*i) for i in range(17)],p.uint(owner+0x10),layers]
 def prepare_shader(self):
  p=self.p;manager=self.call(0x4c9680);p.put_uint(0x763024,manager);shader=self.call(0x4c9f10)
  for name,start,count in ((b'BlendMatrices',0,3),(b'MatDiffuse',3,1)):
   words=struct.unpack('<11I',name.ljust(32,b'\0')+struct.pack('<III',0xdeadbeef,start,count))
   self.call(0x4af940,this=shader,args=words)
  # Native4BE405 sets bit24 for decoded specular power3.5.
  pair=p.allocate(12);output=p.allocate(4);p.mu.mem_write(pair,struct.pack('<III',getattr(self,'cached_shader_key',0x1020011),getattr(self,'cached_shader_types',0),shader))
  self.call(0x4c87a0,this=manager+0x44,args=(output,p.uint(manager+0x48),pair))
  self.arena.release_raw(output);self.arena.release_raw(pair);self.shader_manager=manager;self.cached_shader=shader;return manager
 def on_render_completed(self,skin,bone):
  p=self.p;self.render_instructions=sum(p.visits.values())
  self.check(not p.visits.get(0x4c89eb) and p.uint(self.shader_manager+0x4c)==1,'actual shader cache hit')
  self.check(p.uint(skin+0x20)==self.loaded_material==p.uint(self.renderer+0xc18c)
             and p.uint(self.renderer+0xc9c0)!=self.loaded_material,'owned colored material selected instead of distinct white fallback')
  self.check(not self.sdk.calls and not self.shader_events,'cached branch has no SDK or device shader creation')

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded owned-material SAN/scene/mesh draws')
 delta,device_mode=MODES[mode];f=OwnedMaterialSceneMesh(delta,device_mode);draw=render(device_mode,True,f)
 f.arena.assert_engine_released();f.check(f.loaded_material in f.freed,'actual Skin destructor releases owned decoded material')
 f.check(f.arena.peak_reserved<=65536,'unchanged fixed64KiB arena')
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),f.mesh_input.hex(),
          [f.material_capture,[f.mesh_capture,[f.animation_capture,draw]]]]
 if not return_capture:print('OWNED_MATERIAL_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: owned material SAN/scene/mesh Skin {mode}; skinRead={f.read_instructions}; '
       f'meshRead={f.mesh_read_instructions}; render={f.render_instructions}; peakArena={f.arena.peak_reserved}; '
       f'ownerGenerations={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
