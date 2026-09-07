"""Explicit PC texture device refcount boundary; no real DirectX or GPU."""
from pc_serializer_fixtures import PCWriteBytesFixture

class TextureDeviceFixture(PCWriteBytesFixture):
    def __init__(self,data=b'',renderer_size=0xc9ec):
        super().__init__(data);p=self.p
        if renderer_size not in (0xc9ec,0xca20):raise ValueError('explicit texture/palette renderer storage only')
        # Only storage actually read by4AAEB0: renderer.C9E8. Not ctor/startup.
        self.renderer=p.allocate(renderer_size);self.device=p.allocate(4);table=p.allocate(12)
        p.put_uint(self.device,table);p.put_uint(self.renderer+0xc9e8,self.device);p.put_uint(0x75db68,self.renderer)
        self.device_refs=1;self.device_events=[]
        p.mu.mem_map(0x34070000,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
        p.mu.mem_write(0x34070000,b'\xc3'*0x1000)
        p.put_uint(table+4,0x34070010);p.put_uint(table+8,0x34070020)
        p.seams[0x34070010]=self.addref_device;p.seams[0x34070020]=self.release_device

    def addref_device(self,p):
        if p.uint(p.reg('ESP')+4)!=self.device or not 0<self.device_refs<64:raise AssertionError('explicit live device AddRef')
        self.device_refs+=1;self.device_events.append('AddRef');p.fixture_return(4,eax=self.device_refs)

    def release_device(self,p):
        if p.uint(p.reg('ESP')+4)!=self.device or self.device_refs<=1:raise AssertionError('release only original acquired device ref')
        self.device_refs-=1;self.device_events.append('Release');p.fixture_return(4,eax=self.device_refs)


class TextureUploadBoundaryFixture(TextureDeviceFixture):
    """Declared tiny COM storage for observing engine calls, never pixel conversion.

    No seam for internal60FDB4/61039A. Upload probes stop BEFORE either body;
    successful complete calls are limited to separately observed size/lifetime.
    """
    def __init__(self,data=b'',renderer_size=0xc9ec):
        super().__init__(data,renderer_size=renderer_size);p=self.p
        device_table=p.allocate(0x60);p.put_uint(self.device,device_table)
        self.texture=p.allocate(4);texture_table=p.allocate(0x4c);p.put_uint(self.texture,texture_table)
        self.surface=p.allocate(4);surface_table=p.allocate(0x3c);p.put_uint(self.surface,surface_table)
        self.pixel_storage=p.allocate(256)
        self.texture_refs=0;self.surface_refs=0;self.locked=False;self.events=[]
        self.width=1;self.height=1;self.format=0x15;self.pitch=4
        self.create_result=0
        entries=[(device_table+4,0x10,self.addref_device),(device_table+8,0x20,self.release_device),
            (device_table+0x5c,0x30,self.create_texture),
            (texture_table+4,0x40,self.addref_texture),(texture_table+8,0x50,self.release_texture),
            (texture_table+0x34,0x60,self.get_level_count),(texture_table+0x48,0x70,self.get_surface),
            (surface_table+8,0x80,self.release_surface),(surface_table+0x30,0x90,self.get_desc),
            (surface_table+0x34,0xa0,self.lock_surface),(surface_table+0x38,0xb0,self.unlock_surface)]
        for slot,offset,callback in entries:
            p.put_uint(slot,0x34070000+offset);p.seams[0x34070000+offset]=callback

    def args(self,p,count):return [p.uint(p.reg('ESP')+4+i*4) for i in range(count)]
    def require_surface(self,address):
        if address!=self.surface or self.surface_refs<1:raise AssertionError('explicit live surface')
    def require_texture(self,address):
        if address!=self.texture or self.texture_refs<1:raise AssertionError('explicit live texture')

    def create_texture(self,p):
        args=self.args(p,9);device,width,height,levels,usage,fmt,pool,out,shared=args
        if device!=self.device or not(0<width<=8 and 0<height<=8) or self.texture_refs:
            raise AssertionError('tiny declared CreateTexture boundary')
        self.events.append(['CreateTexture',width,height,levels,usage,fmt,pool,shared])
        self.width=width;self.height=height;self.format=fmt
        if not self.create_result:
            self.texture_refs=1;self.surface_refs=1;p.put_uint(out,self.texture)
        else:p.put_uint(out,0)
        p.fixture_return(36,eax=self.create_result)

    def addref_texture(self,p):
        self.require_texture(self.args(p,1)[0]);self.texture_refs+=1
        self.events.append(['AddRefTexture']);p.fixture_return(4,eax=self.texture_refs)
    def release_texture(self,p):
        self.require_texture(self.args(p,1)[0]);self.texture_refs-=1
        if not self.texture_refs:self.surface_refs-=1
        self.events.append(['ReleaseTexture']);p.fixture_return(4,eax=self.texture_refs)
    def get_level_count(self,p):
        self.require_texture(self.args(p,1)[0]);self.events.append(['GetLevelCount',1]);p.fixture_return(4,eax=1)
    def get_surface(self,p):
        texture,level,out=self.args(p,3);self.require_texture(texture)
        if level!=0:raise AssertionError('only declared one-level texture')
        self.surface_refs+=1;p.put_uint(out,self.surface)
        self.events.append(['GetSurfaceLevel',level]);p.fixture_return(12,eax=0)
    def release_surface(self,p):
        self.require_surface(self.args(p,1)[0])
        if self.surface_refs<=1:raise AssertionError('cannot release texture-owned level')
        self.surface_refs-=1;self.events.append(['ReleaseSurface']);p.fixture_return(4,eax=self.surface_refs)
    def get_desc(self,p):
        surface,out=self.args(p,2);self.require_surface(surface)
        for index,value in enumerate((self.format,1,0,1,0,0,self.width,self.height)):p.put_uint(out+index*4,value)
        self.events.append(['GetDesc',self.format,self.width,self.height]);p.fixture_return(8,eax=0)
    def lock_surface(self,p):
        surface,out,rect,flags=self.args(p,4);self.require_surface(surface)
        if self.locked or rect or flags not in (0,0x10):raise AssertionError('only declared whole-surface size/read or native write lock')
        self.locked=True;p.put_uint(out,self.pitch);p.put_uint(out+4,self.pixel_storage)
        self.events.append(['LockRect',self.pitch] if not flags else ['LockRect',self.pitch,flags]);p.fixture_return(16,eax=0)
    def unlock_surface(self,p):
        self.require_surface(self.args(p,1)[0])
        if not self.locked:raise AssertionError('balanced surface lock')
        self.locked=False;self.events.append(['UnlockRect']);p.fixture_return(4,eax=0)
