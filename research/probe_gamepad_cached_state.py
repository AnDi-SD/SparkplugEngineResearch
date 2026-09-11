"""Bounded original gamepad cache queries; fixtures and assertions are research code."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/gamepad-cached-state'
TABLES={'pc':(0x6f30b4,0x6f3098),'ps2':(0x491460,0x491484)}
PC_AXIS={111:0x90,112:0x94,113:0xa0,114:0xa4}
PS_AXIS={111:0x5c,112:0x60,113:0x54,114:0x58}
PS_BUTTON={115:0x7e,116:0x80,117:0x7f,118:0x81,127:0x76,128:0x78,129:0x77,130:0x79,131:0x7a,132:0x7c,133:0x7b,134:0x7d,135:0x75,136:0x74,137:0x82,138:0x83}
def sha(b):return hashlib.sha256(b).hexdigest().upper()

def execute(c,k):
    pc=k=='pc';primary,interface=TABLES[k];code=c['code'];slot=c['slot']
    axes=PC_AXIS if pc else PS_AXIS;buttons={i:0xb0+i-127 for i in range(127,143)} if pc else PS_BUTTON
    if pc:
        p=PcBlocks();base=p.allocate(4096);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu
    else:
        p=Ps2ScalarPrefix([(0x1ef8e0,0x858),(0x1f0c30,0x70)],stack_window=(0x22000000,4096),upper64=UPPER)
        base=0x21000000;p.map(base,4096);p.map(0x22000000,4096);p.reg('SP',0x22000800);read,write,u=p.read,p.write,p.u
        raw,sections=pristine()
        for a,n in [(0x491460,0x94),(0x477af0,0x150)]:p.map(a,n);write(a,read_window('ps2',raw,a,n,sections)[0])
    put=p.put_uint;write(base,bytes([c['fill']])*4096);put(base,primary);put(base+0x14,interface)
    write(base+0x44,bytes([c['active']]));write(base+0x45,bytes([c['active']]))
    current=c['current'];previous=c['previous'];sparse=c.get('sparse',False)
    for key,off in axes.items():
        old=off+(8 if pc else 16);value=current if key==code or not sparse else c['decoy'];past=previous if key==code or not sparse else c['oldDecoy']
        put(base+off,value);put(base+old,past)
    for key,off in buttons.items():
        write(base+off,bytes([(current if key==code or not sparse else c['decoy'])&255]));write(base+off+16,bytes([(previous if key==code or not sparse else c['oldDecoy'])&255]))
    if not pc:write(base+0xb4,struct.pack('<2f',0.5,0.25))
    entry=p.uint(interface+(0 if pc else 8)+slot*4);outputs=[base+0x800+4*i for i in range(3)];nullable=c.get('nullMask',0)
    args=(code,)+tuple(0 if nullable&(1<<i) else a for i,a in enumerate(outputs)) if slot==5 else (code,)
    before=read(base,4096);expected=bytearray(before)
    known=code in axes or code in buttons;enabled=bool(c['active']) if pc else True
    cur=current if code in axes else current&255;old=previous if code in axes else previous&255
    expected_value=0
    if slot==1:expected_value=int(enabled and known and bool(cur))
    elif slot==2:expected_value=int(enabled and known and bool(cur) and not old)
    elif slot==3:
        if enabled and code in axes:
            expected_value=-cur if code in (112,114) else cur
        elif enabled and code in buttons:expected_value=int(bool(cur)) if pc else cur
    elif slot==4:
        if enabled and known and (pc or code not in (117,118)):
            expected_value=(old-cur if pc and code in (112,114) else cur-old)
    elif slot==5:
        supported=code in axes or (code in buttons if pc else code in (115,116,127,128,129,130,131,132,133,134,137,138))
        if supported:
            vals=([-1000,1000,0x43480000] if pc else [-127,127,0x3f000000]) if code in axes else ([0,1,0] if pc else [0,255,0x3e800000])
            for i,(a,v) in enumerate(zip(outputs,vals)):
                if not nullable&(1<<i):struct.pack_into('<I',expected,a-base,v&0xffffffff)
    else:raise ValueError('Reviewed slots1..5 only')
    if pc:
        assert not nullable or not supported
        p.run(entry,this=base+0x14,args=args);value=p.reg('EAX')&(255 if slot in (1,2) else 0xffffffff)
        result=dict(entry=f'{entry:08X}',completion='original return',blocks=sum(p.visits.values()))
    else:
        for reg,value in zip(('A0','A1','A2','A3','T0'),(base+0x14,)+args):p.reg(reg,value)
        original=[read(a,n) for a,n in p.ranges];result=p.run(entry,[p.RETURN],count=6000,timeout_us=500000);value=p.reg('V0')&0xffffffff
        assert original==[read(a,n) for a,n in p.ranges]
        assert p.reg('A0')==base and p.reg('SP')==0x22000800 and p.reg('RA')==p.RETURN and result['initialUpper64']==result['finalUpper64']
    if slot!=5:assert value==expected_value&0xffffffff,(hex(entry),code,slot,value,expected_value)
    after=read(base,4096);assert after==bytes(expected),[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:12]
    result.update(value=value if slot!=5 else None,returnScope='No return-value claim for output-parameter method' if slot==5 else 'low8 Boolean' if slot in (1,2) else 'low32 integer',guardedBytes=4096,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),outputs=[p.uint(a) for a in outputs] if slot==5 else None,
        scope='Literal original tables and cache layout;whole scalar query calls.No device polling,cache producer or normal startup claim.No game callback seams.')
    return result

def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];start=time.perf_counter()
    report=dict(kind='original-gamepad-cached-state',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[])
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
