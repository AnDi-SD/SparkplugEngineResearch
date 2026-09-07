#!/usr/bin/env python3
"""Same decoded Skin/bone from read through real SAN actor and complete draw.

Completed phases share objects. Actor/animation infrastructure is destroyed
before renderer backing storage; this is not a simultaneous engine frame.
"""
from pathlib import Path
import sys,struct,json,hashlib
from pc_instruction_emulator import run_bounded
from probe_pc_skin_loaded_render import Loaded,specimen
from probe_pc_skin_render import main as render
from inspect_pc_san_keys import DEFAULT,inspect,u32
from probe_pc_san_reader import cstring
from pc_stl_fixtures import install_char_traits
from probe_pc_actor_tick import ActorFixture,bits
from probe_pc_animation_manager import registry_entries

MODES=('0.25','0.5','0.75','1.25')
ASSET_SHA256='706BD0E5C70111BBD7C9524B3A37B1B2D867EC86FDC5A7A7908FAC8BBFCA428E'

class Animated(Loaded):
 def update_world_phase(self):
  self.call(0x421420,this=self.loaded_bone,args=(1,))
 def set_name(self,p):
  raw=cstring(p,p.uint(p.reg('ESP')+4));old=p.uint(p.reg('ECX')+0x10)
  if old:self.arena.release_owned(old)
  entry=self.arena.allocate_owned(len(raw)+10)
  p.mu.mem_write(entry+8,b'\1'+raw+b'\0');p.put_uint(p.reg('ECX')+0x10,entry);p.fixture_return(4)
 def release_name(self,p):
  slot=p.reg('ECX');entry=p.uint(slot)
  if entry:self.arena.release_owned(entry);p.put_uint(slot,0)
  p.fixture_return()
 def event(self,p):
  args=[p.uint(p.reg('ESP')+4*i) for i in range(1,6)]
  self.check(p.reg('ECX')==self.actor and args[1]==self.engine+0x110,'external event sender/queue contract')
  self.actions.append(['event',args[0],int(args[4]==args[3])]);p.fixture_return(20)
 def flush(self,p):
  self.check(p.reg('ECX')==self.engine+0x110,'external flush receiver')
  self.actions.append(['flush']);p.fixture_return()
 def __init__(self,mode,prepare_render=True,retire_reader_inputs=False):
  super().__init__('normal',False,retire_reader_inputs);p=self.p;bone=self.loaded_bone;self.actions=[]
  del p.seams[0x454370];del p.seams[0x453b10];install_char_traits(p)
  p.put_uint(0x768e90,0x5daf152d)
  p.mu.mem_map(0x34030000,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
  p.put_uint(p.uint(0x5a241c+2),0x34030010);p.seams[0x34030010]=lambda m:ActorFixture.floor(None,m)
  p.seams[0x60dd44]=lambda m:ActorFixture.fmod(None,m)
  label=p.allocate(8);p.mu.mem_write(label,b'Plane01\0');self.call(0x4130f0,this=bone,args=(label,));self.arena.release_raw(label)
  self.engine=p.allocate(0x160);p.put_uint(0x755274,self.engine);request=p.allocate(0x38)
  p.seams[0x40fa10]=self.event;p.seams[0x413b40]=self.flush
  self.manager=self.call(0x454640);self.actor=self.call(0x5a3620)
  raw=(DEFAULT/'bbush.san').read_bytes()
  self.check(hashlib.sha256(raw).hexdigest().upper()==ASSET_SHA256,'pinned complete pristine SAN asset')
  self.data=raw[u32(raw,20)+8:];self.position=0
  animation=self.call(0x41a090);serializer=self.call(0x43dab0)
  self.check(self.call(0x43ecc0,this=serializer+0x10,args=(self.stream,animation))&255==1,'whole original SAN object reader')
  self.san_read_instructions=sum(p.visits.values())
  self.check(self.position==len(self.data) and p.uint(animation+0x20)==5 and not self.errors,'complete real SAN keys and tracks')
  name=p.uint(p.uint(animation+0x1c)+0x10)+9
  self.check(cstring(p,name)==cstring(p,p.uint(bone+0x10)+9),'selected decoded SAN name equals retained bone name')
  p.mu.mem_write(bone+8,struct.pack('<H',1)) # explicit external intrusive owner across actor teardown
  self.call(0x5a33f0,this=self.actor,args=(bone,))
  self.check(int.from_bytes(p.mu.mem_read(bone+8,2),'little')==2,'original actor retains externally owned decoded Node')
  for off,val in ((0,animation),(4,1),(0xc,bits(.25)),(0x18,bits(.5)),(0x20,bits(.75)),(0x30,bits(1))):p.put_uint(request+off,val)
  self.check(self.call(0x5a1e30,this=self.actor,args=(request,))==0,'actual start selects first state')
  self.actions.clear();p.put_uint(self.engine+0xb8,bits(float(mode)))
  self.call(0x4535a0,this=self.manager);self.tick_instructions=sum(p.visits.values())
  self.check(all(p.visits.get(a) for a in (0x4535a0,0x5a2380,0x479290)),'actual manager/actor/SAN track sampling bodies')
  state=p.uint(self.actor+0x28);controller=p.uint(p.uint(self.actor+0x40));evaluator=p.uint(controller+0x14)
  self.update_world_phase()
  self.check(p.uint(state+0x48)==1 and p.uint(controller+0x10)==bone and p.uint(evaluator+0x18)==state,'retained bone in real bound controller/evaluator')
  words=[p.uint(bone+offset+i*4) for offset,count in ((0x20,3),(0x30,3),(0x40,9),(0x74,3),(0x80,3),(0x8c,9)) for i in range(count)]
  self.animation_capture=[mode,p.uint(state+0x34),p.uint(self.manager+0x10),p.uint(state+0x48),p.uint(bone+0xb0),words]
  retained=bytes(p.mu.mem_read(bone+0x20,0x90))
  self.call(0x5a35a0,this=self.actor,args=(1,))
  for obj in (animation,serializer):self.call(p.uint(p.uint(obj)),this=obj,args=(1,))
  self.check(registry_entries(p,self.manager)=={},'actual actor/resource destruction releases all SAN name slots')
  self.call(0x4545d0,this=self.manager,args=(1,))
  debug=p.uint(0x75526c)
  if debug:self.call(p.uint(p.uint(debug)),this=debug,args=(1,))
  self.arena.release_raw(self.engine);self.arena.release_raw(request);p.put_uint(0x755274,0)
  if prepare_render:self.prepare_device(False,0xf2f8,0x1b8);self.prepare_submission_device()
  self.check(bytes(p.mu.mem_read(bone+0x20,0x90))==retained and int.from_bytes(p.mu.mem_read(bone+8,2),'little')==1,
             'same animated local/world cache survives actual actor teardown and arena reuse')

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded real-SAN Skin moments')
 f=Animated(mode);draw=render('normal',True,f);f.arena.assert_engine_released()
 f.check(f.arena.peak_reserved<=65536 and len(f.arena.history)==len(f.arena.released),'unchanged fixed arena and all owner generations released')
 directory,payload=specimen('normal');capture=[directory.hex(),payload.hex(),[f.animation_capture,draw]]
 if not return_capture:print('SAN_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: retained SAN/Skin {mode}; skinRead={f.read_instructions}; sanRead={f.san_read_instructions}; '
       f'tick={f.tick_instructions}; peakArena={f.arena.peak_reserved}; ownerGenerations={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
