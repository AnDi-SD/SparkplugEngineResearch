#!/usr/bin/env python3
"""Same unchanged SMO light -> whole first lit shader generation and Skin draw."""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from probe_pc_skin_decoded_light import DecodedLight,KINDS
from probe_pc_skin_generated_render import GenerationDevice
from probe_pc_skin_lights_render import LitSkin
from probe_pc_skin_render import main as render

class GeneratedDecodedLight(DecodedLight):
 generation_expected=True
 arena_best_fit=True
 expected_constant_rows=8
 light_list_high=False
 palette_high=False
 prepare_shader=GenerationDevice.prepare_shader
 def __init__(self,case,mode,arena_size=0x10000):
  self.guest_arena_size=arena_size
  names=('AmbientCol','LightAmbientColorDir0','LightDiffuseColorDir0','LightSpecularColorDir0') if KINDS[int(case.split('-')[-1])]==3 else ('LightMatDiff','LightPos','LightDir','LightAttenuation')
  self.shader_reflection=(('BlendMatrices',0,3),('MatDiffuse',3,1))+tuple((name,start,1) for start,name in enumerate(names,4))
  super().__init__(case,mode)
 def on_skin_read_completed(self):
  super().on_skin_read_completed();self.read_decoded_light()
 def after_mesh_read(self):LitSkin.after_mesh_read(self)
 def on_render_completed(self,skin,bone):
  GenerationDevice.on_render_completed(self,skin,bone);p=self.p;r=self.renderer
  self.check(p.visits.get(0x4bde50) and p.visits.get(0x4ae930),'whole lit geometry and generated constants consumers')
  self.check(p.uint(skin+0x20)==self.loaded_material==p.uint(r+0xc18c),'same decoded lit material selected')
  self.check(any(e[0]=='vertex-constants' and e[3]==8 for e in self.events),'all eight just-reflected light rows reach COM')
  self.light_state=[p.uint(0x764340),0 if not p.uint(r+0xc190) else 9,[7 if p.uint(r+0xf2f8+4*i)==self.light else 0 for i in range(24)],
                    [p.uint(r+0xc178+4*i) for i in range(4)],p.uint(r+0xe4f4+139*4)]

def main(case,mode='normal',return_capture=False,arena_size=0x10000):
 f=GeneratedDecodedLight(case,mode,arena_size);draw=render(mode,True,f);f.arena.assert_engine_released()
 f.check(f.light==f.decoded_light and f.light in f.freed,'same decoded light retained through generation and actual destruction')
 f.check(set(f.sdk.released)=={f.sdk.buffer,f.sdk.table},'all external compiler objects released')
 f.check(f.shader_events==[['create',7],['release',7]],'generated shader handle has one balanced lifetime')
 generated=[f.sdk.calls,draw,f.shader_events]
 core=[f.constant_mode,f.light_world,[f.light_mode,f.light_words,f.light_state,[f.material_capture,[f.mesh_capture,[f.animation_capture,generated]]]]]
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),f.mesh_input.hex(),f.light_input.hex(),[case,f.decoded_state,core]]
 if not return_capture:print('GENERATED_DECODED_LIGHT_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: decoded SMO light first generating Skin {case}/{mode}; lightRead={f.light_read_instructions}; lightWorld={f.light_world_instructions}; render={f.render_instructions}; peak={f.arena.peak_reserved}; arenaLimit={f.arena.capacity}; owners={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
