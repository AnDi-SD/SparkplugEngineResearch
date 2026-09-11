"""Bounded original Fog submissions -> shared graph C ABI raw-state transport."""
from pathlib import Path
import contextlib,ctypes as C,hashlib,io,json,re,struct,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[1]
DLL=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll'
class Fog(C.Structure):
    _fields_=[(name,C.c_uint32) for name in ('known','enabled','mode','color','start','end','density')]

def envelope(wire):
    raw=struct.pack('<I',0x7ac95aec)+b'SBOO'+wire;name=b'fog\0';origin=32+18+len(name)+4
    return b'FFPS'+struct.pack('<7I',0x26,0,origin+len(raw),2,origin,len(raw),1)+struct.pack('<IH',1,len(name))+name+struct.pack('<III',0x7ac95aec,0,len(raw))+bytes(4)+raw

def child(mode,report_path):
    import probe_pc_renderer_fog as original
    started=time.perf_counter();report=Path(report_path).resolve()
    assert report.is_relative_to(ROOT/'local-data/results') and not report.exists()
    report.parent.mkdir(parents=True,exist_ok=True);original_log=io.StringIO()
    actual_mode=mode
    if mode=='sentinel-linear':
        inputs=original.inputs
        original.inputs=lambda _: [b'\xa0\x14'+struct.pack('<5I',3,0xa5a5a5a5,0xa5a5a5a5,0x42f60000,0x7fc12345)+b'\0',*inputs('linear')[1:]]
        actual_mode='linear'
    with contextlib.redirect_stdout(original_log):capture=original.main(actual_mode,True)
    lib=C.CDLL(str(DLL));u=C.c_uint32;p=C.c_void_p
    lib.spv_graph_load.argtypes=[p,u];lib.spv_graph_load.restype=p
    lib.spv_graph_destroy.argtypes=[p];lib.spv_graph_fog_draw.argtypes=[p,u,C.POINTER(Fog)]
    lib.spv_last_error.restype=C.c_char_p
    raw=envelope(bytes.fromhex(capture[1][0]));graph=lib.spv_graph_load(C.c_char_p(raw),len(raw))
    assert graph,(lib.spv_last_error() or b'').decode();checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    try:
        output=Fog();C.memset(C.byref(output),0xcd,C.sizeof(output));before=bytes(output)
        result=lib.spv_graph_fog_draw(graph,1,C.byref(output))
        if mode=='unknown':check(result==0 and bytes(output)==before,'Rejected Fog capture keeps caller output unchanged')
        else:
            check(result==1,'Actual graph Fog submits through shared renderer')
            words=capture[2][0][2];indices={28:0,34:1,35:2,36:3,37:4,38:5}
            slots={'enabled':(1,28),'mode':(2,35),'color':(4,34),'start':(8,36),'end':(16,37),'density':(32,38)}
            submitted={event[0] for event in capture[2][0][3]}
            # The directed A5 case deliberately equals original cached words:
            # those calls are suppressed, but their cache values are known.
            if mode=='sentinel-linear':submitted={28,34,35,36,37}
            for name,(mask,index) in slots.items():
                check(bool(output.known&mask)==(index in submitted),'Known mask follows actual device writes')
                if index in submitted:check(getattr(output,name)==words[indices[index]],'Raw submitted bits match original PC')
        invalid=Fog();C.memset(C.byref(invalid),0xcd,C.sizeof(invalid));old=bytes(invalid)
        check(lib.spv_graph_fog_draw(graph,999,C.byref(invalid))==0 and bytes(invalid)==old,'Absent Fog ID rejects atomically')
        check(lib.spv_graph_fog_draw(None,1,C.byref(invalid))==0 and bytes(invalid)==old,'Null graph rejects atomically')
        check(lib.spv_graph_fog_draw(graph,1,None)==0,'Null output rejects')
        default=Fog();check(lib.spv_graph_fog_draw(graph,0,C.byref(default))==1 and default.known==1 and default.enabled==0,
            'Host default uses an actual disabled constructor Fog')
        repeated=Fog();check(lib.spv_graph_fog_draw(graph,1,C.byref(repeated))==result and (result==0 or bytes(repeated)==bytes(output)),
            'Read-only repeated snapshot does not mutate graph')
    finally:lib.spv_graph_destroy(graph)
    result=dict(status='passed',mode=mode,checks=checks,original=capture,originalLog=original_log.getvalue(),
        originalChecks=int(re.search(r'PASS (\d+)/',original_log.getvalue())[1]),
        fogDrawHex=bytes(output).hex(),nativeSha256=hashlib.sha256(DLL.read_bytes()).hexdigest().upper(),seconds=time.perf_counter()-started,
        inputBindings=[dict(path=path.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(path.read_bytes()).hexdigest().upper())
            for path in (ROOT/'local-data/pc-pristine/WinxClub.exe',Path(__file__).resolve(),Path(original.__file__).resolve())])
    report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');report.with_suffix('.smo').write_bytes(raw)
    print(json.dumps(dict(mode=mode,checks=checks,seconds=result['seconds'])))
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--child']:raise SystemExit(child(*sys.argv[2:]))
    raise SystemExit(subprocess.run([sys.executable,__file__,'--child',*sys.argv[1:]],timeout=30,cwd=ROOT).returncode)
