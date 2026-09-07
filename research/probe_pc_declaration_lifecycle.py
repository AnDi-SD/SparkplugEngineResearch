#!/usr/bin/env python3
"""PC declaration clone/reinitialize/failure semantics, bounded guest COM only."""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from pc_declaration_fixture import DeclarationDeviceFixture
from probe_pc_dx_buffers import check
import probe_pc_dx_buffers as counters

def main(mode):
    if mode not in {'clone','reinitialize','reinitialize-failure','bind-failure'}:
        raise ValueError('explicit declaration lifetime mode required')
    f=DeclarationDeviceFixture();p=f.p
    source=f.call(0x4ae0e0,this=f.renderer,args=(0x840,));old=p.uint(source+0x18)
    check(old in f.declarations and f.declarations[old]==1,'initial live COM declaration')
    if mode=='clone':
        f.call(0x52fd90,this=0x755588) # actual isolated clone-map startup, no atexit
        name=p.allocate(32);p.mu.mem_write(name+8,b'\x01declaration-name\0')
        p.put_uint(source+0x10,name);p.put_uint(source+0xc,0x12345678)
        clone=f.call(0x4c9c90,this=source)
        print('CLONE',f.words(source,0xc,0x1c),f.words(clone,0xc,0x1c),'instructions',sum(p.visits.values()),flush=True)
        check(clone!=source and f.allocations[clone]==0x1c,'actual concrete clone allocates separate exact1C object')
        check(p.uint(clone+0x14)==p.uint(clone+0x18)==0,'clone omits converted FVF and COM payload')
        check(p.uint(clone+0xc)==0 and p.uint(clone+0x10)==name,'actual inherited copy preserves shared name but not opaque root fieldC')
        check(bytes(p.mu.mem_read(name+8,1))==b'\x02','actual name copy increments shared byte reference')
        check(0x412f70 in p.visits and 0x413120 in p.visits and p.uint(0x755590)==1,'actual clone-map registration and inherited copy, no pair seam')
        check(f.declarations[old]==1 and len(f.declaration_arrays)==1,'clone never creates or retains source COM declaration')
        check(f.call(0x4a1bf0,this=source)==0,'actual abstract-base clone slot returns null')
        # Synthetic shared-name input is not a real string-registry allocation.
        p.put_uint(source+0x10,0);p.put_uint(clone+0x10,0)
        f.call(0x6d7db0) # actual static clone-map teardown
        manager=p.uint(0x74e060)
        if manager:f.call(p.uint(p.uint(manager)),this=manager,args=(1,))
    elif mode=='bind-failure':
        original=p.seams[0x340600a0]
        def failed_bind(p):original(p);p.set_reg('EAX',0x80004005)
        p.seams[0x340600a0]=failed_bind
        check(f.call(0x4c9d00,this=source,args=(0xdeadbeef,))&255==1,'native Bind discards failing HRESULT')
        check(p.uint(source+0x18)==old,'failed Bind does not mutate declaration pointer')
    else:
        if mode=='reinitialize-failure':f.mode='declaration-failure'
        check(f.call(0x4c9d20,this=source,args=(0x940,))&255==1,'reinitialize returns true including failed COM create')
        new=p.uint(source+0x18)
        check(p.uint(source+0x14)==0x152,'base FVF changed before second backend create')
        check(f.declarations[old]==1 and not any(e[0]=='release-declaration' for e in f.events),'native reinitialize overwrites old pointer WITHOUT releasing old COM object')
        check((new==0)==(mode=='reinitialize-failure') and new!=old,'failed/successful out pointer overwrites prior value')
        check(f.call(0x4ae0e0,this=f.renderer,args=(0x840,))==source and p.uint(source+0x14)==0x152,'original cache key remains840 despite mutated object FVF152')
    f.clear_declarations(allow_overwritten_leak=mode.startswith('reinitialize'))
    if mode.startswith('reinitialize'):
        check(f.declarations[old]==1,'normal object/map teardown still leaves overwritten old COM resource alive')
        # Explicit fixture cleanup of original leak, not native recovery.
        p.run(0x340600b0,args=(old,))
    check(all(v==0 for v in f.declarations.values()),'all COM fixtures released after separately labelled leak cleanup')
    check(set(f.allocations)==set(f.freed),'tracked native allocations freed')
    print(f'PASS {counters.checks}/{counters.checks}: PC declaration {mode}; no GPU/startup emulation')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
