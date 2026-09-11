"""Ten fresh small cases: exact CRT and actual game geometry/Alpha comparators."""
from pathlib import Path
import hashlib,json,struct,sys,time
from pc_instruction_emulator import run_bounded
from pc_legacy_qsort import install_qsort,DLL,DLL_SHA,QSORT
from probe_pc_san_reader import ReaderFixture
from inspect_serializer_manager import PC_SHA256

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'local-data/results/tools-core-cycle-20260911-1900/sort'
def pack_words(values):return struct.pack('<'+'I'*len(values),*values)
def float_bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]
def cases():
    square=[(-10.,-10.,10.),(10.,-10.,10.),(10.,10.,10.),(-10.,10.,10.)]
    def alpha(name,priorities,distances,particles=None,repeated=False):
        rows=[(i+1,0x10000+16*((i//2)%2),0x20000+(0 if repeated else 16*(i%3)),d,pr,0xaabbcc00|(particles or [0]*len(priorities))[i])
              for i,(pr,d) in enumerate(zip(priorities,distances))]
        return dict(name=name,kind=1,count=len(rows),width=24,input=b''.join(pack_words(row) for row in rows),vertices=b'')
    def geometry(name,positions):
        return dict(name=name,kind=0,count=len(positions),width=2,input=struct.pack('<'+'H'*len(positions),*range(len(positions))),
                    vertices=b''.join(struct.pack('<3f',*point) for point in positions))
    return [alpha('alpha-empty',[],[]),alpha('alpha-single',[7],[float_bits(1.)]),
      geometry('geometry-eight',square*2),geometry('geometry-nine',square*2+[(0.,-0.,10.)]),
      geometry('geometry-equal-twelve',[square[0]]*12),geometry('geometry-duplicate-five',square+[square[0]]),
      alpha('alpha-eight-mixed',[3,0xffffffff,7,2,8,1,0,4],[float_bits(float(x)) for x in [0,8,3,4,6,2,1,7]],[0,0,1,0,1,0,0,1]),
      alpha('alpha-nine-equal',[9]*9,[float_bits(5.)]*9),
      alpha('alpha-nine-repeated',[3]*9,[float_bits(float(x)) for x in [9,1,9,4,9,7,3,0,5]],repeated=True),
      alpha('alpha-nine-nan-zero',[1]*9,[0x7fc00001,0,0x80000000,float_bits(4.),0x7fc00002,float_bits(9.),0x7f800000,float_bits(-1.),0])]

def main(output):
    out=(BASE/output).resolve();assert out.parent==BASE.resolve() and not out.exists()
    out.mkdir(parents=True);start=time.perf_counter()
    report=dict(status='started',pcSha256=PC_SHA256,crtSha256=DLL_SHA,version='7.10.7031.4',
        scope='Explicit named host CRT dependency; original comparator bytes. No historical distribution version or full Alpha frame claim.',
        profile='micro',instructionLimit=100000,secondsPerCall=2,childSeconds=30,workers=1,cases=[])
    (out/'started.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    f=ReaderFixture(b'');p=f.p
    allowed,calls=install_qsort(p,maximum_count=12,widths=(2,24),comparators=(0x4607f0,0x454800))
    active=None;binary=bytearray(b'QST1'+struct.pack('<I',10))
    trace=[]
    def compare_hook(mu,address,size,_):
        left,right=(p.uint(p.reg('ESP')+4*i) for i in (1,2))
        assert active is not None
        if active['kind']==0: trace.append([struct.unpack('<H',p.mu.mem_read(left,2))[0],struct.unpack('<H',p.mu.mem_read(right,2))[0]])
        else: trace.append([p.uint(left),p.uint(right)])
    p.mu.hook_add(p.uc.UC_HOOK_CODE,compare_hook,begin=0x4607f0,end=0x4607f0)
    p.mu.hook_add(p.uc.UC_HOOK_CODE,compare_hook,begin=0x454800,end=0x454800)
    try:
        for spec in cases():
            active={key:value for key,value in spec.items() if key not in ('input','vertices')}
            active.update(inputHex=spec['input'].hex(),verticesHex=spec['vertices'].hex());report['cases'].append(active)
            vb=None
            if spec['kind']==0:
                p.run(0x4123d0,args=(0x5c,),callee_pop=False);vb=p.reg('EAX');f.call(0x45fe50,this=vb)
                f.data=pack_words([0,spec['count'],0])+spec['vertices'];f.position=0
                assert f.call(0x460300,this=vb,args=(f.stream,))&255 and f.position==len(f.data)
                p.put_uint(0x75ff9c,vb)
            extent=len(spec['input']);buffer=p.allocate(extent+32);base=buffer+16
            p.mu.mem_write(buffer,b'\xa5'*16+spec['input']+b'\x5a'*16);trace.clear()
            p.run(QSORT,args=(base,spec['count'],spec['width'],0x4607f0 if spec['kind']==0 else 0x454800),callee_pop=False)
            result=bytes(p.mu.mem_read(base,extent)) if extent else b''
            assert bytes(p.mu.mem_read(buffer,16))==b'\xa5'*16 and bytes(p.mu.mem_read(base+extent,16))==b'\x5a'*16
            active.update(status='returned',outputHex=result.hex(),comparatorPairs=list(trace),instructions=sum(p.visits.values()),limits=p.last_execution_limits)
            name=spec['name'].encode('ascii');binary+=pack_words([len(name)])+name+pack_words([spec['kind'],spec['count'],spec['width'],len(spec['vertices'])])+spec['vertices']+spec['input']+result+pack_words([len(trace)])
            for pair in trace:binary+=pack_words(pair)
            if vb:f.call(p.uint(p.uint(vb)),this=vb,args=(1,))
            assert set(f.allocations)==set(f.freed)
            active['trackedAllocationsFreed']=True
        report['status']='completed';assert len(calls)==10
        (out/'capture.dat').write_bytes(binary)
    except Exception as error:
        report.update(status='bounded-failure',error=str(error),failureIp=f'{p.reg("EIP"):08X}',tail=[f'{x:08X}' for x in p.tail])
    report.update(elapsedSeconds=time.perf_counter()-start,arenaUsed=p.allocated,allowedCrtInstructions=len(allowed))
    report['sources']=[dict(path=str(path.relative_to(ROOT)),sha256=hashlib.sha256(path.read_bytes()).hexdigest().upper())
        for path in [Path(__file__),ROOT/'research/pc_legacy_qsort.py',ROOT/'research/pc_instruction_emulator.py',ROOT/'research/probe_pc_san_reader.py',ROOT/'research/probe_pc_animation_lifecycle.py',DLL]]
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status=report['status'],error=report.get('error'),seconds=report['elapsedSeconds'],cases=[dict(name=c['name'],status=c.get('status'),calls=len(c.get('comparatorPairs',[]))) for c in report['cases']])) )
    return 0 if report['status']=='completed' else 1

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
