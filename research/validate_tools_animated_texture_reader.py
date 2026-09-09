#!/usr/bin/env python3
"""Original texture-track endpoints/ownership and shared inspection ABI."""
from pathlib import Path
import ast,ctypes as C,json,struct,subprocess,sys
from validate_tools_material_reader import dependencies,sha

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'local-data/results/tools-core-cycle-20260909-1900/animated-texture-reader'
MODES=('boundary','duplicate','null','negative','empty')
def original():
    OUT.mkdir(parents=True,exist_ok=True);cases=[];probe=ROOT/'research/probe_pc_anim_texture_track.py'
    for mode in MODES:
        p=subprocess.run([sys.executable,str(probe),'--guest','staged-'+mode],cwd=ROOT,capture_output=True,text=True,timeout=30)
        log=OUT/(mode+'-original.log');log.write_text(p.stdout+p.stderr,encoding='utf-8')
        if p.returncode:raise RuntimeError('Fresh guest failed; no retry: '+str(log))
        marker='ANIM_TRACK_CAPTURE ';line=next(v for v in p.stdout.splitlines() if v.startswith(marker))
        cases.append({'mode':mode,'capture':ast.literal_eval(line[len(marker):]),'log':log.relative_to(ROOT).as_posix(),'log_sha256':sha(log)})
        print('PASS original texture track',mode,flush=True)
    report={'status':'passed','pc_exe_sha256':sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),
        'dependencies':dependencies([Path(__file__),probe]),'cases':cases,
        'limits':'micro64KiB; 100k instructions/2s per original call; 30s process',
        'stream_note':'Original empty FileStream read fails its zero-byte times read; metadata uses a memory-backed stream accepting zero-byte reads. Empty runtime update is never executed.'}
    (OUT/'original.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
class Ref(C.Structure):_fields_=[('offset',C.c_uint32),('size',C.c_uint32)]
class Info(C.Structure):_fields_=[('frames',C.c_uint32),('has_track',C.c_uint32),('duration',C.c_float)]
class Frame(C.Structure):_fields_=[('time',C.c_float),('reference',Ref)]
def compare():
    report=json.loads((OUT/'original.json').read_text(encoding='utf-8'));assert report['status']=='passed'
    assert sha(ROOT/'local-data/pc-pristine/WinxClub.exe')==report['pc_exe_sha256']
    for item in report['dependencies']:assert sha(ROOT/item['path'])==item['sha256'],item['path']
    base=ROOT/'artifacts/native/viewer/Release';dll=C.CDLL(str(base/'SparkplugViewerNative.dll'))
    dll.spv_anim_texture_read.argtypes=[C.c_void_p,C.c_uint32];dll.spv_anim_texture_read.restype=C.c_void_p
    dll.spv_anim_texture_destroy.argtypes=[C.c_void_p];dll.spv_anim_texture_info.argtypes=[C.c_void_p,C.POINTER(Info)]
    dll.spv_anim_texture_frames.argtypes=[C.c_void_p,C.POINTER(Frame),C.c_uint32]
    dll.spv_anim_texture_index.argtypes=[C.c_void_p,C.c_float,C.POINTER(C.c_int32)];dll.spv_last_error.restype=C.c_char_p
    assert C.sizeof(Info)==C.sizeof(Frame)==12
    comparisons=[];samples=0
    for item in report['cases']:
        assert sha(ROOT/item['log'])==item['log_sha256'];mode=item['mode'];capture=item['capture']
        source=json.loads(subprocess.run([str(base/'ViewerMaterialControllerChecks.exe'),'--anim-track',mode],
            capture_output=True,text=True,timeout=20,check=True).stdout);assert source==capture,(source,capture)
        data=bytes.fromhex(capture[1]);raw=C.create_string_buffer(data);handle=dll.spv_anim_texture_read(raw,len(data));assert handle,dll.spv_last_error()
        try:
            info=Info();assert dll.spv_anim_texture_info(handle,C.byref(info));frames=(Frame*info.frames)()
            assert dll.spv_anim_texture_frames(handle,frames,info.frames)
            assert info.has_track==1 and [f.time for f in frames]==capture[2] and info.duration==(capture[2][-1] if capture[2] else 0)
            ids=[struct.unpack_from('<I',data,f.reference.offset)[0] for f in frames];assert ids==capture[3]
            for step in capture[4]:
                index=C.c_int32();assert dll.spv_anim_texture_index(handle,step[3],C.byref(index))
                assert 0<=index.value<len(ids) and ids[index.value]==step[4];samples+=1
            if not frames:
                index=C.c_int32();assert dll.spv_anim_texture_index(handle,0,C.byref(index)) and index.value==-1
            comparisons.append({'mode':mode,'source_full_capture_exact':True,'inspection_exact':True,'original_runtime_samples':len(capture[4]),
                'empty_file_vs_memory_distinction':mode=='empty'})
        finally:dll.spv_anim_texture_destroy(handle)
    # Inline controller/frame body from the already sealed, actual original
    # Material->AnimController->DXTexture guest. No new guest needed.
    old=ROOT/'local-data/results/tools-core-cycle-20260909-1900/material-reader/original.json'
    old_data=json.loads(old.read_text(encoding='utf-8'));assert old_data['status']=='passed'
    for item in old_data['dependencies']:assert sha(ROOT/item['path'])==item['sha256']
    entry=next(v for v in old_data['cases'] if v['family']=='animation_links');assert sha(ROOT/entry['log'])==entry['log_sha256']
    import validate_tools_material_reader as material
    dll.spv_material_read.argtypes=[C.c_void_p,C.c_uint32];dll.spv_material_read.restype=C.c_void_p
    dll.spv_material_destroy.argtypes=[C.c_void_p];dll.spv_material_info.argtypes=[C.c_void_p,C.POINTER(material.Info)]
    dll.spv_material_layers.argtypes=[C.c_void_p,C.POINTER(material.Layer),C.c_uint32]
    data=bytes.fromhex(entry['capture'][1]);raw=C.create_string_buffer(data);handle=dll.spv_material_read(raw,len(data));assert handle
    try:
        info=material.Info();assert dll.spv_material_info(handle,C.byref(info));layers=(material.Layer*info.layers)()
        assert dll.spv_material_layers(handle,layers,info.layers);reference=layers[0].animation
        body=data[reference.offset+8:reference.offset+reference.size]
    finally:dll.spv_material_destroy(handle)
    assert body[:8]==struct.pack('<II',0x16fb0e47,0x4f4f4253);payload=body[8:];raw=C.create_string_buffer(payload)
    handle=dll.spv_anim_texture_read(raw,len(payload));assert handle,dll.spv_last_error()
    try:
        info=Info();assert dll.spv_anim_texture_info(handle,C.byref(info));frames=(Frame*info.frames)()
        assert dll.spv_anim_texture_frames(handle,frames,info.frames)
        assert [frame.time for frame in frames]==[1,2,3]
        assert [struct.unpack_from('<I',payload,frame.reference.offset)[0] for frame in frames]==[9,9,9]
        assert frames[0].reference.size>8 and frames[1].reference.size==frames[2].reference.size==8
    finally:dll.spv_anim_texture_destroy(handle)
    result={'status':'passed','original_report_sha256':sha(OUT/'original.json'),'native_dll_sha256':sha(base/'SparkplugViewerNative.dll'),
        'source_check_exe_sha256':sha(base/'ViewerMaterialControllerChecks.exe'),'cases':comparisons,'runtime_samples':samples,
        'inline_original_report':old.relative_to(ROOT).as_posix(),'inline_original_sha256':sha(old),
        'inline_original_controller_reader':True,'scope':'Original reader/runtime/index/writer captures vs shared source, unresolved frame metadata and unwrapped end-time selection. Full controller looping remains source-tested; ABI selector does not implement a clock. Empty FileStream failure stays distinct from memory inspection.'}
    (OUT/'comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('PASS animated texture:',len(comparisons),'original/source cases;',samples,'original runtime selections; reused original inline graph')
if __name__=='__main__':
    if sys.argv[1:]==['original']:original()
    elif sys.argv[1:]==['compare']:compare()
    else:raise SystemExit('original | compare')
