#!/usr/bin/env python3
"""Original MaterialTexture->CPU TextureData intrusive ownership/alias/clone.

No texture backend required for this actual CPUData lifetime dependency.
Controller fields are absent; this does not claim their clone semantics.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture

def main(mode):
    if mode not in ('alias','rebind','two','clone'):raise ValueError('four small texture ownership cases')
    f=PCFileBytesFixture(b'');p=f.p;checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    material=f.call(0x467f30);texture=f.call(0x41a2d0)
    check(f.allocations[material]==0x68 and p.uint(material+0x34)==0,'original empty material-texture68 factory')
    f.call(0x41e870,this=material,args=(texture,))
    check(p.uint(material+0x34)==texture and (p.uint(texture+8)&65535)==1,'first fallback retains native texture')
    if mode=='alias':
        f.call(0x41e870,this=material,args=(texture,))
        check((p.uint(texture+8)&65535)==1 and texture not in f.freed,'same pointer fallback is actual no-op unlike palette setter')
        f.call(0x41e870,this=material,args=(0,))
        check(p.uint(material+0x34)==0 and texture in f.freed,'clear last owner destroys CPUData')
    elif mode=='rebind':
        replacement=f.call(0x41a2d0);f.call(0x41e870,this=material,args=(replacement,))
        check(texture in f.freed and p.uint(material+0x34)==replacement,'replace releases old fallback')
        check((p.uint(replacement+8)&65535)==1,'replace retains new fallback once')
    else:
        if mode=='clone':
            f.call(0x52fd90,this=0x755588)
            before=bytes(p.mu.mem_read(material+0x10,36));second=f.call(p.uint(p.uint(material)+8),this=material)
            print('MATERIAL_TEXTURE_CLONE',hex(second),'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
            check(second!=material and bytes(p.mu.mem_read(second+0x10,36))==before,'original clone copies texture states')
        else:
            second=f.call(0x467f30);f.call(0x41e870,this=second,args=(texture,))
        check(p.uint(second+0x34)==texture and (p.uint(texture+8)&65535)==2,'two live holders share same retained texture')
        f.call(p.uint(p.uint(second)),this=second,args=(1,))
        check(texture not in f.freed and (p.uint(texture+8)&65535)==1,'first holder destruction preserves shared resource')
    f.call(p.uint(p.uint(material)),this=material,args=(1,))
    if mode=='clone':f.call(0x6d7db0)
    for address in (0x74e060,0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'all original material/texture/clone-map allocations released')
    print(f'PASS {checks}/{checks}: original MaterialTexture fallback {mode}',flush=True)
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
