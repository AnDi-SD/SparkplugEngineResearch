#!/usr/bin/env python3
"""Read-only real PC ColorController payload, actual codec on declared state.

The native controller factory is still excluded; missing wire type fields
cannot establish its true constructor defaults. Type0 state is explicit input.
"""
from pathlib import Path
import hashlib,json,sys
from pc_instruction_emulator import ROOT,run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_material_color import controller_input,OFFSETS
from probe_pc_function_eval import cleanup,bits

ASSET=ROOT/'local-data/pc-pristine/Media/Characters/Knut/lightbeam_projectile.smo'
ASSET_SHA256='7bea3af6dc3643bba1ced3e72b61743eb12f77ee9accdcc6c86d092707d53482'
PAYLOAD='e02800000063cdcccc3d0063cdcccc3d0063cdcccc3d0063cdcccc3d0061cdcccc3d6200000000640000803f0000'

def main(return_capture=False):
    raw=ASSET.read_bytes()
    if len(raw)!=5584 or hashlib.sha256(raw).hexdigest()!=ASSET_SHA256:raise AssertionError('fixed pristine corpus sample hash')
    if raw[564:572]!=bytes.fromhex('853e634c53424f4f'):raise AssertionError('object5 physical564 actual class/header')
    payload=raw[572:618]
    if payload.hex()!=PAYLOAD:raise AssertionError('recorded54-byte object payload')
    f=PCWriteBytesFixture(payload);p=f.p;animations=f.call(0x454640);controller=controller_input(f);material=f.call(0x41a390);checks=3;states=[]
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    p.put_floats(material+0x84,(.375,));f.call(0x423650,this=controller,args=(material,));serializer=f.call(0x441200)
    result=f.call(0x4412e0,this=serializer+0x10,args=(f.stream,controller))&255;maximum=sum(p.visits.values())
    check(result==1 and f.position==len(payload) and not f.errors,'actual reader consumes pristine color payload')
    check(all(p.uint(controller+off+0x4c)==0 for off in OFFSETS) and p.uint(controller+0x1dc)==0,'absent types retain explicit input zero, not factory evidence')
    check(p.floats(controller+0x1c4,1)==(0.,) and p.floats(controller+0x1cc,1)==(1.,),'corpus alpha amplitude0/y1')
    for delta in (.25,.5,.25):
        f.call(0x423190,this=controller,args=(bits(delta),));f.call(0x4373e0,this=controller);maximum=max(maximum,sum(p.visits.values()))
        colors=[list(p.floats(material+off,4)) for off in (0x88,0x78,0x98,0xa8)]
        times=[p.floats(controller+off+0x28,1)[0] for off in OFFSETS]+[p.floats(controller+0x1b8,1)[0]]
        check(colors[1][3]==.375 and times==[0.]*5,'type0 gates real wire yOffset1; no leaf time advances')
        states.append([delta,colors,times,list(p.floats(controller+0x1c,2))])
    f.data=b'';f.position=0;result=f.call(0x441740,this=serializer+0x10,args=(f.stream,controller))&255;maximum=max(maximum,sum(p.visits.values()))
    check(result==1 and f.data==payload and not f.errors,'original writer exactly reproduces pristine payload after runtime');written=f.data
    for obj in (material,serializer):f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    f.call(0x4545d0,this=animations,args=(1,));cleanup(f,());check(set(f.allocations)==set(f.freed),'actual leaves/material/codec/manager allocations released')
    capture=['corpus',payload.hex(),states,written.hex()];print('MATERIAL_COLOR_CORPUS_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: actual pristine color payload/read/runtime/write; maxInstructions={maximum}; heap={p.allocated}; factoryExcluded=true',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
