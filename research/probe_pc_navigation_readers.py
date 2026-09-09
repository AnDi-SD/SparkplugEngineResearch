#!/usr/bin/env python3
"""Bounded original navigation factories/readers; no route or scene scheduler."""
from pathlib import Path
import sys,json,struct
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_manager,empty_fat
from probe_pc_node_relationships import node_rtti
from probe_pc_node_serializer import field

SPECS={'graph':(0x41a6a0,0x444820,3),'set':(0x41a700,0x448eb0,3),'portal':(0x41b2e0,0x446720,2)}
def wire_for(kind,mode):
    if mode=='empty':return bytes(SPECS[kind][2])
    if kind=='graph' and mode=='resize':
        return b'\0\0'+field(2,struct.pack('<I',2))+field(3,struct.pack('<IIBB2B',0,1,7,1,4,9))+field(2,struct.pack('<I',3))+field(2,struct.pack('<I',2))+b'\0'
    if mode!='values':raise ValueError('explicit scalar navigation input')
    if kind=='graph':
        return b'\0\0'+field(2,struct.pack('<I',2))+field(3,struct.pack('<IIBB4B',0,1,0x81,2,7,0xa4,9,0xfa))+field(3,struct.pack('<IIBB',1,0,255,0))+field(9,b'skip')+b'\0'
    if kind=='portal':return b'\0'+field(2,bytes([5,7]))+field(2,bytes([5,7]))+field(2,bytes([255,128]))+field(3,bytes([2,1,254]))+field(3,bytes([0,2,127]))+field(9,b'skip')+b'\0'
    values=[0,1,2,3,4,7,0xffffffff,0x100,0x102]
    matrix=field(1,struct.pack('<II9I',3,3,*values))
    portal_matrix=field(2,struct.pack('<II3I',1,3,3,2,1))
    links=struct.pack('<IBI3BBI',2,0,3,1,1,2,2,0)
    return b'\0'+field(0,struct.pack('<I',3))+matrix+portal_matrix+field(3,links)+field(5,b'\x7f')+field(9,b'skip')+b'\0\0'

def state_for(f,kind,obj):
    p=f.p
    def bytes_vector(offset):
        begin,end=p.uint(obj+offset+4),p.uint(obj+offset+8)
        assert 0<=end-begin<=1024
        return list(p.mu.mem_read(begin,end-begin)) if end>begin else []
    if kind=='graph':
        begin,end=p.uint(obj+0x1fc),p.uint(obj+0x200);rows=[]
        for row in range(begin,end,16):
            cells=[]
            for cell in range(p.uint(row+4),p.uint(row+8),8):
                count=bytes(p.mu.mem_read(cell+4,1))[0];next_portal=bytes(p.mu.mem_read(cell+5,1))[0]
                alts=bytes(p.mu.mem_read(p.uint(cell),count*2)).hex() if count else ''
                cells.append([next_portal,alts])
            rows.append(cells)
        return {'paths':rows}
    if kind=='portal':return {'first_nodes':bytes_vector(0xc0),'second_nodes':bytes_vector(0xd0),'paths':bytes_vector(0xe0),'enabled_index':bytes(p.mu.mem_read(obj+0xf0,2)).hex()}
    def matrix(offset):
        pointer=p.uint(obj+offset)
        if not pointer:return None
        rows,cols,stride=(p.uint(pointer+v) for v in (4,8,12))
        assert rows*cols<=256
        return {'rows':rows,'columns':cols,'stride':stride,'packed':bytes(p.mu.mem_read(p.uint(pointer+16),rows*stride)).hex(),
            'values':[f.call(0x447c60,this=pointer,args=(row,col)) for row in range(rows) for col in range(cols)]}
    count=p.uint(obj+0xb4);assert count<=256
    neighbours=[]
    for index in range(count):
        degree=bytes(p.mu.mem_read(p.uint(obj+0xc4)+index,1))[0]
        neighbours.append(list(p.mu.mem_read(p.uint(p.uint(obj+0xc0)+index*4),degree)) if degree else [])
    return {'node_count':count,'transitions':matrix(0xb8),'portal_transitions':matrix(0xbc),'neighbours':neighbours,'index_enabled':bytes(p.mu.mem_read(obj+0xe0,2)).hex()}

def initialize(f):
    p=f.p;f.call(0x6d38e0);node_rtti(f)
    for entry in (0x6d31e0,0x6d3200,0x6d3230):f.call(entry)
    rows=json.loads((Path(__file__).resolve().parents[1]/'local-data/results/tools-core-cycle-20260909-1900/navigation-readers/registration.json').read_text(encoding='utf-8'))
    for row in rows:
        address=row['registration_object_va'];p.put_uint(address,row['class_hash']);p.put_uint(address+0x48,row['base_registration_va'])
    p.put_uint(0x75e150,0x603625d0);p.put_uint(0x75e150+0x48,0x75dd88)
    p.put_uint(0x75db68,p.allocate(0xc9c8))
    manager,_=empty_manager(f);fat=empty_fat(f);p.put_uint(manager+0x28,fat)
    p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(manager+0x18,2);p.put_uint(0x75dde8,manager)
    f.navigation_manager=manager;f.navigation_fat=fat
    return rows

def cleanup(f,objects):
    p=f.p;f.call(0x466760,this=f.navigation_fat)
    for obj in objects:
        if obj not in f.freed:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    f.call(0x4228a0,this=f.navigation_manager)
    for address in (0x75db90,0x75db6c,0x75db78,0x75526c,0x755264):
        owned=p.uint(address)
        if owned and owned not in f.freed:f.call(p.uint(p.uint(owned)),this=owned,args=(1,))
    if set(f.allocations)!=set(f.freed):
        print('NAV_REMAINING',[(hex(address),size,hex(p.uint(address))) for address,size in f.allocations.items() if address not in f.freed],flush=True)
    assert set(f.allocations)==set(f.freed),'all actual navigation/reader/global allocations freed'

def main(kind,mode='empty'):
    factory,serializer_factory,sections=SPECS[kind]
    f=PCWriteBytesFixture(wire_for(kind,mode));p=f.p;initialize(f)
    obj=f.call(factory);size=f.allocations[obj]
    own=0x1d4 if kind=='graph' else 0xb4
    initial=[f'{p.uint(obj+i):08X}' for i in range(own,size-size%4,4)]
    print('NAV_FACTORY',kind,'size',size,'vtable',hex(p.uint(obj)),'own',initial,flush=True)
    serializer=f.call(serializer_factory);table=p.uint(serializer+0x10);reader=p.uint(table+8)
    print('NAV_READER',kind,hex(reader),'serializer-table',hex(p.uint(serializer)),flush=True)
    result=f.call(reader,this=serializer+0x10,args=(f.stream,obj))&255
    captured={'kind':kind,'mode':mode,'wire':f.data.hex(),'result':result,'position':f.position,'reader':reader,'size':size,'initial':initial,
        'final':[f'{p.uint(obj+i):08X}' for i in range(own,size-size%4,4)],'arena':p.allocated,'state':state_for(f,kind,obj)}
    print('NAV_CAPTURE',json.dumps(captured),flush=True)
    assert result==1 and f.position==len(f.data) and not f.errors,'native empty sections success'
    cleanup(f,[obj,serializer])
    print('PASS navigation empty factory/read/destructor',kind,flush=True);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
