#!/usr/bin/env python3
"""Decoded native mip -> same material holder -> full Fog/SAN/mesh/Skin draw."""
from pathlib import Path
import sys,json,struct
from pc_instruction_emulator import run_bounded
from probe_pc_skin_fog_render import FogSkin
from probe_pc_skin_render import main as render
from probe_pc_renderer_fog import INDICES
from probe_pc_node_serializer import field
from pc_loader_fixtures import empty_manager
from pc_compact_texture_device import install_texture_device

MODES=('raw','dxt1','dxt3','dxt5','failed-device','post-false')
def texture_specimen(mode):
 kind=mode if mode.startswith('dxt') else 'raw';flags,row,format_={'raw':(0,4,0x15),'dxt1':(1,8,0x31545844),'dxt3':(2,16,0x33545844),'dxt5':(3,16,0x35545844)}[kind]
 pixels=bytes(range(1,row+1));native=field(0,b'\1'+struct.pack('<3I',1,1,flags)+b'\1'+struct.pack('<3I',1,row,1)+pixels)+b'\0'
 return field(2,b'\0')+b'\0'+field(6,struct.pack('<I',6))+field(1,native)+b'\0',pixels,flags,format_
class TextureSkin(FogSkin):
 arena_alignment=8
 arena_best_fit=True
 use_material_as_fallback=True
 def __init__(self,mode):self.texture_mode=mode;super().__init__('failed-device' if mode=='failed-device' else 'linear')
 def after_mesh_read(self):
  p=self.p;self.texture_input,pixels,flags,format_=texture_specimen(self.texture_mode);install_texture_device(self,len(pixels));io=self.texture_io
  for record,identity,parent in ((0x763210,0x3f3651b6,0x75df10),(0x75df10,0x2f281e13,0x7555f8),
                                (0x7555f8,0x44de07fd,0x755310),(0x755310,0x415352a1,0)):
   p.put_uint(record,identity);p.put_uint(record+0x48,parent)
  before=set(self.arena.live);manager,_=empty_manager(self);raw_inputs=set(self.arena.live)-before
  p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(0x75dde8,manager)
  self.data=self.texture_input;self.position=0;texture=self.call(0x4ab520);self.loaded_texture=texture;serializer=self.call(0x42b660)
  self.check(self.call(0x42c640,this=serializer+0x10,args=(self.stream,texture))&255==1,'whole original native TextureData-to-DXTexture reader')
  self.texture_read_instructions=sum(p.visits.values())
  self.check(self.position==len(self.data) and not self.errors and p.visits.get(0x4abba0),'actual native mip attachment completed')
  self.check(io.events[0]==['CreateTexture',1,1,0,0,format_,1,0] and bytes(p.mu.mem_read(io.pixel_storage,len(pixels)+4))==pixels+b'\xa5'*4,
             'actual native rows copied into declared COM surface with padding intact')
  self.check(p.uint(texture+0x44)==0xcccccccc and p.uint(texture+0x48)==0,'native mip attachment leaves runtime format uninitialized and bytecount zero')
  self.texture_capture=[[p.uint(texture+off)&(255 if off in (0x1c,0x24) else 0xffffffff) for off in (0x18,0x1c,0x20,0x24,0x28,0x2c)],pixels.hex()]
  self.call(p.uint(p.uint(serializer)),this=serializer,args=(1,));self.call(0x4228a0,this=manager);p.put_uint(0x75dde8,0)
  for address in raw_inputs:self.arena.release_raw(address)
  holder=p.uint(p.uint(p.uint(self.loaded_material+0x4c)+0x18)+0x10);self.call(0x41e870,this=holder,args=(texture,))
  self.check(p.uint(texture+8)&65535==1,'actual material texture setter owns decoded DXTexture')
 def observe(self,p,name,argc):
  if name=='texture':
   device,stage,handle=[p.uint(p.reg('ESP')+4+4*i) for i in range(3)]
   self.check(device==self.device and stage==0 and handle==self.texture_io.texture,'whole material pass binds same decoded COM texture')
   self.events.append([name,stage,9,self.state()]);p.fixture_return(argc*4,eax=0x80004005 if self.fail else 0);return
  super().observe(p,name,argc)
 def on_render_completed(self,skin,bone):
  p=self.p;self.render_instructions=sum(p.visits.values())
  self.check(not p.visits.get(0x4c89eb) and not self.sdk.calls and not self.shader_events,'declared cached shader branch')
  self.check(p.uint(self.renderer+0xc18c)==self.loaded_material and p.uint(self.renderer+0xca20)==self.loaded_fog,'same owning material and Fog selected')
  self.check(p.visits.get(0x4bb650) and p.uint(self.renderer+0xe480)==self.loaded_texture,'actual RTTI texture resolution publishes decoded identity')
  self.check(any(event[:3]==['texture',0,9] for event in self.events),'decoded texture reaches external device before draw')
  self.fog_cache=[p.uint(self.renderer+0xe4f4+4*k) for k in INDICES]
def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded decoded textured Skin')
 f=TextureSkin(mode);device_mode=mode if mode in ('failed-device','post-false') else 'normal';draw=render(device_mode,True,f)
 f.arena.assert_engine_released();io=f.texture_io
 f.check(f.loaded_texture in f.freed and io.device_refs==1 and io.texture_refs==io.surface_refs==0 and not io.locked,'original material/Skin/DXTexture/COM owner teardown')
 f.check(f.arena.peak_reserved<=65536,'fixed engine arena unchanged')
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),f.mesh_input.hex(),f.texture_input.hex(),
          [mode,f.texture_capture,[f.fog_capture,f.fog_cache,[f.material_capture,[f.mesh_capture,[f.animation_capture,draw]]]]]]
 if not return_capture:print('TEXTURED_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: decoded textured Skin {mode}; read={f.read_instructions}; textureRead={f.texture_read_instructions}; '
       f'render={f.render_instructions}; peakArena={f.arena.peak_reserved}; owners={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
