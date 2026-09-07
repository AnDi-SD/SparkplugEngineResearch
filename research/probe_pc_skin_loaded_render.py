#!/usr/bin/env python3
"""Same native Skin and decoded Node retained from completed read through draw.

External allocator reuse within the unchanged 64KiB arena; original completed
diagnostic-manager destruction between phases releases its otherwise transient
4KiB allocation. No Skin/Node/array relocation or replay, no cap increase.
"""
from pathlib import Path
import struct,sys,json
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_fat,empty_manager
from pc_reusing_arena_fixtures import ReusingArena
from probe_pc_node_relationships import node_rtti,CLASS
from probe_pc_node_serializer import field
from probe_pc_skin_render import Fixture,main as render

MODES=('normal','post-false','weights-zero')

def specimen(mode):
 matrix=struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,5,6,7,1)
 body=struct.pack('<II',CLASS,0x4f4f4253)+field(0,struct.pack('<3f',1,2,3))+field(2,struct.pack('<3f',2,3,4))+b'\0'
 ref=struct.pack('<II',7,len(body))+body
 return struct.pack('<IIHIII',1,7,0,CLASS,0,len(body)),b'\0\0'+field(0,struct.pack('<II',0 if mode=='weights-zero' else 4,1)+ref+matrix)+b'\0'

class Loaded(Fixture):
 def check(self,ok,message):
  self.checks+=1
  if not ok:raise AssertionError(message)

 def __init__(self,mode,prepare_render=True,retire_reader_inputs=False):
  self.checks=0;directory,payload=self.skin_specimen(mode) if hasattr(self,'skin_specimen') else specimen(mode)
  self.skin_directory=directory;self.skin_payload=payload
  PCWriteBytesFixture.__init__(self,directory+payload)
  self.arena=ReusingArena(self,getattr(self,'arena_alignment',16));self.arena.best_fit=getattr(self,'arena_best_fit',False)
  p=self.p;rtti_before=set(self.arena.live);node_rtti(self)
  self.reader_rtti_inputs=sorted(set(self.arena.live)-rtti_before);self.call(0x6d38e0)
  reader_before=set(self.arena.live)
  fat=empty_fat(self);manager,_=empty_manager(self);p.put_uint(manager+0x28,fat)
  reader_inputs=sorted(set(self.arena.live)-reader_before)
  for off,val in ((0x10,2),(0x14,1),(0x18,2)):p.put_uint(manager+off,val)
  p.put_uint(0x75dde8,manager);ns=self.call(0x4638f0);serializer=self.call(0x490c50)
  self.call(0x422d90,this=manager,args=(CLASS,ns,0xff,3))
  if hasattr(self,'configure_skin_reader'):self.configure_skin_reader(fat,manager)
  self.check(self.call(0x466b90,this=fat,args=(self.stream,))&255==1,'whole FAT read')
  self.loaded_skin=self.call(0x46a120)
  self.check(self.call(0x491170,this=serializer+0x10,args=(self.stream,self.loaded_skin))&255==1,'whole Skin read')
  self.read_instructions=sum(p.visits.values())
  self.check(all(p.visits.get(a) for a in (0x491170,0x4678b0,0x463a70)),'actual Skin/reference/Node reader bodies')
  self.loaded_bone=p.uint(p.uint(self.loaded_skin+0x68))
  self.check(self.position==len(self.data),'completed read cursor')
  self.check(p.floats(self.loaded_bone+0x74,6)==(1,2,3,2,3,4),'decoded world position/scale cache')
  addresses=(self.loaded_skin,self.loaded_bone,p.uint(self.loaded_skin+0x68),p.uint(self.loaded_skin+0x6c))
  retained=[bytes(p.mu.mem_read(a,n)) for a,n in zip(addresses,(0x70,0xac,4,64))]
  self.call(0x466760,this=fat);self.call(0x4228a0,this=manager);self.call(0x490d10,this=serializer,args=(1,))
  error=p.uint(0x755264);self.check(bool(error),'real transient error manager created by reference resolution')
  self.call(0x4c3430,this=error,args=(1,))
  self.check(p.uint(0x755264)==0,'actual error manager deletion clears singleton')
  if retire_reader_inputs:
   # 466760/4228A0 are complete entry cleanup methods, not destructors of
   # these externally prepared FAT/manager/container backing objects.
   # Withdraw the declared global and end their explicit caller lifetimes.
   self.check(p.uint(manager+0x24)==0,'original serializer entry cleanup complete')
   p.put_uint(0x75dde8,0)
   for address in reader_inputs:self.arena.release_raw(address)
  if prepare_render:
   self.prepare_device(mode=='failed-device',0xf2f8,0x1b8);self.prepare_submission_device()
  self.check(all(bytes(p.mu.mem_read(a,n))==before for a,n,before in zip(addresses,(0x70,0xac,4,64),retained)),
             'same live decoded objects and arrays unchanged across arena reuse')
  if hasattr(self,'on_skin_read_completed'):self.on_skin_read_completed()

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded loaded Skin render specimen')
 f=Loaded(mode);capture=render(mode,True,f);f.arena.assert_engine_released()
 f.check(f.arena.peak_reserved<=65536 and f.arena.reuse_count>0,'unchanged arena cap with tracked reuse')
 f.check(len(f.arena.history)==len(f.arena.released),'every engine allocation generation released exactly once')
 directory,payload=specimen(mode);capture=[directory.hex(),payload.hex(),capture]
 if not return_capture:print('LOADED_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: retained original Skin read/render {mode}; readInstructions={f.read_instructions}; '
       f'peakArena={f.arena.peak_reserved}; rangeReuses={f.arena.reuse_count}; engineGenerations={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0

if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
