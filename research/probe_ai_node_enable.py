"""Actual recursive Node Enable and Yeti v10 over explicit existing Nodes."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/ai-node-binding'
def sha(b):return hashlib.sha256(b).hexdigest().upper()


def execute(c,k):
    pc=k=='pc';yeti=c['kind']=='yeti';delta=0 if pc else 4;enable=0x421640 if pc else 0x1a5b00;entry=(0x5afea0 if pc else 0x270ce0) if yeti else enable
    if pc:
        p=PcBlocks();p.fixture_seh_chain();base=p.allocate(0x6000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu
    else:
        ranges=[(0x1a5b00,0xd8),(0x1163f0,8)]+([(0x270ce0,0x230),(0x369b60,0x60),(0x100810,8)] if yeti else [])
        p=Ps2ScalarPrefix(ranges,stack_window=(0x22000000,4096),upper64=UPPER);base=0x21000000;p.map(base,0x6000);p.map(0x22000000,4096);read,write,u=p.read,p.write,p.u
        p.reg('SP',0x22000800);p.reg('GP',0x4a4170)
        for i in range(16,21):p.reg(str(i),0x56780000+i)
        raw,sections=pristine()
        for a,n in [(0x4902f0,0x40),(0x49b8d0,0x10)]:p.map(a,n);write(a,read_window('ps2',raw,a,n,sections)[0])
    put=p.put_uint;write(base,bytes([c['fill']])*0x6000)
    obj,owner,char,cmd,entity,registry,target=[base+a for a in (0,0x1000,0x1400,0x1800,0x1c00,0x2000,0x2400)]
    nodes=[base+0x2800+i*0x400 for i in range(4)];children=[[1,2],[3],[],[]] if c['tree'] else [[],[],[],[]]
    flags=[0xffffffff,0,0x12345678,0x70a00]
    for i,node in enumerate(nodes):
        put(node,0x6dc4f4 if pc else 0x4902f0);put(node+(0xb0 if pc else 0xb4),flags[i]);head=node+0x200 if pc else node+0x18
        if pc:put(node+0x18,head)
        links=[node+0x240+j*0x10 for j in range(len(children[i]))];chain=[head]+links;forward,backward=(0,4) if pc else (4,0)
        for j,link in enumerate(chain):put(link+forward,chain[(j+1)%len(chain)]);put(link+backward,chain[(j-1)%len(chain)])
        for link,child in zip(links,children[i]):put(link+8,nodes[child]);put(nodes[child]+0x2c,node)
    if yeti:
        put(obj,0x709418 if pc else 0x492f60);put(obj+0x20+delta,owner);put(owner+(0x144 if pc else 0x154),char);put(char+(0x130 if pc else 0x13c),cmd)
        put(owner+0x24,entity);put(entity,0x6f4eb8 if pc else 0x49b8d0);put(obj+0x3e0+delta,nodes[0])
        glob=0x765ad8 if pc else 0x49fd88
        if not pc:p.map(glob,4)
        put(glob,registry);put(registry+(0x28 if pc else 0x24),target);put(registry+0x18,0)
    before=read(base,0x6000);expected=bytearray(before)
    def ew(a,v):struct.pack_into('<I',expected,a-base,v&0xffffffff)
    def eb(a,v):expected[a-base]=v
    if yeti:
        eb(cmd+0x1d,1);ew(obj+0x3a8+delta,target)
        for off in (0x3ac,0x3bc,0x3b0,0x3b4,0x3c4,0x408,0x42c,0x47c,0x3e4):ew(obj+off+delta,0)
        for off in (0x3c0,0x3c8,0x424,0x425,0x3dc,0x3dd,0x3d4):eb(obj+off+delta,0)
        for off in (0x3b8,0x478):eb(obj+off+delta,1)
    state=0 if yeti else c['enabled'];recursive=1 if yeti else c['recursive'];truth=lambda v:bool(v&255) if pc else bool(v)
    touched=[]
    def visit(i):
        touched.append(i);ew(nodes[i]+(0xb0 if pc else 0xb4),(flags[i]|0x200) if truth(state) else (flags[i]&~0x200))
        if truth(recursive):
            for j in children[i]:visit(j)
    visit(0);calls=[];messages=[];query=[];stack_initialized=False
    def observe(machine,address,size,user):
        nonlocal stack_initialized
        if address==entry and not stack_initialized:
            sp=p.reg('ESP' if pc else 'SP');write(sp-0x200,bytes([c['fill']])*0x200);stack_initialized=True
        if address==enable:
            receiver=p.reg('ECX' if pc else 'A0');arg1=p.uint(p.reg('ESP')+4) if pc else p.reg('A1');arg2=p.uint(p.reg('ESP')+8) if pc else p.reg('A2')
            calls.append(dict(node=nodes.index(receiver),enabled=arg1,recursive=arg2))
        elif yeti and address==(0x4e21d0 if pc else 0x369b60):query.append(p.reg('ECX' if pc else 'A0'))
        elif yeti and address==(0x5b7a00 if pc else 0x100810):
            pointer=p.uint(p.reg('ESP')+4) if pc else p.reg('A1');words=list(struct.unpack('<8I',read(pointer,32)));idx=len(messages);messages.append(words)
            assert p.reg('ECX' if pc else 'A0')==entity and words[:6]==[0x2725,0,0,0,obj,0]
            assert words[6]==([0x708f48,0x7093c0,0x7093b4] if pc else [0x45ef18,0x45eef8,0x45ef08])[idx]
            assert words[7]==((c['fill']*0x01010101&0xffffff00)|(0 if idx==0 else 1))
    addresses={entry,enable}|({0x4e21d0,0x5b7a00} if pc else {0x369b60,0x100810}) if yeti else {entry,enable}
    hooks=[u.hook_add((p.uc if pc else __import__('unicorn')).UC_HOOK_CODE,observe,begin=a,end=a) for a in addresses]
    if pc:
        p.run(entry,this=obj if yeti else nodes[0],args=(0,) if yeti else (state,recursive));result=dict(entry=f'{entry:08X}',completion='original return',blocks=sum(p.visits.values()))
    else:
        p.reg('A0',obj if yeti else nodes[0]);p.reg('A1',0 if yeti else state);p.reg('A2',recursive);original=[read(a,n) for a,n in p.ranges]
        result=p.run(entry,[p.RETURN],count=6000,timeout_us=500000);assert original==[read(a,n) for a,n in p.ranges]
        assert p.reg('SP')==0x22000800 and p.reg('RA')==p.RETURN
        assert [p.reg(str(i)) for i in range(16,21)]==[0x56780000+i for i in range(16,21)] and result['initialUpper64']==result['finalUpper64']
    for hook in hooks:u.hook_del(hook)
    assert calls==[dict(node=i,enabled=state,recursive=recursive if j==0 else 1) for j,i in enumerate(touched)],calls
    assert query==([registry] if yeti else []) and len(messages)==(3 if yeti else 0)
    after=read(base,0x6000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:12]
    result.update(enableCalls=calls,entityMessages=messages,registryCalls=query,finalFlags=[p.uint(n+(0xb0 if pc else 0xb4)) for n in nodes],guardedBytes=0x6000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),scope='Literal existing base Nodes and child lists;original recursion and Yeti caller when selected.No scene/resource startup or derived receiver behavior claim.')
    return result


def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];start=time.perf_counter();report=dict(kind='original-node-enable-and-yeti-entry',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[])
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
