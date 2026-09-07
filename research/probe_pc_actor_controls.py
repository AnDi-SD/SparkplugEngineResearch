#!/usr/bin/env python3
"""Original actor capacity reset, query and fade-stop helpers; no game launch."""
from pathlib import Path
import sys,json,struct
from pc_instruction_emulator import run_bounded
from probe_pc_animation_manager import ManagerFixture
from probe_pc_function_eval import cleanup
from probe_pc_actor_tick import bits

MODES=('capacity0','capacity2','capacity19','capacity40','reset19','reset2',
       'fade-positive','fade-third','fade-zero','fade-negative','fade-missing','fade-inactive','fade-duplicate','queries-counter')
OFFSETS=(4,8,0xc,0x10,0x18,0x20,0x24,0x28,0x2c,0x30,0x34,0x3c,0x40,0x44,0x48,0x4c,0x50,0x54,0x58,0x5c)

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded actor control specimen')
 f=ManagerFixture();p=f.p;checks=0;instructions=[]
 def check(ok,message):
  nonlocal checks
  checks+=1
  if not ok:raise AssertionError(message)
 def call(*args,**kwargs):
  result=f.call(*args,**kwargs);instructions.append(sum(p.visits.values()));return result
 check(p.uint(0x741654)==40,'pristine initial configurable capacity')
 count=int(mode[8:]) if mode.startswith('capacity') else 40 if mode.startswith('reset') else 2
 p.put_uint(0x741654,count);actor=call(0x5a3620)
 check(p.uint(actor+0x2c)==count and f.allocations[p.uint(actor+0x28)]==96*count,'actual factory consumes global capacity')
 animations=[]
 if mode.startswith('reset'):
  old=p.uint(actor+0x28);p.put_uint(old+0xc,bits(.625));count=int(mode[5:])
  call(0x5a1600,this=actor,args=(count,));check(old in f.freed and p.uint(actor+0x28)!=old,'whole reset releases and replaces previous state storage')
 if mode.startswith(('capacity','reset')):
  for i in range(count):
   expected=bytearray(96);struct.pack_into('<I',expected,0x44,i)
   check(bytes(p.mu.mem_read(p.uint(actor+0x28)+i*96,96))==expected,'entire native state zeroed except slot index')
 else:
  animations=[call(0x41a090) for _ in range(3)]
  for i in range(2):
   state=p.uint(actor+0x28)+i*96
   for off,value in ((0,animations[0 if mode=='fade-duplicate' else i]),(4,1),(0xc,bits(.625)),(0x10,4),
    (0x20,bits(.75)),(0x30,bits(1)),(0x34,bits(.125)),(0x40,1),(0x48,i+1 if mode=='fade-duplicate' else int(i==0)),
    (0x4c,1),(0x58,bits(.75)),(0x5c,bits(.25))):p.put_uint(state+off,value)
  if mode=='fade-inactive':
   p.put_uint(p.uint(actor+0x28)+0x48,0);p.put_uint(p.uint(actor+0x28)+0x4c,0)
  if mode=='queries-counter':p.put_uint(p.uint(actor+0x28)+0x4c,0)
  else:
   target=animations[2 if mode=='fade-missing' else 0]
   duration=0 if mode=='fade-zero' else -2 if mode=='fade-negative' else 3 if mode=='fade-third' else 2
   before=bytes(p.mu.mem_read(p.uint(actor+0x28),count*96))
   call(0x5a16d0,this=actor,args=(target,bits(duration),bits(-.375)))
   expected=bytearray(before)
   if mode!='fade-missing':
    for off,value in ((0x10,3),(0x20,bits(1/duration if duration>0 else -.375)),(0x3c,1),(0x58,0)):struct.pack_into('<I',expected,off,value)
   check(bytes(p.mu.mem_read(p.uint(actor+0x28),count*96))==expected,'only first matching state fade/rate/stop/threshold changed')
 rows=[]
 for i in range(count):
  state=p.uint(actor+0x28)+96*i;animation=p.uint(state)
  rows.append([animations.index(animation) if animation in animations else -1,*[p.uint(state+off) for off in OFFSETS]])
 queries=[]
 for target in [*animations,0]:
  state=call(0x5a14c0,this=actor,args=(target,));used=call(0x5a1500,this=actor,args=(target,))&255
  index=(state-p.uint(actor+0x28))//96 if state else -1
  expected_index=next((i for i in range(count) if p.uint(p.uint(actor+0x28)+i*96)==target),-1)
  expected_used=any(p.uint(p.uint(actor+0x28)+i*96)==target and p.uint(p.uint(actor+0x28)+i*96+0x48)>0 for i in range(count))
  check(index==expected_index and used==expected_used,'first pointer match differs from any counted binding; running ignored')
  queries.append([index,used])
 capture=[mode,count,rows,queries]
 call(0x5a35a0,this=actor,args=(1,))
 for animation in animations:call(p.uint(p.uint(animation)),this=animation,args=(1,))
 f.close();cleanup(f,());check(set(f.allocations)==set(f.freed),'all original control objects and state arrays freed')
 if not return_capture:print('ACTOR_CONTROL_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {checks}/{checks}: original actor controls {mode}; maximumInstructions={max(instructions)}; arena={p.allocated}',flush=True)
 return capture if return_capture else 0

if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
