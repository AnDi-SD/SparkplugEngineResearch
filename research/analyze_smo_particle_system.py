#!/usr/bin/env python3
"""Read-only full-corpus report for spParticleSystem and emission regions."""

from __future__ import annotations

import argparse
import collections
import hashlib
import itertools
import json
import sqlite3
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_smo_mesh_navigation_set import (  # noqa: E402
    canonical_path, decode_relationship, read_resource,
)

PARTICLE_SYSTEM = 0x5AFA1A4F
MATERIAL = 0x6160348B
FOG = 0x7AC95AEC
RENDER_NODE = 0x603625D0
REGIONS = {12: "point", 13: "plane", 14: "box", 15: "sphere",
           16: "disk", 17: "cylinder", 18: "cone"}
SIZES = {0: 24, 1: 12, 2: 8, 3: 8, 4: 8, 5: 8, 6: 8,
         7: 1, 8: 1, 9: 1, 10: 4, 11: 4,
         12: 12, 13: 32, 14: 24, 15: 16, 16: 16, 17: 20, 18: 24}


@dataclass(frozen=True)
class Particle:
    corpus: str
    file_id: int
    path: str
    object_index: int
    name: str
    material: object | None
    fog: object | None
    alpha_sort: int
    priority: int
    values: tuple[tuple[int, bytes], ...]
    region_type: int
    render_node: object
    serialized_hash: str


def load(connection: sqlite3.Connection) -> list[Particle]:
    rows = connection.execute(
        """SELECT c.corpus_key,o.file_id,f.relative_path,o.object_index,o.name,
                  c.source_kind,c.source_root,ct.relative_path,fo.byte_offset,f.byte_size
           FROM objects o JOIN files f ON f.id=o.file_id JOIN corpora c ON c.id=f.corpus_id
           LEFT JOIN file_occurrences fo ON fo.id=(SELECT MIN(x.id) FROM file_occurrences x WHERE x.file_id=f.id)
           LEFT JOIN containers ct ON ct.id=fo.container_id
           WHERE o.type_hash=? ORDER BY c.id,o.file_id,o.object_index""",
        (PARTICLE_SYSTEM,),).fetchall()
    resources, object_maps = {}, {}
    result = []
    for (corpus,file_id,path,index,name,source_kind,source_root,container_path,
         occurrence_offset,byte_size) in rows:
        if file_id not in resources:
            resources[file_id] = read_resource(source_kind,source_root,path,
                container_path,occurrence_offset,byte_size)
            object_maps[file_id] = {row_id:(row_index,row_type,row_name)
                for row_index,row_id,row_type,row_name in connection.execute(
                    "SELECT object_index,object_id,type_hash,name FROM objects WHERE file_id=?",
                    (file_id,))}
        direct = connection.execute(
            """SELECT section_index,field_type,payload_size,payload_preview,absolute_payload_offset
               FROM direct_fields WHERE file_id=? AND object_index=? AND is_section_terminator=0
               ORDER BY field_index""",(file_id,index)).fetchall()
        fields, serialized = {}, bytearray()
        for section,field_type,size,preview,offset in direct:
            payload=resources[file_id][offset:offset+size]
            if len(payload)!=size or payload[:len(preview)]!=bytes(preview):
                raise ValueError("particle source field mismatch")
            if (section,field_type) in fields:
                raise ValueError("particle field repeated unexpectedly")
            fields[(section,field_type)]=payload
            serialized += struct.pack("<III",section,field_type,size)+payload
        if any(section not in (0,1) for section,_ in fields):
            raise ValueError("particle section count")
        material = decode_relationship(fields[(0,0)],object_maps[file_id]) if (0,0) in fields else None
        fog = decode_relationship(fields[(0,1)],object_maps[file_id]) if (0,1) in fields else None
        if material and material.target_type != MATERIAL: raise ValueError("particle material type")
        if fog and fog.target_type != FOG: raise ValueError("particle fog type")
        if len(fields.get((0,2),b"\0"*4))!=4 or len(fields.get((0,3),b"\0"*4))!=4:
            raise ValueError("renderable scalar")
        alpha=struct.unpack("<I",fields.get((0,2),b"\0"*4))[0]
        priority=struct.unpack("<I",fields.get((0,3),b"\0"*4))[0]
        if alpha>1: raise ValueError("alpha sort")
        own={field_type:payload for (section,field_type),payload in fields.items() if section==1}
        if any(field_type not in range(20) for field_type in own): raise ValueError("unknown particle field")
        for field_type,size in SIZES.items():
            if field_type in own and len(own[field_type])!=size: raise ValueError("particle scalar size")
        regions=[field_type for field_type in REGIONS if field_type in own]
        if len(regions)!=1: raise ValueError("particle requires one region")
        if not all(field in own for field in (0,4,11,19)): raise ValueError("required particle fields")
        if any(own[field][0]>1 for field in (7,8,9) if field in own): raise ValueError("particle bool")
        render=decode_relationship(own[19],object_maps[file_id])
        if render.target_type!=RENDER_NODE: raise ValueError("particle render node type")
        result.append(Particle(corpus,file_id,path,index,name,material,fog,alpha,priority,
            tuple(sorted((key,value) for key,value in own.items() if key!=19)),regions[0],render,
            hashlib.sha256(serialized).hexdigest()))
    return result


def pairing(items, corpus):
    counts=collections.Counter();result={}
    for item in (x for x in items if x.corpus==corpus):
        base=canonical_path(item.path),item.name.lower();ordinal=counts[base];counts[base]+=1
        result[(*base,ordinal)]=item
    return result


def core(item):
    return (item.alpha_sort,item.priority,item.values,item.region_type,
            item.material.target_type if item.material else None,
            item.fog.target_type if item.fog else None,item.render_node.target_type)


def main():
    parser=argparse.ArgumentParser();parser.add_argument("database",type=Path);args=parser.parse_args()
    connection=sqlite3.connect(args.database);items=load(connection)
    print("objects",len(items))
    for corpus,iterator in itertools.groupby(items,key=lambda item:item.corpus):
        group=list(iterator)
        print(corpus,"objects",len(group),"regions",collections.Counter(REGIONS[x.region_type] for x in group),
              "fields",collections.Counter(key for x in group for key,_ in x.values),
              "render encodings",collections.Counter(x.render_node.encoding for x in group),
              "materials",sum(x.material is not None for x in group),"fog",sum(x.fog is not None for x in group))
    for left_name,right_name in (("pc-working","pc-pristine"),("pc-pristine","ps2-pristine")):
        left,right=pairing(items,left_name),pairing(items,right_name);common=left.keys()&right.keys()
        print("compare",left_name,right_name,"paired",len(common),
              "core",sum(core(left[k])==core(right[k]) for k in common),
              "bytes",sum(left[k].serialized_hash==right[k].serialized_hash for k in common),
              "left_only",len(left.keys()-right.keys()),"right_only",len(right.keys()-left.keys()))
    for field in range(19):
        sizes=collections.Counter(len(value) for item in items for key,value in item.values if key==field)
        if sizes: print("field",field,"sizes",sizes)
    connection.close();return 0


if __name__=="__main__": raise SystemExit(main())
