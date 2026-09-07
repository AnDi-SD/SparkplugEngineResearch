#!/usr/bin/env python3
"""Uncompleted research candidate: queued shader generation hits arena fragmentation.

See docs/research/native-pc-skin-queued-generated-boundary.md. No success or
coverage credit; do not repeat unchanged capped input or resume its state.
"""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from probe_pc_skin_queued_mesh_render import QueuedMesh,MODES
from probe_pc_skin_generated_render import GenerationDevice
from probe_pc_skin_render import main as render
from probe_pc_skin_alpha_queue import IDENTITY

class QueuedGenerated(QueuedMesh):
 generation_expected=True
 prepare_shader=GenerationDevice.prepare_shader
 def after_mesh_read(self):
  p=self.p;self.call(0x4ae140,this=self.renderer+0xf358)
  self.check(p.uint(self.renderer+0xf35c)==0 and self.mesh_declaration_refs==1,'completed declaration lookup teardown retains mesh COM owner')
 def before_render(self,skin,bone):
  # Camera is an explicitly prepared consumer view, not an original camera
  # factory. Only CC..231 are read here. Retain this complete view through
  # queued callback dispatch; no live input or owner is retired/replayed.
  p=self.p;p.put_uint(0x75e150,0x603625d0);p.put_uint(0x75e150+0x48,0x75dd88)
  self.queue_support=self.call(0x425520)
  self.queue_camera=p.allocate(0x168)-0xcc
  p.put_floats(self.queue_camera+0xcc,IDENTITY)
  QueuedMesh.before_render(self,skin,bone)
 def on_render_completed(self,skin,bone):
  GenerationDevice.on_render_completed(self,skin,bone)
  p=self.p;r=self.renderer
  self.check(all(p.visits.get(a) for a in (0x454850,0x4248d0,0x46a240,0x4be210)) and not p.visits.get(0x454c30),'actual full queued support/Skin/generated mesh draw, no re-enqueue')
  self.check(p.uint(r+0xc18c)==self.loaded_material==p.uint(r+0xc9c0),'same decoded owning alpha material selected')
  self.flush_state=[p.uint(r+0x4c),p.uint(r+0x44)&255,2 if p.uint(r+0xc190)==self.queue_support+0xf0 else 0,[p.uint(r+0xc9c8+4*i) for i in range(4)]]
  self.check(self.flush_state[:3]==[0,0,2],'queue cleared and support light cache published')

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded queued generated Skin')
 f=QueuedGenerated(mode);draw=render(mode,True,f);f.arena.assert_engine_released()
 f.check(set(f.sdk.released)=={f.sdk.buffer,f.sdk.table} and f.shader_events==[['create',7],['release',7]],'actual generated shader/SDK teardown after queued draw')
 f.check(f.loaded_material in f.freed and f.arena.peak_reserved<=65536,'same owning graph freed inside unchanged arena')
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),f.mesh_input.hex(),
          [mode,f.enqueue_record,f.flush_result,f.inner_results,f.flush_state,[f.material_capture,[f.mesh_capture,[f.animation_capture,[f.sdk.calls,draw,f.shader_events]]]]]]
 if not return_capture:print('QUEUED_GENERATED_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: whole queued generated Skin {mode}; read={f.read_instructions}; render={f.render_instructions}; peakArena={f.arena.peak_reserved}; owners={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
