#!/usr/bin/env python3
"""Real SMO light plus owning child -> native index/write/alias -> fresh read."""
from pathlib import Path
import sys,json,struct
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_fat,empty_manager
from probe_pc_node_relationships import node_rtti,CLASS
from probe_pc_light_corpus import specimen,state
from probe_pc_function_eval import cleanup
from pc_stl_fixtures import install_char_traits
from pc_crt_string_fixtures import install_crt_string
from pc_crt_format_fixtures import install_sprintf
LIGHT=0x6b3e7baa

def graph(p,obj):
 scalar,node=state(p,obj);scalar.pop(12) # opaqueDC is neither written nor initialized
 count=p.uint(obj+0x1c);children=[];link=p.uint(p.uint(obj+0x18))
 for _ in range(count):
  child=p.uint(link+8);children.append(b''.join(bytes(p.mu.mem_read(child+off,n*4)) for off,n in ((0x20,3),(0x30,3),(0x40,9),(0x74,3),(0x80,3),(0x8c,9))).hex());link=p.uint(link)
 return [[scalar,node],children]

def main(case,return_capture=False):
 _,data=specimen(case);f=PCWriteBytesFixture(data);p=f.p;checks=0
 def check(ok,label):
  nonlocal checks
  checks+=1
  if not ok:raise AssertionError(label)
 node_rtti(f);f.call(0x6d38e0);install_char_traits(p);install_crt_string(p);install_sprintf(p);p.mu.mem_write(0x73ff60,b'\0')
 for record,identity,parent in ((0x7634b0,LIGHT,0x75e278),(0x75e278,0x72444900,0x75dd88)):
  p.put_uint(record,identity);p.put_uint(record+0x48,parent)
 tree=p.uint(0x755378);head=p.uint(tree+0x18);root_entry=p.uint(head+4);right=p.allocate(24)
 for off in (0,8):p.put_uint(right+off,head)
 p.put_uint(right+4,root_entry);p.put_uint(root_entry+8,right);p.put_uint(head+8,right);p.put_uint(right+12,LIGHT);p.put_uint(right+16,0x7634b0);p.put_uint(0x7634b0+0x4c,0x4ac000);p.put_uint(tree+0x1c,2)
 fat=empty_fat(f);manager,_=empty_manager(f);p.put_uint(manager+0x28,fat);p.put_uint(0x75dde8,manager)
 for off,value in ((0x10,2),(0x14,2),(0x18,2)):p.put_uint(manager+off,value)
 serializer=f.call(0x43ffd0);ns=f.call(0x4638f0)
 for identity,s in ((LIGHT,serializer),(CLASS,ns)):f.call(0x422d90,this=manager,args=(identity,s,0xff,3))
 light=f.call(0x4400b0,this=serializer,args=(f.stream,));check(f.call(0x440640,this=serializer+0x10,args=(f.stream,light))&255==1 and f.position==len(data),'whole pinned light section read')
 child=f.call(0x421e20);p.put_floats(child+0x20,(1.,2.,3.));p.put_uint(child+0xb0,p.uint(child+0xb0)|1);f.call(0x421a60,this=light,args=(child,))
 check(p.uint(child+0x2c)==light and p.uint(child+8)&65535==1,'actual owning Node child of decoded light')
 check(f.call(0x4672c0,this=serializer,args=(light,))&255==1,'whole Light inherited relationship indexing');index_instructions=sum(p.visits.values())
 check(p.visits.get(0x4639d0) and p.visits.get(0x467300) and p.uint(fat+0x10)==3,'DFS light and child IDs')
 f.data=b'';f.position=0;check(f.call(0x467350,this=serializer,args=(f.stream,light))&255==1,'whole native graph reference writer');write_instructions=sum(p.visits.values())
 check(p.visits.get(0x440110) and p.visits.get(0x463f10) and p.visits.get(0x467260),'actual Light/Node writers and runtime-class object headers')
 written=f.data;check(struct.unpack_from('<III',written)==(1,len(written)-8,LIGHT),'runtime DXLight ID is preserved in saved header')
 metadata=[]
 for obj in (light,child):
  entry=f.call(0x4664f0,this=fat,args=(obj,));metadata.append([p.uint(entry+off) for off in (4,0x10,0x14,0x18)]+[p.uint(entry+0x1c)&255])
 check(f.call(0x467350,this=serializer,args=(f.stream,light))&255==1 and f.data==written+struct.pack('<II',1,0),'repeat root emits no child again')
 check(f.call(0x467350,this=serializer,args=(f.stream,0))&255==1 and f.data.endswith(bytes(4)),'null reference emits four bytes')
 output=f.data;before=graph(p,light);f.call(0x466760,this=fat)
 directory=struct.pack('<I',2)+b''.join(struct.pack('<IHIII',identity,0,kind,offset,size) for identity,kind,offset,size,_ in metadata)
 f.data=directory+output;f.position=0;p.put_uint(manager+0x14,1)
 check(f.call(0x466b90,this=fat,args=(f.stream,))&255==1,'actual directory read with same runtime classes')
 fresh=f.call(0x4678b0,this=serializer,args=(CLASS,f.stream,f.stream));read_instructions=sum(p.visits.values())
 check(fresh and fresh!=light and p.uint(fresh)==0x6f0c88 and p.visits.get(0x440640) and p.visits.get(0x421a60),'fresh whole reference/factory/Light/child/attach read')
 after=graph(p,fresh);check(len(after[1])==1,'same fresh owning child graph')
 check(f.call(0x4678b0,this=serializer,args=(CLASS,f.stream,f.stream))==fresh and not p.visits.get(0x440640),'fresh repeated reference preserves canonical root identity')
 check(f.call(0x4678b0,this=serializer,args=(CLASS,f.stream,f.stream))==0 and f.position==len(f.data) and not f.errors,'null final reference and exact complete cursor')
 f.call(0x466760,this=fat)
 for obj in (light,fresh):f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
 f.call(0x4228a0,this=manager);cleanup(f,tuple(p.uint(a) for a in (0x75db90,0x75db78) if p.uint(a)));check(set(f.allocations)==set(f.freed),'both source and fresh graphs and serializer/FAT owners destroyed')
 capture=[case,data.hex(),output.hex(),metadata,directory.hex(),before,after]
 if not return_capture:print('LIGHT_GRAPH_CAPTURE',json.dumps(capture),flush=True)
 print(f'PASS {checks}/{checks}: native Light graph roundtrip {case}; index={index_instructions}; write={write_instructions}; read={read_instructions}; arena={p.allocated}; bytes={len(output)}',flush=True)
 return capture if return_capture else 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
