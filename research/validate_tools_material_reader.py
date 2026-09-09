#!/usr/bin/env python3
"""Selected original material executions -> shared source and inspection ABI.

Original guests have independent 30s process / micro instruction and heap
limits. No game process, device startup, fake referenced object or cap retry.
"""
from pathlib import Path
import ast,ctypes as C,hashlib,json,struct,subprocess,sys

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'local-data/results/tools-core-cycle-20260909-1900/material-reader'
CASES=[('scalar','repeat'),('scalar','values'),('layers','default'),('layers','uv-zero'),
       ('layers','uv-repeat'),('texture_links','null-after'),('animation_links','null-after'),
       ('uv_links','null-after'),('color_links','null-after')]
MARKERS={'scalar':'MATERIAL_CAPTURE','layers':'MATERIAL_LAYER_CAPTURE','texture_links':'MATERIAL_TEXTURE_CAPTURE',
         'animation_links':'MATERIAL_ANIMATION_CAPTURE','uv_links':'MATERIAL_UV_CAPTURE','color_links':'MATERIAL_COLOR_LINK_CAPTURE'}
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def dependencies(paths):
    pending=list(paths);seen=set()
    while pending:
        p=pending.pop().resolve()
        if p in seen:continue
        seen.add(p)
        for node in ast.walk(ast.parse(p.read_text(encoding='utf-8-sig'))):
            names=[node.module] if isinstance(node,ast.ImportFrom) and node.module else [v.name for v in node.names] if isinstance(node,ast.Import) else []
            for name in names:
                target=ROOT/'research'/(name.split('.')[0]+'.py')
                if target.is_file():pending.append(target)
    return [{'path':p.relative_to(ROOT).as_posix(),'sha256':sha(p)} for p in sorted(seen)]
def original():
    OUT.mkdir(parents=True,exist_ok=True);captures=[];paths=[Path(__file__)]
    for family,mode in CASES:
        probe=ROOT/'research'/('probe_pc_material_'+family+'.py');paths.append(probe)
        result=subprocess.run([sys.executable,str(probe),'--guest',mode],cwd=ROOT,
            capture_output=True,text=True,timeout=30)
        log=OUT/(family+'-'+mode+'-original.log');log.write_text(result.stdout+result.stderr,encoding='utf-8')
        if result.returncode:raise RuntimeError('Original guest failed; no retry: '+str(log))
        line=next(line for line in result.stdout.splitlines() if line.startswith(MARKERS[family]+' '))
        value=line[len(MARKERS[family])+1:]
        capture=json.loads(value) if family in ('uv_links','color_links') else ast.literal_eval(value)
        captures.append({'family':family,'mode':mode,'capture':capture,'log':log.relative_to(ROOT).as_posix(),'log_sha256':sha(log)})
        print('PASS original',family,mode,flush=True)
    report={'status':'passed','pc_exe_sha256':sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),
        'dependencies':dependencies(paths),'cases':captures,'limits':'fresh micro guest; 64KiB heap; 100k instructions/2s per call; 30s process'}
    (OUT/'original.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')

class Ref(C.Structure):_fields_=[('offset',C.c_uint32),('size',C.c_uint32)]
class Info(C.Structure):_fields_=[('states',C.c_uint32*11),('alpha',C.c_uint32),('has_color',C.c_uint32),
    ('colors',C.c_uint32*4),('power',C.c_float),('color',Ref),('passes',C.c_uint32),('layers',C.c_uint32)]
class Layer(C.Structure):_fields_=[('pass_index',C.c_uint32),('index',C.c_uint32),('identity',C.c_uint32),
    ('blend',C.c_uint32),('states_field',C.c_int32),('states',C.c_uint32*9),('has_uv',C.c_uint32),
    ('uv_enabled',C.c_uint32),('matrix',C.c_float*9),('texture',Ref),('animation',Ref),('uv',Ref)]
