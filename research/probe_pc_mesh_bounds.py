#!/usr/bin/env python3
"""Original CPU readers and full PC424230 bounds producer, finite tiny inputs."""
from pathlib import Path
import sys,struct,json
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_function_eval import cleanup

MODES=('triangle','five','seven','u32','rounding','zero-primitives','point')
def specimen(mode):
 points=[(0.,1.,2.),(7.,8.,9.),(14.,15.,16.)];primitive=1;flags=0;kind=2;indices=[0,1,2]
 if mode in ('five','seven','u32'):
  points=[(-13.,2.,7.),(4.,-5.,10.),(1.,3.,-11.),(6.,8.,2.),(-3.,7.,5.),(20.,1.,9.),(-4.,-8.,-3.)][:(5 if mode=='five' else 7)]
  primitive=2;indices=[2,3,4,0,1,2];flags=int(mode=='u32')
 elif mode=='rounding':points=[(.100000001,.200000003,.300000012),(12345.125,-.0009765625,1024.125),(-.001953125,7.100000381,-11.200000763)]
 elif mode=='zero-primitives':primitive=0;kind=3;indices=[0,1]
 elif mode=='point':points=[(3.,-7.,11.)];primitive=1;kind=1;indices=[0]
 elif mode!='triangle':raise ValueError(mode)
 ib=struct.pack('<III',kind,primitive,flags)+struct.pack('<'+('I' if flags else 'H')*len(indices),*indices)
 vb=struct.pack('<III',0,len(points),0)+b''.join(struct.pack('<3f',*v) for v in points)
 return ib,vb

def main(mode,return_capture=False):
 f=PCWriteBytesFixture();p=f.p;checks=0;objects=[];inputs=specimen(mode)
 def check(ok,label):
  nonlocal checks
  checks+=1
  if not ok:raise AssertionError(label)
 for size,ctor,reader,data in ((0x28,0x45f7f0,0x45fb80,inputs[0]),(0x5c,0x45fe50,0x460300,inputs[1])):
  p.run(0x4123d0,args=(size,),callee_pop=False);obj=p.reg('EAX');f.call(ctor,this=obj);objects.append(obj)
  f.data=data;f.position=0
  check(f.call(reader,this=obj,args=(f.stream,))&255==1 and f.position==len(data),'whole CPU buffer reader')
 mesh=f.call(0x4a9e80);objects.insert(0,mesh);before=p.uint(mesh+0x28)
 f.call(0x424230,this=mesh,args=(objects[1],objects[2]));instructions=sum(p.visits.values())
 check(all(p.visits.get(a) for a in (0x424230,0x468370,0x468000)),'whole AABB/sphere and vertex-extrema helpers')
 check(p.uint(mesh+0x28)==before,'bounds helper does not publish valid flag; enclosing initialize does')
 capture=[mode,[p.uint(mesh+0x18+4*i) for i in range(4)],
              [p.uint(mesh+0x2c+4*i) for i in range(3)],[p.uint(mesh+0x38+4*i) for i in range(3)]]
 for obj in objects:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
 cleanup(f,tuple(p.uint(a) for a in (0x75db78,) if p.uint(a)))
 check(set(f.allocations)==set(f.freed),'complete native owning teardown')
 if not return_capture:print('PC_MESH_BOUNDS_CAPTURE',json.dumps([x.hex() for x in inputs]+[capture]),flush=True)
 print(f'PASS {checks}/{checks}: PC mesh bounds {mode}; instructions={instructions}; engineBytes={sum(f.allocations.values())}',flush=True)
 return [x.hex() for x in inputs]+[capture] if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
