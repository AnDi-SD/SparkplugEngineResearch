"""Actual C ABI particle Init projection versus prior original-PC capture."""
from pathlib import Path
import ctypes as C,hashlib,json,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[1]
DLL=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll'
class Info(C.Structure):_fields_=[(k,C.c_uint32) for k in ('count','first','boundary','initialized')]
class Record(C.Structure):_fields_=[('words',C.c_uint32*8),('written',C.c_uint32),('previous',C.c_uint32),('next',C.c_uint32)]
def child(original_name,output_name):
    original=Path(original_name).resolve();output=Path(output_name).resolve()
    assert original.is_relative_to(ROOT) and output.is_relative_to(ROOT/'local-data/results') and not output.exists()
    before=time.perf_counter();capture=json.loads(original.read_text());source=ROOT/capture['asset'];raw=source.read_bytes()
    expected=capture['pools'][1];assert hashlib.sha256(raw).hexdigest().upper()==capture['assetSha256']
    lib=C.CDLL(str(DLL));u=C.c_uint32;p=C.c_void_p;lib.spv_graph_load.argtypes=[p,u];lib.spv_graph_load.restype=p
    lib.spv_graph_destroy.argtypes=[p];lib.spv_last_error.restype=C.c_char_p
    lib.spv_graph_particle_pool.argtypes=[p,u,C.POINTER(Info),C.POINTER(Record),u]
    checks=0
    def check(value,message):
        nonlocal checks
        checks+=1
        if not value:raise AssertionError(message+': '+(lib.spv_last_error() or b'').decode())
    graph=lib.spv_graph_load(C.c_char_p(raw),len(raw));check(graph,'Graph loaded')
    report=dict(status='started',inputSha256=hashlib.sha256(raw).hexdigest().upper(),nativeSha256=hashlib.sha256(DLL.read_bytes()).hexdigest().upper(),
        originalSha256=hashlib.sha256(original.read_bytes()).hexdigest().upper(),scriptSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper())
    try:
        info=Info();check(lib.spv_graph_particle_pool(graph,3,C.byref(info),None,0),'Count query')
        check((info.count,info.first,info.boundary,info.initialized)==(expected['count'],expected['first'],expected['boundary'],1),'Original pool identity')
        records=(Record*(info.count+2))();C.memset(C.addressof(records),0xa5,C.sizeof(records))
        begin=C.cast(C.addressof(records)+C.sizeof(Record),C.POINTER(Record));info_before=bytes(info);bytes_before=bytes(records)
        for bad_id,bad_info,bad_records,capacity in [(3,C.byref(info),begin,info.count-1),(3,C.byref(info),None,1),
            (0,C.byref(info),begin,info.count),(1,C.byref(info),begin,info.count),(3,None,begin,info.count)]:
            check(not lib.spv_graph_particle_pool(graph,bad_id,bad_info,bad_records,capacity),'Malformed boundary rejected')
            check(bytes(info)==info_before and bytes(records)==bytes_before,'Rejected projection writes no outputs')
        check(lib.spv_graph_particle_pool(graph,3,C.byref(info),begin,info.count),'Exact-capacity projection')
        check(bytes(records[0])==b'\xa5'*44 and bytes(records[-1])==b'\xa5'*44,'Output canaries preserved')
        record_bytes=b''
        for i in range(info.count):
            row=records[i+1];check(row.written==1,'Original menu emitted every physical record')
            check((row.previous,row.next)==tuple(expected['links'][i][1:]),'Original ring links')
            record_bytes+=bytes(row.words)
        check(record_bytes.hex()==expected['recordsHex'],'Every original CPU record word matches through ABI')
        report.update(status='passed',records=info.count,recordBytes=len(record_bytes),checks=checks)
    finally:
        lib.spv_graph_destroy(graph);report['seconds']=time.perf_counter()-before
        output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ('status','checks','records','seconds') if k in report}))
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--child']:raise SystemExit(child(*sys.argv[2:]))
    raise SystemExit(subprocess.run([sys.executable,str(Path(__file__)), '--child',*sys.argv[1:]],cwd=ROOT,timeout=30).returncode)
