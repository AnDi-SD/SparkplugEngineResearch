#!/usr/bin/env python3
"""Actual CPU MeshData initialization and whole DXMeshData field writer."""
from pathlib import Path
import sys,json,struct
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_manager
from probe_pc_mesh_bounds import specimen as bounds_specimen
from probe_pc_function_eval import cleanup

def specimen(mode):
 if mode not in ('packed','layout','flags'):return bounds_specimen(mode)
 ib,vb=bounds_specimen('triangle')
 if mode=='packed':
  vb=struct.pack('<III',0x20,3,0)+b''.join(struct.pack('<3f4B',i,i+1,i+2,1,2,3,255) for i in range(3))
 elif mode=='layout':vb=struct.pack('<III',0x840,3,0)+struct.pack('<24f',*range(24))
 else:
  ib=ib[:8]+struct.pack('<I',0x80)+ib[12:]
  vb=vb[:8]+struct.pack('<I',0x42)+vb[12:]
 return ib,vb

def main(mode='triangle',policy='1',return_capture=False,base=False,graph=False):
 inputs=specimen(mode);policy=int(policy)
 if policy not in (0,1,2):raise ValueError('bounded writer policy')
 f=PCWriteBytesFixture();p=f.p;checks=0;buffers=[]
 def check(ok,label):
  nonlocal checks
  checks+=1
  if not ok:raise AssertionError(label)
 for size,ctor,reader,data in ((0x28,0x45f7f0,0x45fb80,inputs[0]),(0x5c,0x45fe50,0x460300,inputs[1])):
  p.run(0x4123d0,args=(size,),callee_pop=False);obj=p.reg('EAX');f.call(ctor,this=obj);buffers.append(obj);f.data=data;f.position=0
  check(f.call(reader,this=obj,args=(f.stream,))&255==1 and f.position==len(data),'actual CPU buffer input reader')
 mesh=f.call(0x41a270);p.put_uint(0x75d428,0x33c34cf0)
 check(f.call(0x434e40,this=mesh,args=(*buffers,0))&255==1,'whole original MeshData initializes both deep copies and bounds')
 init_instructions=sum(p.visits.values());check(p.uint(mesh+0x50)!=buffers[0] and p.uint(mesh+0x54)!=buffers[1],'distinct owned CPU buffer clones')
 manager,_=empty_manager(f);p.put_uint(0x75dde8,manager);p.put_uint(manager+0x18,policy)
 serializer=f.call(0x42aef0 if base else 0x4297c0);f.data=b'';f.position=0
 if graph:
  from pc_loader_fixtures import empty_fat
  from pc_stl_fixtures import install_char_traits
  from pc_crt_string_fixtures import install_crt_string
  from pc_crt_format_fixtures import install_sprintf
  install_char_traits(p);install_crt_string(p);install_sprintf(p);p.mu.mem_write(0x73ff60,b'\0')
  fat=empty_fat(f);p.put_uint(manager+0x28,fat);p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,2)
  f.call(0x422d90,this=manager,args=(0x33c34cf0,serializer,0xff,3))
  check(f.call(0x4672c0,this=serializer,args=(mesh,))&255==1 and p.visits.get(0x5a7db0),'actual no-relationship mesh indexing')
  check(p.uint(fat+0x10)==2,'one resource ID; CPU buffers are not graph references')
 result=f.call(0x467350 if graph else (0x42b170 if base else 0x429ea0),this=serializer if graph else serializer+0x10,args=(f.stream,mesh))&255;write_instructions=sum(p.visits.values())
 check(result==1 and not f.errors and f.position==len(f.data),'whole DXMeshData writer completes')
 check(bool(p.visits.get(0x42b030))==(policy in (0,2)) if base else p.visits.get(0x4298a0),'actual selected payload writer/gate')
 output=f.data;resolved=p.uint(0x13b2250)
 if graph:
  check(struct.unpack_from('<III',output)==(1,len(output)-8,0x33c34cf0),'generic resource header retains actual CPU MeshData class')
  entry=f.call(0x4664f0,this=fat,args=(mesh,));metadata=[p.uint(entry+off) for off in (4,0x10,0x14,0x18)]+[p.uint(entry+0x1c)&255]
  check(f.call(0x467350,this=serializer,args=(f.stream,mesh))&255==1 and f.data==output+struct.pack('<II',1,0),'repeated graph resource does not write payload again')
  check(f.call(0x467350,this=serializer,args=(f.stream,0))&255==1 and f.data.endswith(bytes(4)),'null graph reference')
  output=f.data
  f.call(0x466760,this=fat)
 for obj in (mesh,*buffers,*(() if graph else (serializer,))):f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
 f.call(0x4228a0,this=manager);cleanup(f,tuple(p.uint(a) for a in (0x75db78,0x75db90) if p.uint(a)));check(set(f.allocations)==set(f.freed),'whole CPU/temp/serializer ownership released')
 capture=[('graph:' if graph else '')+('base:' if base else '')+mode,policy,*[v.hex() for v in inputs],output.hex()]
 if graph:capture.append(metadata)
 if not return_capture:print('MESH_WRITER_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {checks}/{checks}: actual {"MeshData" if base else "DXMeshData"} writer {mode}/{policy}; init={init_instructions}; write={write_instructions}; bytes={len(output)}; arena={p.allocated}; resolved={resolved:08x}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
