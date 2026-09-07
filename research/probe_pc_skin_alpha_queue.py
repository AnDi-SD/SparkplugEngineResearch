#!/usr/bin/env python3
"""Whole owning Skin reader -> actual transparent enqueue/pre gate, fixed arena."""
from pathlib import Path
import sys,json,struct
from pc_instruction_emulator import run_bounded
from probe_pc_skin_loaded_render import Loaded,specimen
from probe_pc_skin_owned_material_render import OwnedMaterialSceneMesh
from probe_pc_skin_material_mesh_render import material_specimen
from probe_pc_node_serializer import field
from probe_pc_function_eval import cleanup

MODES=('queued','depth-only','queue-disabled','capacity','already-flushing','alpha-disabled','object-disabled','zero-blend','priority-wrap')
IDENTITY=(1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.)
class AlphaSkin(Loaded):
 configure_skin_reader=OwnedMaterialSceneMesh.configure_skin_reader
 on_skin_read_completed=OwnedMaterialSceneMesh.on_skin_read_completed
 def __init__(self,mode):self.alpha_mode=mode;super().__init__('normal',False,True)
 def skin_specimen(self,mode):
  directory,payload=specimen(mode);material=material_specimen()
  if self.alpha_mode!='zero-blend':material=material.replace(field(3,struct.pack('<I',0)),field(3,struct.pack('<I',1)),1)
  directory=struct.pack('<I',2)+directory[4:]+struct.pack('<IHIII',8,0,0x6160348b,0,len(material))
  return directory,field(0,struct.pack('<II',8,len(material))+material)+payload

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded Skin alpha gate')
 f=AlphaSkin(mode);p=f.p
 # Same fixed arena, including all renderer storage. No legacy external arena.
 renderer=f.arena.allocate_raw_high(0xc9e0);p.put_uint(0x75db68,renderer)
 p.put_uint(0x75e150,0x603625d0);p.put_uint(0x75e150+0x48,0x75dd88)
 support=f.call(0x425520);camera=p.allocate(0x238);skin=f.loaded_skin
 mesh=f.call(0x4a9e80);p.put_floats(mesh+0x18,(1.,2.,3.,1.));f.call(0x479e20,this=skin,args=(mesh,))
 p.put_floats(support+0x138,(*IDENTITY[:12],10.,20.,30.,1.));p.put_floats(camera+0xcc,(*IDENTITY[:12],-1.,-2.,-3.,1.))
 p.mu.mem_write(renderer+0x45,bytes([mode!='queue-disabled']));p.mu.mem_write(renderer+0x44,bytes([mode=='already-flushing']))
 p.mu.mem_write(renderer+0xc9d8,bytes([mode!='alpha-disabled']));p.mu.mem_write(camera+0x231,bytes([mode=='depth-only']))
 p.mu.mem_write(skin+0x18,bytes([mode!='object-disabled']));p.put_uint(skin+0x1c,13)
 p.put_uint(renderer+0x48,0xfffffff9 if mode=='priority-wrap' else 0);p.put_uint(renderer+0x4c,2048 if mode=='capacity' else 0)
 events=[];rx=0x340b0000;p.mu.mem_map(rx,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
 def pre(m):
  f.check([m.uint(m.reg('ESP')+4+4*i) for i in range(3)]==[skin,camera,support+0xb4],'actual pre callback receiver and borrowed inputs')
  events.append(['pre',p.uint(renderer+0x4c),p.uint(renderer+0x44)&255]);m.fixture_return(eax=0)
 p.put_uint(skin+0x2c,rx+0x10);p.seams[rx+0x10]=pre
 references=[p.uint(obj+8)&65535 for obj in (skin,f.loaded_material,f.loaded_bone,support)]
 result=f.call(0x46a240,this=skin,args=(camera,support+0xb4))&255;instructions=sum(p.visits.values())
 queued=mode in ('queued','depth-only','queue-disabled','capacity','priority-wrap')
 f.check(result==0 and bool(p.visits.get(0x454c30))==queued,'whole Skin pre routing and false result after every enqueue outcome')
 f.check(bool(events)==(not queued) and not p.visits.get(0x461d70),'alpha enqueue precedes callbacks and palette reads')
 f.check([p.uint(obj+8)&65535 for obj in (skin,f.loaded_material,f.loaded_bone,support)]==references,'queue borrows all decoded object/support/camera relationships')
 f.check(bool(p.visits.get(0x46a230))==(queued and mode!='queue-disabled'),'capacity check follows actual Skin sphere getter; disabled precedes it')
 record=renderer+0x50
 row=[1 if p.uint(record)==camera else 0,2 if p.uint(record+4)==support+0xb4 else 0,3 if p.uint(record+8)==skin else 0,
      p.uint(record+12),p.uint(record+16),p.uint(record+20)&255]
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),[mode,result,p.uint(renderer+0x4c),p.uint(renderer+0x44)&255,events,row]]
 cleanup(f,(skin,f.loaded_bone,support,*tuple(p.uint(a) for a in (0x75db78,) if p.uint(a))));f.arena.assert_engine_released()
 f.check(f.loaded_material in f.freed and f.arena.peak_reserved<=65536,'original owning teardown and unchanged fixed arena')
 if not return_capture:print('SKIN_ALPHA_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: whole owning Skin alpha {mode}; read={f.read_instructions}; gate={instructions}; peakArena={f.arena.peak_reserved}; owners={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
