"""Original IceWorm holes activation and explicit Kiko activation boundaries."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER
FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/ai-activation-fields'
CATALOG={x['className'][2:-8]:x for x in json.loads((FOLDER/'selection.json').read_text())}
START=(0xc4f44000,0x43610000,0x449b2000)
def sha(b):return hashlib.sha256(b).hexdigest().upper()

def execute(c,k):
    pc=k=='pc';name=c['name'];holes=name=='IceWormHoles';delta=0 if pc else 4;entry=int(CATALOG[name][k]['slots'][9],16);finds=[];compares=[]
    if pc:
        p=PcBlocks();p.fixture_seh_chain();base=p.allocate(0x6000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu;uc=p.uc
    else:
        ranges=[(entry,0x260),(0x1a7160,0x154),(0x1163f0,8),(0x14dd40,0x10),(0x40a8a8,0x10),(0x40a9b8,0x34),(0x109af0,0x14)]
        p=Ps2ScalarPrefix(ranges,stack_window=(0x22000000,4096),upper64=UPPER);base=0x21000000;p.map(base,0x6000);p.map(0x22000000,4096);p.reg('SP',0x22000800);p.reg('GP',0x4a4170);read,write=p.read,p.write;u=p.u
        import unicorn as uc
        raw,sections=pristine()
        for a,n in [(0x4902f0,0x40),(0x49c870,8),(0x4783c0,12)]:p.map(a,n);write(a,read_window('ps2',raw,a,n,sections)[0])
        p.map(0x49fc7c,4)
    obj,owner,entity,character,command,profile,player,player_node,movement,subject,move_parent=[base+x for x in (0,0x800,0xc00,0x1000,0x1400,0x1800,0x1c00,0x2000,0x2400,0x2800,0x2c00)]
    nodes=[base+0x3000+i*0x400 for i in range(6)];root,actor,in_node,out_node,in_decoy,out_decoy=nodes
    put=p.put_uint;write(base,bytes([c['fill']])*0x6000);put(obj,int(CATALOG[name][k]['vtable'],16));put(obj+0x20+delta,owner)
    put(owner+0x24,entity);put(entity+0x24,actor);put(owner+(0x144 if pc else 0x154),character);put(character+(0x130 if pc else 0x13c),command)
    put(character+(0x12c if pc else 0x138),movement);put(character+(0x124 if pc else 0x130),subject);put(subject+4,0)
    put(0x765ad4 if pc else 0x49fc7c,profile);put(profile+0x2b4,player);put(player+0x24,player_node)
    write(player_node+(0x74 if pc else 0x70),struct.pack('<3f',3.25,-4.,8.));write(move_parent+(0x74 if pc else 0x70),struct.pack('<3f',10.,20.,30.))
    put(movement+(0x1e4 if pc else 0x1f0),move_parent if c['moveParent'] else 0);put(owner+(0x1b0 if pc else 0x1c0),c['ownerType'])
    labels=['scene-root','actor','in','out','inlet','outside'];children=[[1]+([2] if c['presence']&1 else [])+([3] if c['presence']&2 else [])+[4,5],[],[],[],[],[]]
    for i,n in enumerate(nodes):
        put(n,0x6dc4f4 if pc else 0x4902f0);put(n+0x10,n+0x100);write(n+0x109,labels[i].encode()+b'\0');put(n+(0xb0 if pc else 0xb4),0x1200);put(n+0x2c,root if i else 0)
        head=n+0x200 if pc else n+0x18
        if pc:put(n+0x18,head)
        links=[n+0x220+j*0x10 for j in range(len(children[i]))];chain=[head]+links;forward,backward=(0,4) if pc else (4,0)
        for j,link in enumerate(chain):put(link+forward,chain[(j+1)%len(chain)]);put(link+backward,chain[(j-1)%len(chain)])
        for link,child in zip(links,children[i]):put(link+8,nodes[child])
    find=0x421330 if pc else 0x1a7160
    def observe(u,address,size,user):
        if address==find:finds.append(p.reg('ECX' if pc else 'A0'))
        else:
            pair=[p.reg('A0'),p.reg('A1')];assert (pair[0]|pair[1])&7;compares.append(pair)
    u.hook_add(uc.UC_HOOK_CODE,observe,begin=find,end=find)
    if not pc:u.hook_add(uc.UC_HOOK_CODE,observe,begin=0x40a8a8,end=0x40a8a8)
    stop=None if holes else (0x410050 if pc else 0x100860) if name=='KikoHole' else (0x420470 if pc else 0x1090e0)
    before=read(base,0x6000);expected=bytearray(before)
    def ew(off,v):struct.pack_into('<I',expected,off,v)
    if holes:
        ew(0x3c0+delta,actor);ew(0x3bc+delta,in_node if c['presence']&1 else 0);ew(0x3b8+delta,out_node if c['presence']&2 else 0);expected[0x3b4+delta]=int(c['ownerType']==1);ew(0x3a8+delta,5)
    elif name=='KikoHole':ew(0x3a8+delta,actor);ew(0x3ac+delta,player_node)
    else:
        assert name=='KikoMoving';ew(0x3b0+delta,actor);ew(0x3b4+delta,player_node);ew(0x3b8+delta,command)
        expected[0x3bc+delta:0x3c8+delta]=struct.pack('<3f',3.25,-4.,8.)
        for off in (actor-base+0x20,movement-base+(0x138 if pc else 0x144)):struct.pack_into('<3I',expected,off,*START)
        ew(actor-base+(0xb0 if pc else 0xb4),0x1201)
        if c['moveParent']:
            pos=struct.unpack('<3f',struct.pack('<3I',*START));struct.pack_into('<3f',expected,movement-base+(0x150 if pc else 0x15c),*[a+b for a,b in zip(pos,(10,20,30))])
    if pc:
        p.run(entry,this=obj,stop_at=stop);result=dict(entry=f'{entry:08X}',completion='declared original boundary;guest discarded' if stop else 'original return',blocks=sum(p.visits.values()))
        if stop:result['stop']=f'{stop:08X}'
        value=p.reg('EAX')&255
    else:
        p.reg('A0',obj);original=[read(a,n) for a,n in p.ranges];saved={r:p.reg(r) for r in ('S0','S1','SP','RA')};result=p.run(entry,[stop or p.RETURN],count=10000,timeout_us=1000000);value=p.reg('V0')&0xffffffff
        assert original==[read(a,n) for a,n in p.ranges]
        if not stop:assert saved=={r:p.reg(r) for r in saved} and result['initialUpper64']==result['finalUpper64']
    consumer=None
    if holes:assert value==1 and finds.count(root)==2
    elif name=='KikoHole':
        consumer=[p.reg('ECX' if pc else 'A0'),p.uint(p.reg('ESP')+4) if pc else p.reg('A1')];assert consumer==[subject,obj]
    else:
        ptrs=[p.uint(p.reg('ESP')+4),p.uint(p.reg('ESP')+8)] if pc else [p.reg('A1'),p.reg('A2')]
        consumer=[list(struct.unpack('<3f',read(a,12))) for a in ptrs];assert consumer==[[-1.,0.,0.],[0.,1.,0.]],consumer
    after=read(base,0x6000);assert after==bytes(expected),[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:15]
    result.update(nativeNodeFindCalls=len(finds),nativeUnalignedStrcmpCalls=len(compares),consumerArguments=consumer,guardedBytes=0x6000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),scope='IceWormHoles full original exact Node search,including absent matches and similar-name decoys;unaligned node string storage declared before execution.KikoHole stops before original listener registration;KikoMoving before orientation helper.No game callback substitution,listener allocation,basis result or whole Kiko activation claim.')
    return result

def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];start=time.perf_counter();report=dict(kind='original-ai-activation-fields',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),catalogSha256=sha((FOLDER/'selection.json').read_bytes()),cases=[])
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
