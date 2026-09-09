#!/usr/bin/env python3
"""Original Model/Skin sections, common resolved reader and metadata ABI."""
from pathlib import Path
import ctypes as C,json,subprocess,sys
from validate_tools_material_reader import dependencies,sha
from probe_pc_model_fields import MODES as MODEL_MODES
from probe_pc_skin_serializer import specimen

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'local-data/results/tools-core-cycle-20260909-1900/model-skin-reader'
CASES=[('model',mode) for mode in MODEL_MODES]+[('skin',mode) for mode in
       ('empty','one','repeat-bone','repeat-field','clear','raw-bits','null')]
def original():
    OUT.mkdir(parents=True,exist_ok=True);captures=[];paths=[Path(__file__)]
    for family,mode in CASES:
        probe=ROOT/'research'/('probe_pc_model_fields.py' if family=='model' else 'probe_pc_skin_serializer.py');paths.append(probe)
        result=subprocess.run([sys.executable,str(probe),'--guest',mode],cwd=ROOT,capture_output=True,text=True,timeout=30)
        log=OUT/(family+'-'+mode+'-original.log');log.write_text(result.stdout+result.stderr,encoding='utf-8')
        if result.returncode:raise RuntimeError('Original guest failed; no retry: '+str(log))
        marker='MODEL_FIELDS_CAPTURE ' if family=='model' else 'SKIN_CAPTURE '
        line=next(line for line in result.stdout.splitlines() if line.startswith(marker))
        captures.append({'family':family,'mode':mode,'capture':json.loads(line[len(marker):]),
            'log':log.relative_to(ROOT).as_posix(),'log_sha256':sha(log)})
        print('PASS original',family,mode,flush=True)
    report={'status':'passed','pc_exe_sha256':sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),
        'dependencies':dependencies(paths),'cases':captures,
        'scope':'Fresh bounded original Model scalar/null sections and actual Skin inline/shared Node palettes. No fake resource or bone objects.',
        'limits':'30s process; micro64KiB heap; 100k instructions/2s each original call'}
    (OUT/'original.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
class Ref(C.Structure):_fields_=[('offset',C.c_uint32),('size',C.c_uint32)]
class Info(C.Structure):_fields_=[('alpha',C.c_uint32),('priority',C.c_uint32),('projection',C.c_uint32),('weights',C.c_uint32),
    ('renderable_mask',C.c_uint32),('model_mask',C.c_uint32),('skin_mask',C.c_uint32),('bones',C.c_uint32),
    ('material',Ref),('fog',Ref),('mesh',Ref)]
class Bone(C.Structure):_fields_=[('reference',Ref),('id',C.c_uint32),('inline_size',C.c_uint32),('matrix',C.c_float*16)]
def compare():
    report=json.loads((OUT/'original.json').read_text(encoding='utf-8'));assert report['status']=='passed'
    assert sha(ROOT/'local-data/pc-pristine/WinxClub.exe')==report['pc_exe_sha256']
    for item in report['dependencies']:assert sha(ROOT/item['path'])==item['sha256'],item['path']
    base=ROOT/'artifacts/native/viewer/Release';dll=C.CDLL(str(base/'SparkplugViewerNative.dll'))
    dll.spv_model_read.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32];dll.spv_model_read.restype=C.c_void_p
    dll.spv_model_destroy.argtypes=[C.c_void_p];dll.spv_model_info.argtypes=[C.c_void_p,C.POINTER(Info)]
    dll.spv_model_bones.argtypes=[C.c_void_p,C.POINTER(Bone),C.c_uint32];dll.spv_last_error.restype=C.c_char_p
    assert (C.sizeof(Info),C.sizeof(Bone))==(56,80)
    checks=[]
    for item in report['cases']:
        assert sha(ROOT/item['log'])==item['log_sha256']
        family,mode,capture=item['family'],item['mode'],item['capture'];data=bytes.fromhex(capture[1])
        if family=='model':
            source=json.loads(subprocess.run([str(base/'ViewerSkinSerializationChecks.exe'),'--model'],
                input=data.hex()+'\n',capture_output=True,text=True,timeout=20,check=True).stdout)
            assert source==capture[2]+[capture[3]],(source,capture)
        else:
            directory,payload,_=specimen(mode);assert payload==data
            source=json.loads(subprocess.run([str(base/'ViewerSkinSerializationChecks.exe'),'--capture',mode],
                input=directory.hex()+'\n'+payload.hex()+'\n',capture_output=True,text=True,timeout=20,check=True).stdout)
            assert source==capture,(source,capture)
        raw=C.create_string_buffer(data);handle=dll.spv_model_read(raw,len(data),0 if family=='model' else 1)
        if family=='skin' and mode=='null':assert not handle
        else:
            assert handle,(family,mode,dll.spv_last_error())
            try:
                info=Info();assert dll.spv_model_info(handle,C.byref(info));bones=(Bone*info.bones)()
                assert dll.spv_model_bones(handle,bones,info.bones)
                if family=='model':
                    assert [info.alpha,info.priority,info.projection]==capture[2]
                    assert (info.renderable_mask,info.model_mask)=={'empty':(0,0),'priority-only':(8,0),'alpha-wide':(4,0),'repeat':(15,2)}[mode]
                    if mode=='repeat':assert info.material.size==info.fog.size==4
                else:
                    assert (info.weights,info.bones)==(capture[4],capture[5])
                    assert b''.join(bytes(bone.matrix) for bone in bones).hex()==capture[6]
                    assert info.skin_mask==int(mode!='empty')
                    for bone in bones:
                        assert bone.id==7 and bone.reference.offset+bone.reference.size<=len(data)
                        assert bone.reference.size==bone.inline_size+8
            finally:dll.spv_model_destroy(handle)
        checks.append({'family':family,'mode':mode,'source_reader_writer_exact':True,'inspection_matches':True})
    refusals=0
    for kind,data in ((2,b'\0\0'),(0,b'\0'),(1,b'\0\0'),(1,bytes.fromhex('0000a00800000000ffffffff00'))):
        raw=C.create_string_buffer(data);handle=dll.spv_model_read(raw,len(data),kind);assert not handle;refusals+=1
    result={'status':'passed','original_report_sha256':sha(OUT/'original.json'),'native_dll_sha256':sha(base/'SparkplugViewerNative.dll'),
        'source_check_exe_sha256':sha(base/'ViewerSkinSerializationChecks.exe'),'cases':checks,'host_refusals':refusals,
        'scope':'Model scalars and Skin weights, matrix bits, repeated/cleared bone IDs, source complete wire; separate unresolved metadata view. Null bone rejected after matrix. Host releases replacement storage that original abandons. No whole-level/rendering/PS2 claim.'}
    (OUT/'comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('PASS Model/Skin original/source/ABI:',len(checks),'selected cases;',refusals,'host guards')
if __name__=='__main__':
    if sys.argv[1:]==['original']:original()
    elif sys.argv[1:]==['compare']:compare()
    else:raise SystemExit('original | compare')
