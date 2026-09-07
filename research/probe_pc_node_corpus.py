#!/usr/bin/env python3
"""Original outer422940 on tiny unchanged Node-only SMOs; NOT whole422B50.

Separate completed index/outer calls establish a bounded composition. A prior
whole constructed Node FFPS scout capped; this does not resume/retry that call.
"""
from pathlib import Path
import hashlib,json,struct,sys
from pc_instruction_emulator import ROOT,run_bounded
from pc_loader_fixtures import PCFileBytesFixture,empty_manager,empty_fat
from probe_pc_node_relationships import node_rtti,CLASS
from probe_pc_node_serializer import check
from probe_pc_san_reader import cstring
import probe_pc_node_serializer as counters

ASSETS={
    'object.smo':'6044809A6448DC5F960DE7FF7609B156DC0BA8165C4B50CDD6DFE4596575D1C2',
    'gameover.smo':'593DDE72EAFC36532B4976B5269EB53D0B3FAC217AB9B97472C6B8C1DBEBE2AA',
}

def main(name,return_capture=False,scene_ready=False):
    if name not in ASSETS:raise ValueError('explicit tiny unchanged Node-only corpus')
    if name=='gameover.smo':
        raise ValueError('Native outer gameover with actual initialized SceneManager capped100k at89A355 during third Node factory, cursor254; disabled, do not retry/resume. Portable load is separate evidence.')
    raw=(ROOT/'local-data/pc-pristine/Media/Menus'/name).read_bytes()
    check(len(raw)<=512 and hashlib.sha256(raw).hexdigest().upper()==ASSETS[name],'unchanged bounded corpus SHA256')
    origin=struct.unpack_from('<I',raw,20)[0]
    f=PCFileBytesFixture(raw);p=f.p;node_rtti(f);f.call(0x6d38e0)
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat)
    p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x4638f0);f.call(0x422d90,this=manager,args=(CLASS,serializer,0xff,3))
    if scene_ready:
        # Declared runtime input: an actual initialized empty SceneManager.
        # Its independently proven constructor executes normally, not a seam
        # for Node attach or a resumed whole-loader frame.
        scene_manager=f.call(0x45adf0)
        check(p.uint(0x75db90)==scene_manager and p.uint(scene_manager+0x1c)==0,'explicit actual empty scene-manager startup')
    # Explicit valid stream position immediately following the separate header.
    f.position=28
    check(f.call(0x466b90,this=fat,args=(f.stream,))&255==1,'actual unchanged resource index')
    check(f.call(0x465cd0,this=fat,args=(f.stream,))&255==1 and f.position==origin,'actual empty file index and expected data boundary')
    p.put_uint(f.stream+0x14,origin)
    try:root=f.call(0x422940,this=manager,args=(f.stream,))
    except AssertionError:
        entries=(0x422940,0x4586b0,0x467550,0x421e20,0x463a70,0x4678b0,0x421a60,0x45adf0,0x458cb0,0x4130f0)
        print('BOUNDED STOP',name,'executed',sum(p.visits.values()),'visited',[(hex(a),p.visits[a]) for a in entries],
              'heap',p.allocated,'cursor',f.position,'errors',f.errors,flush=True)
        raise
    print('CORPUS OUTER',name,'scene-ready',scene_ready,'root',hex(root),'instructions',sum(p.visits.values()),'heap',p.allocated,'position',f.position,'errors',f.errors,flush=True)
    check(root in f.allocations and p.uint(root)==0x6dc4f4 and not f.errors,'actual original outer returns Node graph')
    check(f.position==len(raw),'all unchanged graph bytes consumed')
    rows=[];nodes=[]
    def visit(node):
        check(len(rows)<32 and node not in nodes,'bounded acyclic Node corpus graph')
        index=len(rows);nodes.append(node)
        name_entry=p.uint(node+0x10);label=cstring(p,name_entry+9).decode('latin1') if name_entry else None
        state=b''.join(bytes(p.mu.mem_read(node+offset,count*4)) for offset,count in
                       ((0x20,3),(0x30,3),(0x40,9),(0x74,3),(0x80,3),(0x8c,9)))
        row=[label,p.uint(node+0xb0),state.hex(),[]];rows.append(row)
        head=p.uint(node+0x18);at=p.uint(head)
        while at!=head:
            child=p.uint(at+8);check(p.uint(child+0x2c)==node,'reciprocal native parent')
            row[3].append(visit(child));at=p.uint(at)
        return index
    visit(root)
    check(len(rows)==(2 if name=='object.smo' else 6),'all corpus nodes materialized')
    check(rows[0][0]=='Scene Root','original outer applies root FAT name after payload')
    f.call(0x466760,this=fat);f.call(0x422220,this=root,args=(1,))
    check(all(node in f.freed for node in nodes),'native root teardown releases full corpus tree')
    f.call(0x4228a0,this=manager)
    for address in (0x75db90,0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    check(set(f.allocations)==set(f.freed),'all actual corpus/helper allocations released')
    print('NODE_CORPUS',json.dumps(rows,separators=(',',':')))
    print(f'PASS {counters.checks}/{counters.checks}: actual outer Node corpus {name}; independently staged, not whole loader/GPU')
    return rows if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2],scene_ready='--scene-ready' in sys.argv[3:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
