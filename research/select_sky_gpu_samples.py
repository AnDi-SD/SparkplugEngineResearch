"""Small read-only PC SkyBox selection; class index first, then exact object fields."""
from pathlib import Path
import json,sqlite3,sys,subprocess,time
ROOT=Path(__file__).resolve().parents[1]
def main():
    started=time.perf_counter();db=sqlite3.connect((ROOT/'local-data/results/smo-corpus-v2.sqlite').as_uri()+'?mode=ro',uri=True)
    db.set_progress_handler(lambda:time.perf_counter()-started>5,10000)
    # A single unconstrained join let SQLite start with ubiquitous field_type0
    # via ix_fields_shape and took minutes. Materialize this tiny indexed class
    # selection before looking up fields by their unique file/object prefix.
    rows=db.execute('''SELECT o.file_id,o.object_index,f.relative_path,f.byte_size FROM objects o
        INDEXED BY ix_objects_type_hash JOIN files f ON f.id=o.file_id
        WHERE o.type_hash=? AND f.corpus_id=2 ORDER BY f.byte_size,o.object_index''',(0x7a7124af,)).fetchall()
    assert len(rows)<=256
    selected={}
    for file_id,index,path,size in rows:
        count=db.execute('''SELECT count(*) FROM direct_fields INDEXED BY sqlite_autoindex_direct_fields_1
            WHERE file_id=? AND object_index=? AND section_index=1 AND field_type=0 AND is_section_terminator=0''',(file_id,index)).fetchone()[0]
        assert count>0
        row=selected.setdefault(file_id,dict(path=path,bytes=size,skyBoxes=0,members=0,objects=[]))
        row['skyBoxes']+=1;row['members']+=count;row['objects'].append(dict(index=index,members=count))
    result=dict(seconds=time.perf_counter()-started,objects=len(rows),files=len(selected),
        candidates=sorted(selected.values(),key=lambda r:(-(r['skyBoxes']>1),r['bytes']))[:10])
    out=ROOT/'local-data/results/tools-core-cycle-20260911-1900/sky-gpu/selected-samples-run2.json'
    assert not out.exists();out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));return 0
if __name__=='__main__':
    if '--child' in sys.argv:raise SystemExit(main())
    raise SystemExit(subprocess.run([sys.executable,__file__,'--child'],timeout=10).returncode)
