#!/usr/bin/env python3
"""Whole two-slot Start boundary and fade-stop/tick with real SAN bindings."""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from probe_pc_actor_start import StartFixture
from probe_pc_actor_tick import ActorFixture,bits
from probe_pc_actor_controls import OFFSETS

MODES=('no-free','restart-full','fade-stop','fade-at-zero','fade-negative-rate')

class Fixture(StartFixture):
 def __init__(self):
  super().__init__(2);p=self.p;self.resources=[self.animation,self.load_animation(),self.load_animation()]
  p.mu.mem_map(0x34030000,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
  p.put_uint(p.uint(0x5a241c+2),0x34030010);p.seams[0x34030010]=lambda m:ActorFixture.floor(None,m)
  p.seams[0x60dd44]=lambda m:ActorFixture.fmod(None,m)
  self.actions=[]
 def event(self,p):
  super().event(p)
  code,queue,delay,state,payload=self.events[-1]
  self.actions.append([0,code,(state-self.p.uint(self.actor+0x28))//96,int(code in (4,5))])
 def flush(self,p):
  super().flush(p);self.actions.append([2])
 def capture(self,label,result=0xffffffff):
  p=self.p;rows=[]
  for i in range(2):
   state=self.state(i);animation=p.uint(state)
   rows.append([self.resources.index(animation) if animation in self.resources else -1,*[p.uint(state+off) for off in OFFSETS]])
  inputs=[]
  for controller in self.controllers():
   e=p.uint(controller+0x14);row=[p.uint(e+0x10),p.uint(e+0x14)]
   for i in range(2):
    a=e+0x18+i*0x30;state=p.uint(a)
    row.append([(state-p.uint(self.actor+0x28))//96 if state else -1,int(p.uint(a+4)!=0),*[p.uint(a+8+4*j) for j in range(10)]])
   inputs.append(row)
  return [label,result,p.uint(self.request+0xc),p.uint(self.manager+0x10),rows,inputs,list(self.actions)]

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded actor control pipeline')
 f=Fixture();p=f.p;checks=0;captures=[];instructions=[]
 def check(ok,message):
  nonlocal checks
  checks+=1
  if not ok:raise AssertionError(message)
 def start(index,fade,label):
  f.actions.clear();result=f.start(animation=f.resources[index],fade=fade)
  instructions.append(sum(p.visits.values()));captures.append(f.capture(label,result));return result
 check(p.uint(f.actor+0x2c)==2 and f.allocations[p.uint(f.actor+0x28)]==192,'whole configured factory has two actual slots')
 check(start(0,2 if mode in ('no-free','restart-full') else 0,'start0')==0,'first bound start')
 if mode in ('no-free','restart-full'):
  check(start(1,2,'start1')==1,'second bound start')
  before=bytes(p.mu.mem_read(p.uint(f.actor+0x28),192));inputs=f.snapshot_inputs()
  result=start(2 if mode=='no-free' else 1,0,'request')
  if mode=='no-free':
   check(result==0xffffffff,'whole no-free Start returns sentinel')
   check(p.uint(f.request+0xc)==bits(.25) and not f.actions,'failure precedes request mutation and all events/flush')
   check(bytes(p.mu.mem_read(p.uint(f.actor+0x28),192))==before and f.snapshot_inputs()==inputs,'full two-slot state/input graph unchanged')
   check(not p.visits.get(0x5a1c10) and not p.visits.get(0x5fe9c0),'no binder or third native input attempted')
  else:check(result==1 and p.uint(f.request+0xc)==bits(1),'active same-animation restart allowed despite full capacity')
 else:
  f.actions.clear();duration=0 if mode=='fade-negative-rate' else .5;rate=-.5 if mode=='fade-negative-rate' else 0
  f.call(0x5a16d0,this=f.actor,args=(f.resources[0],bits(duration),bits(rate)))
  check(not f.actions,'control has no immediate events/flush');captures.append(f.capture('fade'))
  deltas=(0,.25) if mode=='fade-at-zero' else (.25,) if mode=='fade-negative-rate' else (.25,.25,.125)
  for index,delta in enumerate(deltas):
   f.actions.clear();p.put_uint(f.engine+0xb8,bits(delta));f.call(0x4535a0,this=f.manager)
   instructions.append(sum(p.visits.values()));captures.append(f.capture('tick'+str(index)))
  if mode=='fade-stop':
   check(captures[-2][4][0][3]==bits(0) and captures[-2][4][0][16]==1,'zero weight equality still running')
   check(p.uint(f.state(0)+0x48)==0 and p.uint(f.state(0)+0x4c)==0,'negative crossing executes actual suppressed Stop and clears bindings')
   check(all(p.uint(p.uint(c+0x14)+0x18)==0 for c in f.controllers()),'actual stop clears both owned controller inputs')
  elif mode=='fade-at-zero':check(captures[2][4][0][3]==bits(1),'sample zero does not cross zero fade threshold')
  else:check(p.floats(f.state(0)+0xc,1)==(1.125,),'negative rate increases weight without upper clamp')
 f.close();check(set(f.allocations)==set(f.freed),'all SAN/actor/name/controller allocations released')
 capture=[mode,captures]
 if not return_capture:print('ACTOR_PIPELINE_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {checks}/{checks}: original actor control pipeline {mode}; maximumInstructions={max(instructions)}; arena={p.allocated}',flush=True)
 return capture if return_capture else 0

if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
