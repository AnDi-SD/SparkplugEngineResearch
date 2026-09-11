"""Bounded real-menu Text C ABI transport/lifetime guards; no full-game GPU claim."""
from pathlib import Path
import ctypes as C, hashlib, json, math, subprocess, sys, time
from check_text_graph import Object, ROOT, DLL, sha, u, p

class Info(C.Structure):
    _fields_=[(name,u) for name in ('font','atlas','vertices','indices','powerAssigned','passes','material')]
class Vertex(C.Structure):
    _fields_=[('position',C.c_float*3),('color',u),('uv',C.c_float*2)]
class Draw(C.Structure):
    _fields_=[(name,u) for name in ('passIndex','vertexAlpha','knownRender','knownUV')]+[
        ('render',u*16),('colors',C.c_float*17),('textures',u*8),('stages',u*80),('knownStages',u*8),('uv',C.c_float*128)]

def child(source_name,output_name):
    source=Path(source_name).resolve();output=Path(output_name).resolve()
    assert source.is_relative_to(ROOT) and output.is_relative_to(ROOT/'local-data/results') and not output.exists()
    raw=source.read_bytes();lib=C.CDLL(str(DLL));start=time.perf_counter();checks=0;graph=None;views=[]
    report=dict(status='running',source=source.relative_to(ROOT).as_posix(),sourceSha256=sha(source),
        nativeSha256=sha(DLL),scriptSha256=sha(__file__),cases=[],scope='Host C ABI geometry/material projection, canonical ownership and guards')
    lib.spv_last_error.restype=C.c_char_p
    for name,args,result in [('spv_graph_load_for_tools',[p,u,u],p),('spv_graph_destroy',[p],None),
        ('spv_graph_info',[p,p,p,p],C.c_int),('spv_graph_object',[p,u,p,u,p],C.c_int),('spv_graph_renderable',[p,u,p],C.c_int),
        ('spv_container_inspect',[p,u],p),('spv_container_info',[p,p],C.c_int),('spv_container_destroy',[p],None),
        ('spv_text_view_create',[p,u],p),('spv_text_view_destroy',[p],None),('spv_text_view_info',[p,p],C.c_int),
        ('spv_text_view_vertices',[p,p,u],C.c_int),('spv_text_view_indices',[p,p,u],C.c_int),('spv_text_view_draws',[p,p,u],C.c_int),
        ('spv_text_view_capture',[p,u,p,u],C.c_int)]:
        fn=getattr(lib,name);fn.argtypes=args;fn.restype=result
    def check(condition,label):
        nonlocal checks
        checks+=1
        if not condition:raise RuntimeError(label+': '+(lib.spv_last_error()or b'').decode())
    try:
        graph=lib.spv_graph_load_for_tools(C.c_char_p(raw),len(raw),0);check(graph,'tool load')
        count,nodes,root=u(),u(),u();check(lib.spv_graph_info(graph,C.byref(count),C.byref(nodes),C.byref(root)),'graph info')
        name=C.create_string_buffer(65536);first_text=None
        for i in range(count.value):
            row=Object();check(lib.spv_graph_object(graph,i,name,len(name),C.byref(row)),'object')
            if row.runtime==0x19a745d7:
                if first_text is None:first_text=(row.offset,row.size)
                inherited=(u*4)();check(lib.spv_graph_renderable(graph,row.id,inherited),'actual Text material');
                handle=lib.spv_text_view_create(graph,row.id);check(handle,'Text create');views.append((row.id,handle,inherited[0]))
        check(len(views)==10,'bounded selected menu contains ten Text resources')
        check(not lib.spv_text_view_create(graph,root.value),'wrong Text target rejects')
        lib.spv_graph_destroy(graph);graph=None
        for identity,handle,material in views:
            info=Info();check(lib.spv_text_view_info(handle,C.byref(info)),'Text outlives external graph handle')
            check((info.vertices,info.indices,info.passes,info.atlas)==(4,6,1,30) and info.material==material,'menu glyph extents and actual custom material/atlas')
            vertices=(Vertex*info.vertices)();indices=(C.c_uint16*info.indices)();draws=(Draw*info.passes)()
            for suffix,buffer in [('vertices',vertices),('indices',indices),('draws',draws)]:
                fn=getattr(lib,'spv_text_view_'+suffix)
                check(fn(handle,buffer,len(buffer)),suffix+' copy')
                canary=(C.c_ubyte*C.sizeof(buffer))(*([0xa5]*C.sizeof(buffer)));before=bytes(canary)
                check(not fn(handle,canary,len(buffer)-1) and bytes(canary)==before,suffix+' short output stays atomic')
                check(not fn(handle,None,len(buffer)),suffix+' missing output guard')
            check(list(indices)==[0,1,2,1,3,2],'actual triangle winding')
            check(all(v.color==0xffd59ae5 for v in vertices),'original vertex color')
            check(all(math.isfinite(n) for v in vertices for n in [*v.position,*v.uv]),'finite upload')
            draw=draws[0]
            check(math.isfinite(draw.colors[16]) if info.powerAssigned else math.isnan(draw.colors[16]),'known versus unknown native power retained explicitly')
            check(draw.textures[0]==30 and list(draw.textures)[1:]==[0]*7,'single canonical Font atlas stage')
            check(draw.vertexAlpha==0 and draw.stages[2]==1 and draw.stages[3]==1,'actual custom menu material retains vertex alpha and address states')
            live=(Draw*info.passes)();check(lib.spv_text_view_capture(handle,2,live,len(live)),'live Text material capture after external graph release')
            check(bytes(live)==bytes(draws),'unchanged actual Text material preserves all draw state words')
            canary=(C.c_ubyte*C.sizeof(live))(*([0xa5]*C.sizeof(live)));before=bytes(canary)
            check(not lib.spv_text_view_capture(handle,3,canary,0) and bytes(canary)==before,'live Text capture guard is atomic')
            report['cases'].append(dict(id=identity,font=info.font,atlas=info.atlas,vertexBytes=bytes(vertices).hex(),
                indexBytes=bytes(indices).hex(),drawSha256=hashlib.sha256(bytes(draws)).hexdigest().upper(),
                material=info.material,powerAssigned=bool(info.powerAssigned),knownRender=draw.knownRender,render=list(draw.render),stages=list(draw.stages[:10])))
        container=lib.spv_container_inspect(C.c_char_p(raw),len(raw));check(container,'container inspection')
        header=(u*9)()
        try:check(lib.spv_container_info(container,header),'container data origin')
        finally:lib.spv_container_destroy(container)
        begin=header[5]+first_text[0];fragment=raw[begin:begin+first_text[1]]
        # The replacement keeps all field lengths and resource identities.
        # Only the existing UInt16-counted two-byte literal becomes two NULs.
        needle=b'\x02\x00\x30\x00'
        check(fragment.count(needle)==1,'bounded fixture finds one exact two-byte menu literal')
        empty=bytearray(raw);empty[begin+fragment.index(needle)+2]=0
        graph=lib.spv_graph_load_for_tools(C.c_char_p(bytes(empty)),len(empty),0);check(graph,'empty Text fixture loads')
        empty_view=lib.spv_text_view_create(graph,views[0][0]);check(empty_view,'empty Text view is valid')
        try:
            info=Info();check(lib.spv_text_view_info(empty_view,C.byref(info)),'empty Text info')
            check(info.vertices==info.indices==info.passes==0,'empty Text produces no material submission or geometry')
            check(lib.spv_text_view_capture(empty_view,2,None,0),'empty live Text draw returns without backend dependencies')
        finally:lib.spv_text_view_destroy(empty_view)
        check(sha(source)==report['sourceSha256'],'source remains unchanged');report['status']='passed'
    except Exception as error:report.update(status='failed',error=str(error))
    finally:
        for _,handle,_ in views:lib.spv_text_view_destroy(handle)
        if graph:lib.spv_graph_destroy(graph)
    report.update(checks=checks,elapsedSeconds=time.perf_counter()-start)
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report.get(k) for k in ('status','checks','error','elapsedSeconds')}));return 0 if report['status']=='passed' else 1

if __name__=='__main__':
    if sys.argv[1:2]==['--child']:raise SystemExit(child(*sys.argv[2:]))
    raise SystemExit(subprocess.run([sys.executable,str(Path(__file__)), '--child',*sys.argv[1:]],cwd=ROOT,timeout=30).returncode)
