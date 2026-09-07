#!/usr/bin/env python3
"""Whole original Skin read/index/write with actual inline Node references.

Manager/FAT/RTTI are explicit valid startup containers. Stream/allocator/CRT
and diagnostic output are boundaries; no Skin/Model/Node/reference body seam.
"""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_fat,empty_manager
from probe_pc_node_relationships import node_rtti,CLASS
from probe_pc_node_serializer import field
from pc_stl_fixtures import install_char_traits
from pc_crt_string_fixtures import install_crt_string
from pc_crt_format_fixtures import install_sprintf

MODES=('empty','zero','one','repeat-bone','repeat-field','clear','unknown','prebound','raw-bits','null')

def specimen(mode):
    if mode not in MODES:raise ValueError('explicit bounded Skin graph specimen')
    matrix=struct.pack('<16f',*range(1,17))
    if mode=='raw-bits':matrix=struct.pack('<16I',0x80000000,0x7fc12345,0x7f800000,0xff800000,*range(12))
    body=struct.pack('<II',CLASS,0x4f4f4253)+field(0,struct.pack('<3f',1,2,3))+b'\0'
    reference=struct.pack('<II',7,0 if mode=='prebound' else len(body))+(b'' if mode=='prebound' else body)
    references=[reference]
    if mode=='repeat-bone':references.append(struct.pack('<II',7,0))
    if mode in ('empty','zero'):references=[]
    if mode=='null':references=[struct.pack('<I',0)]
    weights=0xffffffff if mode=='raw-bits' else 3
    section=field(0,struct.pack('<II',weights,len(references))+b''.join(r+matrix for r in references))
    if mode=='empty':section=b''
    if mode=='repeat-field':section+=field(0,struct.pack('<II',9,1)+struct.pack('<II',7,0)+matrix)
    if mode=='clear':section+=field(0,struct.pack('<II',0,0))
    if mode=='unknown':section=field(9,b'ignored')+section+field(30,b'tail')
    return struct.pack('<IIHIII',1,7,0,CLASS,0,len(body)),b'\0\0'+section+b'\0',matrix

def main(mode,return_capture=False):
    directory,payload,matrix=specimen(mode);f=PCWriteBytesFixture(directory+payload);p=f.p
    node_rtti(f);install_char_traits(p);install_crt_string(p);install_sprintf(p)
    p.mu.mem_write(0x73ff60,b'\0');f.call(0x6d38e0);checks=0;instructions=[]
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    for record,identity,parent in ((0x75e030,0x4fda4542,0x7555f8),(0x760cf8,0x763277db,0x75e030),(0x760520,0x681f2043,0x760cf8)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat)
    for off,val in ((0x10,2),(0x14,1),(0x18,2)):p.put_uint(manager+off,val)
    p.put_uint(0x75dde8,manager)
    serializer=f.call(0x490c50);instructions.append(sum(p.visits.values()));ns=f.call(0x4638f0)
    check(f.allocations[serializer]==0x14 and p.uint(serializer)==0x6ec7cc and p.uint(serializer+0x10)==0x6ec7c0,'actual serializer factory and both vtables')
    for identity,s in ((0x681f2043,serializer),(CLASS,ns)):f.call(0x422d90,this=manager,args=(identity,s,0xff,3))
    check(f.call(0x466b90,this=fat,args=(f.stream,))&255==1,'actual FAT load with known Node RTTI input')
    entry=f.call(0x4664c0,this=fat,args=(7,));skin=f.call(0x46a120);instructions.append(sum(p.visits.values()))
    check(f.allocations[skin]==0x70 and p.uint(skin)==0x6e8c5c and p.uint(skin+0x60)==4,'actual Skin factory size/vtable/default four weights')
    check(bytes(p.mu.mem_read(skin+0x64,12))==bytes(12),'factory empty parallel arrays')
    if mode=='prebound':p.put_uint(entry+0x20,f.call(0x421e20))
    result=f.call(0x491170,this=serializer+0x10,args=(f.stream,skin))&255;instructions.append(sum(p.visits.values()))
    position=f.position-len(directory);bone=p.uint(entry+0x20);count=p.uint(skin+0x64)
    check(result==int(mode!='null') and position==len(payload)-(mode=='null'),'whole original reader result and exact cursor')
    check(p.visits.get(0x4938f0) and p.visits.get(0x473000),'actual inherited Model and block reader bodies')
    if bone:check(p.uint(bone+8)&65535==0,'Skin stores borrowed Node with no retain')
    matrices=bytes(p.mu.mem_read(p.uint(skin+0x6c),count*64)).hex() if count else ''
    bones=[]
    for i in range(count):
        current=p.uint(p.uint(skin+0x68)+4*i)
        check(current==bone and bytes(p.mu.mem_read(p.uint(skin+0x6c)+64*i,64))==matrix,'canonical repeated bone and byte-exact matrix')
        bones.append([7,bytes(p.mu.mem_read(current+0x74,12)).hex()])
    output=None
    if result:
        f.call(0x466760,this=fat);p.put_uint(manager+0x14,2)
        check(f.call(0x4672c0,this=serializer,args=(skin,))&255==1,'actual recursive Skin/Model/Node index');instructions.append(sum(p.visits.values()))
        check(p.visits.get(0x490d30) and p.uint(fat+0x10)==(3 if count else 2),'one Skin plus at most one canonical bone ID')
        f.data=b'';f.position=0
        check(f.call(0x490da0,this=serializer+0x10,args=(f.stream,skin))&255==1,'whole original Skin writer');instructions.append(sum(p.visits.values()))
        check(p.visits.get(0x4935f0) and p.visits.get(0x472d30) and f.position==len(f.data),'actual base sections and backpatched field boundaries')
        if count:check(p.visits.get(0x467350) and p.visits.get(0x463f10),'actual common inline reference and Node writer')
        output=f.data.hex()
    else:
        check(p.uint(0x75ab9c)==0x10010003 and p.uint(skin+0x60)==4 and count==0,'null bone error leaves original palette/default weight unchanged after matrix read')
    capture=[mode,payload.hex(),result,position,p.uint(skin+0x60),count,matrices,bones,output]
    f.call(0x466760,this=fat);f.call(0x46a7a0,this=skin,args=(1,))
    if bone:
        check(bone not in f.freed and p.uint(bone+8)&65535==0,'Skin destructor frees arrays but not borrowed bone')
        f.call(0x422220,this=bone,args=(1,))
    f.call(0x4228a0,this=manager)
    for address in (0x75db90,0x75db78,0x75526c,0x755264):
        obj=p.uint(address)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    leaked=[a for a in f.allocations if a not in f.freed]
    check(sorted(f.allocations[a] for a in leaked)==([4,64] if mode in ('repeat-field','clear','null') else []),'exact original abandoned array storage on replacement/null error')
    # Explicit fixture reclamation after completed native teardown. Never
    # present this reclamation as a native cleanup path.
    for address in leaked:p.run(0x412420,args=(address,),callee_pop=False)
    check(set(f.allocations)==set(f.freed),'all tracked storage accounted for and fixture reclaimed')
    if not return_capture:print('SKIN_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original Skin {mode}; instructions={instructions}; heap={p.allocated}; nativeLeaks={len(leaked)}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
