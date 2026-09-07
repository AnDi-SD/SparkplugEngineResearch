#!/usr/bin/env python3
"""Retained real SAN/scene worlds + decoded mesh -> whole Skin generating draw."""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from probe_pc_skin_mesh_generated_render import MeshRead
from probe_pc_skin_scene_generated_render import SceneGenerated
from probe_pc_skin_san_generated_render import MODES
from probe_pc_skin_loaded_render import specimen
from probe_pc_skin_render import main as render

class SceneMeshGenerated(MeshRead,SceneGenerated):pass

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded SAN/scene/decoded-mesh draws')
 delta,device_mode=MODES[mode];f=SceneMeshGenerated(delta,device_mode);draw=render(device_mode,True,f)
 f.arena.assert_engine_released()
 f.check(set(f.sdk.released)=={f.sdk.buffer,f.sdk.table} and f.shader_events==[['create',7],['release',7]],'all SDK and generated handle lifetimes')
 f.check(f.arena.peak_reserved<=65536,'same fixed64KiB arena')
 directory,payload=specimen('normal')
 capture=[directory.hex(),payload.hex(),f.mesh_input.hex(),[f.mesh_capture,[f.animation_capture,[f.sdk.calls,draw,f.shader_events]]]]
 if not return_capture:print('SCENE_MESH_SKIN_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: SAN/scene/decoded-mesh generating Skin {mode}; skinRead={f.read_instructions}; '
       f'sanRead={f.san_read_instructions}; tick={f.tick_instructions}; meshRead={f.mesh_read_instructions}; '
       f'render={f.render_instructions}; peakArena={f.arena.peak_reserved}; ownerGenerations={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
