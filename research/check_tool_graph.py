"""Targeted application C ABI graph/scene acceptance, one bounded child per file."""
from pathlib import Path
import collections,ctypes as C,hashlib,json,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[1]
DLL=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll'
class Object(C.Structure):_fields_=[(name,C.c_uint32) for name in ('id','wire','runtime','offset','size','isNode')]
def child(source_name,report_name):
    source=Path(source_name).resolve();report=Path(report_name).resolve()
    assert source.is_relative_to(ROOT) and report.is_relative_to(ROOT/'local-data/results') and not report.exists()
    started=time.perf_counter();raw=source.read_bytes();lib=C.CDLL(str(DLL));u=C.c_uint32;p=C.c_void_p
    lib.spv_graph_load.argtypes=[p,u];lib.spv_graph_load.restype=p
    lib.spv_last_error.restype=C.c_char_p
    lib.spv_graph_destroy.argtypes=[p]
    lib.spv_graph_info.argtypes=[p,C.POINTER(u),C.POINTER(u),C.POINTER(u)]
    lib.spv_graph_object.argtypes=[p,u,p,u,C.POINTER(Object)]
    lib.spv_graph_scene_all.argtypes=[p];lib.spv_graph_scene_all.restype=p
    lib.spv_scene_destroy.argtypes=[p]
    lib.spv_scene_node_count.argtypes=[p,C.POINTER(u)]
    lib.spv_scene_sample.argtypes=[p,C.c_float,p,u]
    lib.spv_scene_light_ids.argtypes=[p,p,u,C.POINTER(u)]
    lib.spv_scene_lighting_configure.argtypes=[p,p,u,u]
    def check(value):
        if not value:raise RuntimeError((lib.spv_last_error() or b'Unknown bridge failure').decode('utf-8'))
        return value
    result=dict(source=str(source.relative_to(ROOT)),sourceSha256=hashlib.sha256(raw).hexdigest().upper(),inputBytes=len(raw),
                nativeSha256=hashlib.sha256(DLL.read_bytes()).hexdigest().upper(),scriptSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),status='started',scope='Host shared graph and one sampled scene; no original whole-game/GPU execution.')
    graph=scene=None
    try:
        graph=check(lib.spv_graph_load(C.c_char_p(raw),len(raw)));objects,nodes,root=u(),u(),u()
        check(lib.spv_graph_info(graph,C.byref(objects),C.byref(nodes),C.byref(root)))
        result.update(objects=objects.value,nodes=nodes.value,rootId=root.value,loadSeconds=time.perf_counter()-started)
        classes=collections.Counter();name=C.create_string_buffer(65536);row=Object()
        for i in range(objects.value):
            check(lib.spv_graph_object(graph,i,name,len(name),C.byref(row)));classes[f'{row.runtime:08X}']+=1
        result['runtimeClasses']=dict(classes)
        scene=check(lib.spv_graph_scene_all(graph));count=u();check(lib.spv_scene_node_count(scene,C.byref(count)))
        matrices=(C.c_float*(count.value*16))();check(lib.spv_scene_sample(scene,0,matrices,len(matrices)))
        result['sceneNodes']=count.value
        lights=u();check(lib.spv_scene_light_ids(scene,None,0,C.byref(lights)));ids=(u*lights.value)()
        check(lib.spv_scene_light_ids(scene,ids,lights.value,C.byref(lights)))
        check(lib.spv_scene_lighting_configure(scene,ids,lights.value,1));check(lib.spv_scene_sample(scene,0,matrices,len(matrices)))
        result.update(status='completed',lightIds=list(ids),worldSha256=hashlib.sha256(bytes(matrices)).hexdigest().upper())
    except Exception as error:result.update(status='failed',error=str(error))
    finally:
        if scene:lib.spv_scene_destroy(scene)
        if graph:lib.spv_graph_destroy(graph)
    result['elapsedSeconds']=time.perf_counter()-started;report.parent.mkdir(parents=True,exist_ok=True);report.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({key:result.get(key) for key in ('status','error','objects','nodes','elapsedSeconds')}))
    return 0 if result['status']=='completed' else 1
if __name__=='__main__':
    if sys.argv[1:2]==['--child']:raise SystemExit(child(*sys.argv[2:]))
    try:raise SystemExit(subprocess.run([sys.executable,str(Path(__file__)), '--child',*sys.argv[1:]],cwd=ROOT,timeout=30).returncode)
    except subprocess.TimeoutExpired:print('Graph acceptance exceeded 30 seconds',file=sys.stderr);raise SystemExit(2)
