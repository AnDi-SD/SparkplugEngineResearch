#!/usr/bin/env python3
"""Same decoded Skin graph -> whole generating shader miss -> constants/draw.

Original engine bodies run; explicit SDK/device output is not GPU-valid shader
evidence. Best-fit/high-end caller placement stays inside the same64KiB arena.
"""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from probe_pc_skin_loaded_render import Loaded,specimen
from probe_pc_skin_render import main as render
from pc_stl_fixtures import install_char_traits
from pc_crt_format_fixtures import install_sprintf
from pc_shader_compiler_fixtures import ShaderCompilerOutput

MODES=('normal','failed-device')

class GenerationDevice:
 def prepare_generation_device(self,mode,char_traits_ready=False,renderer_size=0xf2f8):
  p=self.p;self.shader_events=[]
  self.arena.best_fit=True
  self.prepare_device(mode=='failed-device',renderer_size,0x1b8,True,self.arena.allocate_raw_high(renderer_size));self.prepare_submission_device()
  if not char_traits_ready:install_char_traits(p)
  install_sprintf(p)
  self.sdk=ShaderCompilerOutput(p,constants=getattr(self,'shader_reflection',(('BlendMatrices',0,3),('MatDiffuse',3,1))),compact_storage=True)
  self.handle=0x34090800;p.put_uint(self.handle,0x34090810);p.put_uint(0x34090818,0x34090b10)
  p.put_uint(p.uint(self.device)+0x16c,0x34090b00);p.seams[0x34090b00]=self.create;p.seams[0x34090b10]=self.release
 def create(self,p):
  device,code,output=[p.uint(p.reg('ESP')+4+4*i) for i in range(3)]
  self.check(device==self.device and bytes(p.mu.mem_read(code,5))==self.sdk.bytecode,'same just-generated owned bytecode reaches actual device creation')
  self.generated_shader=output-0x50
  p.put_uint(output,self.handle);self.events.append(['create',self.sdk.bytecode.hex(),self.state()]);self.shader_events.append(['create',7])
  p.fixture_return(12,eax=0x80004005 if self.fail else 0)
 def release(self,p):
  self.check(p.uint(p.reg('ESP')+4)==self.handle,'original generated shader releases supplied handle')
  self.shader_events.append(['release',7]);p.fixture_return(4,eax=0)
 def observe(self,p,name,argc):
  if name=='vertex-shader':
   handle=p.uint(p.reg('ESP')+8)
   self.check(handle in (0,self.handle),'actual just-created device shader selected')
   self.events.append([name,7 if handle else 0,self.state()]);p.fixture_return(argc*4,eax=0x80004005 if self.fail else 0);return
  super().observe(p,name,argc)
 def prepare_shader(self):
  p=self.p;empty=p.allocate(28);self.call(0x450d90,this=empty)
  template=self.arena.allocate_owned(0x68);self.call(0x4d0960,this=template,args=(1,empty,2))
  record=self.arena.allocate_owned(0x1dc);self.call(0x4d5120,this=record)
  for offset,value in ((0x48,record),(0x4c,record+0x1dc),(0x50,record+0x1dc)):p.put_uint(template+offset,value)
  self.call(0x4cffc0,this=record)
  for offset,value in ((0x38,b'Main'),(0x54,b'vs_2_0')):
   text=p.allocate(len(value)+1);p.mu.mem_write(text,value+b'\0');self.call(0x4cfe20,this=record+0xd4+offset,args=(text,));self.arena.release_raw(text)
  self.call(0x59a660,this=empty);self.arena.release_raw(empty)
  manager=self.call(0x4c9680);p.put_uint(manager+0x40,template);p.put_uint(0x763024,manager);self.shader_manager=manager;return manager
 def on_render_completed(self,skin,bone):
  p=self.p;self.render_instructions=sum(p.visits.values())
  self.check(all(p.visits.get(a) for a in (0x46a240,0x4bc670,0x4bc4a0,0x4c89eb,0x4c9f10,0x4cffe0,0x4af940,0x4ca030,0x4c87a0,0x4ae930,0x4be210)),
             'whole original Skin/mesh/material/generation/reflection/device/cache/constants/draw bodies')
  self.check(p.uint(self.shader_manager+0x4c)==1 and len(self.sdk.calls)==1,'one original generated cache entry and SDK request')
  self.check(p.uint(self.generated_shader+0x34)==getattr(self,'expected_constant_rows',4) and p.uint(self.generated_shader+0x50)==self.handle,'generated reflection row sum and handle retained')
  self.check(bytes(p.mu.mem_read(p.uint(self.generated_shader+0x4c),p.uint(self.generated_shader+0x48)))==self.sdk.bytecode,
             'shader retains all five opaque SDK bytecode bytes')

class Generated(GenerationDevice,Loaded):
 def __init__(self,mode):
  super().__init__('normal',False,True)
  self.prepare_generation_device(mode)

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded generated Skin draw')
 f=Generated(mode);draw=render(mode,True,f);f.arena.assert_engine_released()
 f.check(set(f.sdk.released)=={f.sdk.buffer,f.sdk.table},'all SDK COM result objects released')
 f.check(f.shader_events==[['create',7],['release',7]],'exact generated device handle lifetime')
 f.check(f.arena.peak_reserved<=65536,'unchanged fixed arena cap')
 directory,payload=specimen('normal');capture=[directory.hex(),payload.hex(),[f.sdk.calls,draw,f.shader_events]]
 if not return_capture:print('GENERATED_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: loaded Skin generating draw {mode}; readInstructions={f.read_instructions}; '
       f'renderInstructions={f.render_instructions}; peakArena={f.arena.peak_reserved}; ownerGenerations={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
