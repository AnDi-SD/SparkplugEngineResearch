#!/usr/bin/env python3
"""Original42EA50 external texture source through native PC file stream."""
from pathlib import Path
import hashlib,json,struct,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from pc_loader_fixtures import empty_manager
from pc_stl_fixtures import install_char_traits,read_cstring
from pc_crt_string_fixtures import install_crt_string
from pc_crt_format_fixtures import install_sprintf
from pc_win32_file_fixtures import Win32FileInput
from probe_pc_texture_cross_upload import CrossUploadFixture,specimen as cross_specimen
from probe_pc_texture_compressed_missing import CompressedMissingMipFixture,specimen as compressed_specimen
from probe_pc_texture_missing_mips import field

PATHS = {'drive':(b'C:\\Media\\SFX\\scene.smo',b'image.tex',b'C:\\Media\\SFX\\image.tex'),
    'relative':(b'Media/SFX/scene.smo',b'sub/image.tex',b'Media/SFX/sub/image.tex'),
    'bare':(b'scene.smo',b'image.tex',b'image.tex'),
    'absolute-ref':(b'C:\\Media\\scene.smo',b'D:\\image.tex',b'C:\\Media\\D:\\image.tex')}


def specimen(kind,path_case):
    source_name,reference,resolved=PATHS[path_case]
    if kind=='dxt1':data,_=compressed_specimen('dxt1',4,'indices')
    else:data=cross_specimen({'rgba':0,'p8':2}[kind],(3,5),'random')
    outer=field(4,struct.pack('<H',len(reference)+1)+reference+b'\0')+b'\0'
    return outer,data,source_name,reference,resolved


