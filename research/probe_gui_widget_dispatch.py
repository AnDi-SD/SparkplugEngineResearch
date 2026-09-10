#!/usr/bin/env python3
"""Paired original GUI notify and widget setter dispatch, at real consumers."""
import hashlib,json,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from capture_native_ranges import EXPECTED,read_window
import probe_pc_animation_lifecycle as lifetime


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    started=time.perf_counter();report=dict(kind='paired-original-gui-notify-and-widget-setters',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Fresh literal receivers with original vtables. Complete Notify return or stop at original GUI enable/virtual Widget or Button refresh. No substituted callback result. Whole receiver/message/output guards.',
        limits='PC100k/2s blocks;PS2 R4000 scalar2000/100ms;outer30s')
    cases=[dict(kind=k,button=b,value=v) for k in ('enabled','visible') for b in (False,True) for v in (0,1,255,0x12345680,0xa5)]
    cases += [dict(kind='notify',code=c,value=v) for c,v in ((0,1),(0x17,1),(0x18,1),(0x19,1),(0x1c,0),(0x1c,255),(0x1d,1))]
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for case in cases:
            report['pending']=case;save();pair={};kind=case['kind'];button=case.get('button',False)
            for platform in ('pc','ps2'):
                if platform=='pc':
                    with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
                    p=f.p;obj=p.allocate(0x60);msg=p.allocate(32);out=p.allocate(8);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
                    vt=0x6deac0 if button else 0x6dea6c
                else:
                    p=Ps2ScalarPrefix([(0x1655d0,0x4c),(0x16bca0,0x34)]);p.map(0x21000000,4096);p.map(0x22000000,4096)
                    obj,msg,out=0x21000000,0x21000200,0x21000300;read,write,put=p.read,p.write,p.put_uint;vt=0x48d7d0 if button else 0x48d810
                    raw,sections=pristine();p.map(vt,56);write(vt,read_window('ps2',raw,vt,56,sections)[0]);p.reg('SP',0x22000800);p.reg('A0',obj)
                write(obj,b'\xa5'*0x60);put(obj,vt);write(msg,b'\x5a'*32);write(out,b'\x69'*8)
                if kind=='notify':write(obj+0x28,bytes([case['value']]));put(msg,case['code']);put(msg+0x18,out)
                before=read(obj,0x60);mbefore=read(msg,32);obefore=read(out,8);expected=bytearray(before);oexpected=bytearray(obefore)
                if kind=='notify':
                    entry=0x4293b0 if platform=='pc' else 0x1655d0;target=(0x4290b0 if platform=='pc' else 0x165240) if case['code']==0x1c else None;argument=case['value'] if target else None
                    if case['code']==0x18:oexpected[:4]=obj.to_bytes(4,'little')
                    arg=msg
                else:
                    entry=({'enabled':0x4358a0,'visible':0x4358b0} if platform=='pc' else {'enabled':0x16bcc0,'visible':0x16bca0})[kind]
                    target=(0x435cb0 if button else 0x4358c0) if platform=='pc' else (0x168d40 if button else 0x16bbc0)
                    offset=0x28 if kind=='enabled' else 0x3c if platform=='pc' else 0x38;expected[offset]=case['value']&255;arg=case['value'];argument=None
                if platform=='pc':
                    if target:p.run(entry,this=obj,args=(arg,),stop_at=target);assert p.reg('ECX')==obj
                    else:f.call(entry,obj,(arg,))
                    if kind=='notify' and target:assert p.uint(p.reg('ESP')+4)==argument
                    r=dict(blocks=sum(p.visits.values()),completion='actual original consumer entry' if target else 'original return')
                else:
                    p.reg('A1',arg);r=p.run(entry,[target or p.RETURN]);assert not target or p.reg('A0')==obj
                    if kind=='notify' and target:assert p.reg('A1')==argument
                assert read(obj,0x60)==bytes(expected) and read(msg,32)==mbefore and read(out,8)==bytes(oexpected),'whole object/message/output guard'
                r.update(target=f'{target:08X}' if target else None,enabledArgument=argument,returnsSelf=kind=='notify' and case['code']==0x18);pair[platform]=r
            assert pair['pc']['enabledArgument']==pair['ps2']['enabledArgument'] and pair['pc']['returnsSelf']==pair['ps2']['returnsSelf']
            report['cases'].append(dict(input=case,**pair));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(cases),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
