#!/usr/bin/env python3
"""Compare shared StaticRenderObject matrix API and serializer with sealed original runs."""
from pathlib import Path
import ctypes as C
import hashlib,json,struct,subprocess,sys

ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
class Field(C.Structure):_fields_=[('field',C.c_uint32),('offset',C.c_uint32),('size',C.c_uint32)]
class Matrices(C.Structure):_fields_=[('world',C.c_float*16),('inverse',C.c_float*16),('mask',C.c_uint32)]

def main(original_path,output):
    original=json.loads(Path(original_path).read_text(encoding='utf-8'))
    assert original['status']=='passed'
    assert sha(ROOT/'local-data/pc-pristine/WinxClub.exe')==original['pc_exe_sha256']
    for item in original['dependencies']:assert sha(ROOT/item['path'])==item['sha256'],item['path']
    base=ROOT/'artifacts/native/viewer/Release';dll=C.CDLL(str(base/'SparkplugViewerNative.dll'))
    dll.spv_static_matrices.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(Field),C.c_uint32,C.POINTER(Matrices)]
    dll.spv_last_error.restype=C.c_char_p
    dll.spv_read_field.argtypes=[C.c_void_p,C.c_uint32,C.c_void_p]
    class Header(C.Structure):_fields_=[('field',C.c_uint32),('size',C.c_uint32),('header_size',C.c_uint32)]
    process=subprocess.run([str(base/'ViewerStaticRenderObjectChecks.exe'),'--roundtrip'],
        input='\n'.join(item['input_hex'] for item in original['cases'])+'\n',
        capture_output=True,text=True,timeout=20,check=True)
    source_lines=process.stdout.splitlines();assert len(source_lines)==len(original['cases'])
    comparisons=[]
    for item,line in zip(original['cases'],source_lines):
        matrices,written=line.split();assert matrices==item['matrices_hex'] and written==item['output_hex']
        data=bytes.fromhex(item['input_hex']);raw=C.create_string_buffer(data);fields=[];offset=0;mask=0
        while offset<len(data):
            header=Header();assert dll.spv_read_field(C.byref(raw,offset),len(data)-offset,C.byref(header))
            offset+=header.header_size
            if header.size==0:break
            if header.field in (1,2):fields.append(Field(header.field,offset,header.size));mask|=1<<header.field
            offset+=header.size
        descriptors=(Field*len(fields))(*fields);result=Matrices()
        assert dll.spv_static_matrices(raw,len(data),descriptors,len(fields),C.byref(result)),dll.spv_last_error().decode()
        assert bytes(result)[:128].hex()==matrices and result.mask==mask
        comparisons.append({'name':item['name'],'matrix_bytes_exact':True,'writer_bytes_exact':True,'field_mask':mask})
    for descriptor in (Field(0,0,64),Field(1,0,63),Field(2,1,64)):
        raw=C.create_string_buffer(64);result=Matrices()
        assert not dll.spv_static_matrices(raw,64,C.byref(descriptor),1,C.byref(result))
    dll.spv_graph_load.argtypes=[C.c_void_p,C.c_uint32];dll.spv_graph_load.restype=C.c_void_p
    dll.spv_graph_destroy.argtypes=[C.c_void_p]
    dll.spv_graph_info.argtypes=[C.c_void_p,C.POINTER(C.c_uint32),C.POINTER(C.c_uint32),C.POINTER(C.c_uint32)]
    # Test-only one-entry FAT envelope around an original-written field stream;
    # this proves host registration, not execution of the original whole loader.
    body=struct.pack('<II',0x56d67170,0x4f4f4253)+bytes.fromhex(original['cases'][0]['output_hex'])
    envelope=struct.pack('<7I',0x53504646,0x26,0,54+len(body),2,54,len(body))
    envelope+=struct.pack('<IIHIII',1,1,0,0x56d67170,0,len(body))+bytes(4)+body
    raw=C.create_string_buffer(envelope);graph=dll.spv_graph_load(raw,len(envelope))
    assert graph,dll.spv_last_error().decode()
    try:
        objects=C.c_uint32();nodes=C.c_uint32();root_id=C.c_uint32()
        assert dll.spv_graph_info(graph,C.byref(objects),C.byref(nodes),C.byref(root_id))
        assert (objects.value,nodes.value,root_id.value)==(1,0,1)
    finally:dll.spv_graph_destroy(graph)
    report={'status':'passed','original_report':str(Path(original_path).resolve().relative_to(ROOT)).replace('\\','/'),
        'original_report_sha256':sha(original_path),'native_dll_sha256':sha(base/'SparkplugViewerNative.dll'),
        'source_check_exe_sha256':sha(base/'ViewerStaticRenderObjectChecks.exe'),'cases':comparisons,'host_refusals':3,
        'host_whole_graph_registration':{'objects':1,'nodes':0,'root_id':1},
        'scope':'Five original full scalar section readers and zero-renderable writers vs source and ABI. Historical original inputs/dependencies verified before reuse. No resource/reference or GPU equivalence claim.'}
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS StaticRenderObject: five exact original/source/ABI cases; three host guards')

if __name__=='__main__':main(*sys.argv[1:])
