#!/usr/bin/env python3
"""Execute unique original secondary-interface adjustor thunks, not payloads."""
import hashlib,json,sys,time,traceback
from pathlib import Path
from pc_instruction_emulator import ROOT,run_bounded
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from capture_native_ranges import EXPECTED,read_window


def guest(output):
    output=Path(output).resolve();source=ROOT/'local-data/results/native-cycle-20260910-1900/serializer-expansion/ps2-inline-identities.json'
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    start=time.perf_counter();report=dict(kind='original-ps2-serializer-secondary-adjustors',status='running',cases=[],routes=[],inputs=dict(ps2=EXPECTED['ps2']),
        identitySourceSha256=hashlib.sha256(source.read_bytes()).hexdigest().upper(),sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Unique exact8-byte original J/addiu-A0 thunks execute once,stop at actual payload method before its first instruction. A0 secondary receiver becomes complete object;other arguments and whole object bytes guarded. Alias slots reuse same observed execution. Payload methods not executed or replaced.',limits='PS2 R4000 scalar10 instructions/100ms,384KiB,outer30s')
    raw,sections=pristine();seen={}
    def save():report['seconds']=time.perf_counter()-start;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for row in json.loads(source.read_text()):
            for slot,entry in enumerate(row['secondaryFirstFiveWords'][:3]):
                b,offset=read_window('ps2',raw,entry,8,sections);j,delay=int.from_bytes(b[:4],'little'),int.from_bytes(b[4:],'little')
                assert j>>26==2 and delay==0x2484fff0,(row['className'],slot,b.hex());target=((entry+4)&0xf0000000)|((j&0x3ffffff)<<2)
                report['routes'].append(dict(className=row['className'],secondarySlot=slot,entry=f'{entry:08X}',target=f'{target:08X}'))
                if entry in seen:assert seen[entry]==target;continue
                report['pending']=dict(entry=f'{entry:08X}',target=f'{target:08X}');save();p=Ps2ScalarPrefix([(entry,8)]);p.map(0x21000000,4096);p.write(0x21000000,b'\xa5'*128)
                p.reg('A0',0x21000010);args=dict(A1=0x11223344,A2=0x55667788,A3=0x12345678,T0=0x23456789)
                for reg,value in args.items():p.reg(reg,value)
                r=p.run(entry,[target],count=10);assert p.reg('A0')==0x21000000 and all(p.reg(k)==v for k,v in args.items()) and p.read(0x21000000,128)==b'\xa5'*128
                r.update(bytes=b.hex(),fileOffset=offset,sourceReceiver=0x21000010,destinationReceiver=p.reg('A0'),otherArgumentsUnchanged=True,wholeObjectUnchanged=True)
                seen[entry]=target;report['cases'].append(r);report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',classes=len({r['className'] for r in report['routes']}),slotRoutes=len(report['routes']),uniqueExecutions=len(report['cases']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
