#!/usr/bin/env python3
"""Decoded SMO Light -> actual manager-selected RenderNode cache -> Skin draw."""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from probe_pc_skin_decoded_light import DecodedLight
from probe_pc_skin_render import main as render

class SelectedLight(DecodedLight):
 def build_light_payload(self,obj):
  p=self.p;self.selection_node=self.call(0x425520);node=self.selection_node;manager=self.call(0x46ab80)
  scene=p.allocate(0x54);head=p.allocate(4);p.put_uint(scene+0x34,manager);p.put_uint(head,node);p.put_uint(manager+0x1c,head);p.put_uint(manager+0x20,0)
  p.put_floats(node+0xd8,(*p.floats(obj+0x74,3),0.));p.mu.mem_write(node+0x121,b'\0')
  self.call(0x45a780,this=manager,args=(obj,));p.put_uint(obj+0x3c,scene);self.call(0x420de0,this=obj,args=(1,))
  super().build_light_payload(obj)
  self.check(p.visits.get(0x46ace0) and p.visits.get(0x46a850) and p.visits.get(0x490b50),'whole light world selects actual RenderNode cache through real LightManager')
  self.selected_cache=[7 if p.uint(node+0xf0+4*i)==obj else 0 for i in range(9)]+[p.uint(node+0x114)]
  self.check((p.uint(node+0x114)==1 and p.uint(node+0xf0)==obj) if self.ordinary else p.uint(node+0x110)==obj,'native cache selection agrees with declared actual shader key')
  self.selection_phase=(manager,scene,head)
 def prepare_light_list(self,obj):
  p=self.p;node=self.selection_node;manager,scene,head=self.selection_phase
  # End explicit borrowed scene/head input; no Scene ctor/auto-attach claim.
  p.put_uint(obj+0x3c,0);self.call(p.uint(p.uint(manager)),this=manager,args=(1,));self.arena.release_raw(head);self.arena.release_raw(scene)
  self.check([7 if p.uint(node+0xf0+4*i)==obj else 0 for i in range(9)]+[p.uint(node+0x114)]==self.selected_cache,'real manager destruction retains borrowed RenderNode cache')
  return self.selection_node+0xf0
 def on_render_teardown(self):
  self.call(self.p.uint(self.p.uint(self.selection_node)),this=self.selection_node,args=(1,));super().on_render_teardown()

def main(case,mode='normal',return_capture=False):
 f=SelectedLight(case,mode);draw=render(mode,True,f);f.arena.assert_engine_released()
 f.check(f.light in f.freed and f.selection_node in f.freed,'same selected light and actual cache owner destroyed')
 core=[f.constant_mode,f.light_world,[f.light_mode,f.light_words,f.light_state,[f.material_capture,[f.mesh_capture,[f.animation_capture,draw]]]]]
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),f.mesh_input.hex(),f.light_input.hex(),[case,f.decoded_state,f.selected_cache,core]]
 if not return_capture:print('SELECTED_LIGHT_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: actual selected SMO light full Skin {case}/{mode}; selection={f.light_world_instructions}; render={f.render_instructions}; peak={f.arena.peak_reserved}; owners={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
