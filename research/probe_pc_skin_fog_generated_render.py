#!/usr/bin/env python3
"""Owned Fog/material/bone/SAN/mesh -> whole first shader generation and draw."""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from probe_pc_skin_fog_render import FogSkin
from probe_pc_skin_generated_render import GenerationDevice
from probe_pc_skin_render import main as render
from probe_pc_renderer_fog import INDICES

MODES=('linear','exp2','unknown','failed-device','post-false')
class GeneratedFogSkin(FogSkin):
 generation_expected=True
 # External placement policy, not original malloc evidence. All objects stay
 # in the same fixed arena; no live block moves and no native body is seamed.
 arena_alignment=8
 arena_best_fit=True
 use_material_as_fallback=True
 prepare_shader=GenerationDevice.prepare_shader
 def on_render_completed(self,skin,bone):
  GenerationDevice.on_render_completed(self,skin,bone);p=self.p
  self.check(p.uint(self.renderer+0xc18c)==self.loaded_material==p.uint(self.renderer+0xc9c0),'same decoded owned material also supplies fallback layers')
  self.check(p.visits.get(0x4ad390) and p.uint(self.renderer+0xca20)==self.loaded_fog,'same owned Fog precedes generating draw')
  self.fog_cache=[p.uint(self.renderer+0xe4f4+4*k) for k in INDICES]
def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded generating owning Fog Skin')
 f=GeneratedFogSkin(mode);device_mode=mode if mode in ('failed-device','post-false') else 'normal';draw=render(device_mode,True,f)
 f.arena.assert_engine_released()
 f.check(f.loaded_fog in f.freed and f.loaded_material in f.freed,'all decoded owners freed by real destructors')
 f.check(set(f.sdk.released)=={f.sdk.buffer,f.sdk.table} and f.shader_events==[['create',7],['release',7]],'all opaque SDK and device shader handles released')
 f.check(f.arena.peak_reserved<=65536 and f.arena.alignment==8,'unchanged total arena with explicit eight-byte placement')
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),f.mesh_input.hex(),
          [mode,f.fog_capture,f.fog_cache,[f.material_capture,[f.mesh_capture,[f.animation_capture,[f.sdk.calls,draw,f.shader_events]]]]]]
 if not return_capture:print('GENERATED_FOG_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: generated owning Fog Skin {mode}; read={f.read_instructions}; '
       f'render={f.render_instructions}; peakArena={f.arena.peak_reserved}; owners={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
