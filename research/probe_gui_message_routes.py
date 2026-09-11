"""Original GUI Notify routing, state writes and explicit actual-call boundaries."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/gui-messages'
CATALOG={c['className']:c for c in json.loads((ROOT/'research/gui-object-contracts-2026-09-10.json').read_text())['classes']}
def sha(b):return hashlib.sha256(b).hexdigest().upper()


def execute(c,k):
    pc=k=='pc';cls=c['className'];info=CATALOG[cls][k];code=c['code'];enabled=bool(c['enabled']);owner_events=[]
    if pc:
        p=PcBlocks();base=p.allocate(0x8000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu
    else:
        p=Ps2ScalarPrefix([(0x16be70,0x274),(0x168e50,0x1cc),(0x373820,0x208),(0x169b80,0x72c),(0x1655d0,0x4c),(0x169370,0xbc)],stack_window=(0x22000000,4096),upper64=UPPER)
        base=0x21000000;p.map(base,0x8000);p.map(0x22000000,4096);p.map(0x49f000,4096);read,write,u=p.read,p.write,p.u
        raw,sections=pristine()
        for a,n in [(int(info['vtable'],16),56),(0x48e560,52),(0x4902f0,64),(0x4772c0,60)]:p.map(a,n);write(a,read_window('ps2',raw,a,n,sections)[0])
        write(0x22000000,bytes([c['stackByte']])*4096);p.reg('SP',0x22000800);p.reg('GP',0x4a4170)
        for i in range(16,19):p.reg(str(i),0x123456700+i)
    obj,msg,event,owner,node,hud,profile,manager,player,out,buf=[base+n for n in (0,0x1000,0x2000,0x3000,0x4000,0x5000,0x6000,0x7000,0x7400,0x7800,0x7c00)]
    write(base,b'\xa5'*0x8000);put=p.put_uint;shift=0 if pc else -4;vt=int(info['vtable'],16);put(obj,vt)
    put(obj+0x18,node if c['rootPresent'] else 0);put(obj+0x24,owner);write(obj+0x28,bytes([c['enabled']]))
    write(obj+0x3c+shift,b'\x01');write(obj+0x3d+shift,bytes([c['highlight']]))
    for off in (0x40,0x44,0x48,0x4c):put(obj+off+shift,0)
    put(obj+0x50+shift,7)
    if cls in ('spButton','wxButton'):
        put(obj+0x54+shift,buf);write(obj+0x58+shift,bytes([c['pressed']]));put(obj+0x5c+shift,0)
    if cls=='spEditBox':
        for off in (0x54,0x58,0x5c,0x60,0x64):put(obj+off+shift,0)
        put(obj+0x68+shift,buf);put(obj+0x6c+shift,32);put(obj+0x70+shift,c['length'])
    text=b'abcdef'[:c['length']] if cls=='spEditBox' else b'action'
    write(buf,text+b'\0'+b'\x69'*(31-len(text)));put(event+0x18,c['eventReject']);put(owner,0x6dcc70 if pc else 0x48e560)
    put(node,0x6dc4f4 if pc else 0x4902f0);put(node+(0xb0 if pc else 0xb4),0x200 if c['nodeEnabled'] else 0)
    put(0x755284 if pc else 0x49fda4,hud);write(hud+0x14,b'\xff')
    put(0x765ad4 if pc else 0x49fc7c,profile);put(profile+0x2b4,player)
    put(0x75db8c if pc else 0x49f8a4,manager);put(manager+0x4c,obj if c['focused'] else player)
    payload=out if code==0x18 else event if cls=='spButton' and code==0xf else c['payload']
    put(msg,code);put(msg+0x18,payload)
    before=read(base,0x8000);expected=bytearray(before);boundary=None;expected_owner=[]
    def byte(a,v):expected[a-base]=v
    def word(a,v):struct.pack_into('<I',expected,a-base,v&0xffffffff)
    def widget():
        nonlocal boundary
        if code==0x1c:boundary='lookup-normal' if c['rootPresent'] else 'base-enable'
        elif code in (0x11,0x12) and enabled:byte(obj+0x3d+shift,int(code==0x11));boundary='refresh'
        elif code in (0xe,0xf,0x10) and enabled:expected_owner.append((code,payload,None))
        elif code==0x18:word(out,obj)
    def button():
        nonlocal boundary
        if code==0x1c and c['rootPresent']:boundary='lookup-pressed'
        elif code==0xf and enabled and c['eventReject']==0:
            expected_owner.append((0x19,buf,0));byte(obj+0x58+shift,1);boundary='refresh'
        elif code in (0x10,0x12) and enabled and c['pressed']:byte(obj+0x58+shift,0);boundary='refresh'
        else:widget()
    if cls=='wxButton':
        if code in (0xe,0xf,0x10):pass
        elif code==0x11:
            if enabled and c['nodeEnabled']:boundary='game-message'
        elif code==0x12:byte(hud+0x14,0)
        elif code==0x1c:
            if payload==0:word(msg+0x1c,(msg&0xffffff00|1) if pc else (c['stackByte']*0x01010100|1))
            elif payload==1:boundary='broadcast'
        else:button()
    elif cls=='spButton':button()
    elif cls=='spEditBox':
        if code==0x1c:
            if c['rootPresent']:boundary='lookup-normal'
        elif code in (0xe,0x10,0x11,0x12):
            if enabled:expected_owner.append((code,payload,None))
        elif code==0xf:
            if enabled:boundary='focus-self'
        elif code==0x13:
            if enabled:boundary='append-character'
        elif code==0x14:
            if enabled:
                if payload==0xd and c['length']:
                    word(obj+0x70+shift,c['length']-1);byte(buf+c['length']-1,0)
                elif payload in (0x1b,0x54) and c['focused']:boundary='focus-null'
        elif code in (0x16,0x17):
            if enabled:byte(obj+0x3d+shift,int(code==0x16));boundary='refresh'
        else:widget()
    else:widget()
    targets={'refresh':int(info['slots'][11],16),'base-enable':0x4290b0 if pc else 0x165240,
        'lookup-normal':0x421330 if pc else 0x1a7160,'lookup-pressed':0x421330 if pc else 0x1a7160,
        'focus-self':0x451ad0 if pc else 0x163fd0,'focus-null':0x451ad0 if pc else 0x163fd0,
        'append-character':0x436c60 if pc else 0x169920,'broadcast':0x40ec00 if pc else 0x1007a0,
        'game-message':0x435d30 if pc else 0x373948}
    stop=targets.get(boundary);owner_entry=0x4293b0 if pc else 0x1655d0
    def observe(machine,address,size,user):
        if address==owner_entry and p.reg('ECX' if pc else 'A0')==owner:
            a=p.uint(p.reg('ESP')+4) if pc else p.reg('A1');owner_events.append(dict(code=p.uint(a),payload=p.uint(a+0x18),tail=p.uint(a+0x1c),message=f'{a:08X}'))
    hook=u.hook_add((p.uc if pc else __import__('unicorn')).UC_HOOK_CODE,observe,begin=owner_entry,end=owner_entry)
    entry=int(info['slots'][1],16)
    if pc:
        p.run(entry,this=obj,args=(msg,),stop_at=stop);result=dict(entry=f'{entry:08X}',stop=f'{p.reg("EIP"):08X}',blocks=sum(p.visits.values()),completion='original return' if stop is None else 'declared original consumer boundary;guest discarded')
    else:
        p.reg('A0',obj);p.reg('A1',msg);original=[p.read(a,n) for a,n in p.ranges];result=p.run(entry,[stop or p.RETURN],timeout_us=500000);assert original==[p.read(a,n) for a,n in p.ranges]
        if stop is None:
            assert p.reg('SP')==0x22000800 and p.reg('RA')==p.RETURN
            assert [p.reg(str(i)) for i in range(16,19)]==[0x123456700+i for i in range(16,19)]
    u.hook_del(hook)
    def argument(i):return p.uint(p.reg('ESP')+4*(i+1)) if pc else p.reg('A'+str(i+1))
    if boundary in ('refresh','base-enable','append-character','broadcast'):assert p.reg('ECX' if pc else 'A0')==obj
    if boundary=='append-character':
        want=payload if pc else (payload&255)|((0xffffffffffffffff<<8)&0xffffffffffffffff if payload&128 else 0)
        assert argument(0)==want
    if boundary in ('focus-self','focus-null'):assert p.reg('ECX' if pc else 'A0')==manager and argument(0)==(obj if boundary=='focus-self' else 0)
    if boundary and boundary.startswith('lookup'):
        assert p.reg('ECX' if pc else 'A0')==node and argument(1)==1 and argument(2)==1
    if boundary=='game-message':
        if pc:assert p.reg('ECX')==obj and [argument(i) for i in range(4)]==[player,0x27d1,16,0]
        else:assert p.reg('A0')==player and [p.uint(p.reg('SP')+0x30+i*4) for i in range(8)]==[0x27d1,0,0,0,obj,0,16,0]
    if boundary=='broadcast':
        if pc:assert [argument(i) for i in range(4)]==[0x2715,3,0,0]
        else:assert [p.reg(n) for n in ('A1','A2','A3','T0')]==[0x2715,3,0,0]
    assert len(owner_events)==len(expected_owner),(owner_events,expected_owner)
    for got,(a,b,tail) in zip(owner_events,expected_owner):assert got['code']==a and got['payload']==b and (tail is None or got['tail']==tail)
    after=read(base,0x8000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:12]
    result.update(boundary=boundary,ownerNotifications=owner_events,guardedBytes=0x8000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),effects=[dict(offset=f'{i:X}',before=a,after=b) for i,(a,b) in enumerate(zip(before,after)) if a!=b],scope='Borrowed GUI receivers with original class vtables and original GUIObject owner Notify. Actual own routing,press message helper and zero-text-node deletion paths. Stops before original refresh/focus/lookup/append/broadcast/game-message consumer. No replacement game returns.')
    return result


def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'message-cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];report=dict(kind='paired-original-gui-message-routes',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[]);start=time.perf_counter()
    for c in cases:
        for k in ('pc','ps2'):
            row=dict(input=c,platform=k);report['cases'].append(row)
            try:row.update(status='passed',**execute(c,k))
            except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
            report['seconds']=time.perf_counter()-start;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
            if row['status']!='passed':break
        if row['status']!='passed':break
    report['status']='passed' if len(report['cases'])==2*len(cases) and all(c['status']=='passed' for c in report['cases']) else 'blocked'
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(status=report['status'],cases=len(report['cases']),seconds=report['seconds'],errors=[c['error'] for c in report['cases'] if 'error' in c])))
    return int(report['status']!='passed')


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
