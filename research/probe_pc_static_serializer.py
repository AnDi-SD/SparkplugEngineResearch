#!/usr/bin/env python3
"""Bounded original StaticRenderObject scalar sections; no renderer/device startup."""
from pathlib import Path
import hashlib,json,struct,sys
from pc_instruction_emulator import ROOT,run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_node_serializer import field

IDENTITY = (1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.)

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def setup(wire):
    f=PCWriteBytesFixture(wire);p=f.p
    # Explicit renderer cache storage only, as in the constructor scout. The
    # original support dtor observes this cache; no renderer factory is invoked.
    renderer=p.allocate(0xca00);p.put_uint(0x75db68,renderer)
    f.call(0x6d38c0)
    return f,f.call(0x41a7c0),f.call(0x44fce0)

def cases():
    affine=(*IDENTITY[:12],7.,8.,9.,1.)
    independent=(*IDENTITY[:12],-1.,-2.,-3.,1.)
    nonaffine=tuple(float(i-7) for i in range(16))
    return [('defaults',[]),('world-only',[(1,affine)]),
        ('independent-inverse',[(1,affine),(2,independent)]),
        ('non-affine',[(2,nonaffine),(1,independent)]),
        ('unknown-repeated',[(2,affine),(1,affine),(9,b'\xaa'),(2,independent),(1,nonaffine)])]

def main(output):
    reports=[]
    for name,items in cases():
        wire=b'';expected=[IDENTITY,IDENTITY]
        for identity,value in items:
            payload=value if isinstance(value,bytes) else struct.pack('<16f',*value)
            wire+=field(identity,payload)
            if identity in (1,2):expected[identity-1]=value
        wire+=b'\0';f,obj,serializer=setup(wire);p=f.p
        assert f.call(0x44fe90,this=serializer+0x10,args=(f.stream,obj))&255
        assert f.position==len(wire) and not f.errors
        actual=bytes(p.mu.mem_read(obj+0x8c,128))
        assert actual==struct.pack('<32f',*expected[0],*expected[1])
        assert p.uint(obj+0x1c)==0
        read_instructions=sum(p.visits.values());read_io=list(f.io)
        f.position=0;f.data=b'';f.io.clear();f.write_calls=0
        assert f.call(0x450140,this=serializer+0x10,args=(f.stream,obj))&255
        assert not f.errors
        written=f.data;write_instructions=sum(p.visits.values());write_io=list(f.io)
        # With no renderables the original still emits both matrices in1/2 order.
        expected_wire=field(1,actual[:64])+field(2,actual[64:])+b'\0'
        assert written==expected_wire,(name,written.hex(),expected_wire.hex())
        for value in (obj,serializer):f.call(p.uint(p.uint(value)),this=value,args=(1,))
        assert set(f.allocations)==set(f.freed)
        reports.append({'name':name,'input_hex':wire.hex(),'matrices_hex':actual.hex(),
            'output_hex':written.hex(),'read_instructions':read_instructions,
            'write_instructions':write_instructions,'read_io':read_io,'write_io':write_io,
            'released_allocations':len(f.freed),'arena_bytes':p.allocated})
    report={'status':'passed','pc_exe_sha256':sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),
        'cases':reports,'profile':'micro','guest_arena_bytes':65536,
        'scope':'Original factories, full scalar section reader and zero-renderable writer, exact matrices, teardown. No reference resolution, scene, GPU or malformed-input policy claim.',
        'dependencies':sorted(({'path':str(Path(m.__file__).resolve().relative_to(ROOT)).replace('\\','/'),'sha256':sha(m.__file__)}
            for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda x:x['path'])}
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS StaticRenderObject: five original scalar readers/writers, all owners released');return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
