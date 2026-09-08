#!/usr/bin/env python3
"""Five representative SMOs through the common native graph; no corpus rescan.

The read-only corpus index supplies previously parsed FAT identities for exactly
matching file hashes. Node structure is checked through actual engine objects.
This is integration/metadata evidence, not a new original-executable probe.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import sqlite3
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/SanToVmd'))
import sparkplug_native as native

FILES=('Characters/Bloom/bloom_jeans.smo','Characters/Icy/Icy.smo',
       'Characters/Knut/knut.smo','Characters/Flora/Flora.smo','Characters/Tecna/Tecna.smo')

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()

def main(output):
    output=output.resolve();output.relative_to(ROOT/'local-data/results')
    database=ROOT/'local-data/results/smo-corpus-v2.sqlite'
    db=sqlite3.connect(database.as_uri()+'?mode=ro',uri=True)
    rows=[]
    for name in FILES:
        path=ROOT/'local-data/pc-pristine/Media'/name;digest=sha(path)
        candidates=db.execute('SELECT id,sha256 FROM files WHERE relative_path=?',(name,)).fetchall()
        file_id=next(key for key,value in candidates if value.upper()==digest)
        entries=db.execute('SELECT object_id,name,type_hash,logical_offset,serialized_size FROM objects WHERE file_id=? ORDER BY object_index',(file_id,)).fetchall()
        started=time.perf_counter()
        with native.Graph(path.read_bytes()) as graph:
            assert len(graph.objects)==len(entries)
            nodes={info.id:node for label,info,node in graph.objects if node is not None}
            pairs=set();children={key:0 for key in nodes}
            for (label,info,node),expected in zip(graph.objects,entries):
                assert (info.id,label,info.wire_class,info.offset,info.size)==expected,(name,info.id)
                pairs.add((info.wire_class,info.runtime_class))
                if node is not None:
                    assert all(math.isfinite(v) for v in (*node.position,*node.orientation,*node.scale))
                    if node.parent:
                        assert node.parent in nodes
                        children[node.parent]+=1
            assert all(node.children==children[key] for key,node in nodes.items())
            row=dict(file=name,sha256=digest,objects=len(entries),nodes=len(nodes),returned_resource_id=graph.root,
                     wire_runtime_class_pairs=[list(pair) for pair in sorted(pairs)],seconds=time.perf_counter()-started)
            rows.append(row)
        print(json.dumps({key:row[key] for key in ('file','objects','nodes','seconds')}),flush=True)
    db.close()
    report=dict(status='passed',validator_sha256=sha(__file__),adapter_sha256=sha(native.__file__),
                native_dll_sha256=sha(native.library()._name),files=rows,
                scope='Original-derived whole resource loader; cached matching-input FAT metadata; native Node parent/child consistency. No GPU/PS2/whole corpus claim.')
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',required=True,type=Path)
    main(parser.parse_args().output)
