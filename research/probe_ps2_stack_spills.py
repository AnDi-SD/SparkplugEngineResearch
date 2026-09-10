"""Bounded feasibility probe: real nested trigger/HUD frames, no guest patches."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED
from ps2_scalar_prefix import Ps2ScalarPrefix
from pc_instruction_emulator import run_bounded

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/ps2-stack-spills'
UPPER=tuple(0 if i==0 else 0xace0000000000000+i*0x101010101 for i in range(32))


def execute(entry):
    p=Ps2ScalarPrefix([(0x3ac730,0x50),(0x3794d0,0x10),(0x37e790,0x1a0),(0x3b3c10,0x50),(0x3b5a00,0x50)],stack_window=(0x22000000,4096),upper64=UPPER)
    base=0x21000000;obj=base;hud=base+0x1000;window=base+0x2000;node=base+0x3000;profile=base+0x4000
    p.map(base,0x5000);p.map(0x49f000,4096);p.map(0x22000000,4096)
    p.write(base,b'\xa5'*0x5000);p.write(0x22000000,b'\x5a'*4096)
    p.put_uint(0x49fda4,hud);p.put_uint(hud+0x60,window);p.put_uint(window+0x18,node)
    p.put_uint(0x49fc7c,profile);p.put_uint(profile+0x504,16)
    p.put_uint(window+0x144,0);p.put_uint(window+0x148,0x41200000);p.write(window+0x14c,b'\x01')
    p.put_uint(window+0x130,0);p.put_uint(window+0x134,0);p.put_uint(node+0xb4,0x40)
    p.write(obj+0x150,b'\x01');p.write(obj+0x144,b'\x01');p.write(obj+0x132,b'\x01')
    p.reg('GP',0x4a4170);p.reg('SP',0x22000800);p.reg('A0',obj)
    p.reg('S0',0x0123456789abcdef);p.reg('S1',0xfedcba9876543210)
    before=p.read(base,0x5000);expected=bytearray(before)
    for offset in [0x150,0x144,0x2000+0x14c]:expected[offset]=0
    if entry!=0x3ac730:
        expected[0x132]=0;struct.pack_into('<I',expected,0x4000+0x504,0)
    for offset,value in [(0x3020,0),(0x3024,0),(0x3028,0xc1200000),(0x30b4,0x41)]:struct.pack_into('<I',expected,offset,value)
    original=[p.read(a,n) for a,n in p.ranges];stack_before=p.read(0x22000000,4096)
    try:
        result=p.run(entry,[p.RETURN],timeout_us=500000)
        assert p.read(base,0x5000)==expected
        assert [p.reg(n) for n in ('SP','RA','S0','S1')]==[0x22000800,p.RETURN,0x0123456789abcdef,0xfedcba9876543210]
        assert p.stack_extension.upper64==list(UPPER)
        assert original==[p.read(a,n) for a,n in p.ranges]
        result.update(status='passed',guardedBytes=0x5000,afterSha256=hashlib.sha256(p.read(base,0x5000)).hexdigest().upper(),codeUnchanged=True,callerRegistersRestored=True)
    except Exception as e:result=dict(status='blocked',error=str(e),traceback=traceback.format_exc())
    result.update(entry=f'{entry:08X}',trace=[f'{a:08X}' for a in p.trace],stackChanges=[i for i,(a,b) in enumerate(zip(stack_before,p.read(0x22000000,4096))) if a!=b])
    return result


def guest(output):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(FOLDER):raise ValueError('Fresh local output required')
    started=time.perf_counter();report=dict(kind='original-ps2-stack-spill-feasibility',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),cases=[])
    for entry in (0x3ac730,0x3b3c10,0x3b5a00):
        row=execute(entry);report['cases'].append(row)
        report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        if row['status']!='passed':break
    report['status']='passed' if len(report['cases'])==3 and all(c['status']=='passed' for c in report['cases']) else 'blocked'
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status=report['status'],seconds=report['seconds'],cases=[{k:c.get(k) for k in ('entry','status','instructions','error')} for c in report['cases']])))
    return int(report['status']!='passed')


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
