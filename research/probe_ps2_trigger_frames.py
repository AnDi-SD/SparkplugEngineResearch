"""Whole original PS2 trigger/HUD frames compared with sealed PC effects."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from ps2_scalar_prefix import Ps2ScalarPrefix
from probe_ps2_stack_spills import UPPER

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/ps2-stack-spills'

def sha(v):return hashlib.sha256(v).hexdigest().upper()
def bits(v):return struct.unpack('<I',struct.pack('<f',v))[0]


def execute(item):
    c=item['input'];pc=item['pcResult'];kind=c['kind'];cls=c['className']
    p=Ps2ScalarPrefix([(0x3ac730,0x100),(0x3b3c10,0xc0),(0x3b5a00,0xb0),(0x379470,0x10),(0x3794d0,0x140),(0x37e790,0x19c)],stack_window=(0x22000000,4096),upper64=UPPER)
    base=0x21000000;obj,profile,hud,window,node,modal,widget,flow,player=[base+n for n in (0,0x1000,0x4000,0x5000,0x6000,0x7000,0x8000,0x9000,0x9400)]
    p.map(base,0xa000);p.map(0x49f000,4096);p.map(0x22000000,4096)
    p.write(base,b'\xa5'*0xa000);p.write(0x22000000,b'\x5a'*4096)
    put=p.put_uint;write=p.write
    put(obj,int(c['vtable'],16));put(0x49fc7c,profile);put(profile+0x2b4,player)
    put(0x49fda4,hud);put(hud+0x60,window);put(hud+0x6c,modal if c['modalPresent'] else 0);put(hud+0x294,widget if c['cachedWidget'] else 0)
    put(0x49fd4c,flow);put(flow+0x1b0,c['gameState']);put(window+0x18,node);put(window+0x144,c['windowType']);put(window+0x148,bits(c['height']))
    write(window+0x14c,bytes([c['hudVisible']]));write(modal+0x14c,bytes([c['modalVisible']]))
    put(window+0x130,0);put(window+0x134,0);put(node+0xb4,c['dirty'])
    put(obj+0x13c,c['tag']);write(obj+0x131,bytes([c['single'],c['active'],c['fired']]))
    write(obj+0x144,bytes([c['suppressed']]));write(obj+0x150,bytes([c['requested']]));write(obj+0x1a0,bytes([c['bypass']]));put(profile+0x504,c['profile504'])
    before=p.read(base,0xa000);expected=bytearray(before)
    # Replay the independent PC result through confirmed ABI field mappings.
    # No game callback or computed replacement result is supplied to PS2.
    offsets=[(0x126,0x132,1),(0x138,0x144,1),(0x144,0x150,1),(0x5144,0x514c,1),(0x60b0,0x60b4,4)]
    mapped=[]
    for effect in pc['effects']:
        source=int(effect['offset'],16);dest=source
        for a,b,n in offsets:
            if a<=source<a+n:dest=b+source-a;break
        assert expected[dest]==effect['before'],(source,dest,expected[dest],effect)
        expected[dest]=effect['after'];mapped.append(dict(pcOffset=f'{source:X}',ps2Offset=f'{dest:X}',before=effect['before'],after=effect['after']))
    boundary=pc['boundary'];stop={None:p.RETURN,'window-message':0x37e818,'cached-widget-operation':0x379584,'uncached-widget-lookup':0x3795ac}[boundary]
    if kind=='window':entry=0x37e790;arg=window
    elif kind=='enter':entry={'wxGenericTrigger':0x3ac780,'wxPivotingDoor':0x3b3c60,'wxPushButton':0x3b5a50}[cls];arg=obj
    else:entry={'wxGenericTrigger':0x3ac730,'wxPivotingDoor':0x3b3c10,'wxPushButton':0x3b5a00}[cls];arg=obj
    p.reg('A0',arg);p.reg('A1',c['visible'] if kind=='window' else 0)
    p.reg('GP',0x4a4170);p.reg('SP',0x22000800);p.reg('S0',0x0123456789abcdef);p.reg('S1',0xfedcba9876543210)
    original=[p.read(a,n) for a,n in p.ranges]
    try:result=p.run(entry,[stop],timeout_us=500000)
    except Exception as e:raise RuntimeError(f'{e};trace tail '+','.join(f'{a:08X}' for a in p.trace[-12:])) from e
    assert original==[p.read(a,n) for a,n in p.ranges]
    after=p.read(base,0xa000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:12]
    assert p.stack_extension.upper64==list(UPPER)
    if stop==p.RETURN:
        assert [p.reg(n) for n in ('SP','RA','S0','S1')]==[0x22000800,p.RETURN,0x0123456789abcdef,0xfedcba9876543210]
    if boundary=='window-message':
        assert p.reg('A0')==player
        assert [p.uint(p.reg('SP')+0x30+i*4) for i in range(8)]==[0x27d1,0,0,0,window,0,6,0]
    if boundary=='cached-widget-operation':assert p.reg('A0')==widget and p.reg('A1')==0
    if boundary=='uncached-widget-lookup':assert [p.reg(n) for n in ('A0','A1','A2')]==[node,0x4a4170-0x5168,1]
    result.update(boundary=boundary,pcEffectMapping=mapped,guardedBytes=0xa000,codeUnchanged=True,
        beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),
        callerRegistersRestored=stop==p.RETURN,scope='Original nested PS2 frames/HUD/window run from method entry;unknown widget/cache/message bodies remain pre-instruction boundaries. Explicit borrowed records and upper64 GPR values;empty widget list only.')
    return result


def guest(output,selection):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection required')
    inputs=FOLDER/'whole-trigger-cases.json';cases=[c for c in json.loads(inputs.read_text()) if c['pilot']==(selection=='pilot')]
    report=dict(kind='original-ps2-whole-trigger-frames',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(inputs.read_bytes()),cases=[]);start=time.perf_counter()
    for item in cases:
        row=dict(input=item['input'],pcSource=item['pcSource']);report['cases'].append(row)
        try:row.update(status='passed',**execute(item))
        except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
        report['seconds']=time.perf_counter()-start;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        if row['status']!='passed':break
    report['status']='passed' if len(report['cases'])==len(cases) and all(c['status']=='passed' for c in report['cases']) else 'blocked'
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(status=report['status'],cases=len(report['cases']),seconds=report['seconds'],errors=[c['error'] for c in report['cases'] if 'error' in c])))
    return int(report['status']!='passed')


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
