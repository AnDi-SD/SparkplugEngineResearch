"""At most4 explicitly stored full mip levels; no GPU or conversion seams."""
from pc_texture_fixtures import TextureUploadBoundaryFixture

class TextureMipChainFixture(TextureUploadBoundaryFixture):
    def __init__(self,data=b''):
        super().__init__(data);self.levels=[];self.surfaces={}

    def create_texture(self,p):
        super().create_texture(p)
        if self.create_result:return
        width,height=self.width,self.height
        table=p.uint(self.surface);self.levels=[];self.surfaces={}
        while True:
            index=len(self.levels)
            if index>=4:raise AssertionError('four tiny full-chain levels')
            surface=self.surface if not index else p.allocate(4)
            pixels=self.pixel_storage if not index else p.allocate(256)
            p.put_uint(surface,table);p.mu.mem_write(pixels,b'\xa5'*256)
            compressed=self.format in (0x31545844,0x33545844,0x35545844)
            row_bytes=max(1,width>>2)*(8 if self.format==0x31545844 else 16) if compressed else width*{0x15:4,0x16:4,0x29:1,0x17:2,0x1a:2}[self.format]
            rows=max(1,height>>2) if compressed else height
            pitch=row_bytes+4
            if pitch*rows>256:raise AssertionError('tiny explicit per-surface storage')
            record=dict(index=index,width=width,height=height,row_bytes=row_bytes,rows=rows,pitch=pitch,
                pixels=pixels,refs=1,locked=False,surface=surface)
            self.levels.append(record);self.surfaces[surface]=record
            if width==height==1:break
            width=max(1,width>>1);height=max(1,height>>1)
        self.pitch=self.levels[0]['pitch']

    def get_level_count(self,p):
        self.require_texture(self.args(p,1)[0]);self.events.append(['GetLevelCount',len(self.levels)])
        p.fixture_return(4,eax=len(self.levels))
    def get_surface(self,p):
        texture,level,out=self.args(p,3);self.require_texture(texture)
        if not 0<=level<len(self.levels):raise AssertionError('surface level within declared full chain')
        rec=self.levels[level];rec['refs']+=1
        if not level:self.surface_refs=rec['refs']
        p.put_uint(out,rec['surface']);self.events.append(['GetSurfaceLevel',level]);p.fixture_return(12,eax=0)
    def require_surface(self,address):
        if address not in self.surfaces or self.surfaces[address]['refs']<1:raise AssertionError('explicit live mip surface')
    def release_surface(self,p):
        address=self.args(p,1)[0];self.require_surface(address);rec=self.surfaces[address]
        if rec['refs']<=1:raise AssertionError('texture retains each declared level')
        rec['refs']-=1
        if not rec['index']:self.surface_refs=rec['refs']
        self.events.append(['ReleaseSurface',rec['index']]);p.fixture_return(4,eax=rec['refs'])
    def release_texture(self,p):
        self.require_texture(self.args(p,1)[0]);self.texture_refs-=1
        if not self.texture_refs:
            for rec in self.levels:rec['refs']-=1
            self.surface_refs=self.levels[0]['refs']
        self.events.append(['ReleaseTexture']);p.fixture_return(4,eax=self.texture_refs)
    def get_desc(self,p):
        surface,out=self.args(p,2);self.require_surface(surface);rec=self.surfaces[surface]
        for index,value in enumerate((self.format,1,0,1,0,0,rec['width'],rec['height'])):p.put_uint(out+index*4,value)
        self.events.append(['GetDesc',self.format,rec['width'],rec['height']]);p.fixture_return(8,eax=0)
    def lock_surface(self,p):
        surface,out,rect,flags=self.args(p,4);self.require_surface(surface);rec=self.surfaces[surface]
        if rec['locked'] or rect or flags not in (0,0x10):raise AssertionError('bounded declared whole mip lock')
        rec['locked']=True;self.locked=True;p.put_uint(out,rec['pitch']);p.put_uint(out+4,rec['pixels'])
        self.events.append(['LockRect',rec['pitch'],flags,rec['index']]);p.fixture_return(16,eax=0)
    def unlock_surface(self,p):
        surface=self.args(p,1)[0];self.require_surface(surface);rec=self.surfaces[surface]
        if not rec['locked']:raise AssertionError('balanced mip lock')
        rec['locked']=False;self.locked=any(r['locked'] for r in self.levels)
        self.events.append(['UnlockRect',rec['index']]);p.fixture_return(4,eax=0)
