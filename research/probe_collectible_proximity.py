#!/usr/bin/env python3
"""Actual gem predicate/height-qualified squared distance, explicit PS2 slices."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_animation_lifecycle import x87_value
from ps2_scalar_prefix import Ps2ScalarPrefix

CASES=ROOT/'local-data/results/native-cycle-20260911-0730/collectible-proximity/cases.json'


def bits(v):return struct.unpack('<I',struct.pack('<f',v))[0]
def sha(v):return hashlib.sha256(v).hexdigest().upper()
def vector(c):
    delta=[b-a for a,b in zip(c['first'],c['second'])]
    if c.get('force',0) or abs(delta[1])<c.get('tolerance',150):delta[1]=0
    return delta


def execute(c,k,phase):
    pc=k=='pc';d=0 if pc else 12
    if pc:
        p=PcBlocks();base=p.allocate(0x8000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
    else:
        p=Ps2ScalarPrefix([(0x3a2e54,0xa4),(0x293210,0xd0),(0x423b58,0x28),(0x109af0,0x14)])
        base=0x21000000;p.map(base,0x8000);p.map(0x22000000,4096);p.reg('SP',0x22000800)
        read,write,put=p.read,p.write,p.put_uint
    obj,record,other_record,node,other_node,point,timer,head=[base+i*0x1000 for i in range(8)]
    write(base,b'\xa5'*0x8000);write(node+(0x74 if pc else 0x70),struct.pack('<3f',*c['second']));write(other_node+(0x74 if pc else 0x70),struct.pack('<3f',*c['first']))
    delta=vector(c);square=sum(v*v for v in delta);expected_return=None;helper_used=False
    if c['kind']=='gem':
        put(obj,0x6fcf20 if pc else 0x4976a0);put(obj+0x24,record);put(record+0x24,node);put(other_record+0x24,other_node);put(obj+0x144+d,other_record)
        write(obj+0x149+d,bytes([c['disabled']]));write(obj+0x148+d,bytes([c['override']]));put(obj+0x140+d,bits(c['radius']))
        flags=0xffffff00|(2 if c['enabled'] else 0)|(8 if c['pause'] else 0)
        helper_used=not c['disabled'] and not c['override'] and bool(flags&2)
        expected_return=0 if c['disabled'] else int(bool(c['override']) or helper_used and square<=c['radius']**2 or not(flags&0x1a))
        if pc:
            write(obj+0x28,b'\1');write(obj+0x38,bytes([c['enabled']]));write(obj+0x39,b'\1');put(obj+0x3c,0x7f7fffff)
            put(obj+0x30,head);put(head,head);put(head+4,head);put(0x755298,timer);write(timer+0x40,bytes([c['pause']]))
        elif not c['disabled']:
            # The original virtual10 is outside this PS2 caller component.
            # Cached-empty flags follow the separately qualified wxEntity
            # contract; this is an input, not a substituted successful call.
            put(record+0xc,flags)
    before=read(base,0x8000);wanted=bytearray(before)
    if pc:
        if c['kind']=='helper':
            p.run(0x597660,args=(other_node,node,c['force'],bits(c['tolerance'])),callee_pop=False)
            value=x87_value(p);assert value==square;expected_return=value
        else:
            p.run(0x544c90,this=obj,args=(point,));value=p.reg('EAX')&255;assert value==expected_return
            if not c['disabled']:wanted[0x100c:0x1010]=struct.pack('<I',flags)
        result=dict(entry='00597660' if c['kind']=='helper' else '00544C90',completion='whole original method return',returnValue=value,blocks=sum(p.visits.values()))
    else:
        if c['kind']=='helper':
            p.reg('A0',other_node);p.reg('A1',node);p.reg('A2',c['force']);p.reg('F12',bits(c['tolerance']));entry=0x293210;stop=0x293280
        elif c['disabled']:
            p.reg('A0',obj);p.reg('A1',point);entry=0x3a2e54;stop=0x3a2ef4
        elif phase=='comparison':
            p.reg('S1',obj);p.reg('S0',flags);p.reg('F0',bits(square));p.reg('F20',bits(c['radius']));entry=0x3a2eb4;stop=0x3a2ef4
        else:
            p.reg('S1',obj);entry=0x3a2e78;stop=0x293280 if helper_used else 0x3a2ef4
        result=p.run(entry,[stop],count=5000,timeout_us=500000)
        if stop==0x293280:
            observed=list(struct.unpack('<3f',read(p.reg('SP')+0x30,12)));assert observed==delta,(observed,delta)
            result.update(completion='original height gate and vector before R5900 square accumulator',observedDelta=observed)
        else:
            assert p.reg('V0')==expected_return;result.update(completion='independent gem comparison tail with explicit square' if phase=='comparison' else 'gem own component before restore',returnValue=p.reg('V0'))
    after=read(base,0x8000);assert after==wanted,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,wanted)) if a!=b][:20]
    result.update(heightFilteredDelta=delta,squaredDistanceInput=square,expectedGemReturn=expected_return if c['kind']=='gem' else None,helperUsed=helper_used,
        beforeSha256=sha(before),afterSha256=sha(after),guardedBytes=0x8000,changedOffsets=[i for i,(a,b) in enumerate(zip(before,after)) if a!=b],
        context='borrowed records with real PC gem vtable/virtual10,empty cached list and original timer getter. PS2 own component receives declared cached flags;actual fabs/vector code executes;accumulator and full v10 caller excluded.')
    return result


def guest(output,selection='pilot'):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Fresh local output')
    if selection not in ('pilot','batch'):raise ValueError('Explicit selection')
    cases=[c for c in json.loads(CASES.read_text()) if c['pilot']==(selection=='pilot')]
    report=dict(kind='original-collectible-proximity',status='running',selection=selection,inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(CASES.read_bytes()),cases=[],
        scope='PC full node helper and gem v11 with actual virtual10. PS2 explicit post-v10/disabled components and height-filter vector helper to accumulator;independent final compare input. No PS2 full squared-distance or gem transaction.')
    started=time.perf_counter()
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for c in cases:
        plans=[('pc','full'),('ps2','own')]
        if c['kind']=='gem' and not c['disabled'] and not c['override'] and c['enabled']:plans.append(('ps2','comparison'))
        for k,phase in plans:
            row=dict(input=c,platform=k,phase=phase);report['cases'].append(row);save()
            try:row.update(status='passed',**execute(c,k,phase))
            except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
            save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='partial' if failed else 'passed';save()
    print(json.dumps(dict(status=report['status'],inputCases=len(cases),executions=len(report['cases']),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
