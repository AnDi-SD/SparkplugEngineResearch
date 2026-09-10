#!/usr/bin/env python3
"""Constant-transform dispatch, PC translation and independent PS2 gates."""
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
    started=time.perf_counter();report=dict(kind='transform-const-eval-independent-dispatch',status='running',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),cases=[],
        scope='PC full finite translation/disabled Evaluate or stops before original rotation/FunctionEval calls. PS2 disabled path or stops before multiply,axis-angle helper and scalar sampler. Virtual slot order differs between platforms. No R5900 COP1 accumulator emulation or rotation golden.')
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    cases=[dict(kind='disabled',position=0,rotation=0,scale=0,time=0.5)]
    cases += [dict(kind='translation',position=flag,rotation=0,scale=0,time=t) for flag,t in ((1,-2.0),(1,0.0),(255,0.5))]
    cases += [dict(kind='rotation-prefix',position=0,rotation=flag,scale=0,time=t) for flag,t in ((1,0.5),(255,-2.0))]
    cases += [dict(kind='scale-prefix',position=0,rotation=0,scale=value,time=0.5) for value in (1,0x80000000)]
    factory=next(r['pcFactory'] for r in json.loads((ROOT/'local-data/results/native-cycle-20260910-1900/engine-core-remainder/catalog-family.json').read_text()) if r['className']=='spTransformConstEval')
    try:
        for c in cases:
            report['pending']=c;save()
            with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
            p=f.p;obj=f.call(factory);assert f.allocations[obj]==104
            p.mu.mem_write(obj+0x64,bytes([c['position'],c['rotation']]));p.put_uint(obj+0x60,c['scale']);p.put_floats(obj+0x10,(1,2,4));p.put_floats(obj+0x1c,(0,0,1));p.put_floats(obj+0x28,(0.25,))
            storage=p.allocate(0x100);buffer=bytearray(b'\xa5'*0x100);buffer[:12]=struct.pack('<3f',10,20,30);buffer[0x20:0x30]=struct.pack('<4f',0,0,0,1);buffer[0x40:0x4c]=struct.pack('<3f',2,3,4);p.mu.mem_write(storage,bytes(buffer))
            before=bytes(p.mu.mem_read(obj,104));time_bits=struct.unpack('<I',struct.pack('<f',c['time']))[0];args=(time_bits,storage,storage+0x20,storage+0x40,storage+0x60,storage+0x64,storage+0x68)
            expected=bytearray(buffer);target=None
            if c['kind'] in ('disabled','translation'):
                f.call(0x601b20,obj,args)
                if c['position']:expected[:12]=struct.pack('<3f',10+c['time'],20+2*c['time'],30+4*c['time'])
                expected[0x60]=int(bool(c['position']));expected[0x64]=0;expected[0x68]=0
            elif c['kind']=='rotation-prefix':
                target=0x464c80;p.run(0x601b20,this=obj,args=args,stop_at=target);expected[0x60]=0
                assert p.uint(p.reg('ESP')+4)==0x3e800000 and p.uint(p.reg('ESP')+8)==obj+0x1c,'rotation angle unscaled by time'
            else:
                target=p.uint(p.uint(obj+0x2c)+0x1c);assert target==0x478680
                p.run(0x601b20,this=obj,args=args,stop_at=target);expected[0x60]=expected[0x64]=0
                assert p.reg('ECX')==obj+0x2c and p.uint(p.reg('ESP')+4)==time_bits
            assert bytes(p.mu.mem_read(storage,256))==bytes(expected) and bytes(p.mu.mem_read(obj,104))==before
            q=Ps2ScalarPrefix([(0x11d004,0x148)]);qo=0x21000000;qb=qo+0x100;q.map(qo,4096);q.map(0x22000000,4096);q.write(qo,before);q.write(qb,bytes(buffer));q.reg('SP',0x22000800)
            for reg,value in [('A0',qo),('S5',qo),('S4',qb),('S3',qb+0x20),('S2',qb+0x40),('S1',qb+0x60),('S0',qb+0x64),('S6',qb+0x68)]:q.reg(reg,value)
            stop={'disabled':0x11d148,'translation':0x11d04c,'rotation-prefix':0x10a990,'scale-prefix':0x11d120}[c['kind']]
            r=q.run(0x11d004,[stop]);ps2expected=bytearray(buffer)
            if c['kind']=='disabled':ps2expected[0x60]=ps2expected[0x64]=ps2expected[0x68]=0
            elif c['kind']=='rotation-prefix':
                ps2expected[0x60]=0;assert q.reg('A1')==qo+0x1c and q.reg('F12')&0xffffffff==0x3e800000
            elif c['kind']=='scale-prefix':ps2expected[0x60]=ps2expected[0x64]=0
            assert q.read(qb,256)==bytes(ps2expected) and q.read(qo,104)==before
            # Full constructor/Clone proof elsewhere remains separate from these inputs.
            p.put_uint(obj+0x60,0);p.mu.mem_write(obj+0x64,b'\0\0');f.call(p.uint(p.uint(obj)),obj,(1,));assert set(f.allocations)==set(f.freed)
            report['cases'].append(dict(input=c,pcStop=f'{target:08X}' if target else 'original return',pcOutput=bytes(expected).hex(),ps2Stop=f'{stop:08X}',ps2Execution=r));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error),pending=report.get('pending'))));return 1
    print(json.dumps(dict(status='passed',cases=len(cases),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
