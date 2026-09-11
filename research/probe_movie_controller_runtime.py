"""Original movie controller -> concrete video interface, plus material query."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER
FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/movie-controller-runtime'
def sha(b):return hashlib.sha256(b).hexdigest().upper()

def execute(c,k):
    pc=k=='pc';kind=c['kind']
    if pc:
        p=PcBlocks();base=p.allocate(4096);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu;uc=p.uc
    else:
        p=Ps2ScalarPrefix([(0x11b1e0,0x58),(0x20d6c0,8),(0x1720c0,12)],stack_window=(0x22000000,4096),upper64=UPPER)
        base=0x21000000;p.map(base,4096);p.map(0x22000000,4096);p.reg('SP',0x22000800);read,write=p.read,p.write;u=p.u
        import unicorn as uc
        raw,sections=pristine()
        for a,n in [(0x48cc90,0x28),(0x491e00,0x60),(0x48ea20,0x34)]:p.map(a,n);write(a,read_window('ps2',raw,a,n,sections)[0])
    put=p.put_uint;write(base,bytes([c['fill']])*4096);material=base+0x100;backend=base+0x300;resource=base+0x400
    put(base,0x6ecb68 if pc else 0x48cc90);put(base+0x1c,material);write(base+0x10,bytes([c['enabled']]))
    put(material,0x6ea94c if pc else 0x48ea20);put(material+(0x70 if pc else 0x88),backend);put(material+(0x68 if pc else 0x80),resource)
    put(backend,0x6f2ac4 if pc else 0x491e00);put(backend+4,0x6f2aa8 if pc else 0x491e30);write(backend+0x18,bytes([c['state']]))
    put(resource+0x18,c['value']);calls=[]
    for a,name in ([(0x4c7320,'video-v0'),(0x48eaa0,'video-v1')] if pc else [(0x20d6c0,'video-v0')]):
        def observe(u,address,size,user,name=name):calls.append(name)
        u.hook_add(uc.UC_HOOK_CODE,observe,begin=a,end=a)
    if kind=='controller':entry=0x492ef0 if pc else 0x11b1e0;this=base;args=(c['deltaBits'],)
    elif kind=='backend':entry=0x4c7320 if pc else 0x20d6c0;this=backend;args=()
    else:assert kind=='query';entry=0x4784d0 if pc else 0x1720c0;this=material;args=()
    before=read(base,4096)
    if pc:
        p.run(entry,this=this,args=args);result=dict(entry=f'{entry:08X}',completion='original return',blocks=sum(p.visits.values()));value=p.reg('EAX')&0xffffffff
    else:
        p.reg('A0',this)
        if kind=='controller':p.reg('F12',c['deltaBits'])
        original=[read(a,n) for a,n in p.ranges];saved={r:p.reg(r) for r in ('S0','S1','SP','RA')};result=p.run(entry,[p.RETURN],count=2000,timeout_us=500000);value=p.reg('V0')&0xffffffff
        assert saved=={r:p.reg(r) for r in saved} and result['initialUpper64']==result['finalUpper64'];assert original==[read(a,n) for a,n in p.ranges]
    wanted=(['video-v0','video-v1'] if pc else ['video-v0']) if kind=='backend' or kind=='controller' and not c['state'] else []
    assert calls==wanted,(calls,wanted)
    if kind=='backend':assert (value&255 if pc else value)==1
    elif kind=='query':assert value==c['value'],(value,c['value'])
    after=read(base,4096);assert after==before
    result.update(originalCallbacks=calls,returnValue=(value&255 if pc else value) if kind=='backend' else value if kind=='query' else None,guardedBytes=4096,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(before),scope='Literal original linked object fields and concrete video tables;all callbacks execute original bytes.No allocation,decoder,rendering or playback claim.PC backend AL result only;PS2 integer result.Observed backend-state byte never changes.')
    return result

def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];start=time.perf_counter();report=dict(kind='original-movie-controller-runtime',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[])
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
