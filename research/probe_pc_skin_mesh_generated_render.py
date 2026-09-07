#!/usr/bin/env python3
"""Actual mesh object reader/COM bytes -> same decoded Skin -> generating draw."""
from pathlib import Path
import sys,struct,json
from pc_instruction_emulator import run_bounded
from probe_pc_skin_loaded_render import Loaded,specimen
from probe_pc_skin_generated_render import GenerationDevice
from probe_pc_skin_render import main as render
from pc_compact_mesh_device import install_mesh_device,clear_mesh_device
from pc_loader_fixtures import empty_manager

MODES=('normal','failed-device')
def mesh_specimen():
 vertices=struct.pack('<21f',*range(21));indices=struct.pack('<3H',0,1,2)
 payload=struct.pack('<IIIIB',0xdeadbeef,999,777,555,0xa5)+struct.pack('<III',2,1,0)+indices+struct.pack('<III',0x803,3,0)+vertices
 return struct.pack('<II',0x33c34cf0,0x4f4f4253)+bytes((0xa1,len(payload)))+payload+b'\0',vertices,indices

class MeshRead:
 def prepare_generation_device(self,mode,char_traits_ready=False,renderer_size=0xf364):
  p=self.p
  # Withdraw only the completed reader's explicitly prepared RTTI lookup tree.
  # The persistent original RTTI records remain in the image for IsKindOf.
  rtti=p.uint(0x755378);head=p.uint(rtti+0x18);entry=p.uint(head+4)
  self.check({rtti,head,entry}.issubset(self.reader_rtti_inputs),'known completed RTTI input allocations')
  p.put_uint(0x755378,0)
  for address in self.reader_rtti_inputs:self.arena.release_raw(address)
  super().prepare_generation_device(mode,char_traits_ready,0xf364);install_mesh_device(self)
  before=set(self.arena.live);manager,_=empty_manager(self);inputs=set(self.arena.live)-before
  p.put_uint(manager+0x10,2);p.put_uint(0x75dde8,manager)
  self.mesh_input,wanted,indices=mesh_specimen();self.data=self.mesh_input;self.position=0
  serializer=self.call(0x4297c0);self.loaded_mesh=self.call(0x42afd0,this=serializer,args=(self.stream,))
  self.check(self.call(0x429bc0,this=serializer+0x10,args=(self.stream,self.loaded_mesh))&255==1,'whole original DXMeshData field reader')
  self.mesh_read_instructions=sum(p.visits.values());mesh=self.loaded_mesh
  self.check(self.position==len(self.data) and not self.errors and all(p.visits.get(a) for a in (0x429a40,0x4aa000,0x4ae0e0)),
             'actual CPU readers/materialization/declaration complete')
  vb=self.mesh_buffers[p.uint(p.uint(mesh+0x58)+0x10)];ib=self.mesh_buffers[p.uint(p.uint(mesh+0x54)+0x10)]
  vertex_bytes=bytes(p.mu.mem_read(vb['data'],vb['size']));index_bytes=bytes(p.mu.mem_read(ib['data'],ib['size']))
  self.check(vertex_bytes==wanted and index_bytes==indices,'serialized bytes survive actual CPU-to-COM copy')
  self.mesh_capture=[[p.uint(mesh+off) for off in (0x44,0x70,0x74,0x48,0x4c,0x78,0x7c,0x80)],vertex_bytes.hex(),index_bytes.hex(),self.declaration_arrays]
  self.call(p.uint(p.uint(serializer)),this=serializer,args=(1,));self.call(0x4228a0,this=manager);p.put_uint(0x75dde8,0)
  for address in inputs:self.arena.release_raw(address)
  if hasattr(self,'after_mesh_read'):self.after_mesh_read()
  self.arena.retire_initial_stream_inputs()
 def observe(self,p,name,argc):
  if name in ('declaration','indices','stream') and hasattr(self,'mesh_com_tokens'):
   args=[p.uint(p.reg('ESP')+4+4*i) for i in range(argc)]
   self.check(args[0]==self.device,'decoded mesh binds actual explicit COM device')
   handle_slot=2 if name=='stream' else 1;args[handle_slot]=self.mesh_com_tokens.get(args[handle_slot],args[handle_slot])
   self.events.append([name,*args[1:],self.state()]);p.fixture_return(argc*4,eax=0x80004005 if self.fail else 0);return
  super().observe(p,name,argc)
 def on_render_teardown(self):clear_mesh_device(self)

class MeshGenerated(MeshRead,GenerationDevice,Loaded):
 def __init__(self,mode):
  super().__init__('normal',False,True)
  self.prepare_generation_device(mode)

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded decoded-mesh Skin draws')
 f=MeshGenerated(mode);draw=render(mode,True,f);f.arena.assert_engine_released()
 f.check(set(f.sdk.released)=={f.sdk.buffer,f.sdk.table} and f.shader_events==[['create',7],['release',7]],'all SDK and shader handle owners released')
 f.check(f.arena.peak_reserved<=65536,'unchanged total engine arena cap')
 directory,payload=specimen('normal')
 capture=[directory.hex(),payload.hex(),f.mesh_input.hex(),[f.mesh_capture,[f.sdk.calls,draw,f.shader_events]]]
 if not return_capture:print('MESH_GENERATED_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: decoded mesh to generating Skin {mode}; meshRead={f.mesh_read_instructions}; '
       f'render={f.render_instructions}; peakArena={f.arena.peak_reserved}; ownerGenerations={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
