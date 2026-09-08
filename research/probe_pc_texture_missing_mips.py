#!/usr/bin/env python3
"""Unchanged loading.smo TextureData slice; original missing-mip path.

COM provides at most five 16x16 RGBA surfaces, with padding. No internal
pixel/filter helper is replaced. Every attempt starts a fresh capped guest.
"""
from pathlib import Path
import hashlib
import json
import math
import random
import struct
import sys
import time
from pc_instruction_emulator import ROOT,run_bounded
from pc_texture_mip_fixtures import TextureMipChainFixture
from pc_loader_fixtures import empty_manager


def field(kind,payload):
    assert 0<=kind<31 and 0<len(payload)<=2048
    return bytes([0xe0+kind])+struct.pack('<I',len(payload))+payload


class MissingMipFixture(TextureMipChainFixture):
    guest_arena_size=131072
    guest_execution_profile='file'
    max_levels=5
    max_surface_bytes=2048

    def __init__(self,data,dimensions=(16,16),*,surface_profile='tiny'):
        assert surface_profile in ('tiny','corpus32')
        maximum=32 if surface_profile=='corpus32' else 16
        self.max_levels=6 if surface_profile=='corpus32' else 5
        self.max_surface_bytes=8192 if surface_profile=='corpus32' else 2048
        assert len(dimensions)==2 and all(n>0 and n<=maximum and not n&(n-1) for n in dimensions)
        self.dimensions=dimensions
        super().__init__(data);self.install_external_inputs()

    def install_external_inputs(self):
        p=self.p
        # Explicit absent external debug module/registry inputs. The original
        # DebugSetMute resolver itself still executes; no DLL or registry access.
        p.put_uint(0x6d915c,0x340700c0);p.seams[0x340700c0]=self.module_absent
        p.put_uint(0x6d9000,0x340700d0);p.seams[0x340700d0]=self.registry_absent
        p.put_uint(p.uint(self.surface)+4,0x340700e0);p.seams[0x340700e0]=self.addref_surface
        p.put_uint(0x6d9274,0x340700f0);p.seams[0x340700f0]=self.ftol
        p.put_uint(0x6d9370,0x34070100);p.seams[0x34070100]=self.floor
        self.ftol_calls=0

    @classmethod
    def install_on_scene(cls,fixture,dimensions=(16,16),*,surface_profile='tiny'):
        assert surface_profile in ('tiny','corpus32')
        maximum=32 if surface_profile=='corpus32' else 16
        assert len(dimensions)==2 and all(n>0 and n<=maximum and not n&(n-1) for n in dimensions)
        from pc_compact_texture_device import install_texture_device
        install_texture_device(fixture,dimensions[0]*4,fixture_class=cls)
        io=fixture.texture_io;io.dimensions=dimensions;io.levels=[];io.surfaces={}
        io.max_levels=6 if surface_profile=='corpus32' else 5
        io.max_surface_bytes=8192 if surface_profile=='corpus32' else 2048
        io.install_external_inputs();return io

    @classmethod
    def install_many_on_scene(cls,fixture,dimensions,*,surface_profile='tiny'):
        """One real device identity, at most two declared independent textures."""
        import copy
        assert 1<=len(dimensions)<=2
        assert surface_profile in ('tiny','corpus32')
        maximum=32 if surface_profile=='corpus32' else 16
        assert all(len(shape)==2 and all(n>0 and n<=maximum and not n&(n-1) for n in shape) for shape in dimensions)
        first=cls.install_on_scene(fixture,dimensions[0],surface_profile=surface_profile);p=fixture.p
        instances=[first]
        if len(dimensions)==2:
            io=copy.copy(first);base=0x34070000
            io.texture=base+0xa00;io.surface=base+0xa50
            texture_table=base+0xa80;surface_table=base+0xb00
            p.put_uint(io.texture,texture_table);p.put_uint(io.surface,surface_table)
            io.dimensions=dimensions[1];io.levels=[];io.surfaces={};io.events=[]
            entries=[(texture_table+4,0x210,io.addref_texture),(texture_table+8,0x220,io.release_texture),
                (texture_table+0x34,0x230,io.get_level_count),(texture_table+0x48,0x240,io.get_surface),
                (surface_table+4,0x250,io.addref_surface),(surface_table+8,0x260,io.release_surface),
                (surface_table+0x30,0x270,io.get_desc),(surface_table+0x34,0x280,io.lock_surface),
                (surface_table+0x38,0x290,io.unlock_surface)]
            for slot,offset,callback in entries:p.put_uint(slot,base+offset);p.seams[base+offset]=callback
            instances.append(io)
        created=set()
        def create(p):
            args=first.args(p,9)
            candidates=[io for io in instances if io.texture not in created and io.dimensions==tuple(args[1:3])]
            assert candidates,'CreateTexture must match an unused declared texture'
            io=candidates[0];io.create_texture(p);created.add(io.texture)
        p.seams[0x34070030]=create
        fixture.texture_ios=instances
        return instances

    @staticmethod
    def text(p,address):
        result=bytearray()
        for i in range(128):
            value=bytes(p.mu.mem_read(address+i,1))[0]
            if not value:return result.decode('ascii')
            result.append(value)
        raise AssertionError('bounded external name')

    def module_absent(self,p):
        name=self.text(p,self.args(p,1)[0]);assert name in ('d3d9.dll','d3d9d.dll')
        self.events.append(['GetModuleHandleA',name,0]);p.fixture_return(4,eax=0)

    def registry_absent(self,p):
        key,name,out=self.args(p,3);name=self.text(p,name)
        assert key==0x80000002 and name=='Software\\Microsoft\\Direct3D'
        self.events.append(['RegOpenKeyA',name,2]);p.fixture_return(12,eax=2)

    def addref_surface(self,p):
        surface=self.args(p,1)[0];self.require_surface(surface);rec=self.surfaces[surface]
        assert rec['refs']<16;rec['refs']+=1
        if not rec['index']:self.surface_refs=rec['refs']
        self.events.append(['AddRefSurface',rec['index']]);p.fixture_return(4,eax=rec['refs'])

    def lock_surface(self,p):
        surface,out,rect,flags=self.args(p,4);self.require_surface(surface);rec=self.surfaces[surface]
        assert not rec['locked'] and flags in (0,0x10,0x800,0x810),('declared lock flags',flags)
        if rect:assert [p.uint(rect+i*4) for i in range(4)]==[0,0,rec['width'],rec['height']]
        rec['locked']=True;self.locked=True;p.put_uint(out,rec['pitch']);p.put_uint(out+4,rec['pixels'])
        self.events.append(['LockRect',rec['pitch'],flags,rec['index']]);p.fixture_return(16,eax=0)

    def ftol(self,p):
        # MSVCR71 _ftol external ABI: pop ST0, truncate to signed EDX:EAX.
        # Decode its integer mantissa exactly, without a binary64 detour.
        top=(p.reg('FPSW')>>11)&7
        mantissa,exponent=p.mu.reg_read(getattr(p.xr,'UC_X86_REG_FP'+str(top)))
        power=exponent&0x7fff;assert power!=0x7fff
        shift=(power or 1)-16383-63
        value=mantissa<<shift if shift>=0 else mantissa>>(-shift)
        if exponent&0x8000:value=-value
        assert -(1<<63)<=value<(1<<63)
        p.set_reg('FPTAG',p.reg('FPTAG')|(3<<(top*2)))
        p.set_reg('FPSW',(p.reg('FPSW')&~0x3800)|(((top+1)&7)<<11))
        p.set_reg('EDX',(value>>32)&0xffffffff);self.ftol_calls+=1
        p.fixture_return(eax=value&0xffffffff)

    def floor(self,p):
        value=struct.unpack('<d',p.mu.mem_read(p.reg('ESP')+4,8))[0]
        assert math.isfinite(value) and abs(value)<=65536
        p.fixture_push_x87(float(math.floor(value)));p.fixture_return()

    def create_texture(self,p):
        device,width,height,levels,usage,fmt,pool,out,shared=self.args(p,9)
        assert device==self.device and (width,height)==self.dimensions and not self.texture_refs
        assert (levels,usage,fmt,pool,shared)==(0,0,0x15,1,0)
        self.events.append(['CreateTexture',width,height,levels,usage,fmt,pool,shared])
        self.width=width;self.height=height;self.format=fmt
        self.texture_refs=self.surface_refs=1;p.put_uint(out,self.texture)
        table=p.uint(self.surface);self.levels=[];self.surfaces={}
        for index in range(self.max_levels):
            surface=self.surface if index==0 else p.allocate(4)
            row_bytes=width*4;pitch=row_bytes+4;capacity=pitch*height
            assert capacity<=self.max_surface_bytes
            pixels=p.allocate(capacity);p.put_uint(surface,table);p.mu.mem_write(pixels,b'\xa5'*capacity)
            rec=dict(index=index,width=width,height=height,row_bytes=row_bytes,rows=height,pitch=pitch,
                     pixels=pixels,refs=1,locked=False,surface=surface)
            self.levels.append(rec);self.surfaces[surface]=rec
            if width==height==1:break
            width=max(1,width//2);height=max(1,height//2)
        assert self.levels[-1]['width']==self.levels[-1]['height']==1
        self.pitch=self.levels[0]['pitch'];p.fixture_return(36,eax=0)


def specimen(pattern='corpus',dimensions=(16,16)):
    if pattern=='icebat':
        assert dimensions==(32,32)
        raw=(ROOT/'local-data/pc-pristine/Media/Characters/Animals/icebat.smo').read_bytes()
        digest=hashlib.sha256(raw).hexdigest().upper()
        assert len(raw)==35571 and digest=='6AEC9CA21EB50FD93E89955551C3557FF19BD21260038FF6E7CF94EEF178BF72'
        assert raw[1336:1344]==struct.pack('<II',0x78ea082b,0x4f4f4253)
        return raw[1344:5496],digest
    path=ROOT/'local-data/pc-pristine/Media/Menus/loading.smo';raw=path.read_bytes()
    digest=hashlib.sha256(raw).hexdigest().upper()
    assert len(raw)==2442 and digest=='0E8EB7A89E952CD0CF096AE4F5E3F1FE4D56BEC3696DB427567F0BB2BF04427E'
    data=raw[1053+8:1053+1088]
    original=b'\0\0\0\xff'*256;assert data.count(original)==1
    rng=random.Random(0x61039a)
    if pattern=='corpus':pixels=original
    elif pattern=='ramp':pixels=bytes(c for y in range(16) for x in range(16) for c in (x*16,y*16,(x+y)*8,(x*17)^(y*17)))
    elif pattern=='impulse':pixels=bytes(c for y in range(16) for x in range(16) for c in [255 if x==y==8 else 0]*4)
    elif pattern=='edges':pixels=bytes(c for y in range(16) for x in range(16) for c in (255 if x==0 else 0,255 if y==15 else 0,0,255))
    elif pattern=='checker':pixels=bytes(c for y in range(16) for x in range(16) for c in [255*((x+y)%2)]*4)
    elif pattern=='random':pixels=bytes(rng.randrange(256) for _ in range(1024))
    else:raise ValueError('declared bounded pixel pattern')
    if dimensions==(16,16):return data.replace(original,pixels),digest
    width,height=dimensions
    pixels=b''.join(pixels[y*64:y*64+width*4] for y in range(height))
    native=field(0,b'\1'+struct.pack('<3I',width,height,0)+b'\1'+struct.pack('<3I',width,width*4,height)+pixels)+b'\0'
    inner=field(2,b'\0')+b'\0'+field(6,struct.pack('<I',6))+field(1,native)+b'\0'
    return field(3,inner)+b'\0',digest


def main(label='first',pattern='corpus',shape='16x16',*,return_capture=False):
    if not label.replace('-','').isalnum() or len(label)>40:raise ValueError('short report label')
    dimensions=tuple(map(int,shape.split('x')))
    assert (pattern=='icebat' and dimensions==(32,32)) or (len(dimensions)==2 and all(n in (1,2,4,8,16) for n in dimensions))
    data,digest=specimen(pattern,dimensions)
    f=MissingMipFixture(data,dimensions,surface_profile='corpus32' if pattern=='icebat' else 'tiny');p=f.p
    manager,_=empty_manager(f);p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(0x75dde8,manager)
    obj=f.call(0x4ab520);serializer=f.call(0x42b660)
    coefficients=[];coefficient_args=[];codecs=[]
    def observe_coefficients(mu,address,size,user):
        if address==0x619219:coefficient_args.append(f.args(p,3))
        else:
            table=p.reg('EAX');assert table in f.allocations
            count=p.uint(table);assert 4<=count<=4096 and count<=f.allocations[table]
            coefficients.append({'args':coefficient_args.pop(),'hex':bytes(p.mu.mem_read(table,count)).hex()})
    p.mu.hook_add(p.uc.UC_HOOK_CODE,observe_coefficients,begin=0x619219,end=0x619219)
    p.mu.hook_add(p.uc.UC_HOOK_CODE,observe_coefficients,begin=0x6194d0,end=0x6194d0)
    def observe_codecs(mu,address,size,user):
        if codecs:return
        state=p.reg('EBX')
        for offset in (0,4):
            codec=p.uint(state+offset);table=p.uint(codec)
            codecs.append({'kind':p.uint(codec+8),'methods':[f'{p.uint(table+i):08X}' for i in (0,4,8)]})
    p.mu.hook_add(p.uc.UC_HOOK_CODE,observe_codecs,begin=0x61c647,end=0x61c647)
    report={'kind':'original-texture-missing-mips','inputSha256':digest,'pattern':pattern,
            'payloadSha256':hashlib.sha256(data).hexdigest().upper(),'originalObjectOffset':1336 if pattern=='icebat' else 1053,'originalObjectSize':4160 if pattern=='icebat' else 1088,
            'dimensions':dimensions,'payloadSize':len(data),'inputKind':'unchanged-slice' if pattern=='icebat' or pattern=='corpus' and dimensions==(16,16) else 'constructed-pixel-case',
            'executionProfile':'file','arenaLimitBytes':p.arena_size,'maxSurfaceBytes':f.max_surface_bytes,'maxLevels':f.max_levels}
    started=time.monotonic()
    try:
        result=f.call(0x42c640,this=serializer+0x10,args=(f.stream,obj))&255
        report.update(nativeInstructions=sum(p.visits.values()),readerResult=result,readerSeconds=time.monotonic()-started,
                      helperVisits={f'{a:08X}':p.visits[a] for a in (0x4abba0,0x4ab030,0x61039a,0x60fdb4,0x619219,
                          0x61a4d4,0x61a5de,0x61a6b1,0x61a884,0x61aa5a,0x61ae22,0x61b316,0x61b6b8,0x61c44f,0x61bdc9)},
                      coefficientTables=coefficients,codecs=codecs)
        assert result==1 and f.position==len(data) and not f.errors
        report['state']=[p.uint(obj+i) for i in (0x18,0x1c,0x20,0x24,0x28,0x2c,0x44,0x48)]
        report['levels']=[]
        for rec in f.levels:
            packed=b''.join(bytes(p.mu.mem_read(rec['pixels']+r*rec['pitch'],rec['row_bytes'])) for r in range(rec['rows']))
            assert all(bytes(p.mu.mem_read(rec['pixels']+r*rec['pitch']+rec['row_bytes'],4))==b'\xa5'*4 for r in range(rec['rows']))
            report['levels'].append({'width':rec['width'],'height':rec['height'],'packedHex':packed.hex()})
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
        target=ROOT/f'local-data/results/cycle-20260908-0700/cp108-texture-missing-{label}.json'
        target.write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps({k:v for k,v in report.items() if k not in ('levels','events','coefficientTables')},sort_keys=True),flush=True)
    return report if return_capture else 0


if __name__=='__main__':
    raise SystemExit(main(*sys.argv[2:]) if sys.argv[1:2]==['--guest'] else run_bounded(Path(__file__),sys.argv[1:]))
