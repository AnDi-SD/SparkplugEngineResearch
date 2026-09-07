#!/usr/bin/env python3
"""Same decoded native texture -> whole first shader generation and Skin draw."""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from probe_pc_skin_texture_render import TextureSkin,MODES
from probe_pc_skin_generated_render import GenerationDevice
from probe_pc_skin_render import main as render
from probe_pc_renderer_fog import INDICES

class GeneratedTextureSkin(TextureSkin):
 generation_expected=True
 prepare_shader=GenerationDevice.prepare_shader
 def after_mesh_read(self):
  # Mesh creation is complete. End the actual declaration lookup map lifetime;
  # native entries borrow their declaration and do not own its COM resource.
  self.call(0x4ae140,this=self.renderer+0xf358)
  self.check(self.p.uint(self.renderer+0xf35c)==0 and self.mesh_declaration_refs==1,
             'actual declaration map teardown preserves decoded mesh declaration')
  super().after_mesh_read()
 def on_render_teardown(self):
  self.check(not self.mesh_declaration_refs and all(b['refs']==0 and b['locks']==0 for b in self.mesh_buffers.values()),
             'all decoded mesh COM owners freed after early map teardown')
 def on_render_completed(self,skin,bone):
  GenerationDevice.on_render_completed(self,skin,bone);p=self.p
  self.check(p.uint(self.renderer+0xc18c)==self.loaded_material and p.uint(self.renderer+0xca20)==self.loaded_fog,'same decoded material and Fog in generating draw')
  self.check(p.visits.get(0x4bb650) and p.uint(self.renderer+0xe480)==self.loaded_texture,'actual decoded texture resolver in generating draw')
  self.check(any(event[:3]==['texture',0,9] for event in self.events),'decoded COM texture reaches device')
  self.fog_cache=[p.uint(self.renderer+0xe4f4+4*k) for k in INDICES]

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded generating textured Skin')
 f=GeneratedTextureSkin(mode);device_mode=mode if mode in ('failed-device','post-false') else 'normal';draw=render(device_mode,True,f)
 f.arena.assert_engine_released();io=f.texture_io
 f.check(f.loaded_texture in f.freed and io.device_refs==1 and io.texture_refs==io.surface_refs==0 and not io.locked,'all original texture/COM owner teardown')
 f.check(set(f.sdk.released)=={f.sdk.buffer,f.sdk.table} and f.shader_events==[['create',7],['release',7]],'all SDK and shader handles released')
 f.check(f.arena.peak_reserved<=65536,'unchanged fixed arena')
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),f.mesh_input.hex(),f.texture_input.hex(),
          [mode,f.texture_capture,[f.fog_capture,f.fog_cache,[f.material_capture,[f.mesh_capture,[f.animation_capture,[f.sdk.calls,draw,f.shader_events]]]]]]]
 if not return_capture:print('GENERATED_TEXTURE_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: generated textured Skin {mode}; read={f.read_instructions}; textureRead={f.texture_read_instructions}; '
       f'render={f.render_instructions}; peakArena={f.arena.peak_reserved}; owners={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