def main(kind='rgba',path_case='drive',reader='dx',label='first',failure='none',*,return_capture=False):
    assert reader in ('dx','common') and label.replace('-','').isalnum() and len(label)<=40
    assert failure in ('none','open')
    outer,data,source_name,reference,resolved=specimen(kind,path_case)
    f=CompressedMissingMipFixture(outer,(4,4),'dxt1') if kind=='dxt1' else CrossUploadFixture(outer,(4,8),0 if kind=='rgba' else 2)
    p=f.p;install_char_traits(p);install_crt_string(p);install_sprintf(p)
    class ExternalFileInput(Win32FileInput):
        def create(self,p):
            super().create(p)
            if failure=='open':
                self.opened=False;self.events[-1].append('failure');p.set_reg('EAX',0xffffffff)
        def close(self,p):
            if failure=='open' and self.args(p,1)==[0xffffffff]:
                assert not self.opened;self.events.append(['close-invalid']);p.fixture_return(4,eax=0)
            else:super().close(p)
    api=ExternalFileInput(p,data,resolved)
    if failure=='open':
        # Identified KERNEL32 GetLastError, declared FILE_NOT_FOUND input.
        p.put_uint(0x6d9144,0x34120060)
        p.seams[0x34120060]=lambda p:p.fixture_return(eax=2)
    p.mu.mem_write(0x73ff60,b'\0')
    name=p.allocate(len(source_name)+1);p.mu.mem_write(name,source_name+b'\0');p.put_uint(f.stream+0x18,name)
    split_calls=[];base=0x34121000;p.mu.mem_map(base,4096,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
    def splitpath(p):
        path,drive,directory,filename,extension=f.args(p,5);raw=read_cstring(p,path,260)
        assert not filename and not extension and len(raw)<260
        prefix=raw[:2] if raw[1:2]==b':' else b'';rest=raw[len(prefix):]
        end=max(rest.rfind(b'/'),rest.rfind(b'\\'))+1;folder=rest[:end]
        assert len(prefix)<3 and len(folder)<256
        p.mu.mem_write(drive,prefix+b'\0');p.mu.mem_write(directory,folder+b'\0')
        split_calls.append([raw.decode('ascii'),prefix.decode('ascii'),folder.decode('ascii')]);p.fixture_return()
    p.put_uint(0x6d9358,base+16);p.seams[base+16]=splitpath
    manager,_=empty_manager(f);p.put_uint(manager+0x10,2 if kind=='dxt1' else 1);p.put_uint(manager+0x14,1);p.put_uint(0x75dde8,manager)
    obj=f.call(0x4ab520);serializer=f.call(0x42b660 if reader=='dx' else 0x42dc30)
    at=time.monotonic();report=dict(kind='native-external-texture-source',textureKind=kind,pathCase=path_case,reader=reader,
        inputSha256=hashlib.sha256(outer).hexdigest().upper(),externalSha256=hashlib.sha256(data).hexdigest().upper(),
        sourceName=source_name.decode('ascii'),reference=reference.decode('ascii'),resolved=resolved.decode('ascii'),failure=failure,phase='read')
    try:
        result=f.call(0x42c640 if reader=='dx' else 0x42f180,this=serializer+0x10,args=(f.stream,obj))&255
        report.update(result=result,instructions=sum(p.visits.values()),cursor=f.position,
            visitedStages={f'{a:08X}':p.visits[a] for a in (0x42ea50,0x416dc0,0x6bd580,0x6bd710,0x45b8a0,0x6bdb60,0x6bd980,0x6be2a0)})
        assert not api.opened and len(split_calls)==1
        if failure=='none':
            assert result==1 and f.position==len(outer) and not f.errors
            assert all(report['visitedStages'].values())
        else:
            assert result==0 and f.position==len(outer)-1
            assert report['visitedStages']['006BDB60']==0 and report['visitedStages']['006BE2A0']==1
        state=[p.uint(obj+a) if n==4 else p.mu.mem_read(obj+a,1)[0] for a,n in ((0x18,4),(0x1c,1),(0x20,4),(0x24,1),(0x28,4),(0x2c,4),(0x44,4),(0x48,4))]
        mips=[]
        for rec in f.levels:
            mips.append(b''.join(bytes(p.mu.mem_read(rec['pixels']+y*rec['pitch'],rec['row_bytes'])) for y in range(rec['rows'])).hex())
            assert all(bytes(p.mu.mem_read(rec['pixels']+y*rec['pitch']+rec['row_bytes'],4))==b'\xa5'*4 for y in range(rec['rows']))
        assert (bool(mips) and state[3]==1) if failure=='none' else (not mips and state[3]==0)
        report.update(state=state,mips=mips,phase='cleanup',fileEvents=api.events,splitCalls=split_calls)
        f.call(0x4abb50,this=obj,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,));f.call(0x4228a0,this=manager)
        for address in (0x75db9c,0x75db78,0x75526c,0x755264):
            owned=p.uint(address)
            if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
        remaining=set(f.allocations)-set(f.freed)
        report['remainingAllocations']=[dict(address=f'{a:08X}',size=f.allocations[a],bytes=bytes(p.mu.mem_read(a,f.allocations[a])).hex()) for a in sorted(remaining)]
        #42EA50 does not free the416DC0 filename allocation. Record before
        #explicit fixture cleanup; source RAII must not reproduce this leak.
        assert len(remaining)==1
        leaked=next(iter(remaining));assert f.allocations[leaked]==len(reference)+1 and bytes(p.mu.mem_read(leaked,len(reference)+1))==reference+b'\0'
        p.run(0x412420,args=(leaked,),callee_pop=False)
        assert set(f.allocations)==set(f.freed) and f.device_refs==1 and f.texture_refs==f.surface_refs==0
        assert all(rec['refs']==0 and not rec['locked'] for rec in f.levels)
        report.update(status='passed',releasedAllocations=len(f.freed),fixtureFreedFilenameBytes=len(reference)+1)
    except (AssertionError,ValueError) as error:
        report.update(status='stopped',error=str(error),stoppedIp=f'{p.reg("EIP"):08X}',cursor=f.position,instructions=sum(p.visits.values()),fileEvents=api.events,splitCalls=split_calls)
        raise
    finally:
        report.update(arenaReservedBytes=p.allocated,elapsedSeconds=time.monotonic()-at)
        (ROOT/f'local-data/results/cycle-20260908-0700/cp118-external-{kind}-{path_case}-{reader}-{label}.json').write_text(json.dumps(report,indent=2)+'\n')
        print('CAPTURE',json.dumps({k:v for k,v in report.items() if k not in ('mips','fileEvents')}),flush=True)
    return report if return_capture else 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
