"""Bounded actual C ABI material submission ownership and output guards."""
from pathlib import Path
import ctypes as C,hashlib,json,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[1]
DLL=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll'
u=C.c_uint32;p=C.c_void_p
class Pass(C.Structure):
    _fields_=[('ordinal',u),('vertex_alpha',u),('known_render',u),('known_uv',u),
        ('render',u*16),('colors',C.c_float*17),('textures',u*8),('stages',u*80),('known_stages',u*8),('uv',C.c_float*128)]
def child(managed_name,output_name):
    managed=Path(managed_name).resolve();output=Path(output_name).resolve()
    assert managed.is_relative_to(ROOT) and output.is_relative_to(ROOT/'local-data/results') and not output.exists()
    expected=json.loads(managed.read_text());source=Path(expected['source']);raw=source.read_bytes()
    assert hashlib.sha256(raw).hexdigest().upper()==expected['hash'] and C.sizeof(Pass)==1044
    lib=C.CDLL(str(DLL));lib.spv_last_error.restype=C.c_char_p
    lib.spv_graph_load.argtypes=[p,u];lib.spv_graph_load.restype=p;lib.spv_graph_destroy.argtypes=[p]
    lib.spv_material_submission_create.argtypes=[p];lib.spv_material_submission_create.restype=p
    lib.spv_material_submission_destroy.argtypes=[p]
    lib.spv_material_submission_capture.argtypes=[p,u,u,C.POINTER(Pass),u,C.POINTER(u)]
    checks=0;start=time.perf_counter();context=None;graph=None
    def check(value,message):
        nonlocal checks
        checks+=1
        if not value:raise AssertionError(message+': '+(lib.spv_last_error() or b'').decode())
    report=dict(source=str(source),inputSha256=expected['hash'],nativeSha256=hashlib.sha256(DLL.read_bytes()).hexdigest().upper(),status='failed')
    try:
        check(not lib.spv_material_submission_create(None),'Missing graph rejected')
        graph=lib.spv_graph_load(C.c_char_p(raw),len(raw));check(graph,'Actual graph load')
        context=lib.spv_material_submission_create(graph);check(context,'Submission context')
        lib.spv_graph_destroy(graph);graph=None
        records=(Pass*10)();C.memset(C.addressof(records),0xa5,C.sizeof(records))
        out=C.cast(C.addressof(records)+C.sizeof(Pass),C.POINTER(Pass));count=u(0xa5a5a5a5)
        baseline=bytes(records);mid=expected['reports'][0]['ObjectId'];n=len(expected['reports'][0]['passes'])
        for ctx,id_,out_,cap,count_ in [(None,mid,out,8,C.byref(count)),(context,0,out,8,C.byref(count)),
            (context,mid,None,8,C.byref(count)),(context,mid,out,n-1,C.byref(count)),
            (context,mid,out,9,C.byref(count)),(context,mid,out,8,None)]:
            check(not lib.spv_material_submission_capture(ctx,id_,5,out_,cap,count_),'Bad output/input refused')
            check(bytes(records)==baseline and count.value==0xa5a5a5a5,'Refusal leaves all outputs intact')
        check(lib.spv_material_submission_capture(context,mid,5,out,8,C.byref(count)),'Context owns graph after external graph disposal')
        check(count.value==n,'All material passes copied')
        check(bytes(records[0])==b'\xa5'*1044 and bytes(records)[(n+1)*1044:]==baseline[(n+1)*1044:],'Canaries and unused capacity retained')
        first=[bytes(records[i+1]) for i in range(n)]
        check(lib.spv_material_submission_capture(context,mid,5,out,8,C.byref(count)),'Repeated capture succeeds')
        # Icy has no pending controller inputs in this ABI slice.
        check(first==[bytes(records[i+1]) for i in range(n)],'Suppressed device callbacks retain complete transport')
        check(records[1].known_render==0xffff,'All required raster states known')
        check(hashlib.sha256(source.read_bytes()).hexdigest().upper()==expected['hash'],'Input file unchanged')
        report.update(status='passed',materialID=mid,passes=n)
    finally:
        if context:lib.spv_material_submission_destroy(context)
        if graph:lib.spv_graph_destroy(graph)
        report.update(checks=checks,seconds=time.perf_counter()-start)
        output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--child']:raise SystemExit(child(*sys.argv[2:]))
    raise SystemExit(subprocess.run([sys.executable,str(Path(__file__)), '--child',*sys.argv[1:]],cwd=ROOT,timeout=30).returncode)
