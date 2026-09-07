#!/usr/bin/env python3
"""Unchanged SMO light -> virtual world -> same lit Skin/SAN/mesh shader draw."""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from probe_pc_light_corpus import specimen,state
from probe_pc_skin_light_constants import LitConstants,VIEW
from probe_pc_skin_render import main as render
KINDS={0:3,1:1,2:1,3:1,4:0,5:0,6:0,7:3,8:3}
class DecodedLight(LitConstants):
 def __init__(self,case,mode):
  self.corpus_case=case;self.corpus_row,self.light_input=specimen(case);kind=KINDS[int(case.split('-')[-1])]
  if mode not in ('normal','failed-device','post-false') or (mode!='normal' and case!='corpus-6'):raise ValueError('bounded decoded light draw modes')
  light_mode={0:'directional',1:'point',3:'ambient-special'}[kind] if mode=='normal' else mode
  super().__init__(light_mode)
 def after_mesh_read(self):
  super().after_mesh_read();self.read_decoded_light()
 def read_decoded_light(self):
  p=self.p;p.put_uint(0x73fe98,0x7f234567);p.put_floats(0x7600e0,(.125,.25,.5))
  self.data=self.light_input;self.position=0;serializer=self.call(0x43ffd0)
  self.decoded_light=self.call(0x4400b0,this=serializer,args=(self.stream,));obj=self.decoded_light;p.put_uint(obj+0xdc,0xa1b2c3d4)
  self.check(p.uint(obj+0xb0)&8,'actual Light constructor dirty8 triggers payload even for empty Node section')
  self.check(self.call(0x440640,this=serializer+0x10,args=(self.stream,obj))&255==1,'whole unchanged SMO light header and field reader')
  self.light_read_instructions=sum(p.visits.values())
  self.check(self.position==len(self.data) and not self.errors and p.uint(obj+0xc0)==self.light_kind,'same decoded light type selects declared cache key')
  self.decoded_state=state(p,obj);self.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
 def create_light_for_render(self):return self.decoded_light
 def prepare_light_world(self,obj):
  p=self.p;p.put_floats(self.renderer+0xca80,VIEW)
 def build_light_payload(self,obj):
  p=self.p;self.call(0x4b58d0,this=obj,args=(0,));self.light_world_instructions=sum(p.visits.values())
  self.check(p.visits.get(0x428c30) and p.visits.get(0x421420) and p.visits.get(0x4b53c0),'whole virtual light world builds payload on same decoded object')
  self.check(not p.uint(obj+0xb0)&8,'whole native world clears decoded dirty8')
  self.light_world=[p.uint(obj+off+4*i) for off in (0x74,0xa4,0x58) for i in range(3)]
def main(case,mode='normal',return_capture=False):
 f=DecodedLight(case,mode);draw=render(mode,True,f);f.arena.assert_engine_released()
 f.check(f.light==f.decoded_light and f.light in f.freed,'same decoded light retained through whole draw and actual destruction')
 core=[f.constant_mode,f.light_world,[f.light_mode,f.light_words,f.light_state,[f.material_capture,[f.mesh_capture,[f.animation_capture,draw]]]]]
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),f.mesh_input.hex(),f.light_input.hex(),[case,f.decoded_state,core]]
 if not return_capture:print('DECODED_LIGHT_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: decoded SMO light full Skin {case}/{mode}; lightRead={f.light_read_instructions}; lightWorld={f.light_world_instructions}; render={f.render_instructions}; peak={f.arena.peak_reserved}; owners={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
