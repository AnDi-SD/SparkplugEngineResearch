"""Original registry-based AI entries and actual base-wxEntity notifications."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER
from probe_ai_action_entry_fields import FIELDS

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/ai-registry-entry'
CATALOG={c['className'][2:-8]:c for c in json.loads((ROOT/'research/ai-action-construction-contracts-2026-09-10.json').read_text())['classes']}
def sha(b):return hashlib.sha256(b).hexdigest().upper()


def execute(c,k):
    pc=k=='pc';name=c['name'];delta=0 if pc else 4;entry=int(CATALOG[name][k]['slots'][10],16);calls=[];messages=[]
    if pc:
        p=PcBlocks();p.fixture_seh_chain();base=p.allocate(0x4000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu
    else:
        ranges=[(entry,0x300 if name in ('YetiAttack','KnutAttack') else 0x100),(0x369b60,0x60),(0x100810,8)]
        p=Ps2ScalarPrefix(ranges,stack_window=(0x22000000,4096),upper64=UPPER);base=0x21000000;p.map(base,0x4000);p.map(0x22000000,4096);read,write,u=p.read,p.write,p.u
        p.reg('SP',0x22000800);p.reg('GP',0x4a4170)
        for i in range(16,21):p.reg(str(i),0x56780000+i)
        raw,sections=pristine()
        for a,n in [(0x496780,0x60),(0x49b8d0,0x28)]:p.map(a,n);write(a,read_window('ps2',raw,a,n,sections)[0])
    obj,owner,char,cmd,entity,node,registry,target=[base+x for x in (0,0x1000,0x1400,0x1800,0x2000,0x2400,0x2800,0x2c00)]
    put=p.put_uint;write(base,bytes([c['fill']])*0x4000);put(obj,int(CATALOG[name][k]['vtable'],16));put(obj+0x20+delta,owner)
    put(owner,0x702708 if pc else 0x496780);put(owner+(0x144 if pc else 0x154),char);put(char+(0x130 if pc else 0x13c),cmd if c['commandPresent'] else 0)
    put(owner+0x24,entity if c['entityPresent'] else 0);put(entity,0x6f4eb8 if pc else 0x49b8d0);put(entity+0x24,node)
    position=(-1.5,0.,2.5) if c['fill']==0xa5 else (3.25,-4.,0.);write(node+0x20,struct.pack('<3f',*position))
    glob=0x765ad8 if pc else 0x49fd88
    if not pc:p.map(glob,4)
    put(glob,registry);put(registry+(0x28 if pc else 0x24),target if c['target'] else 0);put(registry+0x18,0)
    selector=name not in FIELDS and not c['target'];boundary='selector' if selector else 'resource-field' if name=='YetiAttack' else 'cached-node-field' if name=='KnutAttack' else None
    stop=(0x591c80 if pc else 0x225df0) if selector else (0x5aff46 if pc else 0x270d5c) if name=='YetiAttack' else (0x5acb26 if pc else 0x25d72c) if name=='KnutAttack' else None if pc else p.RETURN
    before=read(base,0x4000);expected=bytearray(before)
    def ew(a,v):struct.pack_into('<I',expected,a-base,v&0xffffffff)
    def eb(a,v):expected[a-base]=v
    ew(obj+0x3a8+delta,target if c['target'] else 0)
    if name in FIELDS:
        config=FIELDS[name]
        for key,value,width in [('zeroWords',0,4),('oneWords',1,4),('twoWords',2,4),('zeroBytes',0,1),('oneBytes',1,1)]:
            for off in config[key]:(ew if width==4 else eb)(obj+off+delta,value)
        if name=='WinxFly' and c['commandPresent']:ew(cmd+4,0)
    elif name=='GolemAttack':
        for off in (0x3ac,0x3b0):ew(obj+off+delta,0)
        ew(obj+0x3b4+delta,0x481c4000);eb(obj+0x3bc+delta,0)
    elif name=='YetiAttack':
        eb(cmd+0x1d,1)
        for off in (0x3ac,0x3bc,0x3b0,0x3b4,0x3c4,0x408,0x42c,0x47c):ew(obj+off+delta,0)
        for off in (0x3c0,0x3c8,0x424,0x425,0x3dc,0x3dd):eb(obj+off+delta,0)
    elif name=='KnutAttack':
        ew(obj+0x3b0+delta,5)
        for off in (0x3bc,0x3b4,0x3c0,0x3c4,0x3c8,0x3cc,0x3fc,0x400):ew(obj+off+delta,0)
        for off in (0x3b8,0x3ac,0x3e8,0x404):eb(obj+off+delta,0)
        eb(obj+0x3dc+delta,1);expected[0x3d0+delta:0x3dc+delta]=struct.pack('<3f',*position)
    else:raise ValueError(name)
    notify=0x5b7a00 if pc else 0x100810;query=0x4e21d0 if pc else 0x369b60
    def observe(machine,address,size,user):
        if address==entry:
            sp=p.reg('ESP' if pc else 'SP');write(sp-0x200,bytes([c['fill']])*0x200)
        elif address==query:calls.append(p.reg('ECX' if pc else 'A0'))
        elif address==notify:
            receiver=p.reg('ECX' if pc else 'A0');message=p.uint(p.reg('ESP')+4) if pc else p.reg('A1');words=list(struct.unpack('<8I',read(message,32)))
            messages.append(dict(receiver=receiver,words=words,sourceStackWord=f'{words[7]:08X}'))
            assert receiver==entity
            if name=='GolemAttack':assert words==[0x271f,0,0,0,obj,0,0,(c['fill']*0x01010101&0xffffff00)|1]
            elif name=='KnutAttack':
                idx=len(messages)-1;assert idx<2;assert words[:6]==[0x2725,0,0,0,obj,0]
                assert words[6]==[(0x708f48 if pc else 0x45e8e8),(0x6ff4f8 if pc else 0x45e8d8)][idx]
                assert words[7]==(c['fill']*0x01010101&0xffffff00)|(1 if idx==0 else 0)
            else:raise AssertionError('Unexpected base entity Notify')
    hooks=[u.hook_add((p.uc if pc else __import__('unicorn')).UC_HOOK_CODE,observe,begin=a,end=a) for a in (entry,notify,query)]
    if pc:
        p.run(entry,this=obj,args=(c['argument'],),stop_at=stop);result=dict(entry=f'{entry:08X}',stop=None if stop is None else f'{stop:08X}',completion='original return' if boundary is None else 'declared original boundary',blocks=sum(p.visits.values()))
    else:
        p.reg('A0',obj);p.reg('A1',c['argument']);original=[read(a,n) for a,n in p.ranges];result=p.run(entry,[stop],count=6000,timeout_us=500000);assert original==[read(a,n) for a,n in p.ranges]
        if boundary is None:
            assert p.reg('SP')==0x22000800 and p.reg('RA')==p.RETURN
            assert [p.reg(str(i)) for i in range(16,21)]==[0x56780000+i for i in range(16,21)] and result['initialUpper64']==result['finalUpper64']
    for hook in hooks:u.hook_del(hook)
    assert calls==[registry];assert len(messages)==(int(c['entityPresent']) if name=='GolemAttack' else 2 if name=='KnutAttack' else 0)
    consumer=None
    if selector:
        consumer=dict(receiver=p.reg('ECX' if pc else 'A0'),key=p.uint(p.reg('ESP')+4) if pc else p.reg('A1')&0xffffffff,parameter=p.uint(p.reg('ESP')+8) if pc else p.reg('A2')&0xffffffff)
        assert consumer==dict(receiver=owner,key=int(name=='KnutAttack'),parameter=0)
    after=read(base,0x4000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:12]
    result.update(boundary=boundary,consumer=consumer,registryCalls=calls,entityMessages=messages,guardedBytes=0x4000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),preEntryStackFill=c['fill'])
    return result


def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];start=time.perf_counter()
    report=dict(kind='original-ai-registry-entry',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[])
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
