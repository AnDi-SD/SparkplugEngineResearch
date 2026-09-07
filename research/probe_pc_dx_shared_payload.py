#!/usr/bin/env python3
"""Original shared DX payload wire/cursor and bounded failure boundaries.

Memory stream operations and COM are explicit fixture inputs. The valid path
executes both native factories, writer, reader, Init, wrappers and destruction.
init-return-false substitutes ONLY Init to observe its caller's return contract.
create-failure records a native null dereference and never resumes that guest.
"""
from pathlib import Path
import hashlib,json,struct,subprocess,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_dx_buffers import BufferFixture
from probe_pc_san_reader import ReaderFixture

MODES=('valid','null-target','read-index-failure','read-vertex-failure','tell-failure',
       'write-index-size-failure','write-vertex-size-failure','write-index-failure',
       'write-vertex-failure','init-return-false','create-failure')
checks=0
def check(condition,label):
    global checks
    checks+=1
    if not condition:raise AssertionError(label)

class SharedFixture(BufferFixture):
    guest_execution_profile='file'
    guest_arena_size=131072
    def __init__(self,data,case):
        super().__init__('create-failure' if case=='create-failure' else 'shared')
        self.case=case;self.data=data;self.position=0;self.io=[];self.errors=[]
        self.output=bytearray();self.reads=0;self.writes=0;self.init_args=[]
        p=self.p;self.stream=p.allocate(32);vt=p.allocate(0x44);p.put_uint(self.stream,vt)
        self.address=p.allocate(len(data));p.mu.mem_write(self.address,data)
        for offset,address,callback in ((0x2c,0x34060100,self.tell),(0x30,0x34060110,self.read),
                (0x38,0x34060120,self.write),(0x40,0x34060130,self.buffer)):
            p.put_uint(vt+offset,address);p.seams[address]=callback
        p.seams[0x413540]=lambda p:ReaderFixture.error_message(self,p)
        if case=='init-return-false':p.seams[0x4c29c0]=self.init_false
    def tell(self,p):
        self.io.append(('tell',self.position))
        if self.case=='tell-failure':p.fixture_return(4,eax=0);return
        p.put_uint(p.uint(p.reg('ESP')+4),self.position);p.fixture_return(4,eax=1)
    def read(self,p):
        sp=p.reg('ESP');out,count=p.uint(sp+4),p.uint(sp+8);self.reads+=1
        failed=(self.case=='read-index-failure' and self.reads==1 or
                self.case=='read-vertex-failure' and self.reads==2)
        check(count==4,'reader requests exact u32 header fields')
        self.io.append(('read',self.position,count,not failed))
        if failed:p.fixture_return(8,eax=0);return
        check(self.position+count<=len(self.data),'bounded memory-stream input')
        p.mu.mem_write(out,self.data[self.position:self.position+count]);self.position+=count
        p.fixture_return(8,eax=1)
    def write(self,p):
        sp=p.reg('ESP');address,count=p.uint(sp+4),p.uint(sp+8);self.writes+=1
        failed=self.case in MODES[5:9] and self.writes==MODES[5:9].index(self.case)+1
        check(count<=1024 and len(self.output)+count<=2048,'bounded output')
        self.io.append(('write',count,not failed))
        if not failed:self.output.extend(p.mu.mem_read(address,count))
        p.fixture_return(8,eax=int(not failed))
    def buffer(self,p):
        self.io.append(('buffer',self.position));p.fixture_return(eax=self.address)
    def init_false(self,p):
        self.init_args=[p.uint(p.reg('ESP')+4+i*4) for i in range(4)]
        p.fixture_return(16,eax=0)

