#!/usr/bin/env python3
"""Original compressed missing-mip branch; bounded caller-owned COM storage."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import time
from pc_instruction_emulator import ROOT,run_bounded
from pc_loader_fixtures import empty_manager
from probe_pc_texture_missing_mips import MissingMipFixture,field


class CompressedMissingMipFixture(MissingMipFixture):
    def __init__(self,data,dimensions,kind):
        assert kind in ('dxt1','dxt3','dxt5')
        self.compressed_format={'dxt1':0x31545844,'dxt3':0x33545844,'dxt5':0x35545844}[kind]
        super().__init__(data,dimensions)

    def create_texture(self,p):
        device,width,height,levels,usage,fmt,pool,out,shared=self.args(p,9)
        assert device==self.device and (width,height)==self.dimensions and not self.texture_refs
        assert (levels,usage,fmt,pool,shared)==(0,0,self.compressed_format,1,0)
        self.events.append(['CreateTexture',width,height,levels,usage,fmt,pool,shared])
        self.width=width;self.height=height;self.format=fmt
        self.texture_refs=self.surface_refs=1;p.put_uint(out,self.texture)
        table=p.uint(self.surface);self.levels=[];self.surfaces={}
        for index in range(5):
            surface=self.surface if index==0 else p.allocate(4)
            row_bytes=max(1,width//4)*(8 if fmt==0x31545844 else 16)
            rows=max(1,height//4);pitch=row_bytes+4;capacity=pitch*rows
            assert capacity<=2048
            pixels=p.allocate(capacity);p.put_uint(surface,table);p.mu.mem_write(pixels,b'\xa5'*capacity)
            rec=dict(index=index,width=width,height=height,row_bytes=row_bytes,rows=rows,pitch=pitch,
                     pixels=pixels,refs=1,locked=False,surface=surface)
            self.levels.append(rec);self.surfaces[surface]=rec
            if width==height==1:break
            width=max(1,width//2);height=max(1,height//2)
        assert self.levels[-1]['width']==self.levels[-1]['height']==1
        self.pitch=self.levels[0]['pitch'];p.fixture_return(36,eax=0)


def specimen(kind,dimension,pattern='red'):
    assert kind in ('dxt1','dxt3','dxt5') and dimension in (4,8,16)
    assert pattern in ('red','indices','transparent')
    flags={'dxt1':1,'dxt3':2,'dxt5':3}[kind]
    colors=struct.pack('<HHI',0xf800,0x001f,0 if pattern=='red' else 0xe4e4e4e4)
    if pattern=='transparent':colors=struct.pack('<HHI',0,0xffff,0xffffffff)
    alpha=b'\xff'*8 if kind=='dxt3' else b'\xff\0'+b'\0'*6 if kind=='dxt5' else b''
    block=alpha+colors
    stride=dimension//4*len(block);rows=dimension//4;pixels=block*(dimension//4)**2
    native=field(0,b'\1'+struct.pack('<3I',dimension,dimension,flags)+b'\1'+struct.pack('<3I',dimension,stride,rows)+pixels)+b'\0'
    data=field(2,b'\0')+b'\0'+field(6,struct.pack('<I',6))+field(1,native)+b'\0'
    return data,pixels


def main(kind='dxt1',dimension='4',pattern='red',label='first',*,return_capture=False):
    assert label.replace('-','').isalnum() and len(label)<=40
    dimension=int(dimension);data,input_pixels=specimen(kind,dimension,pattern)
    f=CompressedMissingMipFixture(data,(dimension,dimension),kind);p=f.p
    manager,_=empty_manager(f);p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(0x75dde8,manager)
    obj=f.call(0x4ab520);serializer=f.call(0x42b660)
    codecs=[]
    def observe_codecs(mu,address,size,user):
        if codecs:return
        state=p.reg('EBX')
        for offset in (0,4):
            codec=p.uint(state+offset);table=p.uint(codec)
            codecs.append({'kind':p.uint(codec+8),'methods':[f'{p.uint(table+i):08X}' for i in (0,4,8)],
                           'decodeBlock':f'{p.uint(codec+0x8c):08X}','encodeBlock':f'{p.uint(codec+0x90):08X}'})
    p.mu.hook_add(p.uc.UC_HOOK_CODE,observe_codecs,begin=0x61c647,end=0x61c647)
    report={'kind':'original-compressed-missing-mips','codec':kind,'dimension':dimension,'pattern':pattern,
            'payloadSha256':hashlib.sha256(data).hexdigest().upper(),'payloadHex':data.hex(),
            'executionProfile':'file','arenaLimitBytes':p.arena_size,'maxSurfaceBytes':2048,'maxLevels':5}
    started=time.monotonic()
    try:
        result=f.call(0x42c640,this=serializer+0x10,args=(f.stream,obj))&255
        report.update(nativeInstructions=sum(p.visits.values()),readerResult=result,readerSeconds=time.monotonic()-started,codecs=codecs,
                      helperVisits={f'{a:08X}':p.visits[a] for a in (0x4abba0,0x4ab030,0x61039a,0x60fdb4,0x61c44f)})
        assert result==1 and f.position==len(data) and not f.errors
        report['state']=[p.uint(obj+i) for i in (0x18,0x1c,0x20,0x24,0x28,0x2c,0x44,0x48)]
        report['levels']=[]
        for rec in f.levels:
            packed=b''.join(bytes(p.mu.mem_read(rec['pixels']+r*rec['pitch'],rec['row_bytes'])) for r in range(rec['rows']))
            assert all(bytes(p.mu.mem_read(rec['pixels']+r*rec['pitch']+rec['row_bytes'],4))==b'\xa5'*4 for r in range(rec['rows']))
            report['levels'].append({'width':rec['width'],'height':rec['height'],'packedHex':packed.hex()})
        assert report['levels'][0]['packedHex']==input_pixels.hex()
        report['events']=f.events.copy()
        f.call(0x4abb50,this=obj,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,));f.call(0x4228a0,this=manager)
        for address in (0x75db78,0x75526c,0x755264):
            owned=p.uint(address)
            if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
        assert set(f.allocations)==set(f.freed)
        assert f.texture_refs==f.surface_refs==0 and f.device_refs==1
        assert all(rec['refs']==0 and not rec['locked'] for rec in f.levels)
        report.update(status='completed',releasedAllocations=len(f.freed))
    except (AssertionError,ValueError) as error:
        report.update(status='stopped',error=str(error),stoppedIp=f'{p.reg("EIP"):08X}',
                      instructions=sum(p.visits.values()),tail=[f'{a:08X}' for a in p.tail],events=f.events.copy())
        raise
    finally:
        report.update(cursor=f.position,arenaReservedBytes=p.allocated,seconds=time.monotonic()-started,ftolCalls=f.ftol_calls)
        target=ROOT/f'local-data/results/cycle-20260908-0700/cp111-compressed-{kind}-{dimension}-{pattern}-{label}.json'
        target.write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps({k:v for k,v in report.items() if k not in ('events','payloadHex')},sort_keys=True),flush=True)
    return report if return_capture else 0


if __name__=='__main__':
    raise SystemExit(main(*sys.argv[2:]) if sys.argv[1:2]==['--guest'] else run_bounded(Path(__file__),sys.argv[1:]))
