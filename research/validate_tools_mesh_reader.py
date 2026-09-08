#!/usr/bin/env python3
"""Fresh bounded original PC mesh reader vs the tools ABI on identical bytes."""
from pathlib import Path
import ctypes as C
import hashlib,json,struct,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_mesh_serializer import main as original

class Info(C.Structure):
    _fields_=[(name,C.c_uint32) for name in ('field','field_offset','index_offset','vertex_offset',
        'primitive','primitives','vertices','indices','index_width','stride','runtime_stride',
        'runtime_bytes','flags','attributes','plan0','plan1','plan2','plan3','plan_byte')]
class Vertex(C.Structure):
    _fields_=[('position',C.c_float*3),('normal',C.c_float*3),('uv0',C.c_float*2),
              ('uv1',C.c_float*2),('weights',C.c_float*4),('color',C.c_uint32),('bones',C.c_uint32)]
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def main(mode,output):
    if mode not in ('dx-cross','dx-native','dx-both','dx-packed','dx-unknown'):raise ValueError('Explicit mode required')
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    native=original(mode,True)
    dllpath=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll'
    dll=C.CDLL(str(dllpath));dll.spv_abi_version.restype=C.c_uint32
    assert dll.spv_abi_version()==2 and C.sizeof(Info)==76 and C.sizeof(Vertex)==64
    dll.spv_last_error.restype=C.c_char_p
    dll.spv_mesh_read.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,C.c_uint32];dll.spv_mesh_read.restype=C.c_void_p
    dll.spv_mesh_destroy.argtypes=[C.c_void_p]
    dll.spv_mesh_info.argtypes=[C.c_void_p,C.POINTER(Info)]
    dll.spv_mesh_vertices.argtypes=[C.c_void_p,C.POINTER(Vertex),C.c_uint32]
    dll.spv_mesh_indices.argtypes=[C.c_void_p,C.POINTER(C.c_uint32),C.c_uint32]
    def check(result):
        if not result:raise AssertionError(dll.spv_last_error().decode())
    data=bytes.fromhex(native[1]);payload=C.create_string_buffer(data[8:])
    handle=dll.spv_mesh_read(payload,len(data)-8,0,1 if mode=='dx-cross' else 2);check(handle)
    try:
        info=Info();check(dll.spv_mesh_info(handle,C.byref(info)))
        assert (info.primitive,info.primitives,info.vertices,info.indices)==(2,1,3,3)
        assert (info.runtime_stride,info.runtime_bytes,info.indices*info.index_width)==(native[4],native[5],native[6])
        vertices=(Vertex*info.vertices)();indices=(C.c_uint32*info.indices)()
        check(dll.spv_mesh_vertices(handle,vertices,info.vertices));check(dll.spv_mesh_indices(handle,indices,info.indices))
        assert list(indices)==list(struct.unpack('<3H',bytes.fromhex(native[9])))
        runtime=bytes.fromhex(native[8])
        for index,vertex in enumerate(vertices):
            values=struct.unpack_from('<'+str(info.runtime_stride//4)+'f',runtime,index*info.runtime_stride)
            assert tuple(vertex.position)==values[:3]
            if mode=='dx-packed':
                assert tuple(float(v) for v in struct.pack('<I',vertex.bones))==values[3:7]
            else:
                assert tuple(vertex.normal)==values[3:6] and tuple(vertex.uv0)==values[6:8]
        assert info.stride==(16 if mode=='dx-packed' else 32)
        record={'status':'passed','mode':mode,'input_sha256':hashlib.sha256(data).hexdigest().upper(),
            'native_capture':native,'native_dll_sha256':sha(dllpath),
            'source_info':{name:getattr(info,name) for name,_ in Info._fields_},
            'dependencies':sorted(({'path':str(Path(m.__file__).resolve().relative_to(ROOT)).replace('\\','/'),
                                    'sha256':sha(m.__file__)} for m in list(sys.modules.values())
                                   if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),
                                  key=lambda item:item['path']),
            'scope':'Fresh original standalone mesh reader and native teardown; same bytes through tools ABI. Bounded COM fixture, no GPU or whole-file claim.'}
    finally:dll.spv_mesh_destroy(handle)
    target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
    print('PASS original/tools mesh',mode,flush=True);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
