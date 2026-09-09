#!/usr/bin/env python3
"""Bounded original spatial readers on actual resources and published FAT refs.

Explicit initialized FAT/manager containers, external reference owners and CPU
renderer cache storage. No replacement of ReadReference or per-field bodies.
This is not whole Scene/game startup; the direct resource loader is separate.
"""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from probe_pc_read_reference import ReadReferenceFixture
from probe_pc_node_serializer import field

SPECS={
 'partition':(0x67672341,0x426910,4502448,0x44b660),
 'bsp':(0x7362ab22,0x480b90,4509136,0x44ceb0),
 'system':(0x912cc341,0x48e7c0,4501008,0x44af40),
 'zone':(0x61254ab3,0x480fd0,4511296,0x44d7a0),
 'portal':(0x6523ac37,0x481370,4512960,0x44dde0),
 'portal-node':(0xabb5ab2c,0x481810,4515088,0x44e670),
 'payload':(0x94bbca2a,0x4cd950,4516784,0x44ecf0)}
REFS={7:(0x61254ab3,0x480fd0),9:(0x912cc341,0x48e7c0),
 11:(0x56d67170,0x41a7c0),13:(0x6523ac37,0x481370),15:(0x94bbca2a,0x4cd950),
 17:(0x67672341,0x426910),19:(0x67672341,0x426910),21:(0x763277db,0x479ed0),
 23:(0x763277db,0x479ed0),25:(0x47a97c0e,0x4653a0)}
PLANE=struct.pack('<4f',1,0,0,2)
POLYGON=struct.pack('<I9f',3,2,0,0,2,1,0,2,0,1)
def ref(identity):return struct.pack('<II',identity,0) if identity else bytes(4)
def wire_for(mode):
    kind,variant=mode.split(':')
    if variant=='empty':return b'\0'*(3 if kind=='system' else 2 if kind in ('bsp','zone','portal-node') else 1),()
    if variant!='values':raise ValueError('Explicit empty or values case required')
    if kind=='partition':
        items=[(1,struct.pack('<I',0x11223344)),(3,ref(7)),(5,ref(9)),(4,ref(13)),(4,ref(13)),
               (7,ref(11)),(7,ref(11)),(6,ref(15)),(6,ref(0)),(0,ref(25)),(0,ref(25)),(3,ref(0)),
               (3,ref(7)),(1,struct.pack('<I',0xaabbccdd)),(12,b'skip')]
        return b''.join(field(k,v) for k,v in items)+b'\0',(7,9,11,13,15,25)
    if kind=='bsp':return (field(2,struct.pack('<I',0)+ref(17))+field(2,struct.pack('<I',1)+ref(19))+b'\0'
        +field(1,POLYGON)+field(0,PLANE)+field(1,struct.pack('<I',0))+b'\0'),(17,19)
    if kind=='system':return b'\0\0'+field(0,ref(17))+field(0,ref(17))+b'\0',(17,)
    if kind=='zone':return b'\0'+b''.join(field(0,ref(i)) for i in (17,19,17))+b'\0',(17,19)
    if kind=='portal':return field(0,ref(7))+field(1,POLYGON)+field(2,b'\x7f')+field(0,ref(0))+b'\0',(7,)
    if kind=='portal-node':return b'\0'+field(0,ref(13))+field(0,ref(13))+b'\0',(13,)
    if kind=='payload':return (field(1,struct.pack('<I',0x11223344))+field(0,ref(21))
        +field(1,struct.pack('<I',0x55667788))+field(0,ref(23))+field(0,ref(21))
        +field(1,struct.pack('<I',0x99aabbcc))+b'\0'),(21,23)
    raise ValueError(kind)

