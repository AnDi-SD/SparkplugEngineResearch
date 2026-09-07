#!/usr/bin/env python3
"""Whole original Skin->DXMesh->material->cached shader->device draw chain."""
from pathlib import Path
import sys,struct,json
from pc_instruction_emulator import run_bounded
from probe_pc_renderer_submit import SubmitFixture
from probe_pc_function_eval import cleanup

class Fixture(SubmitFixture):
 def state(self):
  return [self.tokens[self.p.uint(self.renderer+off)] for off in (0xc9fc,0xca04,0xca08)]+[self.p.uint(self.renderer+0xc9bc)]
 def observe(self,p,name,argc):
  args=[p.uint(p.reg('ESP')+4+4*i) for i in range(argc)]
  if name=='transform':
   self.events.append([name,args[1],[p.uint(args[2]+4*i) for i in range(16)],self.state()])
   p.fixture_return(argc*4,eax=0x80004005 if self.fail else 0);return
  super().observe(p,name,argc)

MODES=('normal','pre-false','post-false','failed-device','weights-zero')
def main(mode,return_capture=False,fixture=None):
 if mode not in MODES:raise ValueError('bounded unlit Skin render specimen')
 f=fixture or Fixture(mode=='failed-device');p=f.p;r=f.renderer;f.call(0x6d38e0)
 if hasattr(f,'loaded_material') and getattr(f,'use_material_as_fallback',True):material=f.loaded_material
 else:
  material=f.call(0x4a9460);p.put_uint(material+0xb8,0);p.put_uint(material+0x38,2)
  owner=f.call(0x45f610);f.call(0x423960,this=material,args=(0,owner))
  for j in range(2):f.call(0x45f5e0,this=owner,args=(j,f.call(0x460e50)))
 p.put_uint(r+0xc9c0,material)
 if hasattr(f,'loaded_mesh'):
  mesh=f.loaded_mesh;declaration=p.uint(mesh+0x84);ib=p.uint(mesh+0x54);vb=p.uint(mesh+0x58);f.buffers=[ib,vb]
 else:
  declaration=f.call(0x4c9c20);ib=f.call(0x4b2010);vb=f.call(0x4b1e30);f.buffers=[ib,vb]
  for obj in f.buffers:p.mu.mem_write(obj+8,b'\1\0')
  mesh=f.call(0x4a9e80)
  args=(ib,vb,2,11,13,17,19,declaration,32,0x803)
  for off,value in zip((0x54,0x58,0x50,0x78,0x48,0x7c,0x4c,0x84,0x74,0x44),args):p.put_uint(mesh+off,value)
 for i,obj in enumerate((declaration,ib,vb),1):f.tokens[obj]=i
 if fixture is None:
  skin=f.call(0x46a120);bone=f.call(0x421e20)
  p.put_floats(bone+0x74,(1,2,3));p.put_floats(bone+0x80,(2,3,4))
 else:skin=f.loaded_skin;bone=f.loaded_bone
 f.call(0x479e20,this=skin,args=(mesh,))
 palette=f.arena.allocate_raw_high(64) if getattr(f,'palette_high',False) else p.allocate(64);p.put_uint(r+0xc9b8,palette);p.mu.mem_write(palette,b'\xcc'*64)
 def allocate(n):p.run(0x417190,args=(n,),callee_pop=False);return p.reg('EAX')
 if fixture is None:
  pointers=allocate(4);matrix=allocate(64);p.put_uint(pointers,bone)
  p.put_floats(matrix,(1,0,0,0,0,1,0,0,0,0,1,0,5,6,7,1));f.call(0x46a100,this=skin,args=(1,pointers,matrix))
  if mode=='weights-zero':p.put_uint(skin+0x60,0)
 if hasattr(f,'prepare_shader'):manager=f.prepare_shader()
 else:
  manager=f.call(0x4c9680);p.put_uint(0x763024,manager);shader=f.call(0x4c9f10)
  for name,kind,start,count in ((b'BlendMatrices',5,0,3),(b'MatDiffuse',8,3,1)):
   words=struct.unpack('<11I',name.ljust(32,b'\0')+struct.pack('<III',0xdeadbeef,start,count));f.call(0x4af940,this=shader,args=words)
  pair=p.allocate(12);output=p.allocate(4);p.mu.mem_write(pair,struct.pack('<III',0x20011,0,shader));f.call(0x4c87a0,this=manager+0x44,args=(output,p.uint(manager+0x48),pair))
 p.put_uint(r+0xc9bc,9)
 def callback(m,name):
  if m.uint(m.reg('ESP')+4)!=skin:raise AssertionError('original callback receiver')
  if hasattr(f,'on_callback'):f.on_callback(name)
  f.events.append([name,f.state()]);m.fixture_return(eax=0 if mode==name+'-false' else 1)
 for off,entry,name in ((0x2c,0x34090e00,'pre'),(0x30,0x34090e10,'post')):
  p.put_uint(skin+off,entry);p.seams[entry]=lambda m,n=name:callback(m,n)
 if hasattr(f,'before_render'):f.before_render(skin,bone)
 f.events.clear();result=f.invoke_render(skin,mode) if hasattr(f,'invoke_render') else f.call(0x46a240,this=skin,args=(0,0))&255
 instructions=sum(p.visits.values());checks=0
 if hasattr(f,'on_render_completed'):f.on_render_completed(skin,bone)
 def check(ok,label):
  nonlocal checks
  checks+=1
  if not ok:raise AssertionError(label)
 check(result==int(mode not in ('pre-false','post-false')),'actual Skin return semantics')
 check(p.uint(r+0xc9bc)==(9 if mode=='pre-false' else 1 if mode=='post-false' else 0),'count published after transform and cleared only on complete success')
 if mode=='pre-false':check(len(f.events)==1 and bytes(p.mu.mem_read(palette,64))==b'\xcc'*64,'pre failure has no palette/world/mesh side effects')
 else:
  check(all(p.visits.get(a) for a in (0x461d70,0x426b00,0x462680,0x4bbb60,0x4bc670,0x4bc4a0,0x4bc290,0x4c8980,0x4ae930,0x4be210)),'all original palette/matrix/mesh/material/cached-shader bodies executed')
  check(bool(p.visits.get(0x4c89eb))==getattr(f,'generation_expected',hasattr(f,'prepare_shader')),'declared cache hit or actual complete generating miss')
  if not hasattr(f,'animation_capture'):
   check(p.floats(palette+48,4)==(11,20,31,1),'noncommuting inverseBind translation times cached world scale/position')
  else:
   check(bytes(p.mu.mem_read(palette,64))!=bytes(p.mu.mem_read(p.uint(skin+0x6c),64)),
         'animated cached bone changes the inverse-bind palette input')
  transform=next((event for event in f.events if event[0]=='transform'),None)
  check(transform and transform[-1][-1]==9 and f.events[-1]==['post',[1,2,3,1]],'world device sees old count; post sees active count')
  check(any(e[0]=='indexed' for e in f.events) and any(e[0]=='vertex-constants' for e in f.events),'palette reaches actual constant upload and indexed draw')
 check(p.uint(skin+0x60)==(0 if mode=='weights-zero' else 4),'weight word is untouched and unused by this render caller')
 capture=[mode,result,p.uint(skin+0x60),p.uint(r+0xc9bc),bytes(p.mu.mem_read(palette,64)).hex(),
  [p.uint(r+0xca40+4*i) for i in range(16)],int.from_bytes(p.mu.mem_read(r+0xf2f4,1),'little'),p.uint(r+0xc194),list(f.events)]
 for obj in (skin,bone,material,declaration,ib,vb,manager):
  if obj not in f.freed:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
 if hasattr(f,'on_render_teardown'):f.on_render_teardown()
 cleanup(f,tuple(p.uint(a) for a in (0x75db78,) if p.uint(a)))
 check(set(f.allocations)==set(f.freed),'all native objects and helper allocations released; prepared renderer lifetime is external')
 if not return_capture:print('SKIN_RENDER_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {checks}/{checks}: original Skin render {mode}; instructions={instructions}; heap={p.allocated}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
