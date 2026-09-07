#!/usr/bin/env python3
"""Original PC animation FIELD writer; no whole FFPS save/startup claim.

Independent bounded reader/writer calls, never a resumed whole-loader cap.
Actual nested block/header patching executes; output stays in host byte buffer.
"""
import hashlib
from pathlib import Path
import struct
import sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import animation_rtti,empty_fat,empty_manager
from inspect_pc_san_keys import DEFAULT,u32,fields,inspect
from probe_pc_san_loader import capture
from compare_pc_san_reader import compare

checks=0


def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)


def cleanup(f,animation,serializer):
    p=f.p
    f.call(p.uint(p.uint(animation)),this=animation,args=(1,))
    f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    debug=p.uint(0x75526c)
    if debug:f.call(p.uint(p.uint(debug)),this=debug,args=(1,))
    check(f.bound==f.unbound,'all declared name fixture references released')
    check(set(f.allocations)==set(f.freed),'all native writer/reader/descriptor allocations freed')


def writer_case(name,fail_write=None):
    if name not in {'empty','bbush.san','bflower.san','barrel.san','bw.san'}:
        raise ValueError('bounded field-writer specimen list only')
    if fail_write is not None and (name,fail_write) not in {('empty',1),('empty',9),('bbush.san',100)}:
        raise ValueError('Only independently completed failure contracts are enabled; call30 formatting boundary stays open')
    raw=b'';source=b''
    if name!='empty':
        summary,tracks=inspect(DEFAULT/name);raw=(DEFAULT/name).read_bytes()
        source=raw[u32(raw,20)+8:]
    f=PCWriteBytesFixture(source);p=f.p;animation_rtti(f)
    fat=empty_fat(f);manager,_=empty_manager(f)
    p.put_uint(manager+0x28,fat);p.put_uint(manager+0x18,2);p.put_uint(0x75dde8,manager)
    animation=f.call(0x41a090);serializer=f.call(0x43dab0)
    if source:
        check(f.call(0x43ecc0,this=serializer+0x10,args=(f.stream,animation))&255==1,'native input field reader succeeds')
        check(f.position==len(source) and not f.errors,'input fields fully consumed')
    before=bytes(p.mu.mem_read(animation,0x84))
    f.data=b'';f.position=0;f.io.clear()
    f.fail_write_call=fail_write
    result=f.call(0x43dfe0,this=serializer+0x10,args=(f.stream,animation))&255
    visited=set(p.visits);instructions=sum(p.visits.values());output=f.data
    if fail_write is not None:
        check(result==0 and f.write_calls==fail_write,'native writer stops at injected stream-write failure')
        check(bool(f.errors),'native write diagnostics')
        check(bytes(p.mu.mem_read(animation,0x84))==before,'failed write preserves source object storage')
        cleanup(f,animation,serializer)
        print('WRITER_FAILURE',name,'write-call',fail_write,'partial-bytes',len(output),
              'position',f.position,'instructions',instructions,flush=True)
        return output
    check(result==1,'native field writer succeeds')
    check(not f.errors and f.position==len(output),'writer restores output end after patch-back')
    check(bytes(p.mu.mem_read(animation,0x84))==before,'writer preserves animation object storage')
    check(0x472710 in visited and 0x472b00 in visited,'actual block BeginObject/FinalizeObject called')
    decoded=list(fields(output,0))
    check([kind for kind,_,_ in decoded[:9]]==[0,64,12,6,7,8,9,10,11],
          'writer always emits duration/count and seven pool counts in native order')
    check(u32(decoded[1][1])==p.uint(animation+0x20),'writer reserve hint equals logical track count, not capacity')
    if source:
        check(0x472d30 in visited and 0x472e20 in visited and 0x43ddc0 in visited,
              'native nested begin/end and key writer actually visited')
        check(sum(1 for kind,_,_ in decoded if kind==1)==len(tracks),'one name completes every serialized track')
    if name in {'bbush.san','bflower.san','bw.san'}:
        check(output==source,'all native field bytes exactly match shipped native encoding')
    elif name=='barrel.san':
        check(not any(kind==64 for kind,_,_ in fields(source,0)),'barrel source omits track reserve hint')
        check(output[:5]+output[11:]==source and output[5:11]==b'\x7f\x40'+struct.pack('<I',len(tracks)),
              'barrel output differs ONLY by inserted field64 logical track count')
    if name=='bbush.san':
        original=capture(f,animation,summary,tracks,raw)
        f.position=0;reloaded=f.call(0x41a090)
        check(f.call(0x43ecc0,this=serializer+0x10,args=(f.stream,reloaded))&255==1,'writer output accepted by native reader')
        reread=capture(f,reloaded,summary,tracks,raw)
        # Runtime PRS/identity fields only; usedPools originates in readonly parser.
        del original['usedPools'];del reread['usedPools']
        compared=compare(original,reread)
        check(compared==227,'all captured native reread fields compared')
        f.call(p.uint(p.uint(reloaded)),this=reloaded,args=(1,))
    elif name=='empty':
        check(output==bytes.fromhex('60000000007f40000000006c0000000066000000006700000000680000000069000000006a000000006b0000000000'),
              'empty native writer exact47 bytes')
    cleanup(f,animation,serializer)
    print('WRITER',name,'bytes',len(output),'SHA256',hashlib.sha256(output).hexdigest().upper(),
          'instructions',instructions,'byte-identical-to-input',output==source,
          'fields',[(kind,len(payload)) for kind,payload,_ in decoded[:9]],flush=True)
    return output


def main(names):
    if len(names)>2:raise ValueError('one writer asset and optional failed write call per30s child')
    emit=len(names)==2 and names[1]=='emit'
    if names==['bbush.san','30']:
        raise ValueError('Prior call reached ErrorManager formatting IAT6D9328, not mapped; no blind retry/forwarding')
    result=writer_case(names[0] if names else 'empty',int(names[1]) if len(names)==2 and not emit else None)
    if emit:print('SAN_OUTPUT_HEX',result.hex(),flush=True)
    print(f'PASS {checks}/{checks}: original PC animation field writer contracts')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
