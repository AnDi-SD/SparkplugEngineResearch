"""Existing texture COM contract on the same device, with external page storage.

Only interface tables and the declared256-byte COM surface are outside engine
arena. Original TextureData/serializer/DXTexture allocations remain inside it.
"""
from pc_texture_fixtures import TextureUploadBoundaryFixture
def install_texture_device(f,row_bytes):
 p=f.p;base=0x34070000;p.mu.mem_map(base,0x1000,p.uc.UC_PROT_ALL)
 io=object.__new__(TextureUploadBoundaryFixture);io.p=p;io.renderer=f.renderer;io.device=f.device
 io.texture=base+0x400;io.surface=base+0x450;texture_table=base+0x500;surface_table=base+0x600
 p.put_uint(io.texture,texture_table);p.put_uint(io.surface,surface_table)
 io.pixel_storage=base+0x800;p.mu.mem_write(io.pixel_storage,b'\xa5'*256)
 io.device_refs=1;io.device_events=[];io.texture_refs=0;io.surface_refs=0;io.locked=False;io.events=[]
 io.width=io.height=1;io.format=0x15;io.pitch=row_bytes+4;io.create_result=0
 device_table=p.uint(f.device)
 entries=[(device_table+4,0x10,io.addref_device),(device_table+8,0x20,io.release_device),(device_table+0x5c,0x30,io.create_texture),
          (texture_table+4,0x40,io.addref_texture),(texture_table+8,0x50,io.release_texture),(texture_table+0x34,0x60,io.get_level_count),
          (texture_table+0x48,0x70,io.get_surface),(surface_table+8,0x80,io.release_surface),(surface_table+0x30,0x90,io.get_desc),
          (surface_table+0x34,0xa0,io.lock_surface),(surface_table+0x38,0xb0,io.unlock_surface)]
 for slot,offset,callback in entries:p.put_uint(slot,base+offset);p.seams[base+offset]=callback
 f.texture_io=io
