"""Explicit D3D9 caps input and read-only particle capture; no engine replacements."""
import struct

REGION_SIZES={0:0,1:3,2:6,3:4,4:8,5:4,6:5,7:6}

def install_particle_caps(f,supported=False):
    p=f.p
    # Original spDXRenderer secondary table, not a replacement capability method.
    p.put_uint(f.renderer+0x18,0x6f28a0)
    p.put_uint(p.uint(f.device)+0x1c,0x340600d0)
    f.particle_caps_calls=0
    def caps(p):
        sp=p.reg('ESP');assert p.uint(sp+4)==f.device
        output=bytearray(0x130)
        struct.pack_into('<I',output,0x8c,0x100000 if supported else 0)
        p.mu.mem_write(p.uint(sp+8),bytes(output));f.particle_caps_calls+=1
        p.fixture_return(8,eax=0)
    p.seams[0x340600d0]=caps

def particle_state(f,obj):
    p=f.p
    def read(offset,size):return bytes(p.mu.mem_read(obj+offset,size))
    tag=p.uint(obj+0x70);assert tag in REGION_SIZES
    state=b''.join(read(a,n) for a,n in ((0x18,1),(0x1c,4),(0xac,3),(0xb4,4),(0xb8,8),
        (0xc4,8),(0xcc,24),(0xe4,12),(0xf0,8),(0xf8,8),(0x100,8),(0x60,16),(0x70,4)))
    if tag:state+=bytes(p.mu.mem_read(p.uint(obj+0x74),REGION_SIZES[tag]*4))
    return state+read(0x98,20)
