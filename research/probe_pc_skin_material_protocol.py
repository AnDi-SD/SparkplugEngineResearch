#!/usr/bin/env python3
"""Original owned Skin material state-save/preserved-selection full draw."""
from pathlib import Path
import sys,json
from pc_instruction_emulator import run_bounded
from probe_pc_skin_owned_material_render import OwnedMaterialSceneMesh
from probe_pc_skin_render import main as render

MODES=('restore','pre-false','post-false','failed-device','preserve-selection')
class MaterialProtocol(OwnedMaterialSceneMesh):
 def __init__(self,mode):
  self.protocol_mode=mode;self.cached_shader_key=0x20011 if mode=='preserve-selection' else 0x1020011
  self.protocol_trace=[];super().__init__('0.25','failed-device' if mode=='failed-device' else 'normal')
 def protocol_state(self):
  p=self.p;r=self.renderer;selected=p.uint(r+0xc18c)
  return [p.uint(r+0xc1c4)&255,p.uint(0x7400fc)&255,0 if not selected else 1 if selected==self.loaded_material else 2,
          p.uint(r+0xc194),p.uint(r+0xc9bc)]
 def before_render(self,skin,bone):
  p=self.p;r=self.renderer;p.mu.mem_write(self.loaded_material+0x6c,b'\1')
  p.mu.mem_write(r+0xc1c4,b'\x7b');p.mu.mem_write(0x7400fc,b'\x33');p.put_uint(skin+0x28,0x80402010)
  if self.protocol_mode=='preserve-selection':
   p.mu.mem_write(r+0xc188,b'\1');p.put_uint(r+0xc18c,p.uint(r+0xc9c0));p.put_uint(r+0xc194,0x10203040)
 def on_callback(self,name):self.protocol_trace.append([name,self.protocol_state()])
 def observe(self,p,name,argc):
  if name=='transform':self.protocol_trace.append([name,self.protocol_state()])
  super().observe(p,name,argc)
 def on_render_completed(self,skin,bone):
  p=self.p;self.render_instructions=sum(p.visits.values());self.protocol_trace.append(['complete',self.protocol_state()])
  self.check(not p.visits.get(0x4c89eb) and not self.sdk.calls and not self.shader_events,'only declared cached shader branch')
  self.check(p.uint(self.renderer+0xc1c4)&255==0x7b,'restored before post failure or unchanged after pre failure')
  self.check(p.uint(0x7400fc)&255==(0x33 if self.protocol_mode=='pre-false' else 0x7b),'single original shared saved byte')
  if self.protocol_mode=='pre-false':self.check(len(self.protocol_trace)==2,'pre failure skips all device and post phases')
  else:
   self.check(self.protocol_trace[1][0]=='transform' and self.protocol_trace[1][1][0]==0,'device sees cleared render state byte')
   expected=p.uint(self.renderer+0xc9c0) if self.protocol_mode=='preserve-selection' else self.loaded_material
   self.check(p.uint(self.renderer+0xc18c)==expected,'original selection-preservation gate')

def main(mode,return_capture=False):
 if mode not in MODES:raise ValueError('bounded Skin material protocol')
 f=MaterialProtocol(mode);device_mode=mode if mode in ('pre-false','post-false','failed-device') else 'normal'
 draw=render(device_mode,True,f);f.arena.assert_engine_released()
 f.check(f.loaded_material in f.freed and f.arena.peak_reserved<=65536,'owning teardown within unchanged arena')
 capture=[f.skin_directory.hex(),f.skin_payload.hex(),f.mesh_input.hex(),
          [mode,f.protocol_trace,[f.material_capture,[f.mesh_capture,[f.animation_capture,draw]]]]]
 if not return_capture:print('SKIN_MATERIAL_PROTOCOL_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {f.checks}/{f.checks}: Skin material protocol {mode}; read={f.read_instructions}; '
       f'render={f.render_instructions}; peakArena={f.arena.peak_reserved}; owners={len(f.arena.history)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