def compare():
    report=json.loads((OUT/'original.json').read_text(encoding='utf-8'))
    assert report['status']=='passed' and sha(ROOT/'local-data/pc-pristine/WinxClub.exe')==report['pc_exe_sha256']
    for item in report['dependencies']:assert sha(ROOT/item['path'])==item['sha256'],item['path']
    base=ROOT/'artifacts/native/viewer/Release';dll=C.CDLL(str(base/'SparkplugViewerNative.dll'))
    dll.spv_material_read.argtypes=[C.c_void_p,C.c_uint32];dll.spv_material_read.restype=C.c_void_p
    dll.spv_material_destroy.argtypes=[C.c_void_p]
    dll.spv_material_info.argtypes=[C.c_void_p,C.POINTER(Info)]
    dll.spv_material_layers.argtypes=[C.c_void_p,C.POINTER(Layer),C.c_uint32]
    dll.spv_last_error.restype=C.c_char_p
    assert (C.sizeof(Info),C.sizeof(Layer))==(88,124)
    checks=[]
    for item in report['cases']:
        assert sha(ROOT/item['log'])==item['log_sha256']
        family,mode,capture=item['family'],item['mode'],item['capture'];data=bytes.fromhex(capture[1])
        raw=C.create_string_buffer(data);handle=dll.spv_material_read(raw,len(data))
        assert handle,(family,mode,dll.spv_last_error())
        try:
            info=Info();assert dll.spv_material_info(handle,C.byref(info));layers=(Layer*info.layers)()
            assert dll.spv_material_layers(handle,layers,info.layers)
            if family in ('scalar','layers','texture_links'):
                switch={'scalar':'--material','layers':'--material-layer','texture_links':'--material-texture'}[family]
                result=subprocess.run([str(base/'ViewerMaterialSerializationChecks.exe'),switch,mode],capture_output=True,text=True,timeout=20,check=True)
                source=json.loads(result.stdout);assert source==capture,(family,source,capture)
            if family=='scalar':
                state=bytes.fromhex(capture[4]);assert bytes(info.states)==state[:44] and info.alpha==state[44]
                assert info.has_color==1 and list(info.colors)==[0x12345678,0xabcdef01,0xff102030,0x87654321] and info.power==3.5
                assert info.passes==capture[5]
            elif family=='layers':
                assert (info.passes,info.layers)==(1,1);layer=layers[0]
                assert (bytes(layer.states)+bytes(layer.matrix)+bytes([layer.uv_enabled])).hex()==capture[2]
                assert (layer.pass_index,layer.index,layer.identity,layer.blend)==(0,0,0x234c576b,2)
            else:
                reference=info.color if family=='color_links' else getattr(layers[0],{'texture_links':'texture','animation_links':'animation','uv_links':'uv'}[family])
                assert reference.size and reference.offset+reference.size<=len(data)
                identity=struct.unpack_from('<I',data,reference.offset)[0]
                assert identity==(0 if family=='texture_links' else 7)
                # Existing original guest asserts actual cleared texture / retained
                # controller ownership before its independent runtime/write checks.
                # UV/color runtime outputs are not confused with read-time state.
            checks.append({'family':family,'mode':mode,'source_writer_exact':family in ('scalar','layers','texture_links'),'inspection_matches':True})
        finally:dll.spv_material_destroy(handle)
    refusals=0
    for data in (b'',b'\xa0\x2c\x00',b'\xa6\x08'+struct.pack('<II',7,32)+b'\0'):
        raw=C.create_string_buffer(data);handle=dll.spv_material_read(raw,len(data));assert not handle;refusals+=1
    result={'status':'passed','original_report_sha256':sha(OUT/'original.json'),
        'native_dll_sha256':sha(base/'SparkplugViewerNative.dll'),'source_check_exe_sha256':sha(base/'ViewerMaterialSerializationChecks.exe'),
        'cases':checks,'host_refusals':refusals,
        'scope':'Material scalar/pass/standard-layer reader metadata and effective reference assignments. Unresolved referenced objects are not loaded. Existing original guests additionally check actual reference ownership; no whole-level/GPU equivalence claim.'}
    (OUT/'comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('PASS material original/source/inspection:',len(checks),'selected cases;',refusals,'host guards')
if __name__=='__main__':
    if sys.argv[1:]==['original']:original()
    elif sys.argv[1:]==['compare']:compare()
    else:raise SystemExit('original | compare')
