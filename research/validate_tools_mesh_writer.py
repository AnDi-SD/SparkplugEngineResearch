#!/usr/bin/env python3
"""Fresh original mesh writer vs the typed tool bridge, within guest limits."""
from pathlib import Path
import ctypes as C
import hashlib,json,struct,sys
from pc_instruction_emulator import ROOT,run_bounded
import probe_pc_mesh_writer as fixture

# Extend only the fixture input bytes. All original construction, readers,
# writer calls and destructor/ownership checks remain in the existing harness.
base_specimen=fixture.specimen
def specimen(mode):
    if mode=='carrier':
        return (struct.pack('<III3H',2,1,0,0,0,0),struct.pack('<III3f4fI3fI4f',
            0x197e,1,0,0,0,0,1,0,0,0,0,0,1,0,0x00ffffff,0,0,0,0))
    if mode!='skinned':return base_specimen(mode)
    ib,_=base_specimen('triangle')
    vb=struct.pack('<III',0x197e,3,0)+b''.join(struct.pack('<3f4fI3fI4f',
        i,i+1,i+2,.1,.2,.3,.4,0xff030201,0,0,2,0xaabbccdd,.2,.4,.6,.8) for i in range(3))
    return ib,vb
fixture.specimen=specimen

class Vertex(C.Structure):
    _fields_=[('p',C.c_float*3),('n',C.c_float*3),('uv0',C.c_float*2),('uv1',C.c_float*2),('weights',C.c_float*4),('color',C.c_uint32),('bones',C.c_uint32)]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def main(output):
    dllpath=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll';dll=C.CDLL(str(dllpath))
    dll.spv_last_error.restype=C.c_char_p
    dll.spv_mesh_write_triangles.argtypes=[C.POINTER(Vertex),C.c_uint32,C.POINTER(C.c_uint32),C.c_uint32,C.c_uint32,C.c_uint32]
    dll.spv_mesh_write_triangles.restype=C.c_void_p
    dll.spv_serialized_bytes_size.argtypes=[C.c_void_p,C.POINTER(C.c_uint32)]
    dll.spv_serialized_bytes_copy.argtypes=[C.c_void_p,C.POINTER(C.c_ubyte),C.c_uint32]
    dll.spv_serialized_bytes_destroy.argtypes=[C.c_void_p]
    cases=[]
    for mode in ('triangle','layout','packed','skinned','carrier'):
        for kind in ((1,) if mode=='carrier' else (0,1)):
            capture=fixture.main(mode,str(kind),True,base=kind==0)
            ib,vb,expected=[bytes.fromhex(v) for v in capture[2:5]]
            flags,count,_=struct.unpack_from('<III',vb);vertices=(Vertex*count)();stride=(len(vb)-12)//count
            for i,v in enumerate(vertices):
                start=12+i*stride;v.p[:]=struct.unpack_from('<3f',vb,start)
                if mode=='layout':v.n[:]=struct.unpack_from('<3f',vb,start+12);v.uv0[:]=struct.unpack_from('<2f',vb,start+24)
                if mode=='packed':v.bones=struct.unpack_from('<I',vb,start+12)[0]
                if mode in ('skinned','carrier'):
                    v.weights[:]=struct.unpack_from('<4f',vb,start+12);v.bones=struct.unpack_from('<I',vb,start+28)[0]
                    v.n[:]=struct.unpack_from('<3f',vb,start+32);v.color=struct.unpack_from('<I',vb,start+44)[0]
                    v.uv0[:]=struct.unpack_from('<2f',vb,start+48);v.uv1[:]=struct.unpack_from('<2f',vb,start+56)
            indices=(C.c_uint32*3)(*struct.unpack_from('<3H',ib,12))
            owner=dll.spv_mesh_write_triangles(vertices,count,indices,3,flags,kind);assert owner,dll.spv_last_error().decode()
            try:
                size=C.c_uint32();assert dll.spv_serialized_bytes_size(owner,C.byref(size))
                buffer=(C.c_ubyte*size.value)();assert dll.spv_serialized_bytes_copy(owner,buffer,size.value)
                actual=bytes(buffer);assert actual[:8]==struct.pack('<I4s',0x33c34cf0,b'SBOO')
                assert actual[8:]==expected,(mode,kind,expected.hex(),actual.hex())
                cases.append({'mode':mode,'kind':kind,'flags':flags,'original_fields_hex':expected.hex(),'tool_object_sha256':hashlib.sha256(actual).hexdigest().upper()})
            finally:dll.spv_serialized_bytes_destroy(owner)
    for flags,kind,count,idx in ((2,0,3,[0,1,2]),(0x80000000,0,3,[0,1,2]),(0,2,3,[0,1,2]),(0,0,3,[0,1,3]),(0,0,0,[0,1,2])):
        v=(Vertex*3)();i=(C.c_uint32*3)(*idx)
        assert not dll.spv_mesh_write_triangles(v,count,i,3,flags,kind),'Expected explicit host refusal'
    report={'status':'passed','pc_exe_sha256':sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),'native_dll_sha256':sha(dllpath),'cases':cases,'host_rejections':5,
        'dependencies':sorted(({'path':str(Path(m.__file__).resolve().relative_to(ROOT)).replace('\\','/'),'sha256':sha(m.__file__)} for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda x:x['path']),
        'scope':'Fresh original CPU IB/VB readers, MeshData owner initialization, base/DX whole field writer, complete owner cleanup. Tool host maps typed attributes using original layout; only UInt16 triangle lists, flags0 buffer storage. No GPU or combined-buffer cached size claim.'}
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results');target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS mesh writers: nine exact original field streams, five host refusals');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
