#!/usr/bin/env python3
"""Original parent/child PRS world update vs the common tools scene."""
from pathlib import Path
import ctypes as C
import hashlib,json,struct,sys
from pc_instruction_emulator import ROOT,PcInstructions,run_bounded
from probe_pc_node_world import node

class Node(C.Structure):
    _fields_=[('parent',C.c_int32),('position',C.c_float*3),('rotation',C.c_float*4),('scale',C.c_float*3),('billboard',C.c_uint32)]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def main(output):
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    p=PcInstructions();parent=node(p,(10,20,30),(2,3,4));child=node(p,(1,2,3),(.5,2,3))
    q=p.allocate(16);p.put_floats(q,(0,0,.5,.5));p.run(0x420640,child,[q]);quaternion_instructions=sum(p.visits.values())
    p.put_uint(child+0x2c,parent);p.run(0x421420,parent,[0]);parent_instructions=sum(p.visits.values())
    p.run(0x421420,child,[0]);child_instructions=sum(p.visits.values())
    out=p.allocate(64);p.run(0x461d70,out,[child+0x74,child+0x8c,child+0x80]);matrix_instructions=sum(p.visits.values())
    expected=bytes(p.mu.mem_read(out,64))
    path=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll';dll=C.CDLL(str(path));dll.spv_last_error.restype=C.c_char_p
    dll.spv_scene_create.argtypes=[C.POINTER(Node),C.c_uint32];dll.spv_scene_create.restype=C.c_void_p
    dll.spv_scene_sample.argtypes=[C.c_void_p,C.c_float,C.POINTER(C.c_float),C.c_uint32]
    dll.spv_scene_destroy.argtypes=[C.c_void_p]
    nodes=(Node*2)(Node(-1,(10,20,30),(0,0,0,1),(2,3,4),0),Node(0,(1,2,3),(0,0,.5,.5),(.5,2,3),0))
    scene=dll.spv_scene_create(nodes,2);assert scene,dll.spv_last_error().decode()
    try:
        result=(C.c_float*32)();assert dll.spv_scene_sample(scene,0,result,32),dll.spv_last_error().decode()
        assert bytes(result)[64:]==expected
        values=struct.unpack('<16f',expected)
        assert values==(.5,.5,0,0,-3,3,0,0,0,0,12,0,12,26,42,1)
    finally:dll.spv_scene_destroy(scene)
    report={'status':'passed','pc_exe_sha256':sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),'native_dll_sha256':sha(path),
        'native_world_hex':expected.hex(),'native_world':values,'native_instructions':{'quaternion_setter':quaternion_instructions,
        'parent_world':parent_instructions,'child_world':child_instructions,'affine_builder':matrix_instructions},'guest_arena_bytes':p.allocated,
        'dependencies':sorted(({'path':str(Path(m.__file__).resolve().relative_to(ROOT)).replace('\\','/'),'sha256':sha(m.__file__)}
            for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda x:x['path']),
        'scope':'Original quaternion setter/world/affine functions with explicit bounded Node-layout inputs. Guest does not claim factories, attachment or destructor execution. Tool scene uses actual reconstructed Node owners. Nonunit quaternion and nonuniform parent scale are deliberately exercised.'}
    target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS original/tools parent-child world matrix: exact float bits, nonunit quaternion/nonuniform scale');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
