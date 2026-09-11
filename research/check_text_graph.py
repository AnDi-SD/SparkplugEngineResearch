"""Bounded strict/tool-policy comparison and actual graph Text/Font C ABI checks."""
from pathlib import Path
import ctypes as C,hashlib,json,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[1];DLL=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll'
u=C.c_uint32;p=C.c_void_p
class Object(C.Structure):_fields_=[(k,u) for k in ('id','wire','runtime','offset','size','node')]
class Text(C.Structure):_fields_=[(k,u) for k in ('font','color','wrap','alignment','width','textBytes','present','boundsMask')]+[(k,C.c_float*n) for k,n in (('sphere',4),('minimum',3),('maximum',3))]
class Font(C.Structure):_fields_=[(k,u) for k in ('height','baseline','present','image')]
class Glyph(C.Structure):_fields_=[('width',u),('uv0',C.c_float*2),('uv1',C.c_float*2)]
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def child(source_name,output_name):
    source=Path(source_name).resolve();output=Path(output_name).resolve()
    assert source.is_relative_to(ROOT) and output.is_relative_to(ROOT/'local-data/results') and not output.exists()
    started=time.perf_counter();lib=C.CDLL(str(DLL));raw=source.read_bytes();checks=0
    lib.spv_last_error.restype=C.c_char_p
    for name,args,restype in [('spv_graph_load',[p,u],p),('spv_graph_load_for_tools',[p,u,u],p),('spv_graph_destroy',[p],None),
      ('spv_graph_info',[p,p,p,p],C.c_int),('spv_graph_object',[p,u,p,u,p],C.c_int),
      ('spv_graph_legacy_texture_ids',[p,p,u,p],C.c_int),('spv_graph_text',[p,u,p],C.c_int),
      ('spv_graph_text_bytes',[p,u,p,u],C.c_int),('spv_graph_text_node',[p,u,p],C.c_int),
      ('spv_graph_font',[p,u,p,p,u],C.c_int),('spv_graph_scene_all',[p],p),('spv_scene_destroy',[p],None),
      ('spv_scene_node_count',[p,p],C.c_int),('spv_scene_sample',[p,C.c_float,p,u],C.c_int),
      ('spv_graph_reference_trace_info',[p,p,p,p],C.c_int)]:
        fn=getattr(lib,name);fn.argtypes=args;fn.restype=restype
    def check(value,message='native call'):
        nonlocal checks
        checks+=1
        if not value:raise RuntimeError(message+': '+(lib.spv_last_error()or b'').decode('utf-8'))
    report=dict(status='running',source=source.relative_to(ROOT).as_posix(),sourceSha256=sha(source),nativeSha256=sha(DLL),
      scriptSha256=sha(__file__),scope='Actual loaded classes and declared tool compatibility; not an original full-file run',texts=[],fonts=[],textNodes=[])
    graph=scene=None
    try:
        strict=lib.spv_graph_load(C.c_char_p(raw),len(raw));report['strictAccepted']=bool(strict)
        report['strictError']=None if strict else (lib.spv_last_error()or b'').decode('utf-8')
        if strict:lib.spv_graph_destroy(strict)
        graph=lib.spv_graph_load_for_tools(C.c_char_p(raw),len(raw),1);check(graph,'tool graph load')
        count=u();check(lib.spv_graph_legacy_texture_ids(graph,None,0,C.byref(count)))
        ids=(u*count.value)();check(lib.spv_graph_legacy_texture_ids(graph,ids,count.value,C.byref(count)))
        report['legacyTextureIds']=list(ids);check(bool(ids)!=report['strictAccepted'],'strict versus explicit compatibility distinction')
        canary=u(0xa5a5a5a5)
        if len(ids):check(not lib.spv_graph_legacy_texture_ids(graph,None,1,C.byref(canary)) and canary.value==0xa5a5a5a5,'compatibility output guard')
        objects,nodes,root=u(),u(),u();check(lib.spv_graph_info(graph,C.byref(objects),C.byref(nodes),C.byref(root)))
        report.update(objects=objects.value,nodes=nodes.value,root=root.value)
        rows={};name=C.create_string_buffer(65536)
        for index in range(objects.value):
            item=Object();check(lib.spv_graph_object(graph,index,name,len(name),C.byref(item)));rows[item.id]=item
        for row in rows.values():
            if row.runtime==0x4693490a:
                info=Font();glyphs=(Glyph*224)();check(lib.spv_graph_font(graph,row.id,C.byref(info),glyphs,224))
                check(not info.image or rows[info.image].runtime==0x3f3651b6,'Font holds actual runtime DXTexture')
                report['fonts'].append(dict(id=row.id,image=info.image,height=info.height,baseline=info.baseline if info.present else None,
                  zeroWidth=glyphs[16].width,glyphSha256=hashlib.sha256(bytes(glyphs)).hexdigest().upper()))
                out=(C.c_ubyte*C.sizeof(Font))(*([0xa5]*C.sizeof(Font)));before=bytes(out)
                check(not lib.spv_graph_font(graph,row.id,out,glyphs,223) and bytes(out)==before,'atomic wrong Font glyph count')
            if row.runtime==0x19a745d7:
                info=Text();check(lib.spv_graph_text(graph,row.id,C.byref(info)));stored=(C.c_ubyte*info.textBytes)()
                check(info.present<=1 and info.boundsMask<=3 and info.textBytes<=65535,'Text scalar guards')
                check(lib.spv_graph_text_bytes(graph,row.id,stored,info.textBytes))
                check(not info.font or rows[info.font].runtime==0x4693490a,'Text retains actual Font')
                report['texts'].append(dict(id=row.id,font=info.font,text=bytes(stored).hex(),width=info.width,color=info.color,
                  wrap=info.wrap,alignment=info.alignment,boundsMask=info.boundsMask,sphere=list(info.sphere),minimum=list(info.minimum),maximum=list(info.maximum)))
                out=(C.c_ubyte*(info.textBytes+1))(*([0xa5]*(info.textBytes+1)));before=bytes(out)
                check(not lib.spv_graph_text_bytes(graph,row.id,out,len(out)) and bytes(out)==before,'atomic wrong text extent')
            if row.runtime==0x52e86efe:
                text=u();check(lib.spv_graph_text_node(graph,row.id,C.byref(text)))
                check(not text.value or rows[text.value].runtime==0x19a745d7,'TextNode cached actual Text')
                report['textNodes'].append(dict(id=row.id,text=text.value))
        fontById={row['id']:row for row in report['fonts']}
        for text in report['texts']:
            check(text['text']=='30' and text['width']==fontById[text['font']]['zeroWidth'],'selected original menu literal zero width matches glyph')
            check(text['boundsMask']==3,'loaded menu text bounds are defined')
        if report['texts']:check(len(report['texts'])==len(report['fonts'])==len(report['textNodes'])==10,'ten original menu text chains')
        bad=(C.c_ubyte*72)(*([0xa5]*72));before=bytes(bad)
        check(not lib.spv_graph_text(graph,root.value,bad) and bytes(bad)==before,'wrong Text target guard')
        refs,payloads,origin=u(),u(),u();check(lib.spv_graph_reference_trace_info(graph,C.byref(refs),C.byref(payloads),C.byref(origin)))
        report['trace']=dict(references=refs.value,payloads=payloads.value,origin=origin.value)
        scene=lib.spv_graph_scene_all(graph);check(scene,'create scene');nodeCount=u();check(lib.spv_scene_node_count(scene,C.byref(nodeCount)))
        matrices=(C.c_float*(nodeCount.value*16))();check(lib.spv_scene_sample(scene,0,matrices,len(matrices)))
        report['sceneNodes']=nodeCount.value
        check(not lib.spv_graph_load_for_tools(C.c_char_p(raw),len(raw),2),'invalid trace flag rejects before loading')
        check(sha(source)==report['sourceSha256'],'source file unchanged')
        report['status']='passed'
    except Exception as error:report.update(status='failed',error=str(error))
    finally:
        if scene:lib.spv_scene_destroy(scene)
        if graph:lib.spv_graph_destroy(graph)
    report.update(checks=checks,elapsedSeconds=time.perf_counter()-started)
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report.get(k) for k in ('status','error','checks','legacyTextureIds','objects','nodes','elapsedSeconds')}))
    return 0 if report['status']=='passed' else 1
if __name__=='__main__':
    if sys.argv[1:2]==['--child']:raise SystemExit(child(*sys.argv[2:]))
    raise SystemExit(subprocess.run([sys.executable,str(Path(__file__)), '--child',*sys.argv[1:]],cwd=ROOT,timeout=30).returncode)
