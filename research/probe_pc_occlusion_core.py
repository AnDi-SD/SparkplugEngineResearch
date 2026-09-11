"""Batched original fresh Occlusion Init/reader with exact named CRT dependency."""
from pathlib import Path
import hashlib,json,struct,sys,time
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_navigation_readers import initialize,cleanup
from pc_crt_format_fixtures import install_sprintf
from pc_crt_string_fixtures import install_crt_string
from pc_legacy_qsort import install_qsort,DLL_SHA
from inspect_serializer_manager import PC_SHA256

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'local-data/results/tools-core-cycle-20260911-1900/occlusion'
class Fixture(PCWriteBytesFixture):
    guest_execution_profile='character'
    guest_arena_size=0x20000 # one bounded batch; avoids five protected preparations
def words(values):return struct.pack('<'+'I'*len(values),*values)
def main(name):
    out=(BASE/name).resolve();assert out.parent==BASE.resolve() and not out.exists();out.mkdir(parents=True)
    started=time.perf_counter();source=ROOT/'local-data/pc-pristine/Media/Levels/Challenges/race_02.smo';raw=source.read_bytes()
    assert hashlib.sha256(raw).hexdigest().upper()=='AD39C13B896718755CC98445D943FA191640636F940930086CB802629E268436'
    report=dict(status='started',profile='character',instructionLimit=4000000,secondsPerCall=16,childSeconds=30,
        guestArenaBytes=0x20000,workers=1,pcSha256=PC_SHA256,crtSha256=DLL_SHA,source=str(source.relative_to(ROOT)),cases=[])
    (out/'started.json').write_text(json.dumps(report,indent=2)+'\n')
    (out/'probe-source.py').write_bytes(Path(__file__).read_bytes())
    f=Fixture();p=f.p;initialize(f);install_sprintf(p);install_crt_string(p)
    p.mu.mem_write(0x73ff60,b'\0');p.put_uint(0x760640,0x43d24430);p.put_uint(0x760688,0x75dd88)
    _,sorts=install_qsort(p,maximum_count=64,widths=(2,),comparators=(0x4607f0,))
    def cpu(wire,vertex):
        p.run(0x4123d0,args=(0x5c if vertex else 0x28,),callee_pop=False);obj=p.reg('EAX')
        f.call(0x45fe50 if vertex else 0x45f7f0,this=obj);f.data=wire;f.position=0
        assert f.call(0x460300 if vertex else 0x45fb80,this=obj,args=(f.stream,))&255 and f.position==len(wire)
        return obj
    def state(obj,world_done):
        ib,local,world=(p.uint(obj+offset) for offset in (0xd0,0xc8,0xcc));assert ib and local and world
        count=p.uint(ib+0x1c);assert count<=192 and p.uint(ib+0x20)&1==0
        index=list(struct.unpack('<'+'H'*count,p.mu.mem_read(p.uint(ib+0x24),count*2)))
        vertices=p.uint(world+0x1c);assert vertices<=64 and p.uint(local+0x1c)==vertices
        vb=p.uint(world+0x54);local_vb=p.uint(local+0x54)
        faces=list(range(p.uint(obj+0xfc),p.uint(obj+0x100),32));assert len(faces)<=64
        edge_slots=range(p.uint(obj+0xdc),p.uint(obj+0xe0),4);edges=[p.uint(at) for at in edge_slots];assert len(edges)<=192
        def fidx(ptr):return 0xffffffff if not ptr else faces.index(ptr)
        def vidx(ptr):assert vb<=ptr<vb+vertices*12 and (ptr-vb)%12==0;return (ptr-vb)//12
        data=[p.uint(obj+0x1b4)&255,(p.uint(obj+0x1b4)>>8)&255,p.uint(obj+0xd4),p.uint(obj+0x160)&255,
              len(index),*index,vertices,*[p.uint(local_vb+i*4) for i in range(vertices*3)],
              *[p.uint(vb+i*4) for i in range(vertices*3)],*[p.uint(obj+0x17c+4*i) for i in range(6)],
              *[p.uint(obj+0x194+4*i) for i in range(4)],int(world_done)]
        if world_done:data += [p.uint(obj+0x1a4+4*i) for i in range(4)]
        data += [len(faces)]
        for face in faces:data += [*[p.uint(face+4*i) for i in range(4)],*[vidx(p.uint(face+16+4*i)) for i in range(3)]]
        data += [len(edges)]
        for edge in edges:
            outgoing=[p.uint(at) for at in range(p.uint(edge+0x14),p.uint(edge+0x18),4)]
            data += [vidx(p.uint(edge)),vidx(p.uint(edge+4)),fidx(p.uint(edge+12)),fidx(p.uint(edge+8)),p.uint(edge+0x24)&255,
                     p.uint(edge+0x20),len(outgoing),*[edges.index(pointer) for pointer in outgoing]]
        return data
    square=[(-10.,-10.,10.),(10.,-10.,10.),(10.,10.,10.),(-10.,10.,10.)]
    def pair(points,indices,wide=False):
        return (words([2,len(indices)//3,int(wide)])+struct.pack('<'+('I' if wide else 'H')*len(indices),*indices),
                words([0,len(points),0])+b''.join(struct.pack('<3f',*point) for point in points))
    inputs=[('race-quad',0,raw[190026:190050],raw[190055:190115]),
      ('positive-wide',0,*pair([(1.,2.,3.),(4.,2.,3.),(4.,6.,3.),(1.,6.,3.)],[0x10000+i for i in (0,1,2,0,2,3)],True)),
      ('duplicate-five',0,*pair(square+[square[0]],[0,1,2,4,2,3])),
      ('discarded-outlier',0,*pair(square+[square[0],(999.,200.,40.)],[0,1,2,4,2,3])),
      ('race-reader',1,raw[189986:190116],b'')]
    binary=bytearray(b'OCV1'+words([len(inputs)]))
    try:
        for case_name,reader,ib_wire,vb_wire in inputs:
            row=dict(name=case_name,reader=reader,inputIndexHex=ib_wire.hex(),inputVertexHex=vb_wire.hex());report['cases'].append(row)
            obj=f.call(0x470a70);temporary=[];serializer=None
            if reader:
                serializer=f.call(0x44f320);f.data=ib_wire;f.position=0
                entry=0x44f400;call_args=(f.stream,obj);this=serializer
            else:
                temporary=[cpu(ib_wire,False),cpu(vb_wire,True)];entry=0x470fe0;call_args=tuple(temporary);this=obj
            call_start=time.perf_counter();result=f.call(entry,this=this,args=call_args)&255
            row.update(result=result,instructions=sum(p.visits.values()),seconds=time.perf_counter()-call_start)
            assert result==1 and (not reader or f.position==len(ib_wire));before=state(obj,False)
            # Explicit local PRS, no Scene linkage. Every original point transform
            # and Occlusion world operation executes normally.
            p.put_floats(obj+0x20,(1.,2.,3.));p.put_floats(obj+0x30,(-2.,3.,4.));p.put_floats(obj+0x40,(0.,1.,0.,-1.,0.,0.,0.,0.,1.))
            p.put_uint(obj+0xb0,p.uint(obj+0xb0)|1)
            f.call(0x46dd90,this=obj,args=(0,));after=state(obj,True)
            row.update(status='returned',initialWords=before,worldWords=after)
            label=case_name.encode();binary+=words([len(label)])+label+words([reader,len(ib_wire),len(vb_wire)])+ib_wire+vb_wire
            for values in (before,after):binary+=words([len(values)])+words(values)
            for buffer in temporary:f.call(p.uint(p.uint(buffer)),this=buffer,args=(1,))
            if serializer:f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
            f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
            row['objectAndInputsFreed']=all(x in f.freed for x in (obj,*temporary,*([serializer] if serializer else [])))
            assert row['objectAndInputsFreed']
            (out/'before-cleanup.json').write_text(json.dumps(report,indent=2)+'\n')
        cleanup(f,[]);assert set(f.allocations)==set(f.freed)
        report.update(status='completed',allTrackedAllocationsFreed=True,sortCalls=len(sorts))
        (out/'capture.dat').write_bytes(binary)
    except Exception as error:
        report.update(status='bounded-failure',error=str(error),failureIp=f'{p.reg("EIP"):08X}',tail=[f'{a:08X}' for a in p.tail])
    report.update(elapsedSeconds=time.perf_counter()-started,arenaUsed=p.allocated)
    report['sources']=[dict(path=str(path.relative_to(ROOT)),sha256=hashlib.sha256(path.read_bytes()).hexdigest().upper()) for path in [Path(__file__),ROOT/'research/pc_legacy_qsort.py',ROOT/'research/pc_instruction_emulator.py',ROOT/'research/pc_serializer_fixtures.py',ROOT/'research/probe_pc_navigation_readers.py']]
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(status=report['status'],error=report.get('error'),seconds=report['elapsedSeconds'],arenaUsed=report['arenaUsed'],cases=[{k:c.get(k) for k in ('name','result','status','instructions','seconds')} for c in report['cases']])))
    return 0 if report['status']=='completed' else 1
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
