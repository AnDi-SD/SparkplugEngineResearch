"""Bounded common-runtime mip transport checks; selected textures, no corpus sweep."""
from pathlib import Path
import ctypes as C, hashlib, json, subprocess, sys, time
from check_text_graph import Object, ROOT, DLL, sha, u, p
class Texture(C.Structure):_fields_=[(n,u) for n in ('width','height','format','mips')]
class Mip(C.Structure):_fields_=[(n,u) for n in ('width','height','bytes')]
def child(source_name,output_name):
    source=Path(source_name).resolve();output=Path(output_name).resolve()
    assert source.is_relative_to(ROOT) and output.is_relative_to(ROOT/'local-data/results') and not output.exists()
    started=time.perf_counter();raw=source.read_bytes();lib=C.CDLL(str(DLL));checks=0;graph=None
    report=dict(status='running',source=source.relative_to(ROOT).as_posix(),sourceSha256=sha(source),nativeSha256=sha(DLL),
        scriptSha256=sha(__file__),textures=[],scope='At most ten actual runtime textures, complete bounded mip outputs')
    lib.spv_last_error.restype=C.c_char_p
    for name,args,result in [('spv_graph_load_for_tools',[p,u,u],p),('spv_graph_destroy',[p],None),
        ('spv_graph_info',[p,p,p,p],C.c_int),('spv_graph_object',[p,u,p,u,p],C.c_int),('spv_graph_texture',[p,u,p],C.c_int),
        ('spv_graph_texture_bgra',[p,u,p,u],C.c_int),('spv_graph_texture_mip_info',[p,u,u,p],C.c_int),
        ('spv_graph_texture_mip_bgra',[p,u,u,p,u],C.c_int)]:
        fn=getattr(lib,name);fn.argtypes=args;fn.restype=result
    def check(value,label):
        nonlocal checks
        checks+=1
        if not value:raise RuntimeError(label+': '+(lib.spv_last_error()or b'').decode())
    try:
        graph=lib.spv_graph_load_for_tools(C.c_char_p(raw),len(raw),0);check(graph,'graph load')
        count,nodes,root=u(),u(),u();check(lib.spv_graph_info(graph,C.byref(count),C.byref(nodes),C.byref(root)),'graph info')
        name=C.create_string_buffer(65536);candidates=[]
        for index in range(count.value):
            row=Object();check(lib.spv_graph_object(graph,index,name,len(name),C.byref(row)),'object metadata')
            if row.runtime==0x3f3651b6:
                info=Texture();check(lib.spv_graph_texture(graph,row.id,C.byref(info)),'runtime texture info')
                candidates.append((row.id,index,info))
        groups={}
        for row in candidates:groups.setdefault(tuple(getattr(row[2],n) for n,_ in Texture._fields_),row)
        selected=list(groups.values())[:10]
        for row in candidates:
            if len(selected)<10 and row not in selected:selected.append(row)
        report.update(availableTextures=len(candidates),selectedTextures=len(selected))
        check(bool(selected),'at least one actual texture selected')
        for identity,index,texture in selected:
            check(texture.format in (3,4) and 0<texture.mips<=16,'selected BGRA runtime chain bounds')
            result=dict(id=identity,objectIndex=index,width=texture.width,height=texture.height,format=texture.format,mips=[])
            width,height=texture.width,texture.height
            for level in range(texture.mips):
                mip=Mip();check(lib.spv_graph_texture_mip_info(graph,identity,level,C.byref(mip)),'mip metadata')
                check((mip.width,mip.height,mip.bytes)==(width,height,width*height*4) and mip.bytes<=16*1024*1024,'mip extent')
                pixels=(C.c_ubyte*mip.bytes)();check(lib.spv_graph_texture_mip_bgra(graph,identity,level,pixels,mip.bytes),'mip pixels')
                if level==0:
                    old=(C.c_ubyte*mip.bytes)();check(lib.spv_graph_texture_bgra(graph,identity,old,mip.bytes) and bytes(old)==bytes(pixels),'existing base API is byte-identical')
                canary=(C.c_ubyte*min(mip.bytes,64))(*([0xa5]*min(mip.bytes,64)));before=bytes(canary)
                check(not lib.spv_graph_texture_mip_bgra(graph,identity,level,canary,mip.bytes-1) and bytes(canary)==before,'short pixel output remains atomic')
                check(not lib.spv_graph_texture_mip_bgra(graph,identity,level,None,mip.bytes),'missing pixel output guard')
                result['mips'].append(dict(level=level,width=width,height=height,bytes=mip.bytes,sha256=hashlib.sha256(pixels).hexdigest().upper()))
                width=max(1,width//2);height=max(1,height//2)
            check(result['mips'][-1]['width']==result['mips'][-1]['height']==1,'actual runtime chain ends at one pixel')
            out=(C.c_ubyte*C.sizeof(Mip))(*([0xa5]*C.sizeof(Mip)));before=bytes(out)
            check(not lib.spv_graph_texture_mip_info(graph,identity,texture.mips,out) and bytes(out)==before,'past-last mip metadata is atomic')
            check(not lib.spv_graph_texture_mip_bgra(graph,identity,0xffffffff,out,4) and bytes(out)==before,'invalid mip pixels are atomic')
            report['textures'].append(result)
        out=(C.c_ubyte*12)(*([0xa5]*12));before=bytes(out)
        check(not lib.spv_graph_texture_mip_info(graph,root.value,0,out) and bytes(out)==before,'wrong target type guard')
        check(sha(source)==report['sourceSha256'],'source remains unchanged');report['status']='passed'
    except Exception as error:report.update(status='failed',error=str(error))
    finally:
        if graph:lib.spv_graph_destroy(graph)
    report.update(checks=checks,elapsedSeconds=time.perf_counter()-started)
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report.get(k) for k in ('status','checks','error','selectedTextures','elapsedSeconds')}));return 0 if report['status']=='passed' else 1
if __name__=='__main__':
    if sys.argv[1:2]==['--child']:raise SystemExit(child(*sys.argv[2:]))
    raise SystemExit(subprocess.run([sys.executable,str(Path(__file__)), '--child',*sys.argv[1:]],cwd=ROOT,timeout=30).returncode)
