"""Original basis helper: PC cross product, paired stale-local branch and Kiko."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER
FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/kiko-moving-orientation'
START=(-1954.,225.,1241.);END=(-2356.,225.,1117.)
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def packed(v):return struct.pack('<'+str(len(v))+'f',*v)
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]

def execute(c,k):
    pc=k=='pc';basis=c['kind']=='basis';same=basis and c['same'];fwd=c['forward'] if basis else [-1.,0.,0.];up=c['up'] if basis else [0.,1.,0.]
    entry=(0x420470 if pc else 0x1090e0) if basis else (0x5ad280 if pc else 0x25b930);stop=0x1091d8 if not pc and not same else None;calls=[]
    if pc:
        p=PcBlocks();base=p.allocate(4096);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu;uc=p.uc
    else:
        p=Ps2ScalarPrefix([(0x1090e0,0x170),(0x423b58,0x28),(0x25b930,0x1fc),(0x109af0,0x14)],stack_window=(0x22000000,4096),upper64=UPPER)
        base=0x21000000;p.map(base,4096);p.map(0x22000000,4096);p.reg('SP',0x22000800);p.reg('GP',0x4a4170);read,write=p.read,p.write;u=p.u
        import unicorn as uc
        raw,sections=pristine();p.map(0x4783c0,12);write(0x4783c0,read_window('ps2',raw,0x4783c0,12,sections)[0]);p.map(0x49fc7c,4)
        for i in range(20,23):p.reg('F'+str(i),struct.unpack('<I',packed([i+.5]))[0])
    put=p.put_uint;write(base,bytes([c['fill']])*4096);this=base;args=()
    if basis:
        write(base+0x100,packed(fwd));write(base+0x120,packed(up));args=(base+0x100,base+0x120)
        def seed_stack(u,address,size,user):
            # Explicit initial bytes for this method's unwritten local vector.
            write(p.reg('ESP' if pc else 'SP')-(0x18 if pc else 0x10),packed(c['stackVector']))
        u.hook_add(uc.UC_HOOK_CODE,seed_stack,begin=entry,end=entry)
    else:
        owner,entity,char,cmd,profile,player,node,pnode,movement,parent=[base+a for a in (0x400,0x440,0x500,0x700,0x800,0xb00,0xb40,0xc00,0xc80,0xf00)]
        # Non-overlapping accessed fields; action extends only through3C8 here.
        catalog=json.loads((ROOT/'research/ai-action-construction-contracts-2026-09-10.json').read_text());record=next(x for x in catalog['classes'] if x['className']=='wxKikoMovingAIAction');put(base,int(record[k]['vtable'],16))
        put(base+(0x20 if pc else 0x24),owner);put(owner+0x24,entity);put(entity+0x24,node);put(owner+(0x144 if pc else 0x154),char)
        put(char+(0x130 if pc else 0x13c),cmd);put(char+(0x12c if pc else 0x138),movement);put(0x765ad4 if pc else 0x49fc7c,profile);put(profile+0x2b4,player);put(player+0x24,pnode)
        write(pnode+(0x74 if pc else 0x70),packed([3.25,-4.,8.]));put(node+(0xb0 if pc else 0xb4),0x1200)
        write(parent+(0x74 if pc else 0x70),packed([10.,20.,30.]));put(movement+(0x1e4 if pc else 0x1f0),parent if c['parent'] else 0)
    helper=0x420470 if pc else 0x1090e0
    def observe(u,address,size,user):
        ptrs=[p.uint(p.reg('ESP')+4),p.uint(p.reg('ESP')+8)] if pc else [p.reg('A1'),p.reg('A2')]
        values=[list(struct.unpack('<3f',read(a,12))) for a in ptrs];assert values==[list(struct.unpack('<3f',packed(fwd))),list(struct.unpack('<3f',packed(up)))];calls.append(values)
    u.hook_add(uc.UC_HOOK_CODE,observe,begin=helper,end=helper)
    before=read(base,4096);expected=bytearray(before)
    def ew(off,v):struct.pack_into('<I',expected,off,v)
    result_basis=(c['stackVector'] if same else cross(up,fwd))+up+fwd
    if basis:
        if not stop:expected[:36]=packed(result_basis)
    else:
        d=0 if pc else 4;ew(0x3b0+d,node);ew(0x3b4+d,pnode);ew(0x3b8+d,cmd);expected[0x3bc+d:0x3c8+d]=packed([3.25,-4.,8.])
        expected[node-base+0x20:node-base+0x2c]=packed(START);ew(node-base+(0xb0 if pc else 0xb4),0x1201)
        off=movement-base+(0x138 if pc else 0x144);expected[off:off+12]=packed(START)
        if c['parent']:
            off=movement-base+(0x150 if pc else 0x15c);expected[off:off+12]=packed([x+y for x,y in zip(START,[10.,20.,30.])])
        if not stop:expected[node-base+0x40:node-base+0x64]=packed(result_basis);ew(0x3a8+d,1);expected[0x3bc+d:0x3c8+d]=packed(END)
    if pc:
        p.run(entry,this=this,args=args);result=dict(entry=f'{entry:08X}',completion='original return',blocks=sum(p.visits.values()))
        if not basis:assert p.reg('EAX')&255==1
    else:
        p.reg('A0',this)
        if basis:p.reg('A1',args[0]);p.reg('A2',args[1])
        original=[read(a,n) for a,n in p.ranges];saved={r:p.reg(r) for r in ('S0','S1','S2','SP','RA','F20','F21','F22')};result=p.run(entry,[stop or p.RETURN],count=5000,timeout_us=1000000)
        assert original==[read(a,n) for a,n in p.ranges]
        if not stop:assert saved=={r:p.reg(r) for r in saved} and result['initialUpper64']==result['finalUpper64']
    assert len(calls)==1
    after=read(base,4096);assert after==bytes(expected),[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:15]
    result.update(basisCalls=calls,guardedBytes=4096,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),matrixBytes=after[:36].hex() if basis and not stop else None,scope='Original basis helper,including explicit initial bytes of otherwise unwritten local vector.PC whole cross branch;PS2 stops before MULA1091D8.No ACC extension,no SQRT invocation,no game callback return substitution.Kiko PC whole,PS2 still bounded.')
    return result

def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];start=time.perf_counter();report=dict(kind='original-orientation-basis',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[])
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
