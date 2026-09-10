#!/usr/bin/env python3
"""Paired game copies with original PC methods and explicit PS2 prefixes."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from ps2_scalar_prefix import Ps2ScalarPrefix
import probe_pc_animation_lifecycle as lifetime


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    started=time.perf_counter();report=dict(kind='paired-game-copy-state-contracts',status='running',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),cases=[],diagnostics=[],
        scope='Original PC VectorWrapper/Perception Copy and MoviePlayer parameter setup. PS2 original bounded register/bit-transfer prefixes with actual base Copy and no-op clone completion when applicable. No PS2 arithmetic/FPU equivalence or populated perception tree claim. Signaling NaN x87 load/store effects recorded separately.')
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def fresh():
        with patch.object(lifetime,'PcInstructions',PcBlocks):return TimerFixture()
    def ps2(ranges):
        q=Ps2ScalarPrefix(ranges);q.map(0x21000000,4096);q.map(0x22000000,4096);q.map(0x49f000,4096)
        q.reg('SP',0x22000800);q.reg('GP',0x4a4170);q.write(0x49f810,struct.pack('<I',0x21000e00))
        return q
    try:
        triples=((0,0,0),(0x80000000,0x80000000,0x80000000),(0x3f800000,0xc0200000,0x7f7fffff),(1,0x80000001,0x007fffff),
                 (0x7f800000,0xff800000,0x7fc12345),(0x7f812345,0x3f800000,0x40000000),(0x3f800000,0x7f812345,0xff812345))
        for words in triples:
            report['pending']=dict(kind='vector-copy',words=words);save();f=fresh();p=f.p;source=f.call(0x596df0);dest=f.call(0x596df0)
            p.mu.mem_write(source+0x10,struct.pack('<3I',*words));src=bytes(p.mu.mem_read(source,28));before=bytes(p.mu.mem_read(dest,28));expected=bytearray(before)
            signaling=any((v&0x7f800000)==0x7f800000 and v&0x3fffff and not v&0x400000 for v in words[1:])
            expected[0x10:0x1c]=struct.pack('<3I',*words)
            assert f.call(0x596da0,source,(dest,))&255==1
            actual=bytes(p.mu.mem_read(dest,28));pc_words=struct.unpack('<3I',actual[0x10:0x1c])
            assert bytes(p.mu.mem_read(source,28))==src
            if signaling:assert actual[:0x14]==bytes(expected[:0x14]),'diagnostic still guards unrelated bytes and integer X'
            else:assert actual==bytes(expected)
            q=ps2([(0x2a4f30,0x60),(0x100320,0x48),(0x104f00,8)]);s=0x21000000;d=s+0x100;q.write(s,src);q.write(d,before);q.reg('A0',s);q.reg('A1',d)
            r=q.run(0x2a4f30,[0x2a4f90]);assert q.reg('V0')==1
            exp=bytearray(before);exp[0x10:0x1c]=struct.pack('<3I',*words);assert q.read(d,28)==bytes(exp) and q.read(s,28)==src
            for a in (dest,source):f.call(p.uint(p.uint(a)),a,(1,))
            assert set(f.allocations)==set(f.freed)
            result=dict(kind='vector-copy',inputWords=[f'{v:08X}' for v in words],pcResult=[f'{v:08X}' for v in pc_words],ps2Result=[f'{v:08X}' for v in words],ps2Execution=r)
            if signaling:
                result['limitation']='Unicorn preserves signaling NaN through x87 FLD/FSTP. Game x87 control/exception mode and hardware result not established;no PC NaN golden or platform equality claim. PS2 LWC1/SWC1 bit transfer remains verified.'
                report['diagnostics'].append(result)
            else:report['cases'].append(result)
            report.pop('pending');save()
        fields=[(x,4) for x in (0x24,0x28,0x2c,0x30,0x34,0x38,0x3c,0x44,0x4c,0x50,0x54,0x74)]+[(x,1) for x in (0x40,0x48,0x70)]
        for seed in (0,1,255):
            report['pending']=dict(kind='perception-copy',seed=seed);save();f=fresh();p=f.p;source=f.call(0x4f2020);dest=f.call(0x4f2020)
            originals={a:bytes(p.mu.mem_read(a,124)) for a in (source,dest)}
            for i,(offset,n) in enumerate(fields):p.mu.mem_write(source+offset,bytes(((seed+i*29+j*13)&255) for j in range(n)))
            src=bytes(p.mu.mem_read(source,124));expected=bytearray(originals[dest])
            for offset,n in fields:expected[offset:offset+n]=src[offset:offset+n]
            assert f.call(0x4f20d0,source,(dest,))&255==1
            assert bytes(p.mu.mem_read(dest,124))==bytes(expected) and bytes(p.mu.mem_read(source,124))==src
            # Fresh PS2 prefix after its independently recorded container-copy phase.
            q=ps2([(0x36ee18,0xc0),(0x104f00,8)]);s=0x21000000;d=s+0x100;q.write(s,src);q.write(d,originals[dest]);q.reg('S1',s);q.reg('S0',d)
            r=q.run(0x36ee18,[0x36eed8]);assert q.reg('V0')==1 and q.read(d,124)==bytes(expected) and q.read(s,124)==src
            # Restore only caller-provided primitive test fields before cleanup.
            # This does not claim populated runtime dependencies can be destroyed.
            for a in (dest,source):
                for offset,n in fields:p.mu.mem_write(a+offset,originals[a][offset:offset+n])
                f.call(p.uint(p.uint(a)),a,(1,))
            assert set(f.allocations)==set(f.freed)
            report['cases'].append(dict(kind='perception-copy',seed=seed,copiedFields=fields,ps2Execution=r,scope='PC full Copy with actual empty trees;PS2 post-container scalar tail. Pointer68/6C remain null. Modified primitive inputs restored before cleanup.'));report.pop('pending');save()
        for left,right,flag in ((0,0,0),(0xffffffff,0x80000000,255),(800,600,1),(0x11223344,0xaabbccdd,2)):
            report['pending']=dict(kind='movie-setup',left=left,right=right,flag=flag);save();f=fresh();p=f.p;obj=f.call(0x601dd0);assert f.allocations[obj]==200
            inp=p.allocate(0x50);p.mu.mem_write(inp,b'\xa5'*0x50);p.put_uint(inp+0x40,left);p.put_uint(inp+0x44,right);p.mu.mem_write(inp+0x4c,bytes([flag]));data=bytes(p.mu.mem_read(inp,0x50))
            before=bytes(p.mu.mem_read(obj,200));expected=bytearray(before);expected[0x14:0x18]=bytes(4);expected[0x1c:0x24]=struct.pack('<2I',left,right);expected[0x24]=flag
            assert f.call(0x5fa110,obj,(inp,))&255==1 and bytes(p.mu.mem_read(obj,200))==bytes(expected) and bytes(p.mu.mem_read(inp,0x50))==data
            q=ps2([(0x382320,0x24)]);s=0x21000000;d=s+0x100;q.write(s,data);q.write(d,before);q.reg('A0',d);q.reg('A1',s)
            r=q.run(0x382320,[q.RETURN]);assert q.reg('V0')==1 and q.read(d,200)==bytes(expected) and q.read(s,0x50)==data
            f.call(p.uint(p.uint(obj)),obj,(1,));assert set(f.allocations)==set(f.freed)
            report['cases'].append(dict(kind='movie-setup',inputFields=[left,right,flag],ps2Execution=r,scope='Identical writes relative to direct method receiver;PS2 storage is a declared record,not an executed MoviePlayerPS2 constructor or allocation claim.'));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error),pending=report.get('pending'))));return 1
    print(json.dumps(dict(status='passed',cases=len(report['cases']),diagnostics=len(report['diagnostics']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
