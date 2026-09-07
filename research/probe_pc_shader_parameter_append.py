#!/usr/bin/env python3
"""Actual shader by-value44 parameter append4AF940→45A6C0; no compiler seam."""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from probe_pc_function_eval import cleanup
from pc_serializer_fixtures import PCWriteBytesFixture

NAMES=('view_proj_matrix','view_matrix','VPTransform','inv_view_matrix','BlendMatrices','AmbientCol','ConstColor','MatDiffuse','MatSpecular','MatSpecularPwr','LightMatDiff','LightMatSpec','LightPos','LightDir','LightInner','LightOuter','UVTransform','ViewDirLightDir0','LightAmbientColorDir0','LightDiffuseColorDir0','LightSpecularColorDir0','LightAttenuation','','unknown','matDiffuse','MatDiffuse[0]')
def main(mode,return_capture=False):
    if mode not in ('names','overlap','wrap'):raise ValueError('bounded parameter vector append')
    f=PCWriteBytesFixture();p=f.p;shader=f.call(0x4c9f10);captures=[];maximum=0;checks=0
    if mode=='wrap':p.put_uint(shader+0x34,0xfffffffe)
    names=NAMES if mode=='names' else ('MatDiffuse','MatSpecularPwr','unknown','BlendMatrices')
    for i,name in enumerate(names):
        encoded=name.encode()+b'\0';raw=encoded+b'\xcc'*(32-len(encoded));start=17 if mode=='overlap' else i*7;count=(i%5) if mode!='wrap' else (3,0,0xffffffff,2)[i]
        words=struct.unpack('<11I',raw+struct.pack('<III',0xdeadbeef,start,count))
        f.call(0x4af940,this=shader,args=words);maximum=max(maximum,sum(p.visits.values()));checks+=1
        if not p.visits.get(0x45a6c0) or not p.visits.get(0x4ae660) or not p.visits.get(0x4af850):raise AssertionError('actual protected entry/name/vector chain')
        begin=p.uint(shader+0x3c);end=p.uint(shader+0x40)
        if end-begin!=44*(i+1):raise AssertionError('one actual descriptor appended')
        captures.append([p.uint(shader+0x34),[[p.uint(begin+44*j+4*k) for k in range(11)] for j in range(i+1)]])
    cleanup(f,(shader,));checks+=1;capture=[mode,captures]
    if not return_capture:print('SHADER_PARAMETER_APPEND_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original shader append {mode}; maxInstructions={maximum};heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
