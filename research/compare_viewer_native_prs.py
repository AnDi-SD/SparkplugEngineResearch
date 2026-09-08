"""Compare the production DLL's SAN reader/sampler with bounded original PC output."""
from pathlib import Path
import ctypes as c
import hashlib
import json
ROOT=Path(__file__).resolve().parents[1]

class Sample(c.Structure):
    _fields_=[('position',c.c_float*3),('rotation',c.c_float*4),('scale',c.c_float*3),('valid',c.c_uint32)]

def main():
    base=ROOT/'local-data/results/viewer-sparkplug-core-20260908'
    original=json.loads((base/'icy-original-prs.json').read_text(encoding='utf-8'))
    library=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll'
    dll=c.CDLL(str(library))
    dll.spv_clip_load.argtypes=[c.c_void_p,c.c_uint32];dll.spv_clip_load.restype=c.c_void_p
    dll.spv_clip_sample.argtypes=[c.c_void_p,c.c_uint32,c.c_float,c.POINTER(Sample)]
    dll.spv_clip_destroy.argtypes=[c.c_void_p]
    dll.spv_last_error.restype=c.c_char_p
    data=(ROOT/'local-data/pc-pristine/Media/Characters/Icy/xiwa.san').read_bytes()
    buffer=c.create_string_buffer(data);handle=dll.spv_clip_load(buffer,len(data))
    assert handle,dll.spv_last_error()
    export=json.loads((ROOT/'local-data/results/tool-cycle-20260908-1900/shared-san-export-v4/input.json').read_text())
    case=next(row for row in export['cases'] if row['name']=='icy-walk')
    previous=next(row for row in case['samples'] if row['seconds']==1)['tracks']
    rows=[]
    try:
        for row in original['rows']:
            sample=Sample();assert dll.spv_clip_sample(handle,row['ordinal'],row['seconds'],c.byref(sample)),dll.spv_last_error()
            values=[list(sample.position),list(sample.rotation),list(sample.scale)]
            errors=[];previousErrors=[]
            for role in range(3):
                if not row['validity'][role]:continue
                assert sample.valid&(1<<role)
                errors.extend(abs(a-b) for a,b in zip(values[role],row['prs'][role]))
                old=previous.get(row['name'],{}).get(str(role+2))
                if old:
                    # The recorded tracks are raw PC PRS; only the source-node
                    # and exported mesh arrays use the receiver coordinate basis.
                    previousErrors.extend(abs(a-b) for a,b in zip(old,row['prs'][role]))
            rows.append({**row,'dllPrs':values,'dllMaximumError':max(errors,default=0),'previousCSharpMaximumError':max(previousErrors,default=0)})
    finally:dll.spv_clip_destroy(handle)
    report={'dllSha256':hashlib.sha256(library.read_bytes()).hexdigest().upper(),'sourceSha256':hashlib.sha256(data).hexdigest().upper(),
        'originalReportSha256':hashlib.sha256((base/'icy-original-prs.json').read_bytes()).hexdigest().upper(),
        'dllMaximumError':max(r['dllMaximumError'] for r in rows),'previousCSharpMaximumError':max(r['previousCSharpMaximumError'] for r in rows),'rows':rows}
    (base/'icy-prs-comparison-v2.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print({k:v for k,v in report.items() if k!='rows'})
    for row in rows:
        if max(row['dllMaximumError'],row['previousCSharpMaximumError'])>1e-4:
            print({k:row[k] for k in ('name','dllMaximumError','previousCSharpMaximumError')})

if __name__=='__main__':main()
