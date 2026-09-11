"""Original bank removal and empty bank teardown; allocation seams are explicit."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
import probe_pc_animation_lifecycle as lifetime
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER
FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/audio-bank-ownership'
def sha(b):return hashlib.sha256(b).hexdigest().upper()

def execute(c,k):
    pc=k=='pc';teardown=c['kind']=='teardown';ids=c.get('ids',[]);key=c.get('key',0);banks=[];factory_calls=[];destructors=[]
    if pc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=lifetime.LifetimeFixture()
        p=f.p;base=p.allocate(0x2000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu
    else:
        p=Ps2ScalarPrefix([(0x1232e0,0x118),(0x1ea940,0x94),(0x121500,0x114),(0x111660,0x28),(0x101250,0x60),(0x102b50,0x9c)],stack_window=(0x22000000,4096),upper64=UPPER)
        base=0x21000000;p.map(base,0x2000);p.map(0x22000000,4096);p.reg('SP',0x22000800);p.reg('GP',0x4a4170);read,write,u=p.read,p.write,p.u
        raw,sections=pristine()
        for a,n in [(0x491150,0x30),(0x4911b0,0x80)]:p.map(a,n);write(a,read_window('ps2',raw,a,n,sections)[0])
    put=p.put_uint;write(base,bytes([c['fill']])*0x2000);put(base,0x6f26d8 if pc else 0x4911b0);array=base+0x800
    def observe(machine,address,size,user):
        if address==(0x4c7450 if pc else 0x1eaac0):factory_calls.append(f'{address:08X}')
        if address==(0x4c7510 if pc else 0x1ea940):destructors.append(p.reg('ECX' if pc else 'A0'))
    hook=u.hook_add(p.uc.UC_HOOK_CODE if pc else 4,observe)
    for i,ident in enumerate([7] if teardown else ids):
        if pc:
            p.run(0x4c7450);bank=p.reg('EAX');assert p.uint(bank)==0x6f2b00 and f.allocations[bank]==40
        else:
            bank=base+0x1000+0x80*i;put(bank,0x491150)
            for off in (4,0x18,0x1c,0x20,0x24,0x28):put(bank+off,0)
        put(bank+0x10,ident);banks.append(bank)
    preparation_calls=len(factory_calls);factory_calls.clear()
    if pc:
        put(base+0x84,array);put(base+0x88,array+4*len(banks));put(base+0x8c,array+16)
    else:put(base+0x7c,4);put(base+0x80,len(banks));put(base+0x84,array)
    for i,bank in enumerate(banks):put(array+4*i,bank)
    before=read(base,0x2000);expected=bytearray(before);bank_before=[read(bank,40 if pc else 44) for bank in banks];free_before=list(f.freed) if pc else []
    match=next((i for i,n in enumerate(ids) if n==key),None);stop=None
    if teardown:entry=0x4c7510 if pc else 0x1ea940;receiver=banks[0];args=(0,)
    else:
        entry=0x48f9b0 if pc else 0x1232e0;receiver=base;args=(key,)
        if not pc:stop=0x10d810 if match is not None else 0x1eaac0 if key==1 else None
    if pc:
        p.run(entry,this=receiver,args=args);value=p.reg('EAX');result=dict(entry=f'{entry:08X}',completion='original return',blocks=sum(p.visits.values()))
    else:
        p.reg('A0',receiver);p.reg('A1',args[0]);saved={r:p.reg(r) for r in ('S0','S1','S2','SP','RA')};original=[read(a,n) for a,n in p.ranges]
        result=p.run(entry,[stop or p.RETURN],count=10000,timeout_us=1000000);value=p.reg('V0')&0xffffffff;assert original==[read(a,n) for a,n in p.ranges]
        if not stop:assert saved=={r:p.reg(r) for r in saved} and result['initialUpper64']==result['finalUpper64']
    u.hook_del(hook)
    if teardown:
        assert value==banks[0]
        assert destructors==banks
        if pc:
            assert banks[0] not in f.freed and p.uint(banks[0]+0x1c)==p.uint(banks[0]+0x20)==p.uint(banks[0]+0x24)==0
            assert p.uint(banks[0])==0x6daeb8,hex(p.uint(banks[0]))
            expected_frees=[int.from_bytes(bank_before[0][0x1c:0x20],'little')];assert f.freed[len(free_before):]==expected_frees
        else:struct.pack_into('<I',expected,banks[0]-base,0x48c5e0);assert not factory_calls
    elif pc:
        assert value&255==1;wanted=list(banks)
        if match is not None:
            wanted[match]=wanted[-1];wanted.pop();assert destructors==[banks[match]]
            assert f.freed[len(free_before):]==[int.from_bytes(bank_before[match][0x1c:0x20],'little'),banks[match]]
            struct.pack_into('<I',expected,0x800+4*match,banks[-1])
        else:assert not destructors and f.freed==free_before
        if key==1:
            assert len(factory_calls)==1;new=p.uint(array+len(wanted)*4);assert new not in banks and p.uint(new)==0x6f2b00 and p.uint(new+0x10)==1;wanted.append(new);struct.pack_into('<I',expected,0x800+4*(len(wanted)-1),new)
        else:assert not factory_calls
        struct.pack_into('<I',expected,0x88,array+4*len(wanted));assert [p.uint(array+4*i) for i in range(len(wanted))]==wanted
    else:
        if match is not None:
            assert destructors==[banks[match]] and stop==0x10d810 and p.reg('A0')==banks[match]
            struct.pack_into('<I',expected,banks[match]-base,0x48c5e0)
        elif key==1:assert stop==0x1eaac0 and not destructors
        else:assert value==1 and not destructors
    after=read(base,0x2000);assert after==bytes(expected),[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:15]
    result.update(returnValue=value if not stop else None,banks=[f'{a:08X}' for a in banks],destructorReceivers=[f'{a:08X}' for a in destructors],setupFactoryCalls=preparation_calls,operationFactoryCalls=len(factory_calls),freed=[f'{a:08X}' for a in f.freed[len(free_before):]] if pc else [],guardedBytes=8192,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),scope='PC original bank factories/destructors/removal with existing allocation/free and SEH fixtures;PS2 literal empty-bank state with zero SDK handle and no allocated vector storage.No allocator result substitution on PS2;delete/factory continuation stops explicitly.Non-deleting teardown comparison uses different declared storage ownership on each platform.')
    return result

def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];start=time.perf_counter();report=dict(kind='original-audio-bank-ownership',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[])
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
