#!/usr/bin/env python3
"""Original readable Occlusion leaves on explicit internal geometry fixtures.

These prepared edges/faces are inputs, never a full Init or asset-load claim.
The previously capped Init/topology entry is not called. Allocation/deletion
uses the original object and edge destructors with bounded host heap seams.
"""
from pathlib import Path
import sys,json,struct
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_navigation_readers import initialize,cleanup
from pc_crt_format_fixtures import install_sprintf
from pc_crt_string_fixtures import install_crt_string


def main(mode):
    f=PCWriteBytesFixture();p=f.p;initialize(f);install_sprintf(p);install_crt_string(p)
    p.mu.mem_write(0x73ff60,b'\0') # explicit native console-output setting
    p.put_uint(0x760640,0x43d24430);p.put_uint(0x760688,0x75dd88)
    obj=f.call(0x470a70)
    if mode=='factory':
        state={'size':f.allocations[obj],'border_count':p.uint(obj+0xd4),'planar_word':p.uint(obj+0x160),'walk_stamp':p.uint(obj+0x164),'initialized_word':p.uint(obj+0x1b4)}
        state['empty_planarity_result']=f.call(0x46e3b0,this=obj)&255
        cleanup(f,[obj]);state['all_native_allocations_freed']=set(f.allocations)==set(f.freed)
        print('OCCLUSION_FACTORY_CAPTURE',json.dumps(state),flush=True);return 0
    # Explicit existing name-manager boundary gives diagnostics a valid %s.
    name=p.allocate(32);p.mu.mem_write(name,b'occlusion-leaf-fixture\0')
    f.call(0x4130f0,this=obj,args=(name,))
    def heap(size):
        p.run(0x4123d0,args=(size,),callee_pop=False)
        at=p.reg('EAX');p.mu.mem_write(at,bytes(size));return at
    def vector(offset,at,count,stride):
        for delta,word in ((4,at),(8,at+count*stride),(12,at+count*stride)):
            p.put_uint(obj+offset+delta,word)
    points=[(0.,0.,0.),(1.,0.,0.),(2.,0.,0.),(2.,1.,0.)]
    planes=[(0.,0.,1.,0.),(0.,0.,1.,7.)]
    specs=[(0,1,0,-1),(1,2,1,-1),(2,3,1,-1)]
    entry=0x46e820
    if mode.startswith('connect:'):
        points=[(0.,0.,0.),(1.,0.,0.),(1.,1.,0.),(1.,-1.,0.),(2.,0.,0.),(1.0005,0.,0.),(1.002,0.,0.)]
        planes=[(0.,0.,1.,0.),(0.,0.,1.,0.)];entry=0x4705a0
        variants={
            'open':[(0,1,0,-1)],'turn':[(0,1,0,-1),(1,2,0,-1)],
            'concave':[(0,1,0,-1),(1,3,0,-1)],'collinear':[(0,1,0,-1),(1,4,0,-1)],
            'cycle':[(0,1,0,-1),(1,2,0,-1),(2,0,0,-1)],'reverse':[(0,1,0,-1),(1,0,0,-1)],
            'internal':[(0,1,0,1),(1,3,0,-1)],'zero':[(0,1,0,-1),(1,1,0,-1)],
            'tiny':[(0,1,0,-1),(1,5,0,-1)],'near':[(0,1,0,-1),(5,2,0,-1)],
            'far':[(0,1,0,-1),(6,2,0,-1)],'partial':[(0,1,0,-1),(1,2,0,-1),(1,3,0,-1)]}
        specs=variants[mode.split(':')[1]]
    elif mode.startswith('convex'):
        planes=[(0.,1.,0.,0.),(0.,0.,float(mode.split(':')[1]),0.)]
        specs=[(0,1,0,1)];entry=0x46d670
    elif mode.startswith('link'):
        specs=[(0,1,0,-1),(1,0,1,-1)];entry=0x46e1a0
        if mode=='link:three':specs.append((1,0,1,-1))
        if mode=='link:concave':planes=[(0.,1.,0.,0.),(0.,0.,-1.,0.)]
    elif mode.startswith('remove'):
        specs=[(0,1,0,1),(1,0,1,0),(2,3,1,-1)];entry=0x46eb10
        if mode=='remove:normal':planes[1]=(0.,.01,1.,0.)
    elif mode.startswith('planar'):
        entry=0x46e3b0
        if mode=='planar:same':planes[1]=planes[0]
        if mode=='planar:near':planes[1]=(0.,0.,1.,.0005)
        # closed bypasses comparison even though plane distance differs.
    elif mode=='merge:turn':specs=[(0,1,0,-1),(1,3,1,-1)]
    elif mode=='merge:zero':specs=[(0,0,0,-1),(0,1,1,-1)]
    elif mode!='merge:chain':raise ValueError(mode)
    vertices=p.allocate(len(points)*12)
    p.put_floats(vertices,[v for point in points for v in point])
    faces=heap(len(planes)*32);vector(0xf8,faces,len(planes),32)
    for index,plane in enumerate(planes):p.put_floats(faces+index*32,plane)
    edges=[];edge_list=heap(len(specs)*4);vector(0xd8,edge_list,len(specs),4)
    for index,(start,end,own,opposite) in enumerate(specs):
        edge=heap(0x28);edges.append(edge);p.put_uint(edge_list+index*4,edge)
        for offset,value in ((0,vertices+start*12),(4,vertices+end*12),(8,faces+opposite*32 if opposite>=0 else 0),(12,faces+own*32)):
            p.put_uint(edge+offset,value)
        p.mu.mem_write(edge+0x24,b'\1')
    p.put_uint(obj+0xd4,0 if mode=='planar:closed' else len(specs))
    p.mu.mem_write(obj+0x160,b'\x7f') # declared prior state; failure must preserve it.
    result=f.call(entry,this=obj,args=(edges[0],) if mode.startswith(('convex','connect:')) else ())&255
    out_edges=[]
    for at in range(p.uint(obj+0xdc),p.uint(obj+0xe0),4):
        edge=p.uint(at)
        out_edges.append({'original_index':edges.index(edge),'start':(p.uint(edge)-vertices)//12,'end':(p.uint(edge+4)-vertices)//12,
            'own':(p.uint(edge+12)-faces)//32,'opposite':(p.uint(edge+8)-faces)//32 if p.uint(edge+8) else -1,'border':bytes(p.mu.mem_read(edge+0x24,1))[0]})
    report={'mode':mode,'entry':hex(entry),'result':result,'border_count':p.uint(obj+0xd4),'planar':bytes(p.mu.mem_read(obj+0x160,1))[0],
        'edges':out_edges,'deleted_edges':[i for i,edge in enumerate(edges) if edge in f.freed]}
    if mode.startswith('connect:'):
        report['outgoing']=[[edges.index(p.uint(at)) for at in range(p.uint(edge+0x14),p.uint(edge+0x18),4)] for edge in edges]
        report['repeat_result']=f.call(entry,this=obj,args=(edges[0],))&255
    cleanup(f,[obj]);report['all_native_allocations_freed']=set(f.allocations)==set(f.freed);report['arena_bytes']=p.allocated
    print('OCCLUSION_TOPOLOGY_CAPTURE',json.dumps(report),flush=True);return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
