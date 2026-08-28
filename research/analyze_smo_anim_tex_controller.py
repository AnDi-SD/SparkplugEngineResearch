#!/usr/bin/env python3
"""Read-only full-corpus report for spAnimTexController frame tracks."""

from __future__ import annotations

import argparse, collections, hashlib, itertools, sqlite3, struct, sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parent))
from analyze_smo_mesh_navigation_set import canonical_path,decode_relationship,read_resource

ANIM_TEX=0x16FB0E47
TEXTURE=0x78EA082B

@dataclass(frozen=True)
class Controller:
    corpus:str;file_id:int;path:str;object_index:int;name:str
    times:tuple[float,...];textures:tuple[object,...];serialized_hash:str

def load(connection):
    rows=connection.execute("""select c.corpus_key,o.file_id,f.relative_path,o.object_index,o.name,
      c.source_kind,c.source_root,ct.relative_path,fo.byte_offset,f.byte_size
      from objects o join files f on f.id=o.file_id join corpora c on c.id=f.corpus_id
      left join file_occurrences fo on fo.id=(select min(x.id) from file_occurrences x where x.file_id=f.id)
      left join containers ct on ct.id=fo.container_id where o.type_hash=? order by c.id,o.file_id,o.object_index""",
      (ANIM_TEX,)).fetchall();resources={};maps={};result=[]
    for corpus,file_id,path,index,name,kind,root,container,offset,size in rows:
        if file_id not in resources:
            resources[file_id]=read_resource(kind,root,path,container,offset,size)
            maps[file_id]={oid:(oi,ot,on) for oi,oid,ot,on in connection.execute(
                'select object_index,object_id,type_hash,name from objects where file_id=?',(file_id,))}
        field=connection.execute("""select payload_size,payload_preview,absolute_payload_offset
          from direct_fields where file_id=? and object_index=? and is_section_terminator=0""",
          (file_id,index)).fetchall()
        if len(field)!=1: raise ValueError('controller field count')
        payload_size,preview,payload_offset=field[0]
        payload=resources[file_id][payload_offset:payload_offset+payload_size]
        if payload[:len(preview)]!=bytes(preview) or len(payload)<4: raise ValueError('source mismatch')
        count=struct.unpack_from('<I',payload)[0]
        if len(payload)<4+count*4: raise ValueError('short times')
        times=struct.unpack_from(f'<{count}f',payload,4);cursor=4+count*4;textures=[]
        for _ in range(count):
            if cursor+8>len(payload):raise ValueError('short texture relationship')
            inline=struct.unpack_from('<I',payload,cursor+4)[0];relation_size=8+inline
            if cursor+relation_size>len(payload):raise ValueError('texture relationship size')
            relation=decode_relationship(payload[cursor:cursor+relation_size],maps[file_id])
            if relation.target_type!=TEXTURE:raise ValueError('texture target type')
            textures.append(relation);cursor+=relation_size
        if cursor!=len(payload) or any(times[i]>=times[i+1] for i in range(len(times)-1)):
            raise ValueError('frame times or trailing bytes')
        result.append(Controller(corpus,file_id,path,index,name,times,tuple(textures),
            hashlib.sha256(payload).hexdigest()))
    return result

def pairing(items,corpus):
    counts=collections.Counter();result={}
    for item in (x for x in items if x.corpus==corpus):
        base=canonical_path(item.path);ordinal=counts[base];counts[base]+=1
        result[(base,ordinal)]=item
    return result
def core(x):return x.times,tuple(t.target_type for t in x.textures)
def main():
    p=argparse.ArgumentParser();p.add_argument('database',type=Path);a=p.parse_args();c=sqlite3.connect(a.database);items=load(c)
    print('objects',len(items))
    for corpus,it in itertools.groupby(items,key=lambda x:x.corpus):
        group=list(it);print(corpus,'objects',len(group),'frames',collections.Counter(len(x.times) for x in group),
          'encodings',collections.Counter(t.encoding for x in group for t in x.textures),
          'duration',collections.Counter(round(x.times[-1],6) for x in group))
    for ln,rn in (('pc-working','pc-pristine'),('pc-pristine','ps2-pristine')):
        l,r=pairing(items,ln),pairing(items,rn);keys=l.keys()&r.keys();print('compare',ln,rn,'paired',len(keys),
          'core',sum(core(l[k])==core(r[k]) for k in keys),'bytes',sum(l[k].serialized_hash==r[k].serialized_hash for k in keys),
          'left_only',len(l.keys()-r.keys()),'right_only',len(r.keys()-l.keys()))
    c.close();return 0
if __name__=='__main__':raise SystemExit(main())
