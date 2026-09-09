#!/usr/bin/env python3
"""Bounded actual PC LensFlare factory/reader observations, no GPU flare pass."""
from pathlib import Path
import json,sys,struct
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_navigation_readers import initialize,cleanup
from probe_pc_material_links import material_rtti
from probe_pc_node_serializer import field

def main(mode='factory'):
    if mode not in ('factory','empty','scalars','counted','links','resize'):raise ValueError('Declared tiny LensFlare case only')
    data=b'\0\0'
    if mode=='scalars':data=b'\0'+field(2,struct.pack('<II',0xbf800000,0x7fc12345))+field(3,bytes(4))+field(9,b'skip')+b'\0'
    if mode=='counted':data=b'\0'+field(1,struct.pack('<I8I',2,0,0xaabbccdd,0x3f000000,0x40000000,0,0x11223344,0xbf800000,0x40400000))+b'\0'
    f=PCWriteBytesFixture(data);p=f.p;initialize(f)
    rows=json.loads((Path(__file__).resolve().parents[1]/'local-data/results/tools-core-cycle-20260909-1900/lens-flare/registration.json').read_text(encoding='utf8'))
    for row in rows:
        address=row['registration_object_va'];p.put_uint(address,row['class_hash']);p.put_uint(address+0x48,row['base_registration_va'])
    obj=f.call(0x4d7bd0);serializer=f.call(0x4d7e90)
    material=0
    if mode in ('links','resize'):
        material_rtti(f);material=f.call(0x4a9460);p.put_uint(material+8,(p.uint(material+8)&0xffff0000)|1)
        f.data=struct.pack('<IIHIII',1,7,0,0x6160348b,0,0);f.position=0
        assert f.call(0x466b90,this=f.navigation_fat,args=(f.stream,))&255
        p.put_uint(f.call(0x4664c0,this=f.navigation_fat,args=(7,))+0x20,material)
        record=struct.pack('<IIIII',7,0,0xaabbccdd,0x3f000000,0x40000000)
        data=b'\0'+field(0,record)+field(0,struct.pack('<4I',0,0x12345678,0xbf800000,0x7fc12345))+field(1,struct.pack('<I',2)+record+record)+b'\0'
        if mode=='resize':
            empty=struct.pack('<4I',0,0x11223344,0xbf000000,0x40400000)
            data=data[:-1]+field(1,struct.pack('<I',1)+empty)+field(1,struct.pack('<I',3)+empty+record+empty)+b'\0'
        f.data=data;f.position=0
    captured={'object_size':f.allocations[obj],'object_vtable':f'{p.uint(obj):08X}',
        'object_words':[f'{p.uint(obj+i):08X}' for i in range(0,f.allocations[obj]//4*4,4)],
        'serializer_size':f.allocations[serializer],'serializer_vtable':f'{p.uint(serializer):08X}',
        'reader_vtable':f'{p.uint(serializer+16):08X}',
        'reader_slots':[f'{p.uint(p.uint(serializer+16)+i):08X}' for i in range(0,24,4)],
        'object_slots':[f'{p.uint(p.uint(obj)+i):08X}' for i in range(0,100,4)]}
    print('LENS_FACTORY_CAPTURE',json.dumps(captured),flush=True)
    if mode=='factory':
        raise RuntimeError('LensFlare destructor4D7BB0 already capped in factory attempt1; never retry it')
    result=f.call(0x4d9010,this=serializer+16,args=(f.stream,obj))&255
    begin,end=p.uint(obj+0xbc),p.uint(obj+0xc0);assert 0<=end-begin<=4*0x58
    state={'occlusion_bits':[p.uint(obj+0xd8),p.uint(obj+0xdc)],'render_node':p.uint(obj+0xe8),
        'primary':{'material':p.uint(obj+0x74),'tail':[p.uint(obj+offset) for offset in (0xb4,0xac,0xb0)]},
        'counted':[{'material':p.uint(at+0x14),'tail':[p.uint(at+offset) for offset in (0x54,0x4c,0x50)]} for at in range(begin,end,0x58)]}
    if material:
        for value in [state['primary'],*state['counted']]:
            assert value['material'] in (0,material);value['material']=7 if value['material'] else 0
        state['material_refs']=p.uint(material+8)&65535
    report={'mode':mode,'wire':data.hex(),'result':result,'position':f.position,'state':state,'arena_bytes':p.allocated,
        'errors':f.errors,'cleanup':'No target destructor claim: its protected entry capped earlier; this fresh guest process releases the arena.'}
    print('LENS_READER_CAPTURE',json.dumps(report),flush=True)
    assert result==1 and f.position==len(data) and not f.errors,'original bounded LensFlare read completed'
    print('PASS original LensFlare reader',mode,flush=True)
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
