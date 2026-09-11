"""Bounded original PC readers keep the legacy menu's degenerate CDCD strip tail.

The DX COM allocation/lock/declaration fixture is reused explicitly. This
checks reader/upload bytes and normal teardown, not an original GPU frame.
"""
from pathlib import Path
import hashlib,json,struct,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from pc_loader_fixtures import empty_manager
from probe_pc_dx_mesh_payload import MeshFixture

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def guest(folder):
 out=ROOT/folder;out.mkdir(parents=True,exist_ok=False);started=time.perf_counter()
 (out/'probe-source.py').write_bytes(Path(__file__).read_bytes())
 source=ROOT/'local-data/pc-pristine/Media/Menus/menu.smo';raw=source.read_bytes()
 report=dict(status='running',source=str(source.relative_to(ROOT)).replace('\\','/'),sourceSha256=sha(source),
  pcSha256=sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),cases=[],checks=0,
  scope='Actual original standalone reader and upload copy; declared COM/stream/allocation fixture, no GPU/whole-game proof')
 def check(ok,message):
  report['checks']+=1
  if not ok:raise AssertionError(message)
 try:
  # Exact first two spans from common container/graph inspection. Validate all
  # framing and source SHA in the retained report; no input rewriting.
  for identity,offset,size in ((6,5767,202),(11,6283,750)):
   data=raw[offset:offset+size];check(len(data)==size and data[:9]==struct.pack('<II',0x33c34cf0,0x4f4f4253)+b'\xe0','actual source framing')
   field_size=struct.unpack_from('<I',data,9)[0];check(field_size+14==size and data[-1]==0,'single field0 extent')
   kind,primitives,flags=struct.unpack_from('<III',data,13);check(kind==3 and flags==0,'selected UInt16 strip')
   count=primitives+2;index_bytes=data[25:25+count*2];indices=struct.unpack('<'+str(count)+'H',index_bytes)
   vf,vertices,aux=struct.unpack_from('<III',data,25+count*2);vertex_bytes=data[37+count*2:-1]
   check(indices[-2:]==(0xcdcd,0xcdcd) and all(i<vertices for i in indices[:-2]),'only two trailing raw indices are invalid')
   f=MeshFixture(data);p=f.p;manager,_=empty_manager(f);p.put_uint(manager+0x10,1);p.put_uint(0x75dde8,manager)
   serializer=f.call(0x42aef0);mesh=f.call(0x42afd0,this=serializer,args=(f.stream,))
   check(f.call(0x42b420,this=serializer+0x10,args=(f.stream,mesh))&255==1,'actual common field reader accepts source')
   check(f.position==len(data) and not f.errors and 0x45fb80 in p.visits and 0x4aa000 in p.visits,'actual IB reader and DX initializer completed')
   ib=p.uint(mesh+0x54);vb=p.uint(mesh+0x58)
   actual_indices=bytes(p.mu.mem_read(f.buffers[p.uint(ib+0x10)]['data'],len(index_bytes)))
   actual_vertices=bytes(p.mu.mem_read(f.buffers[p.uint(vb+0x10)]['data'],len(vertex_bytes)))
   check(actual_indices==index_bytes and actual_vertices==vertex_bytes,'original uploads exact raw index and vertex bytes')
   row=dict(id=identity,offset=offset,size=size,inputSha256=hashlib.sha256(data).hexdigest().upper(),
    primitives=primitives,vertices=vertices,indices=list(indices),meshWords=f.words(mesh,0x44,0x88),
    uploadedIndices=actual_indices.hex(),vertexSha256=hashlib.sha256(actual_vertices).hexdigest().upper(),
    instructions=sum(p.visits.values()),arena=p.allocated,limits=p.last_execution_limits)
   (out/f'mesh-{identity}.bin').write_bytes(data)
   f.call(0x4aa350,this=mesh,args=(1,));f.clear_declarations()
   f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,));f.call(0x4228a0,this=manager)
   for address in (0x75db78,0x75526c,0x755264):
    obj=p.uint(address)
    if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
   check(all(b['refs']==0 for b in f.buffers.values()) and set(f.allocations)==set(f.freed),'normal original teardown releases every tracked allocation')
   row['released']=True;report['cases'].append(row)
  check(sha(source)==report['sourceSha256'],'original source unchanged');report['status']='passed-scoped-reader-upload'
 except Exception as error:report.update(status='failed',error=repr(error))
 report['elapsedSeconds']=time.perf_counter()-started
 report['dependencies']=sorted((dict(path=Path(m.__file__).resolve().relative_to(ROOT).as_posix(),sha256=sha(m.__file__))
  for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda row:row['path'])
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:report.get(k) for k in ('status','checks','elapsedSeconds','error')}));return 0 if report['status'].startswith('passed') else 1
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(guest(*sys.argv[2:]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
