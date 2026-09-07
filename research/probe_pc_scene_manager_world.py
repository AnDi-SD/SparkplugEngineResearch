#!/usr/bin/env python3
"""Original scene-manager world traversal over explicit borrowed scene views.

Actual manager factory/clone/destructor and real Node world bodies; list input
and virtual overrides are declared external state. No spScene ctor claim.
"""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from probe_pc_san_reader import ReaderFixture
from probe_pc_function_eval import cleanup

MODES=('empty','world-three','append','remove-next','false-root','clone')

def append_scene_input(f,manager,scene):
 p=f.p;head=p.uint(manager+0x18);tail=p.uint(head+4)
 if hasattr(f,'arena'):entry=f.arena.allocate_owned(12)
 else:entry=p.allocate(12);f.allocations[entry]=12
 for off,value in ((0,head),(4,tail),(8,scene)):p.put_uint(entry+off,value)
 p.put_uint(tail,entry);p.put_uint(head+4,entry);p.put_uint(manager+0x1c,p.uint(manager+0x1c)+1)
 return entry

def list_scenes(p,manager):
 head=p.uint(manager+0x18);entry=p.uint(head);result=[]
 while entry!=head:
  if len(result)>=8:raise AssertionError('bounded fixture scene list')
  result.append(p.uint(entry+8));entry=p.uint(entry)
 if len(result)!=p.uint(manager+0x1c):raise AssertionError('scene list/count mismatch')
 return result

class Fixture(ReaderFixture):
 def check(self,ok,message):
  self.checks+=1
  if not ok:raise AssertionError(message)
 def __init__(self,mode):
  super().__init__(b'');self.checks=0;self.mode=mode;p=self.p
  self.call(0x6d38e0);self.manager=self.call(0x45adf0);self.nodes=[self.call(0x421e20) for _ in range(3)]
  self.scenes=[p.allocate(0x54) for _ in self.nodes];self.trace=[];self.entries=[];self.detached=[]
  self.check(self.allocations[self.manager]==0x24 and p.uint(self.manager)==0x6e7154,'exact original manager factory')
  self.check(p.uint(0x75db90)==self.manager and p.uint(self.manager+0x20)==0,'published singleton/current null')
  for i,(scene,node) in enumerate(zip(self.scenes,self.nodes)):
   p.put_uint(scene+0x14,node);p.mu.mem_write(scene+0x24,bytes((i%2,(i+1)%2)))
   p.put_floats(node+0x20,(float(i+1),float(2*i+2),float(3*i+3)))
   p.put_uint(node+0xb0,(p.uint(node+0xb0)|1)&~0x200)
  initial=0 if mode=='empty' else 2 if mode=='append' else 3
  for scene in self.scenes[:initial]:self.entries.append(append_scene_input(self,self.manager,scene))
  self.original_vtables=[p.uint(node) for node in self.nodes]
  if mode not in ('world-three','empty','clone'):
   p.mu.mem_map(0x340d0000,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
   table=p.allocate(0x80);p.mu.mem_write(table,bytes(p.mu.mem_read(self.original_vtables[0],0x80)))
   p.put_uint(table+0x30,0x340d0010);p.seams[0x340d0010]=self.callback
   for node in self.nodes:p.put_uint(node,table)
  else:
   def observe(mu,address,size,_):
    if address==0x421420 and p.reg('ECX') in self.nodes:self.record(p)
   p.mu.hook_add(p.uc.UC_HOOK_CODE,observe)
 def record(self,p):
  node=p.reg('ECX');scene=p.uint(self.manager+0x20);flags=p.uint(p.reg('ESP')+4)
  self.check(scene==self.scenes[self.nodes.index(node)] and flags==0,'current scene and zero root argument during callback')
  self.trace.append([self.scenes.index(scene)+1,self.nodes.index(node)+1,flags])
 def callback(self,p):
  self.record(p)
  if len(self.trace)==1 and self.mode=='append':
   self.entries.append(append_scene_input(self,self.manager,self.scenes[2]))
  elif len(self.trace)==1 and self.mode=='remove-next':
   entry=self.entries[1];following=p.uint(entry);previous=p.uint(entry+4)
   p.put_uint(previous,following);p.put_uint(following+4,previous)
   p.put_uint(self.manager+0x1c,p.uint(self.manager+0x1c)-1);self.detached.append(entry)
  p.fixture_return(4,eax=0 if self.mode=='false-root' else 1)

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded scene-manager cases')
 f=Fixture(mode);p=f.p;clone_capture=[]
 if mode=='clone':
  p.put_uint(f.manager+0x20,f.scenes[1]);pair_owner=p.allocate(0x24);p.put_uint(0x74e060,pair_owner);pairs=[]
  def pair(p):
   pairs.append((p.uint(p.reg('ESP')+4),p.uint(p.reg('ESP')+8)));p.fixture_return(8)
  p.seams[0x412f70]=pair;clone=f.call(0x45ae50,this=f.manager)
  f.check(pairs==[(f.manager,clone)],'actual clone records original/source pair')
  clone_capture=[int(p.uint(0x75db90)==clone),len(list_scenes(p,clone)),int(p.uint(clone+0x20)==0)]
  f.call(0x45add0,this=f.manager,args=(1,));clone_capture.append(int(p.uint(0x75db90)==0))
  f.call(0x45add0,this=clone,args=(1,));p.put_uint(0x74e060,0)
  remaining=[];current=0
 else:
  p.put_uint(f.manager+0x20,0x12345678)
  f.call(0x45a7d0,this=f.manager);f.update_instructions=sum(p.visits.values())
  f.check(0x45a7d0 in p.visits,'whole original manager traversal')
  f.check(p.uint(f.manager+0x20)==0,'current scene cleared on normal/empty return')
  remaining=[f.scenes.index(a)+1 for a in list_scenes(p,f.manager)];current=0
  if mode=='append':f.check([t[0] for t in f.trace]==[1,2,3],'same traversal sees appended entry')
  if mode=='remove-next':f.check([t[0] for t in f.trace]==[1,3],'post-call next read skips detached entry')
  if mode=='false-root':f.check(len(f.trace)==3,'root false does not stop traversal')
  if mode=='world-three':f.check(all(p.floats(n+0x74,3)==(i+1.,2*i+2.,3*i+3.) for i,n in enumerate(f.nodes)),'real disabled Node roots update without scene flag gates')
  f.call(0x45add0,this=f.manager,args=(1,))
 worlds=[[p.uint(node+off+4*i) for off,count in ((0x74,3),(0x80,3)) for i in range(count)] for node in f.nodes]
 f.check(all(n not in f.freed and s not in f.freed for n,s in zip(f.nodes,f.scenes)),'manager destruction borrows scenes and roots')
 for n,vt in zip(f.nodes,f.original_vtables):p.put_uint(n,vt)
 for entry in f.detached:p.run(0x412420,args=(entry,),callee_pop=False)
 cleanup(f,f.nodes)
 f.check(set(f.allocations)==set(f.freed),'all tracked manager/list/Node owners released')
 capture=[mode,f.trace,worlds,remaining,current,clone_capture]
 if not return_capture:print('SCENE_MANAGER_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: original scene manager {mode}; update={getattr(f,"update_instructions",0)}; heap={p.allocated}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
