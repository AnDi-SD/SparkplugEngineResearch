#!/usr/bin/env python3
"""Real SAN -> original scene-manager world traversal -> same Skin draw.

Scene/root view is explicit. Native plain parent attachment, manager traversal
and Node worlds execute; no forced bone world call or scene-init substitution.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_skin_san_generated_render import AnimatedGenerated,main as linked
from probe_pc_scene_manager_world import append_scene_input

class SceneGenerated(AnimatedGenerated):
 def update_world_phase(self):
  p=self.p;bone=self.loaded_bone;manager=self.call(0x45adf0);root=self.call(0x421e20)
  p.put_floats(root+0x20,(10.,-20.,30.));p.put_uint(root+0xb0,p.uint(root+0xb0)|1)
  # Attach while manager list is empty: actual plain-parent route has no
  # scene membership to publish. The subsequent scene view is caller input.
  self.call(0x421a60,this=root,args=(bone,))
  self.check(p.uint(bone+0x2c)==root and p.uint(bone+0x3c)==0,'actual plain parent attachment before explicit scene view')
  scene=p.allocate(0x54);p.put_uint(scene+0x14,root);append_scene_input(self,manager,scene)
  trace=[]
  def observe(mu,address,size,_):
   if address==0x421420 and p.reg('ECX') in (root,bone):
    trace.append((p.reg('ECX'),p.uint(p.reg('ESP')+4),p.uint(manager+0x20)))
  hook=p.mu.hook_add(p.uc.UC_HOOK_CODE,observe)
  self.call(0x45a7d0,this=manager);instructions=sum(p.visits.values());p.mu.hook_del(hook)
  self.check([row[0] for row in trace]==[root,bone] and trace[0][1]==0 and all(row[2]==scene for row in trace),
             'whole manager dispatch reaches root then same SAN bone under current scene')
  self.check(p.uint(manager+0x20)==0,'current scene cleared after actual world traversal')
  worlds=bytes(p.mu.mem_read(bone+0x74,60))
  self.call(0x422220,this=root,args=(1,));self.call(0x45add0,this=manager,args=(1,));self.arena.release_raw(scene)
  self.check(p.uint(bone+0x2c)==0 and bone not in self.freed and bytes(p.mu.mem_read(bone+0x74,60))==worlds,
             'same bone/world survives original parent and borrowed manager teardown')
  print(f'SCENE_WORLD_PHASE instructions={instructions}; inherited={trace[1][1]}',flush=True)

def main(mode,return_capture=False):return linked(mode,return_capture,SceneGenerated)
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