def main(mode):
    wire,needed=wire_for(mode);kind=mode.split(':')[0]
    f=ReadReferenceFixture(b'',ids=tuple(REFS));p=f.p
    renderer=p.allocate(0xca00);p.put_uint(0x75db68,renderer)
    f.call(0x6d38c0);f.call(0x6d38e0)
    for record,identity,parent in ((0x7555f8,0x44de07fd,0x755310),(0x75dd88,0x695c0f65,0x7555f8),
        (0x75e1b8,0x67672341,0x755310),(0x7613f8,0x7362ab22,0x75e1b8),
        (0x761458,0x61254ab3,0x75dd88),(0x762730,0x912cc341,0x75dd88),
        (0x7614b8,0x6523ac37,0x7555f8),(0x761518,0xabb5ab2c,0x75dd88),
        (0x75db08,0x56d67170,0x7555f8),(0x765938,0x94bbca2a,0x755310),
        (0x7651f0,0x9cbb56a2,0x765938),(0x75e030,0x4fda4542,0x7555f8),(0x760cf8,0x763277db,0x75e030)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    identity,factory,serializer_factory,reader=SPECS[kind]
    target=f.call(factory);serializer=f.call(serializer_factory);f.objects.extend((target,serializer))
    refs={}
    for key in needed:
        type_id,create=REFS[key];obj=f.call(create);refs[key]=obj;f.objects.append(obj)
        p.put_uint(obj+8,(p.uint(obj+8)&0xffff0000)|1) # explicit external owner, not ctor default
        entry=f.call(0x4664c0,this=f.fat,args=(key,));p.put_uint(entry+0x10,type_id);p.put_uint(entry+0x20,obj)
    if 25 in refs:
        primitive=f.call(0x4879c0);f.objects.append(primitive)
        p.put_uint(primitive+8,2);p.put_uint(refs[25]+0x14,primitive) # decoded explicit primitive owner
    f.data=wire;f.position=0;f.io.clear()
    result=f.call(reader,this=serializer+0x10,args=(f.stream,target))&255
    assert result==1 and f.position==len(wire) and not f.errors,(mode,result,f.position,f.errors)
    def key(pointer):return 0 if not pointer else next(i for i,v in refs.items() if v==pointer)
    def ids(offset):
        begin,end=p.uint(target+offset+4),p.uint(target+offset+8)
        assert 0<=end-begin<=256 and (end-begin)%4==0
        return [key(p.uint(i)) for i in range(begin,end,4)]
    state={}
    if kind in ('partition','bsp'):
        state={'color':p.uint(target+0x50),'zone':key(p.uint(target+0x60)),
          'system':key(p.uint(target+0x74)),'payload':key(p.uint(target+0x78)),
          'portals':ids(0x64),'statics':ids(0x30),'collisions':ids(0x10),
          'children':[key(p.uint(p.uint(target+0x58)+i*4)) for i in range(p.uint(target+0x5c))]}
        if kind=='bsp':state.update(plane=bytes(p.mu.mem_read(target+0x84,16)).hex() if mode.endswith('values') else None,
                                    polygon_count=p.uint(target+0xa8))
        if state['payload']:f.objects.remove(refs[state['payload']])
        for child in state['children']:
            if child:f.objects.remove(refs[child]);assert p.uint(refs[child]+0x54)==target
        if 25 in refs:
            begin,end=p.uint(refs[25]+0x70),p.uint(refs[25]+0x74)
            assert [p.uint(i) for i in range(begin,end,4)]==[target,target]
    elif kind=='system':
        state={'root':key(p.uint(target+0x1d4)),'flags':p.uint(target+0xb0)}
        if state['root']:f.objects.remove(refs[state['root']])
    elif kind=='zone':state={'roots':ids(0xb4)}
    elif kind=='portal-node':state={'portals':ids(0xb4)}
    elif kind=='portal':state={'destination':key(p.uint(target+0x14)),'open':p.uint(target+0x20)&255,
        'polygon_count':p.uint(target+0x18),'plane':bytes(p.mu.mem_read(target+0x24,16)).hex() if mode.endswith('values') else None}
    elif kind=='payload':state={'color':p.uint(target+0x84),'renderables':ids(0x14),
        'model_colors':{str(k):p.uint(v+0x28) for k,v in refs.items()}}
    instructions=sum(p.visits.values());arena=p.allocated
    # Collision/world readers may instantiate the same extra bounded managers
    # already accounted for by probe_pc_collision_core.cleanup.
    f.call(0x466760,this=f.fat)
    for obj in dict.fromkeys(f.objects):f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    f.call(0x4228a0,this=f.manager)
    for slot in (0x75db90,0x75db6c,0x75db78,0x75526c,0x755264,0x74e060):
        obj=p.uint(slot)
        if obj and obj not in f.freed:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    assert set(f.allocations)==set(f.freed),{hex(k):v for k,v in f.allocations.items() if k not in f.freed}
    print('SPATIAL_CAPTURE '+json.dumps({'mode':mode,'wire':wire.hex(),'state':state,
        'read_instructions':instructions,'arena_bytes':arena,'freed_allocations':len(f.freed)}),flush=True)
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
