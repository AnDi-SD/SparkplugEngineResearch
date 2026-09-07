#!/usr/bin/env python3
"""Same native Light world/payload -> whole lit Skin shader constants and draw."""
from pathlib import Path
import sys,json,struct
from pc_instruction_emulator import run_bounded
from probe_pc_skin_lights_render import LitSkin
from probe_pc_skin_render import main as render

MODES=('directional','point','spot','disabled','failed-device','post-false','directional-special','disabled-special','ambient-special','empty-special')
VIEW=(2.,0.,0.,0.,0.,3.,0.,0.,0.,0.,4.,0.,5.,6.,7.,1.)
class LitConstants(LitSkin):
 def __init__(self,mode):self.constant_mode=mode;super().__init__(mode.removesuffix('-special'))
 def prepare_shader(self):
  manager=super().prepare_shader();p=self.p
  names=(b'AmbientCol',b'LightAmbientColorDir0',b'LightDiffuseColorDir0',b'LightSpecularColorDir0') if self.constant_mode.endswith('-special') else (b'LightMatDiff',b'LightPos',b'LightDir',b'LightAttenuation')
  for start,name in enumerate(names,4):self.call(0x4af940,this=self.cached_shader,args=struct.unpack('<11I',name.ljust(32,b'\0')+struct.pack('<III',0xdeadbeef,start,1)))
  self.check(p.uint(self.cached_shader+0x34)==8,'all eight constant rows produced by original append/type resolver')
  return manager
 def prepare_light_world(self,obj):
  p=self.p;p.put_floats(obj+0x20,(4.,5.,6.));p.put_floats(obj+0x40,(1.,0.,0.,0.,1.,0.,1.,2.,3.))
  self.call(0x421420,this=obj,args=(1,))
  self.check(p.floats(obj+0x74,3)==(4.,5.,6.) and p.floats(obj+0xa4,3)==(1.,2.,3.),'whole base Node world producer on same actual DXLight')
  self.light_world=[p.uint(obj+off+4*i) for off in (0x74,0xa4,0x58) for i in range(3)]
  p.put_floats(self.renderer+0xca80,VIEW)
 def on_render_completed(self,skin,bone):
  super().on_render_completed(skin,bone)
  self.check(self.p.visits.get(0x4ae930),'whole original constant builder executes')
  self.check(any(e[0]=='vertex-constants' and e[3]==8 for e in self.events),'eight complete native rows reach COM')

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded full Skin light constants')
 f=LitConstants(mode);device_mode=mode if mode in ('failed-device','post-false') else 'normal';draw=render(device_mode,True,f);f.arena.assert_engine_released()
 f.check(f.light in f.freed and f.loaded_material in f.freed,'all actual light/Skin/material owners released')
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),f.mesh_input.hex(),
          [mode,f.light_world,[f.light_mode,f.light_words,f.light_state,[f.material_capture,[f.mesh_capture,[f.animation_capture,draw]]]]]]
 if not return_capture:print('SKIN_LIGHT_CONSTANTS_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: full Skin light constants {mode}; read={f.read_instructions}; render={f.render_instructions}; peakArena={f.arena.peak_reserved}; owners={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
