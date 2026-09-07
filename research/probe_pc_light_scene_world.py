#!/usr/bin/env python3
"""Decoded DXLight -> actual scene attachment -> full world/cache refresh."""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_light_world import specimen
from probe_pc_light_corpus import state
from probe_pc_function_eval import cleanup
MODES=('directional','point','spot','disabled','ambient','shadow')

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded scene light world')
 data=specimen('point' if mode=='shadow' else mode);f=PCWriteBytesFixture(data);p=f.p;checks=0
 def check(ok,label):
  nonlocal checks
  checks+=1
  if not ok:raise AssertionError(label)
 f.call(0x6d38e0);p.put_uint(0x73fe98,0x7f234567);p.put_floats(0x7600e0,(.125,.25,.5));p.put_uint(0x755274,p.allocate(0x160))
 for record,identity,parent in ((0x7634b0,0x6b3e7baa,0x75e278),(0x75e278,0x72444900,0x75dd88),(0x75e150,0x603625d0,0x75dd88),(0x75dd88,0x695c0f65,0x7555f8),(0x7555f8,0x44de07fd,0x755310),(0x755310,0x415352a1,0)):
  p.put_uint(record,identity);p.put_uint(record+0x48,parent)
 renderer=p.allocate(0xc9c8);p.put_uint(0x75db68,renderer)
 scene=f.call(0x45ebc0);root=p.uint(scene+0x14);manager=p.uint(scene+0x34);p.put_uint(manager+0x1c,scene+0x18)
 nodes=[f.call(0x425520) for _ in range(2)]
 for i,node in enumerate(nodes):
  f.call(0x421a60,this=root,args=(node,));p.put_floats(node+0xd8,(100.*i,0.,0.,2.));p.mu.mem_write(node+0x121,bytes([i==0]))
 serializer=f.call(0x43ffd0);light=f.call(0x4400b0,this=serializer,args=(f.stream,));p.put_uint(light+0xdc,0xa1b2c3d4)
 check(f.call(0x440640,this=serializer+0x10,args=(f.stream,light))&255==1 and f.position==len(data) and not f.errors,'whole decoded light read')
 if mode=='shadow':p.mu.mem_write(light+0xec,b'\1')
 f.call(0x421a60,this=root,args=(light,))
 check(p.uint(light+0x3c)==scene and p.uint(manager+0x10)==light and p.uint(manager+0x18)==1,'actual typed Node attach registers same decoded DXLight')
 check(p.uint(light+8)&65535==1,'scene list borrows, root owns light once')
 def capture(label):
  caches=[[7 if p.uint(node+0xf0+4*i)==light else 0 for i in range(9)]+[p.uint(node+0x114)] for node in nodes]
  return [label,state(p,light),[p.uint(light+0xf0+4*i) for i in range(26)],caches]
 rows=[capture('attached')];maximum=0
 for label in ('world','far-position','dirty','disabled','enabled','inactive','active','sphere-change','inherited'):
  if label=='far-position':p.put_floats(light+0x20,(1000.,0.,0.));p.put_uint(light+0xb0,p.uint(light+0xb0)|1)
  if label=='dirty':p.put_uint(light+0xb0,p.uint(light+0xb0)|8)
  if label in ('disabled','enabled'):p.mu.mem_write(light+0xed,bytes([label=='enabled']));p.put_uint(light+0xb0,p.uint(light+0xb0)|8)
  if label in ('inactive','active'):f.call(0x420de0,this=light,args=(int(label=='active'),));p.put_uint(light+0xb0,p.uint(light+0xb0)|8)
  if label=='sphere-change':p.put_floats(nodes[1]+0xd8,(1000.,0.,0.,2.));p.put_uint(light+0xb0,p.uint(light+0xb0)|8)
  f.call(0x4b58d0,this=light,args=(8 if label=='inherited' else 0,));maximum=max(maximum,sum(p.visits.values()))
  refreshed=bool(p.visits.get(0x46ace0))
  check(refreshed==(label!='far-position'),'full base Light refresh follows post-Node dirty8, while DX position-only refresh is separate')
  check(not p.uint(light+0xb0)&15,'whole scene Light consumes own dirty flags')
  if refreshed:check(p.visits.get(0x46a850)==2,'actual manager visits both registered render nodes')
  rows.append(capture(label))
 f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,));f.call(0x45eb50,this=scene,args=(1,))
 check(all(obj in f.freed for obj in (light,*nodes)),'actual scene destroys registered light and render nodes')
 cleanup(f,tuple(p.uint(a) for a in (0x75db90,0x75db98,0x75db78) if p.uint(a)))
 check(set(f.allocations)==set(f.freed),'all actual scene and helper owners released')
 capture_value=[mode,data.hex(),rows]
 if not return_capture:print('LIGHT_SCENE_WORLD_CAPTURE',json.dumps(capture_value),flush=True)
 print(f'PASS {checks}/{checks}: decoded Light whole scene world {mode}; worldMax={maximum}; arena={p.allocated}; engine={sum(f.allocations.values())}',flush=True)
 return capture_value if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
