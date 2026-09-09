#!/usr/bin/env python3
"""Original CollisionInfo scalar reader and affine matrix vs common tool API."""
from pathlib import Path
import ctypes as C,hashlib,json,struct,sys
from pc_instruction_emulator import ROOT,run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_node_serializer import field
from probe_pc_collision_core import cleanup,state
from validate_tools_node_world import Node

class Field(C.Structure):_fields_=[('field',C.c_uint32),('offset',C.c_uint32),('size',C.c_uint32)]
class Values(C.Structure):
    _fields_=[('p',C.c_float*3),('q',C.c_float*4),('s',C.c_float*3),('r',C.c_float*9),('group',C.c_uint32),('mask',C.c_uint32)]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def main(output):
    path=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll';dll=C.CDLL(str(path));dll.spv_last_error.restype=C.c_char_p
    dll.spv_collision_info_values.argtypes=[C.POINTER(C.c_ubyte),C.c_uint32,C.POINTER(Field),C.c_uint32,C.POINTER(Values)]
    dll.spv_node_local.argtypes=[C.POINTER(Node),C.POINTER(C.c_float),C.c_uint32]
    def transform(q,p=(7,8,9),s=(2,3,4)):return struct.pack('<10f',*p,*q,*s)
    nonunit=transform((0,0,.5,.5));zero=transform((0,0,0,0));cases=[]
    for name,items in (
        ('defaults',[]),('nonunit',[(2,nonunit)]),('zero-quaternion',[(2,zero)]),
        ('group-only',[(1,struct.pack('<I',0xffffffff))]),
        ('unknown-repeated',[(1,struct.pack('<I',7)),(2,nonunit),(9,b'\xaa'),(2,zero),(1,bytes(4))])):
        wire=b'';descriptors=[];expected_q=(0,0,0,1);mask=0
        for identity,payload in items:
            encoded=field(identity,payload)
            if identity in (1,2):descriptors.append(Field(identity,len(wire)+len(encoded)-len(payload),len(payload)));mask|=1<<identity
            if identity==2:expected_q=struct.unpack_from('<4f',payload,12)
            wire+=encoded
        wire+=b'\0';f=PCWriteBytesFixture(wire);p=f.p;f.call(0x6d38e0)
        info=f.call(0x4653a0);serializer=f.call(0x438960)
        assert f.call(0x438a80,this=serializer+0x10,args=(f.stream,info))&255==1
        assert f.position==len(wire) and not f.errors
        instructions=sum(p.visits.values());original=state(f,info)
        matrix=p.allocate(64);f.call(0x461d70,this=matrix,args=(info+0x20,info+0x2c,info+0x50));original_matrix=bytes(p.mu.mem_read(matrix,64))
        raw=(C.c_ubyte*len(wire)).from_buffer_copy(wire);fields=(Field*len(descriptors))(*descriptors);value=Values()
        assert dll.spv_collision_info_values(raw,len(wire),fields,len(fields),C.byref(value)),dll.spv_last_error().decode()
        assert value.group==original['group'] and value.mask==mask
        assert struct.pack('<15f',*value.p,*value.r,*value.s)==struct.pack('<15f',*original['position'],*original['orientation'],*original['scale'])
        assert tuple(value.q)==expected_q
        node=Node(-1,tuple(value.p),tuple(value.q),tuple(value.s),0)
        actual=(C.c_float*16)();assert dll.spv_node_local(C.byref(node),actual,16)
        assert bytes(actual)==original_matrix
        cleanup(f,[info,serializer])
        cases.append({'name':name,'input_hex':wire.hex(),'original':original,'raw_quaternion':expected_q,'matrix_hex':original_matrix.hex(),
            'reader_instructions':instructions,'released_allocations':len(f.freed),'arena_bytes':p.allocated})
    for descriptor in (Field(0,0,4),Field(2,0,39),Field(2,1,40)):
        raw=(C.c_ubyte*40)();value=Values();assert not dll.spv_collision_info_values(raw,40,C.byref(descriptor),1,C.byref(value))
    report={'status':'passed','pc_exe_sha256':sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),'native_dll_sha256':sha(path),'cases':cases,'host_rejections':3,
        'dependencies':sorted(({'path':str(Path(m.__file__).resolve().relative_to(ROOT)).replace('\\','/'),'sha256':sha(m.__file__)} for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda x:x['path']),
        'scope':'Fresh original CollisionInfo/serializer factories, whole scalar field reader, affine builder and complete owning teardown. No primitive reference fixture, attached Node, collision query or GPU. Tool raw quaternion observation does not change actual Matrix3 state.'}
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results');target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS CollisionInfo: five original scalar states and matrices exact, three descriptor refusals');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
