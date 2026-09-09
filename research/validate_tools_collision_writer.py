#!/usr/bin/env python3
"""Fresh original MeshBV construction/read/write/teardown vs typed tools writer."""
from pathlib import Path
import contextlib,ctypes as C,hashlib,io,json,struct,sys
from pc_instruction_emulator import ROOT,run_bounded
import probe_pc_mesh_bv_core as fixture
from validate_tools_mesh_writer import Vertex

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def main(output):
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results');target.parent.mkdir(parents=True,exist_ok=True)
    dllpath=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll';dll=C.CDLL(str(dllpath));dll.spv_last_error.restype=C.c_char_p
    dll.spv_mesh_bv_write_triangles.argtypes=[C.POINTER(Vertex),C.c_uint32,C.POINTER(C.c_uint32),C.c_uint32];dll.spv_mesh_bv_write_triangles.restype=C.c_void_p
    dll.spv_serialized_bytes_size.argtypes=[C.c_void_p,C.POINTER(C.c_uint32)]
    dll.spv_serialized_bytes_copy.argtypes=[C.c_void_p,C.POINTER(C.c_ubyte),C.c_uint32];dll.spv_serialized_bytes_destroy.argtypes=[C.c_void_p]
    field=fixture.field;cases=[]
    for name,positions,indices in (
        ('triangle',[(0,0,0),(2,0,0),(0,4,0)],[0,1,2]),
        ('tetrahedron',[(0,0,0),(2,0,0),(0,4,0),(0,0,6)],[0,2,1,0,1,3,0,3,2,1,2,3])):
        geometry=struct.pack('<III',2,len(indices)//3,0)+struct.pack('<'+'H'*len(indices),*indices)+struct.pack('<III',0,len(positions),0)+b''.join(struct.pack('<3f',*v) for v in positions)
        # Explicit input-only extension; original fixture still executes all owners and native methods.
        fixture.field=lambda identity,payload:field(identity,geometry if identity==0 else payload)
        original_path=target.parent/('original-'+name+'.json')
        with contextlib.redirect_stdout(io.StringIO()):fixture.main('geometry',str(original_path))
        original=json.loads(original_path.read_text(encoding='utf-8'))
        vertices=(Vertex*len(positions))()
        for v,position in zip(vertices,positions):v.p[:]=position
        ids=(C.c_uint32*len(indices))(*indices)
        owner=dll.spv_mesh_bv_write_triangles(vertices,len(vertices),ids,len(ids));assert owner,dll.spv_last_error().decode()
        try:
            size=C.c_uint32();assert dll.spv_serialized_bytes_size(owner,C.byref(size))
            result=(C.c_ubyte*size.value)();assert dll.spv_serialized_bytes_copy(owner,result,size.value)
            actual=bytes(result);assert actual==struct.pack('<I4s',0x3f453de7,b'SBOO')+bytes.fromhex(original['written'])
            cases.append({'name':name,'vertices':len(vertices),'triangles':len(indices)//3,'original_report':str(original_path.relative_to(ROOT)).replace('\\','/'),
                'original_report_sha256':sha(original_path),'tool_object_sha256':hashlib.sha256(actual).hexdigest().upper(),'original_released_allocations':original['releasedAllocations']})
        finally:dll.spv_serialized_bytes_destroy(owner)
    fixture.field=field
    for count,ids in ((0,[0,1,2]),(3,[0,1,3]),(3,[0,1])):
        vertices=(Vertex*3)();indices=(C.c_uint32*len(ids))(*ids)
        assert not dll.spv_mesh_bv_write_triangles(vertices,count,indices,len(ids))
    report={'status':'passed','pc_exe_sha256':sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),'native_dll_sha256':sha(dllpath),'cases':cases,'host_rejections':3,
        'dependencies':sorted(({'path':str(Path(m.__file__).resolve().relative_to(ROOT)).replace('\\','/'),'sha256':sha(m.__file__)} for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda x:x['path']),
        'scope':'Original MeshBV factory, actual reader/tree preparation, writer438680 and full owner teardown; source exposes ownership/bounds only, not OPCODE collision queries. Geometry-only output; face records are not generated.'}
    target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print('PASS MeshBV writer: two original byte matches, three host refusals');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
