#!/usr/bin/env python3
"""Paired original action gate, preserving real virtual selection boundaries."""
import hashlib,json,struct,sys,time,traceback
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
    started=time.perf_counter();report=dict(kind='paired-original-ai-behavior-action-gate',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Literal guarded base receiver/action key;pristine base vtables. Gate returns normally or stops at the actual virtual action-selection consumer. No fake consumer return or full lifecycle assertion.')
    cases=[('same-zero',0,0,1,0,1),('same-one',1,1,1,0,1),('disabled-enable',0,1,0,0,1),('disabled-disable',1,0,0,0,1),
        ('no-current-enable',0,1,1,0,0),('no-current-disable',1,0,1,0,0),('inhibited-enable',0,1,1,1,1),('inhibited-disable',1,0,1,1,1),
        ('enable',0,1,1,0,1),('disable',1,0,1,0,1),('raw-input-low-byte',0,0x123402,1,0,1),('raw-nonzero-current',2,1,1,0,1),('low-byte-equal',2,0x10002,1,0,1)]
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for label,old,value,enabled,inhibited,has_action in cases:
            case=dict(label=label,old=old,input=value,enabled=enabled,inhibited=inhibited,hasAction=has_action);report['pending']=case;save();pair={}
            changed=(value&255)!=old and bool(enabled);calls=changed and has_action and not inhibited
            for platform in ('pc','ps2'):
                if platform=='pc':
                    with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
                    p=f.p;obj=p.allocate(0x1b0);action=p.allocate(0x2c);write=lambda a,b:p.mu.mem_write(a,bytes(b));read=lambda a,n:bytes(p.mu.mem_read(a,n));put=p.put_uint
                    size=0x1b0;active,saved,current,permission,block,action_key=0x138,0x128,0x124,0x14d,0x140,0x28
                    write(obj,b'\xcc'*size);write(action,b'\xdd'*0x2c);put(obj,0x702708)
                else:
                    p=Ps2ScalarPrefix([(0x225620,0x90)]);p.map(0x21000000,4096);p.map(0x22000000,4096);obj,action=0x21000000,0x21000800
                    write,read,put=p.write,p.read,p.put_uint;size=0x1c0;active,saved,current,permission,block,action_key=0x148,0x134,0x130,0x15d,0x150,0x2c
                    write(obj,b'\xcc'*size);write(action,b'\xdd'*0x30);put(obj,0x496780);raw,sections=pristine();p.map(0x496780,108);write(0x496780,read_window('ps2',raw,0x496780,108,sections)[0])
                    p.reg('SP',0x22000800);p.reg('A0',obj);p.reg('A1',value)
                write(obj+active,bytes([old]));write(obj+permission,bytes([enabled]));write(obj+block,bytes([inhibited]));put(obj+current,action if has_action else 0);put(obj+saved,0x87654321);put(action+action_key,0x76543210)
                before=read(obj,size);expected=bytearray(before)
                if changed:expected[active]=value&255
                if calls and value&255:struct.pack_into('<I',expected,saved,0x76543210)
                if platform=='pc':
                    if calls:
                        p.run(0x591620,this=obj,args=(value,),stop_at=0x591c80)
                    else:f.call(0x591620,obj,(value,))
                    r=dict(blocks=sum(p.visits.values()),completion='actual selector entry' if calls else 'original return')
                    consumer=dict(this=p.reg('ECX'),key=p.uint(p.reg('ESP')+4),parameter=p.uint(p.reg('ESP')+8)) if calls else None
                else:
                    r=p.run(0x225620,[p.RETURN,0x225df0]);consumer=dict(this=p.reg('A0'),key=p.reg('A1')&0xffffffff,parameter=p.reg('A2')&0xffffffff) if p.stop==0x225df0 else None
                assert read(obj,size)==bytes(expected),'whole receiver guard'
                assert bool(consumer)==bool(calls),(case,consumer)
                if consumer:assert consumer==dict(this=obj,key=0 if value&255 else 0x87654321,parameter=0)
                r.update(active=read(obj+active,1)[0],saved=p.uint(obj+saved),selectedKey=consumer['key'] if consumer else None,parameter=consumer['parameter'] if consumer else None);pair[platform]=r
            assert all(pair['pc'][k]==pair['ps2'][k] for k in ('active','saved','selectedKey','parameter'))
            report['cases'].append(dict(input=case,**pair));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(cases),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
