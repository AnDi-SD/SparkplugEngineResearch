#!/usr/bin/env python3
"""Whole PC Light/Data serializer fields, writer, and actual DXLight round trip."""
from pathlib import Path
import sys,struct,json
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_node_serializer import field
from probe_pc_function_eval import cleanup

MODES=('default','values','repeat','disabled','raw-bits','unknown','no-terminator','failed-color','fail-write','global-white','header-junk','header-eof')
def specimen(mode):
 node=b'\0'
 values=[(0,struct.pack('<I',2)),(1,b'\1'),(2,struct.pack('<I',0x80402010)),(3,b'\1'),
         (4,struct.pack('<f',2.5)),(5,struct.pack('<f',123.)),(6,struct.pack('<f',.5)),(7,struct.pack('<f',1.)),(8,b'\1')]
 if mode in ('default','header-junk','header-eof','global-white'):return node+b'\0'
 if mode=='disabled':return node+field(8,b'\0')+b'\0'
 if mode=='raw-bits':values=[(0,struct.pack('<I',0xffffffff)),(4,struct.pack('<I',0x7fc12345)),(5,struct.pack('<I',0xff800000)),(6,struct.pack('<I',0x80000000)),(7,struct.pack('<I',0x7fc54321))]
 if mode=='unknown':return node+field(9,b'abcd')+field(8,b'\1')+b'\0'
 if mode=='no-terminator':return node+field(0,struct.pack('<I',1))
 if mode=='failed-color':return node+field(0,struct.pack('<I',1))+bytes((0xa2,4))
 data=b''.join(field(k,v) for k,v in values)+b'\0'
 if mode=='repeat':data=field(0,struct.pack('<I',1))+field(9,b'xyz')+data
 if mode in MODES:return node+data
 raise ValueError(mode)

def snapshot(p,obj):
 return [p.uint(obj+0xc0)]+[p.uint(obj+0xc4+i*4) for i in range(4)]+[p.mu.mem_read(obj+o,1)[0] for o in (0xec,0xd4,0xed)]+[p.uint(obj+o) for o in (0xd8,0xe0,0xe4,0xe8,0xdc,0xb0)]

def main(kind,mode,return_capture=False):
 if kind not in ('data','base') or mode not in MODES:raise ValueError('explicit bounded light serializer case')
 data=specimen(mode);f=PCWriteBytesFixture();p=f.p;checks=0
 def check(value,label):
  nonlocal checks
  checks+=1
  if not value:raise AssertionError(label)
 serializer=f.call(0x43ffd0 if kind=='data' else 0x471590)
 check(f.allocations[serializer]==0x14,'actual serializer factory14')
 header=(b'JUNKHEAD' if mode=='header-junk' else struct.pack('<II',0x5e6402df,0x4f4f4253))
 if mode=='header-eof':header=b''
 f.data=header;f.position=0
 light=f.call(0x4400b0,this=serializer,args=(f.stream,)) if kind=='data' else f.call(0x4ac000)
 check(bool(light)==(kind!='data' or mode!='header-eof'),'actual Data header factory return')
 if not light:
  check(f.position==0 and len(f.errors)==1,'external all-or-nothing read failure leaves position0')
  cleanup(f,(serializer,));check(set(f.allocations)==set(f.freed),'complete header failure cleanup')
  captured=[kind,mode,data.hex(),0,0,[],None,[]]
 else:
  check(f.allocations[light]==0x158 and p.uint(light)==0x6f0c88,'actual concrete DXLight independent of header words')
  p.put_uint(light+0xb0,0);p.put_uint(light+0xdc,0xa1b2c3d4)
  if mode=='global-white':p.put_uint(0x73fe9c,0xff804020)
  f.data=data;f.position=0;f.errors.clear()
  reader=0x440640 if kind=='data' else 0x471670;writer=0x440110 if kind=='data' else 0x471af0
  result=f.call(reader,this=serializer+0x10,args=(f.stream,light))&255;instructions=sum(p.visits.values());state=snapshot(p,light);position=f.position
  check(result==int(mode!='failed-color'),'native read result including missing-terminator success')
  check(state[-2]==0xa1b2c3d4,'opaque DC preserved')
  check(bool(state[-1]&8)==(mode not in ('default','global-white','header-junk','header-eof')),'each known field marks light dirty8')
  if mode=='disabled':check(state[7]==0,'wire false disables light before writer')
  if mode=='failed-color':check(state[0]==1 and state[1:5]==[0x3f800000]*4 and len(f.errors)==1,'earlier type mutation survives failed color read')
  written=None;roundstate=[];writeinstructions=0
  if result:
   f.data=b'';f.position=0
   if mode=='fail-write':f.fail_write_call=1
   ok=f.call(writer,this=serializer+0x10,args=(f.stream,light))&255;writeinstructions=sum(p.visits.values())
   check(ok==int(mode!='fail-write'),'whole writer return')
   check(snapshot(p,light)==state,'writer preserves object and opaque bits')
   if ok:
    written=f.data.hex();roundlight=f.call(0x4ac000);p.put_uint(roundlight+0xb0,0);p.put_uint(roundlight+0xdc,0xa1b2c3d4)
    f.position=0;check(f.call(reader,this=serializer+0x10,args=(f.stream,roundlight))&255==1,'actual fresh DXLight re-read')
    roundstate=snapshot(p,roundlight)
    if mode=='disabled':check(roundstate[7]==1,'native omitted false Enabled reloads constructor true')
    f.call(p.uint(p.uint(roundlight)),this=roundlight,args=(1,))
  captured=[kind,mode,data.hex(),result,position,state,written,roundstate]
  cleanup(f,(light,serializer));check(set(f.allocations)==set(f.freed),'complete native owner cleanup')
  print(f'LIGHT_IO instructions read={instructions} write={writeinstructions}; engineBytes={sum(f.allocations.values())}',flush=True)
 if not return_capture:print('LIGHT_SERIALIZER_CAPTURE',json.dumps(captured),flush=True)
 print(f'PASS {checks}/{checks}: original Light serializer {kind}/{mode}',flush=True)
 return captured if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2],sys.argv[3]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