def main(case):
    check(case in MODES,'explicit case')
    indices=struct.pack('<3H',0,1,2);vertices=struct.pack('<24f',*range(24))
    raw=struct.pack('<II',len(indices),len(vertices))+indices+vertices
    f=SharedFixture(raw,case);p=f.p
    obj=f.call(0x4c28e0);serializer=f.call(0x4c1df0)
    check(f.allocations[obj]==0x1c and f.allocations[serializer]==0x14,'actual complete factory sizes')
    check(p.uint(obj)==0x6f23a0 and p.uint(serializer)==0x6f2108,'actual vtables')
    report={'kind':'native-dx-shared-payload','case':case,'arenaLimitBytes':p.arena_size,
        'maxAllocationBytes':f.max_allocation_size,'inputSha256':hashlib.sha256(raw).hexdigest().upper()}
    at=time.monotonic()
    try:
        if case=='valid' or case in MODES[5:9]:
            combined=p.allocate(0x3c)
            for offset,value in ((0x2c,f.address+14),(0x30,f.address+8),(0x34,96),(0x38,6)):
                p.put_uint(combined+offset,value)
            result=f.call(0x4c1ed0,this=serializer+0x10,args=(f.stream,combined))&255
            report['writeInstructions']=sum(p.visits.values())
            if case!='valid':
                fail_index=MODES[5:9].index(case)
                check(result==0 and len(f.errors)==1,'writer reports the selected failed operation')
                check(bytes(f.output)==raw[:(0,4,8,14)[fail_index]],'writer stops at exact partial prefix')
                check(f.writes==fail_index+1,'writer does not continue after failure')
            else:check(result==1 and bytes(f.output)==raw and not f.errors,'original writer exact 110 bytes')
        if case=='valid' or case not in MODES[5:9]:
            result=f.call(0x4c1fa0,this=serializer+0x10,args=(f.stream,0 if case=='null-target' else obj))&255
            report['readInstructions']=sum(p.visits.values())
            expected_position={'null-target':0,'read-index-failure':0,'read-vertex-failure':4}.get(case,8)
            check(f.position==expected_position,'exact native cursor')
            if case in ('valid','init-return-false'):
                check(result==1 and not f.errors,'reader success return and diagnostics')
                check([x[0] for x in f.io if x[0]!='write']==['read','read','tell','buffer','buffer'],
                    'reader never reads or seeks past the two sizes')
                if case=='init-return-false':
                    check(f.init_args==[6,96,f.address+8,f.address+14] and not f.buffers,
                        'outer reader ignores explicit false Init return; Init body substituted')
                else:
                    observed={b['kind']:bytes(p.mu.mem_read(b['data'],b['size'])).hex() for b in f.buffers.values()}
                    check(observed=={'index':indices.hex(),'vertex':vertices.hex()},'all actual Init bytes copied')
                    check(f.events==[('create','index',6,8,101,1),('lock','index'),('unlock','index'),
                        ('create','vertex',96,8,0,1),('lock','vertex'),('unlock','vertex')],
                        'actual Init COM argument and copy order')
                    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugDXSharedMeshTests.exe'
                    result_source=subprocess.run([str(binary),'--capture'],capture_output=True,check=True,timeout=10)
                    expected=json.loads(result_source.stdout)
                    check(expected=={'wire':bytes(f.output).hex(),'indices':observed['index'],'vertices':observed['vertex'],
                        'sequentialCursor':110,'contiguousCursor':f.position},'source exact wire/buffer/cursor comparison')
                    report['sourceExecutableSha256']=hashlib.sha256(binary.read_bytes()).hexdigest().upper()
                    report['exactPayloadBytes']=110
            else:
                check(result==0 and not f.buffers,'failed read does not initialize buffers')
                check(len(f.errors)==(0 if case=='null-target' else 1),'exact reader diagnostics')
        report.update(cursor=f.position,io=f.io,events=list(f.events),errors=[e.decode('latin1') for e in f.errors])
        f.call(p.uint(p.uint(obj)),this=obj,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
        for address in (0x75526c,0x755264):
            singleton=p.uint(address)
            if singleton:f.call(p.uint(p.uint(singleton)),this=singleton,args=(1,))
        check(set(f.allocations)==set(f.freed),'all native allocations freed')
        check(all(b['refs']==0 and b['locks']==0 for b in f.buffers.values()),'all COM inputs released and unlocked')
        report.update(status='passed',nativeAssertions=checks,releasedAllocations=len(f.freed))
    except AssertionError as error:
        report.update(status='stopped',error=str(error),stoppedIp=f'{p.reg("EIP"):08X}',
            instructions=sum(p.visits.values()),cursor=f.position,io=f.io,events=list(f.events),
            retainedAllocations=len(set(f.allocations)-set(f.freed)),guestResumed=False)
        if not (case=='create-failure' and p.reg('EIP')==0x4c2ab6 and 'read' in str(error).lower()):raise
        check(f.events==[('create','index',6,8,101,1)],'null COM result stops in first buffer copy')
        report['expectedBoundary']=True
    finally:
        report.update(arenaReservedBytes=p.allocated,elapsedSeconds=time.monotonic()-at)
        directory=ROOT/'local-data/results/cycle-20260908-0700';directory.mkdir(parents=True,exist_ok=True)
        (directory/f'cp104-shared-{case}.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report),flush=True)
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
