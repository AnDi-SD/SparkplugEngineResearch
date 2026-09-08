#!/usr/bin/env python3
"""Five selected PC-distribution containers, including native PS2 payloads.

Tests the actual C ABI metadata batch against cached FAT rows with exact input
hash matches. No alternative parser, unknown-class factory or corpus rescan.
"""
from pathlib import Path
import ctypes as C
import hashlib,json,sqlite3,sys,time
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/SanToVmd'))
import sparkplug_native as native
class Info(C.Structure):
    _fields_=[(name,C.c_uint32) for name in ('signature','version','tag','size','platform','data_offset','data_size','count','status')]
class Entry(C.Structure):
    _fields_=[(name,C.c_uint32) for name in ('table_offset','id','name_offset','name_bytes','class_id','offset','size','signature_class','signature_flags')]
FILES=('Characters/Bloom/bloom_jeans.smo','Menus/igmenu_opt_pc.smo','SFX/tile_bad.smo',
    'Menus/igmenu_opt_ps2.smo','Menus/mmenu_new_ps2.smo')
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def main(output):
    output=Path(output).resolve();output.relative_to(ROOT/'local-data/results')
    lib=native.library();assert C.sizeof(Info)==C.sizeof(Entry)==36
    for name,result,arguments in (
        ('inspect',C.c_void_p,[C.c_void_p,C.c_uint32]),('destroy',None,[C.c_void_p]),
        ('info',C.c_int,[C.c_void_p,C.POINTER(Info)]),('entries',C.c_int,[C.c_void_p,C.POINTER(Entry),C.c_uint32])):
        call=getattr(lib,'spv_container_'+name);call.restype=result;call.argtypes=arguments
    db=sqlite3.connect((ROOT/'local-data/results/smo-corpus-v2.sqlite').as_uri()+'?mode=ro',uri=True);rows=[]
    for name in FILES:
        path=ROOT/'local-data/pc-pristine/Media'/name;data=path.read_bytes();digest=sha(path)
        ids=db.execute('select id,sha256 from files where relative_path=?',(name,)).fetchall()
        file_id=next(key for key,value in ids if value.upper()==digest)
        expected=db.execute('select table_offset,object_id,name,type_hash,logical_offset,physical_offset,serialized_size from objects where file_id=? order by object_index',(file_id,)).fetchall()
        source=C.create_string_buffer(data);start=time.perf_counter();handle=native.check(lib.spv_container_inspect(source,len(data)))
        try:
            info=Info();native.check(lib.spv_container_info(handle,C.byref(info)));assert info.count==len(expected)
            entries=(Entry*info.count)();native.check(lib.spv_container_entries(handle,entries,info.count))
            for entry,row in zip(entries,expected):
                assert (entry.table_offset,entry.id,entry.class_id,entry.offset,info.data_offset+entry.offset,entry.size)==(row[0],row[1],row[3],row[4],row[5],row[6])
                raw=data[entry.name_offset:entry.name_offset+entry.name_bytes]
                assert not raw or raw[-1]==0
                assert raw.split(b'\0',1)[0].decode('utf-8')==(row[2] or '')
                assert entry.signature_flags==7 and entry.signature_class==entry.class_id
            assert info.status==0 and info.signature==0x53504646
            rows.append(dict(file=name,sha256=digest,objects=info.count,platform_mask=info.platform,
                container_bytes=len(data),metadata_bytes=C.sizeof(Entry)*info.count,seconds=time.perf_counter()-start))
        finally:lib.spv_container_destroy(handle)
        print(json.dumps(rows[-1]),flush=True)
    db.close();report=dict(status='passed',validator_sha256=sha(__file__),native_dll_sha256=sha(lib._name),files=rows,
        scope='Shared original-derived raw header/FAT/object-header reader; cached same-hash metadata. PS2 payloads are inspected, no PS2 execution or unknown runtime creation.')
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main(*sys.argv[1:])
