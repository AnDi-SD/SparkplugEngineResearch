"""Original gamepad normalization through original virtual range/value methods."""
import json,math,struct,sys,time,traceback
from pathlib import Path
from probe_gamepad_cached_state import FOLDER,TABLES,PC_AXIS,PS_AXIS,PS_BUTTON,sha
from capture_native_ranges import EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER

def execute(c,k):
    pc=k=='pc';primary,interface=TABLES[k];code=c['code'];current=c['current'];active=c['active'];calls=[]
    if pc:
        p=PcBlocks();base=p.allocate(4096);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu
    else:
        p=Ps2ScalarPrefix([(0x1ef7f0,0x4c0),(0x1f0c30,0x70)],stack_window=(0x22000000,4096),upper64=UPPER)
        base=0x21000000;p.map(base,4096);p.map(0x22000000,4096);p.reg('SP',0x22000800);p.reg('F20',0x42fa0000)
        read,write,u=p.read,p.write,p.u;raw,sections=pristine()
        for a,n in [(0x491460,0x94),(0x477af0,0x70)]:p.map(a,n);write(a,read_window('ps2',raw,a,n,sections)[0])
    write(base,b'\xa5'*4096);p.put_uint(base,primary);p.put_uint(base+0x14,interface);write(base+0x44,bytes([active,active]))
    axes=PC_AXIS if pc else PS_AXIS;buttons={i:0xb0+i-127 for i in range(127,143)} if pc else PS_BUTTON
    assert code in axes or code in (127,138)
    if code in axes:p.put_uint(base+axes[code],current)
    else:write(base+buttons[code],bytes([current]))
    if not pc:write(base+0xb4,struct.pack('<2f',c['axisFraction'],c['buttonFraction']))
    entry=p.uint(interface+(0 if pc else 8)+24);before=read(base,4096)
    targets=(0x4cbe60,0x4cba80) if pc else (0x1ef8e0,0x1efbd0)
    def observe(machine,address,size,user):
        if address in targets:calls.append(dict(entry=f'{address:08X}',receiver=p.reg('ECX' if pc else 'A0'),code=p.uint(p.reg('ESP')+4) if pc else p.reg('A1')))
    hook=u.hook_add(p.uc.UC_HOOK_CODE if pc else 4,observe)
    if pc:
        p.run(entry,this=base+0x14,args=(code,));top=(p.reg('FPSW')>>11)&7;mantissa,exponent=p.mu.reg_read(getattr(p.xr,'UC_X86_REG_FP'+str(top)))
        value=math.ldexp(mantissa,(exponent&0x7fff)-16383-63) if mantissa else 0.;value=-value if exponent&0x8000 else value
        result=dict(entry=f'{entry:08X}',completion='original return',blocks=sum(p.visits.values()),x87Mantissa=f'{mantissa:016X}',x87Exponent=f'{exponent:04X}')
    else:
        p.reg('A0',base+0x14);p.reg('A1',code);saved={r:p.reg(r) for r in ('S0','S1','SP','RA','F20')};original=[read(a,n) for a,n in p.ranges]
        result=p.run(entry,[p.RETURN],count=6000,timeout_us=500000);bits=p.reg('F0')&0xffffffff;value=struct.unpack('<f',struct.pack('<I',bits))[0];result['floatBits']=f'{bits:08X}'
        assert original==[read(a,n) for a,n in p.ranges] and saved=={r:p.reg(r) for r in saved} and result['initialUpper64']==result['finalUpper64']
    u.hook_del(hook)
    if pc and not active:expected=0.;expected_calls=[]
    elif pc and code in buttons:expected=float(bool(current));expected_calls=[targets[1]]
    else:
        signed=-current if code in (112,114) else current
        if pc:
            adjusted=0 if abs(signed)<=200 else signed-200 if signed>0 else signed+200;denominator=800
        else:
            maximum=127 if code in axes else 255;deadzone=maximum*c['axisFraction' if code in axes else 'buttonFraction']
            adjusted=signed-deadzone if signed>0 else signed+deadzone if signed<0 else 0;denominator=maximum-deadzone
        expected=max(-1.,min(1.,adjusted/denominator));expected_calls=list(targets)
        if not pc:expected=struct.unpack('<f',struct.pack('<f',expected))[0]
    assert value==expected,(k,code,current,value,expected)
    assert [int(x['entry'],16) for x in calls]==expected_calls and all(x['receiver']==base+(0x14 if pc else 0) and x['code']==code for x in calls)
    after=read(base,4096);assert after==before
    result.update(value=value,expectedValue=expected,virtualCalls=calls,guardedBytes=4096,beforeSha256=sha(before),afterSha256=sha(after),scope='Original scalar normalization and actual range/value calls;PC x87 default harness precision,PS2 selected finite FPU model samples.No general EE/hardware rounding equivalence.No device polling or callback seams.')
    return result

def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'normalized-cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];start=time.perf_counter();report=dict(kind='original-gamepad-normalization',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[])
    for c in cases:
        for k in c['platforms']:
            row=dict(input=c,platform=k);report['cases'].append(row)
            try:row.update(status='passed',**execute(c,k))
            except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
            report['seconds']=time.perf_counter()-start;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
            if row['status']!='passed':break
        if row['status']!='passed':break
    report['status']='passed' if len(report['cases'])==sum(len(c['platforms']) for c in cases) and all(c['status']=='passed' for c in report['cases']) else 'blocked'
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(status=report['status'],cases=len(report['cases']),seconds=report['seconds'],errors=[c['error'] for c in report['cases'] if 'error' in c])))
    return int(report['status']!='passed')

if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
