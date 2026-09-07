#!/usr/bin/env python3
"""Original palette clone/index registration/texture setter and destruction.

Known alias/null hazards use stop-before reads, not intentional unsafe execution.
No original renderer/palette routine is replaced with a synthetic success.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture
from pc_palette_fixtures import PaletteFixture

def main(mode):
    if mode not in ('clone','copy-ctor','register','register-fail','reuse','replace','alias','dtor'):raise ValueError('bounded palette cases')
    f=PCFileBytesFixture(b'') if mode in ('clone','copy-ctor') else PaletteFixture();p=f.p;checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    palette=f.call(0x4b2c80);pixels=bytes(range(256))*4;p.mu.mem_write(palette+0x14,pixels)
    owned=[palette]
    if mode=='copy-ctor':
        p.put_uint(palette+0x10,7)
        target=p.allocate(0x414);f.allocations[target]=0x414;p.mu.mem_write(target,b'\xcc'*0x414);owned.append(target)
        result=f.call(0x4b2c50,this=target,args=(palette,))
        print('PALETTE_COPY_CTOR',hex(result),[hex(p.uint(target+off)) for off in (0,8,0xc,0x10,0x14)],'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
        check(result==target and p.uint(target)==0x6f0a6c,'actual copy constructor restores palette vtable')
        check(p.uint(target+0x10)==0xffffffff,'copy constructor resets index')
        check(bytes(p.mu.mem_read(target+0x14,1024))==pixels,'actual copy constructor copies all1024 entries unlike virtual blank clone')
    elif mode=='clone':
        f.call(0x52fd90,this=0x755588) # actual static clone-map startup, not a fake pair recorder
        p.put_uint(palette+0x10,7)
        clone=f.call(0x4b2cf0,this=palette);owned.append(clone)
        print('PALETTE_CLONE',hex(clone),'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
        check(clone!=palette and f.allocations[clone]==0x414,'original palette clone creates independent object')
        check(p.uint(clone+0x10)==0xffffffff,'clone resets palette index through constructor')
        check(bytes(p.mu.mem_read(clone+0x14,1024))==b'\xcc'*1024,'original no-op copy leaves clone colors constructor-uninitialized')
    elif mode.startswith('register') or mode=='reuse':
        if mode=='register-fail':f.palette_result=0x8876086c
        result=f.call(0x4bb7d0,this=f.renderer,args=(palette,))&255
        check(result==1 and p.uint(palette+0x10)==0,'native registration publishes index even on declared COM failure')
        check(f.palette_events==[[0,pixels.hex()]],'original renderer sends exact1024 palette bytes to device slot11C')
        check(p.uint(f.renderer+0xca10)==1 and p.uint(f.renderer+0xca1c)==0,'fresh index increments counter without free-list')
        if mode=='reuse':
            result=f.call(0x4bb840,this=f.renderer,args=(palette,))&255
            check(result==1 and p.uint(f.renderer+0xca1c)==1,'unregister appends one native free-list node')
            check(p.uint(palette+0x10)==0,'unregister does not clear palette index sentinel')
            replacement=f.call(0x4b2c80);owned.append(replacement);p.mu.mem_write(replacement+0x14,pixels[::-1])
            result=f.call(0x4bb7d0,this=f.renderer,args=(replacement,))&255
            print('PALETTE_REUSE',result,'instructions',sum(p.visits.values()),'heap',p.allocated,flush=True)
            check(result==1 and p.uint(replacement+0x10)==0,'original list removal reuses oldest free index')
            check(p.uint(f.renderer+0xca10)==1 and p.uint(f.renderer+0xca1c)==0,'reuse leaves counter and empties free-list')
            check(p.uint(f.palette_root)==f.palette_root and p.uint(f.palette_root+4)==f.palette_root,'native circular sentinel restored')
    else:
        texture=f.call(0x4ab520)
        f.call(0x4b93c0,this=texture,args=(palette,))
        check(p.uint(texture+0x40)==palette and not f.palette_events,'initial palette assignment neither registers nor increments ownership')
        if mode=='alias':
            f.call(0x4b93c0,this=texture,args=(palette,))
            check(palette in f.freed and p.uint(texture+0x40)==palette,'native same-pointer setter deletes palette then stores dangling pointer')
        elif mode=='replace':
            replacement=f.call(0x4b2c80);owned.append(replacement)
            # New palette remains index-1: original unregister branch safely skips.
            # Observer proves original setter passes NEW, not OLD pointer.
            seen=[]
            def observe(mu,address,size,user):
                if address==0x4bb840:seen.append(p.uint(p.reg('ESP')+4))
            hook=p.mu.hook_add(p.uc.UC_HOOK_CODE,observe)
            f.call(0x4b93c0,this=texture,args=(replacement,));p.mu.hook_del(hook)
            check(seen==[replacement],'original setter unregister call argument is new palette')
            check(palette in f.freed and p.uint(texture+0x40)==replacement,'setter deletes old palette and assigns replacement')
        f.call(0x4abb50,this=texture,args=(1,))
        remaining=owned[-1]
        if mode!='alias':check(remaining not in f.freed,'native DX destructor does not destroy attached palette')
        check(f.device_refs==1,'native DX destructor releases device ref')
        # Explicit external cleanup of observed surviving palette, not native ownership claim.
    for obj in owned:
        if obj not in f.freed:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    if mode=='clone':f.call(0x6d7db0)
    for address in (0x74e060,0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'all allocations freed including declared external palette cleanup')
    print(f'PASS {checks}/{checks}: original palette {mode}; no GPU',flush=True)
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
