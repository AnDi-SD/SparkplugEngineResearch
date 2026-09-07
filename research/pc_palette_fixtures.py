"""Tiny declared palette free-index list + COM SetPaletteEntries, no native API seam.

Original renderer4BB7D0/4BB840 and texture4B93C0 remain original instructions.
Only renderer fields actually consumed and empty std::list sentinel are supplied;
this is not renderer construction or GPU initialization.
"""
from pc_texture_fixtures import TextureUploadBoundaryFixture

class PaletteFixture(TextureUploadBoundaryFixture):
    def __init__(self,data=b''):
        super().__init__(data,renderer_size=0xca20);p=self.p
        old=p.uint(self.device);table=p.allocate(0x120)
        p.mu.mem_write(table,bytes(p.mu.mem_read(old,0x60)));p.put_uint(self.device,table)
        p.put_uint(table+0x11c,0x340700c0);p.seams[0x340700c0]=self.set_palette_entries
        self.palette_events=[];self.palette_result=0
        p.put_uint(self.renderer,0x6efa40);p.put_uint(self.renderer+0xca10,0)
        self.palette_root=p.allocate(12);p.put_uint(self.palette_root,self.palette_root);p.put_uint(self.palette_root+4,self.palette_root)
        p.put_uint(self.renderer+0xca18,self.palette_root);p.put_uint(self.renderer+0xca1c,0)
        # Debug allocator is the same explicit bounded storage boundary.
        p.seams[0x4123f0]=self.allocate

    def set_palette_entries(self,p):
        device,index,entries=self.args(p,3)
        if device!=self.device or index>=8:raise AssertionError('explicit tiny palette upload')
        palette=entries-0x14
        if self.allocations.get(palette)!=0x414 or palette in self.freed:raise AssertionError('live original palette source')
        self.palette_events.append([index,bytes(p.mu.mem_read(entries,1024)).hex()])
        p.fixture_return(12,eax=self.palette_result)
