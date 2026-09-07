#!/usr/bin/env python3
"""Same decoded bone: real SAN sampling -> first shader generation -> draw.

Completed phases retain the actual Skin/Node, with SAN owners destroyed before
renderer backing. SDK/device results and geometry remain explicit inputs.
"""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from probe_pc_skin_san_render import Animated
from probe_pc_skin_generated_render import GenerationDevice
from probe_pc_skin_loaded_render import specimen
from probe_pc_skin_render import main as render

MODES={'quarter':('0.25','normal'),'three-quarter':('0.75','normal'),
       'loop':('1.25','normal'),'failed-device':('0.5','failed-device')}

class AnimatedGenerated(GenerationDevice,Animated):
 def __init__(self,delta,device_mode):
  super().__init__(delta,False,True)
  retained=bytes(self.p.mu.mem_read(self.loaded_bone+0x20,0x90))
  self.prepare_generation_device(device_mode,True)
  self.check(bytes(self.p.mu.mem_read(self.loaded_bone+0x20,0x90))==retained,
             'same animated Node cache survives generating-renderer preparation')

def main(mode,return_capture=False,fixture_type=AnimatedGenerated):
 if mode not in MODES:raise ValueError('bounded SAN generating Skin scenarios')
 delta,device_mode=MODES[mode];f=fixture_type(delta,device_mode)
 draw=render(device_mode,True,f);f.arena.assert_engine_released()
 f.check(set(f.sdk.released)=={f.sdk.buffer,f.sdk.table},'all SDK COM results released')
 f.check(f.shader_events==[['create',7],['release',7]],'exact generated handle lifetime')
 f.check(f.arena.peak_reserved<=65536,'unchanged fixed arena cap')
 directory,payload=specimen('normal')
 capture=[directory.hex(),payload.hex(),[f.animation_capture,[f.sdk.calls,draw,f.shader_events]]]
 if not return_capture:print('SAN_GENERATED_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: real SAN to generating Skin draw {mode}; skinRead={f.read_instructions}; '
       f'sanRead={f.san_read_instructions}; tick={f.tick_instructions}; render={f.render_instructions}; '
       f'peakArena={f.arena.peak_reserved}; ownerGenerations={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
