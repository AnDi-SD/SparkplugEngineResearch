#!/usr/bin/env python3
"""Original common pixels -> DX upload -> full mip chain, bounded fresh guest."""
from pathlib import Path
import hashlib,json,random,struct,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from pc_loader_fixtures import empty_manager
from probe_pc_texture_missing_mips import MissingMipFixture,field


class CrossUploadFixture(MissingMipFixture):
    def __init__(self,data,dimensions,fmt):
        assert fmt in range(5)
        self.pixel_format=fmt
        super().__init__(data,dimensions)

    def create_texture(self,p):
        device,width,height,levels,usage,fmt,pool,out,shared=self.args(p,9)
        expected=(0x15,0x16,0x29,0x17,0x1a)[self.pixel_format]
        assert device==self.device and (width,height)==self.dimensions and not self.texture_refs
        assert (levels,usage,fmt,pool,shared)==(0,0,expected,1,0)
        self.events.append(['CreateTexture',width,height,levels,usage,fmt,pool,shared])
        self.width=width;self.height=height;self.format=fmt
        self.texture_refs=self.surface_refs=1;p.put_uint(out,self.texture)
        table=p.uint(self.surface);self.levels=[];self.surfaces={}
        for index in range(5):
            surface=self.surface if not index else p.allocate(4)
            row_bytes=width*(4,4,1,2,2)[self.pixel_format];pitch=row_bytes+4;capacity=pitch*height
            assert capacity<=2048
            pixels=p.allocate(capacity);p.put_uint(surface,table);p.mu.mem_write(pixels,b'\xa5'*capacity)
            rec=dict(index=index,width=width,height=height,row_bytes=row_bytes,rows=height,pitch=pitch,
                pixels=pixels,refs=1,locked=False,surface=surface)
            self.levels.append(rec);self.surfaces[surface]=rec
            if width==height==1:break
            width=max(1,width//2);height=max(1,height//2)
        assert self.levels[-1]['width']==self.levels[-1]['height']==1
        self.pitch=self.levels[0]['pitch'];p.fixture_return(36,eax=0)


def specimen(fmt,shape,pattern):
    width,height=shape;assert fmt in range(5) and all(1<=n<=16 for n in shape)
    size=(4,4,1,2,2)[fmt];rng=random.Random(0x42e100+fmt+width*17+height)
    if pattern=='random':pixels=bytes(rng.randrange(256) for _ in range(width*height*size))
    elif pattern=='ramp':pixels=bytes(i%256 for i in range(width*height*size))
    elif pattern in ('corpus','legacy'):
        assert fmt==0 and shape==(16,16)
        raw=(ROOT/'local-data/pc-pristine/Media/SFX/star_currency.smo').read_bytes()
        assert hashlib.sha256(raw).hexdigest().upper()=='2A5FC66D3F297B6015AB98D59CD911DB70E488D6E7E3DE91C6E6455E98491316'
        if pattern=='legacy':return raw[385:1437]
        pixels=raw[411:1435];assert len(pixels)==1024
    else:raise ValueError('bounded declared pixel pattern')
    nested=field(5,struct.pack('<4I',width,height,fmt,size)+pixels)+b'\0'
    return field(2,b'\0')+b'\0'+field(6,struct.pack('<I',1))+field(0,nested)+b'\0'


def main(fmt=0,shape='2x2',pattern='random',label='first',reader='dx',*,return_capture=False):
    assert label.replace('-','').isalnum() and len(label)<=40
    dimensions=tuple(map(int,shape.split('x')));data=specimen(fmt,dimensions,pattern)
    normalized=tuple(max(2,1<<(n-1).bit_length()) for n in dimensions)
    f=CrossUploadFixture(data,normalized,fmt);p=f.p;manager,_=empty_manager(f)
    p.put_uint(manager+0x10,1);p.put_uint(manager+0x14,1);p.put_uint(0x75dde8,manager)
    assert reader in ('dx','common')
    obj=f.call(0x4ab520);serializer=f.call(0x42dc30 if reader=='common' else 0x42b660);at=time.monotonic();checks=0
    coefficients=[];pending=[]
    def observe_coefficients(mu,address,size,user):
        if address==0x619219:pending.append(f.args(p,3))
        else:
            table=p.reg('EAX');assert table in f.allocations
            count=p.uint(table);assert 4<=count<=4096 and count<=f.allocations[table]
            coefficients.append({'args':pending.pop(),'hex':bytes(p.mu.mem_read(table,count)).hex()})
    p.mu.hook_add(p.uc.UC_HOOK_CODE,observe_coefficients,begin=0x619219,end=0x619219)
    p.mu.hook_add(p.uc.UC_HOOK_CODE,observe_coefficients,begin=0x6194d0,end=0x6194d0)
    codec_rows=[]
    def observe_encoder(mu,address,size,user):
        codec=p.reg('ECX');width=p.uint(codec+0x68)
        if width!=normalized[0]:return
        row,z,pixels=f.args(p,3);assert width<=16 and len(codec_rows)<32
        table=p.uint(codec+0x34)
        codec_rows.append({'row':row,'z':z,'width':width,'ditherHex':bytes(p.mu.mem_read(table,128)).hex(),
            'diffusion':p.uint(codec+0x5c),'gamma':p.uint(codec+0x10),'premultiply':p.uint(codec+0x54),
            'inputHex':bytes(p.mu.mem_read(pixels,width*16)).hex()})
    if fmt==0 and label.startswith('trace'):p.mu.hook_add(p.uc.UC_HOOK_CODE,observe_encoder,begin=0x61f153,end=0x61f153)
    def check(ok,text):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(text)
    report=dict(kind='native-cross-texture-upload',format=fmt,shape=dimensions,pattern=pattern,reader=reader,
        inputSha256=hashlib.sha256(data).hexdigest().upper(),inputBytes=len(data),phase='read',
        executionProfile='file',arenaLimitBytes=p.arena_size,maxSurfaceBytes=2048,maxLevels=5)
    try:
        result=f.call(0x42f180 if reader=='common' else 0x42c640,this=serializer+0x10,args=(f.stream,obj))&255
        report.update(result=result,cursor=f.position,instructions=sum(p.visits.values()),
            visitedStages={f'{a:08X}':p.visits[a] for a in (0x42ea50,0x42e100,0x423250,0x4ab650,0x60fdb4,0x61039a,0x4aac50)})
        report['errors']=[value.decode('ascii',errors='replace') if isinstance(value,bytes) else value for value in f.errors]
        check(result==1 and f.position==len(data) and (pattern=='legacy' or not f.errors),'complete native cross reader')
        state=[p.uint(obj+a) if n==4 else p.mu.mem_read(obj+a,1)[0] for a,n in ((0x18,4),(0x1c,1),(0x20,4),(0x24,1),(0x28,4),(0x2c,4),(0x44,4),(0x48,4))]
        mips=[]
        for rec in f.levels:
            packed=b''.join(bytes(p.mu.mem_read(rec['pixels']+y*rec['pitch'],rec['row_bytes'])) for y in range(rec['rows']))
            check(all(bytes(p.mu.mem_read(rec['pixels']+y*rec['pitch']+rec['row_bytes'],4))==b'\xa5'*4 for y in range(rec['rows'])),'pitch padding unchanged')
            mips.append(packed.hex())
        if pattern=='legacy':check(not mips and not state[3],'legacy section is skipped without initializing a texture')
        else:check(state[3]==1 and len(mips)==len(f.levels)>0,'native initialized full texture chain')
        report.update(state=state,mips=mips,events=f.events,coefficientTables=coefficients,codecRows=codec_rows,phase='cleanup')
        f.call(0x4abb50,this=obj,args=(1,));f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,));f.call(0x4228a0,this=manager)
        for address in (0x75db78,0x75526c,0x755264):
            owned=p.uint(address)
            if owned:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
        check(set(f.allocations)==set(f.freed),'all native allocations released')
        check(f.device_refs==1 and f.texture_refs==f.surface_refs==0 and not f.locked,'all COM refs released')
        check(all(rec['refs']==0 and not rec['locked'] for rec in f.levels),'all mip surfaces released')
        report.update(status='passed',nativeAssertions=checks,releasedAllocations=len(f.freed))
    except (AssertionError,ValueError) as error:
        report.update(status='stopped',error=str(error),stoppedIp=f'{p.reg("EIP"):08X}',cursor=f.position,instructions=sum(p.visits.values()))
        raise
    finally:
        report.update(arenaReservedBytes=p.allocated,elapsedSeconds=time.monotonic()-at)
        suffix='-common' if reader=='common' else ''
        target=ROOT/f'local-data/results/cycle-20260908-0700/cp115-cross-{fmt}-{shape}-{pattern}-{label}{suffix}.json'
        target.write_text(json.dumps(report,indent=2)+'\n')
        print('CAPTURE',json.dumps({k:v for k,v in report.items() if k not in ('mips','events','coefficientTables','codecRows')}),flush=True)
    return report if return_capture else 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(int(sys.argv[2]),*sys.argv[3:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
