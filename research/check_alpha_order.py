"""Bounded C ABI alpha transport, reusing pinned original CRT/comparator captures."""
from pathlib import Path
import ctypes as C,hashlib,json,math,struct,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[1]
DLL=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll'
ORIGINAL=ROOT/'local-data/results/tools-core-cycle-20260911-1900/sort/original-run1/report.json'
u=C.c_uint32;f=C.c_float;p=C.c_void_p
class Input(C.Structure):_fields_=[('token',u),('priority',u),('particle',u),('center',f*3),('world',f*16)]
class Output(C.Structure):_fields_=[('token',u),('priority',u),('particle',u),('distance',f)]
class Info(C.Structure):_fields_=[('queued',u),('priority',u),('particle',u),('sphere',f*4)]
class Model(C.Structure):_fields_=[('mesh',u),('material',u),('fog',u),('alpha',u),('priority',u),('projection',u)]
class Pass(C.Structure):_fields_=[('blend',u),('layers',u)]
IDENTITY=(f*16)(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
POINTS=[(0,0,0),(1,0,0),(1,1,0),(1,1,1),(2,0,0),(2,1,0),(2,1,1),(2,1,math.sqrt(2)),(2,2,0),(3,0,0)]
def child(output_name):
    output=Path(output_name).resolve();assert output.is_relative_to(ROOT/'local-data/results') and not output.exists()
    assert (C.sizeof(Input),C.sizeof(Output),C.sizeof(Info))==(88,16,28)
    lib=C.CDLL(str(DLL));lib.spv_last_error.restype=C.c_char_p
    lib.spv_alpha_order.argtypes=[C.POINTER(Input),u,C.POINTER(f),u,u,C.POINTER(Output)]
    lib.spv_graph_load.argtypes=[p,u];lib.spv_graph_load.restype=p;lib.spv_graph_destroy.argtypes=[p]
    lib.spv_graph_alpha_info.argtypes=[p,u,C.POINTER(Info)]
    lib.spv_graph_model.argtypes=[p,u,C.POINTER(Model)]
    lib.spv_graph_pass.argtypes=[p,u,u,C.POINTER(Pass)]
    checks=0;start=time.perf_counter();observations=[];graph=None
    def check(ok,message):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(message+': '+(lib.spv_last_error() or b'').decode())
    report=dict(status='failed',nativeSha256=hashlib.sha256(DLL.read_bytes()).hexdigest().upper(),originalCaptureSha256=hashlib.sha256(ORIGINAL.read_bytes()).hexdigest().upper())
    try:
        for case in json.loads(ORIGINAL.read_text())['cases']:
            if case['kind']!=1 or case['name']=='alpha-nine-nan-zero':continue
            before=bytes.fromhex(case['inputHex']);expected=bytes.fromhex(case['outputHex']);count=case['count']
            rows=(Input*count)();result=(Output*count)()
            for i in range(count):
                raw=before[i*24:(i+1)*24];distance=struct.unpack_from('<f',raw,12)[0]
                rows[i]=Input(struct.unpack_from('<I',raw)[0],struct.unpack_from('<I',raw,16)[0],raw[20],(f*3)(*POINTS[int(distance)]),IDENTITY)
            check(lib.spv_alpha_order(rows,count,IDENTITY,0,0,result),'Original finite alpha batch accepted')
            for i,row in enumerate(result):
                raw=expected[i*24:(i+1)*24]
                check((row.token,row.priority,row.particle)==(struct.unpack_from('<I',raw)[0],struct.unpack_from('<I',raw,16)[0],raw[20]),'Original full permutation and priority')
                check(struct.pack('<f',row.distance)==raw[12:16],'Native metric reproduces pinned original key bits')
            observations.append(dict(name=case['name'],tokens=[row.token for row in result],original=True))
        rows=(Input*3)(Input(11,1,0,(f*3)(10,0,0),IDENTITY),Input(22,2,0,(f*3)(0,0,1),IDENTITY),Input(33,1,0,(f*3)(0,0,3),IDENTITY))
        result=(Output*3)()
        check(lib.spv_alpha_order(rows,3,IDENTITY,0,0,result),'Spatial metric batch')
        check([r.token for r in result]==[22,11,33],'Priority precedes full squared distance')
        check(lib.spv_alpha_order(rows,3,IDENTITY,1,0,result),'Explicit depth-only batch')
        check([r.token for r in result]==[22,33,11],'Byte231 depth metric changes order')
        check(lib.spv_alpha_order(rows,3,IDENTITY,0,0xfffffffe,result),'Priority wrap batch')
        check([r.priority for r in result]==[0xffffffff,0xffffffff,0],'Original unsigned priority wraps')
        rows[0].particle=1;rows[0].priority=0xffffffff
        check(lib.spv_alpha_order(rows,3,IDENTITY,0,0,result),'Exact particle batch')
        check(result[2].token==11,'Exact particles follow ordinary items regardless of priority')
        guarded=(Output*5)();C.memset(C.addressof(guarded),0xa5,C.sizeof(guarded));target=C.cast(C.addressof(guarded)+16,C.POINTER(Output));before=bytes(guarded)
        for input_,count,view_,depth,out in [(None,1,IDENTITY,0,target),(rows,65537,IDENTITY,0,target),(rows,3,None,0,target),
            (rows,3,IDENTITY,2,target),(rows,3,IDENTITY,0,None)]:
            check(not lib.spv_alpha_order(input_,count,view_,depth,0,out),'Invalid bounded alpha inputs rejected')
            check(bytes(guarded)==before,'No output mutation on refusal')
        rows[0].center[1]=math.nan
        check(not lib.spv_alpha_order(rows,3,IDENTITY,0,0,target) and bytes(guarded)==before,'Non-finite source center refused atomically')
        rows[0].center[1]=0
        check(lib.spv_alpha_order(rows,3,IDENTITY,0,0,target),'Guarded valid batch')
        check(bytes(guarded[0])==b'\xa5'*16 and bytes(guarded[4])==b'\xa5'*16,'Canaries survive exact output count')
        check(lib.spv_alpha_order(None,0,IDENTITY,0,0,None),'Empty null-buffer batch')
        source=ROOT/'local-data/pc-pristine/Media/Characters/Icy/Icy.smo';raw=source.read_bytes()
        graph=lib.spv_graph_load(C.c_char_p(raw),len(raw));check(graph,'Actual Icy graph')
        model=Model();pass_=Pass();info=Info()
        check(lib.spv_graph_model(graph,4,C.byref(model)) and lib.spv_graph_pass(graph,model.material,0,C.byref(pass_)),'Actual Model and pass views')
        check(lib.spv_graph_alpha_info(graph,4,C.byref(info)),'Actual Skin alpha projection')
        check(info.queued==int(bool(model.alpha and pass_.blend)) and info.priority==model.priority and info.particle==0,'Common material/object gate and exact class projection')
        for owner,id_,out in [(None,4,C.byref(info)),(graph,5,C.byref(info)),(graph,0,C.byref(info)),(graph,4,None)]:
            check(not lib.spv_graph_alpha_info(owner,id_,out),'Invalid alpha object view rejected')
        report.update(status='passed',originalCases=len(observations),actualSkin=dict(queued=info.queued,priority=info.priority,sphere=list(info.sphere)))
    finally:
        if graph:lib.spv_graph_destroy(graph)
        report.update(checks=checks,seconds=time.perf_counter()-start,observations=observations)
        output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--child']:raise SystemExit(child(*sys.argv[2:]))
    raise SystemExit(subprocess.run([sys.executable,str(Path(__file__)), '--child',*sys.argv[1:]],cwd=ROOT,timeout=30).returncode)
