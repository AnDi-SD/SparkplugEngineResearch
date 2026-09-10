#!/usr/bin/env python3
"""Original PC packet codec against actual spMemoryStream, with bounded stops."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from capture_native_ranges import EXPECTED
import probe_pc_animation_lifecycle as lifetime

FIELDS=((0x10,2,0x499711),(0x12,2,0x499737),(0x14,2,0x49975d),(0x16,1,0x499783),(0x18,4,0x4997a9),(0x1c,4,0x4997cf),(0x20,2,0x4997f6))


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    started=time.perf_counter();report=dict(kind='original-pc-network-packet-memory-codec',status='running',inputs={'pc':EXPECTED['pc']},cases=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Actual NetworkPacket and MemoryStream factories/resize/read/write/teardown.17-byte packed header. Truncated inputs stop at the next actual field-read call site,oversize stops before payload read/write;no out-of-bounds transfer or substituted logger. Complete read failure only for empty stream. Network operation/caller size policy not claimed.',limits='PC micro100k/2s per call,outer30s;packet payload256,stream<=512,whole object/buffer guards')
    cases=[dict(kind='roundtrip',size=n,tcp=t) for n,t in ((0,0),(1,1),(16,255),(255,1),(256,0))]
    cases += [dict(kind='truncated-header',available=n,size=4,tcp=1) for n in (0,1,3,5,6,9,13,16)]
    cases += [dict(kind=k,size=257,tcp=1) for k in ('oversize-read','oversize-write')]
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for case in cases:
            report['pending']=case;save()
            with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
            p=f.p;packet=f.call(0x499960);initial=bytes(p.mu.mem_read(packet,40));payload=p.uint(packet+0x24)
            assert f.allocations[packet]==40 and f.allocations[payload]==256
            size=case['size'];body=bytes((i*31+7)&255 for i in range(size));wire=struct.pack('<HHHBIIH',0x1234,0xfedc,0x8765,case['tcp'],0x11223344,0x80000000,size)+body
            stream=f.call(0x465560);assert f.allocations[stream]==56 and p.uint(stream)==0x6e7e50
            read=lambda a,n:bytes(p.mu.mem_read(a,n))
            def resize(n):
                assert f.call(0x465500,stream,(n,))&255==1
                assert p.uint(stream+0x1c)==n and p.uint(stream+0x20)==n and p.uint(stream+0x2c)==0
                return p.uint(stream+0x30)
            reader=case['kind']!='oversize-write';available=case.get('available',len(wire));raw=wire[:available]
            if reader:
                data=resize(len(raw));p.mu.mem_write(data,raw);before=read(packet,40);buffer_before=read(payload,256);expected=bytearray(before)
                at=0;next_stop=None
                for offset,n,call in FIELDS:
                    if at+n>len(raw):next_stop=call;break
                    expected[offset:offset+n]=raw[at:at+n];at+=n
                if case['kind']=='roundtrip':
                    assert f.call(0x4996d0,packet,(stream,))&255==1;expected_buffer=body+buffer_before[size:];completion='original Read return true'
                    assert p.uint(stream+0x2c)==len(wire)
                elif not available:
                    assert f.call(0x4996d0,packet,(stream,))&255==0;expected_buffer=buffer_before;completion='original Read empty return false'
                elif case['kind']=='truncated-header':
                    assert next_stop;p.run(0x4996d0,this=packet,args=(stream,),stop_at=next_stop);expected_buffer=buffer_before;completion=f'next unavailable scalar call{next_stop:08X};not executed'
                    assert p.uint(stream+0x2c)==at
                    assert p.uint(p.reg('ESP'))==packet+FIELDS[next(i for i,x in enumerate(FIELDS) if x[2]==next_stop)][0]
                else:
                    p.run(0x4996d0,this=packet,args=(stream,),stop_at=0x499823);expected_buffer=buffer_before;completion='before original payload ReadData request'
                    assert p.uint(stream+0x2c)==17 and (p.uint(p.reg('ESP')),p.uint(p.reg('ESP')+4))==(payload,257)
                assert read(packet,40)==bytes(expected) and read(payload,256)==expected_buffer,'whole packet and payload guards'
                assert read(data,len(raw))==raw,'input stream bytes unchanged'
            else:
                at=0
                for offset,n,_ in FIELDS:p.mu.mem_write(packet+offset,wire[at:at+n]);at+=n
                data=resize(512);assert f.call(0x465550,stream)&255==1
                before=read(packet,40);buffer_before=read(payload,256);p.run(0x499850,this=packet,args=(stream,),stop_at=0x49992f)
                assert (p.uint(p.reg('ESP')),p.uint(p.reg('ESP')+4))==(payload,257) and p.uint(stream+0x2c)==17
                assert read(data,17)==wire[:17] and read(packet,40)==before and read(payload,256)==buffer_before
                completion='before original payload WriteData request'
            if case['kind']=='roundtrip':
                clone=f.call(0x4999c0,packet);assert clone!=packet and read(clone,36)==initial[:36] and p.uint(clone+0x24)!=payload
                assert read(p.uint(clone+0x24),256)==b'\xcc'*256,'Clone allocates fresh packet payload'
                f.call(p.uint(p.uint(clone)),clone,(1,));assert clone in f.freed
                assert f.call(0x465550,stream)&255==1;assert f.call(0x499850,packet,(stream,))&255==1
                data=p.uint(stream+0x30);assert p.uint(stream+0x20)==len(wire) and p.uint(stream+0x2c)==len(wire) and read(data,len(wire))==wire,'original Write exact17-byte header and payload'
                assert read(packet,40)==bytes(expected) and read(payload,256)==expected_buffer,'Write leaves source packet unchanged'
                completion+=';original Write return true and fresh Clone'
            for a in (stream,packet):f.call(p.uint(p.uint(a)),a,(1,));assert a in f.freed
            assert set(f.allocations)==set(f.freed),'actual packet/stream buffers and objects all freed'
            report['cases'].append(dict(input=case,completion=completion,wireHeaderHex=wire[:17].hex(),originalBufferBytes=256,memoryStreamBytes=56,allTrackedAllocationsFreed=True));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error),pending=report.get('pending'))));return 1
    print(json.dumps(dict(status='passed',cases=len(cases),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
