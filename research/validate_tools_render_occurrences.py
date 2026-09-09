"""Verify actual support-slot scene projection across independent C ABI consumers.

This checks integration and host identities, not a full native visibility frame.
Model/Skin render semantics reuse the original-PC/source evidence.
"""
from pathlib import Path
import argparse,ctypes as C,hashlib,json,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/SanToVmd'))
import sparkplug_native as native
U,F,H=C.c_uint32,C.c_float,C.c_void_p
class Container(C.Structure):
    _fields_=[('id',U),('kind',U),('members',U),('world',F*16),('inverse',F*16)]
class Occurrence(C.Structure):
    _fields_=[('renderable',U),('rigid_node',U),('world',F*16)]
class Model(C.Structure):
    _fields_=[(name,U) for name in ('mesh','material','fog','alpha','priority','projection')]
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def main():
    parser=argparse.ArgumentParser();parser.add_argument('source',type=Path);parser.add_argument('output',type=Path);args=parser.parse_args()
    report=json.loads(args.source.read_text(encoding='utf-8'))
    assert report['status']=='passed' and report['file_sha256']==sha(report['file'])
    dll=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll'
    assert report['native_dll_sha256']==sha(dll)
    lib=native.library()
    signatures={
        'spv_graph_scene_all':(H,[H]),'spv_scene_node_count':(C.c_int,[H,C.POINTER(U)]),
        'spv_scene_sample':(C.c_int,[H,F,C.POINTER(F),U]),
        'spv_graph_render_containers':(C.c_int,[H,C.POINTER(Container),U,C.POINTER(U)]),
        'spv_graph_render_members':(C.c_int,[H,U,C.POINTER(U),U]),
        'spv_graph_render_occurrence':(C.c_int,[H,U,U,C.POINTER(Occurrence)]),
        'spv_graph_model':(C.c_int,[H,U,C.POINTER(Model)]),
    }
    for name,(result,arguments) in signatures.items():getattr(lib,name).restype=result;getattr(lib,name).argtypes=arguments
    assert C.sizeof(Occurrence)==72 and C.sizeof(Container)==140
    guards=[]
    def refuses(name,result):
        assert not result,name
        guards.append({'case':name,'error':lib.spv_last_error().decode('utf-8')})
    graph=native.Graph(Path(report['file']).read_bytes());scene=None
    try:
        scene=native.check(lib.spv_graph_scene_all(graph._get()))
        node_count=U();native.check(lib.spv_scene_node_count(scene,C.byref(node_count)))
        worlds=(F*(16*node_count.value))();native.check(lib.spv_scene_sample(scene,0,worlds,len(worlds)))
        count=U();native.check(lib.spv_graph_render_containers(graph._get(),None,0,C.byref(count)))
        containers=(Container*count.value)();native.check(lib.spv_graph_render_containers(graph._get(),containers,len(containers),C.byref(count)))
        expected={(row['container'],row['slot']):row for row in report['placements']}
        unsupported={(row['container'],row['slot']):row for row in report['unsupported']}
        assert len(expected)==report['occurrences']
        visited=set();rejected=[];seen=[]
        for container in containers:
            members=(U*container.members)();native.check(lib.spv_graph_render_members(graph._get(),container.id,members,len(members)))
            for slot,member in enumerate(members):
                key=(container.id,slot);value=Occurrence()
                if key in unsupported:
                    refuses('unsupported-member',lib.spv_graph_render_occurrence(graph._get(),container.id,slot,C.byref(value)))
                    assert unsupported[key]['renderable']==member;rejected.append(key);continue
                native.check(lib.spv_graph_render_occurrence(graph._get(),container.id,slot,C.byref(value)))
                row=expected[key];assert value.renderable==row['renderable']==member
                assert value.rigid_node==row['rigid_node'] and bytes(value.world).hex().upper()==row['world_hex']
                model=Model();native.check(lib.spv_graph_model(graph._get(),member,C.byref(model)))
                assert model.mesh==row['mesh'] and model.material==row['material']
                visited.add(key);seen.append(member)
        assert visited==set(expected) and set(rejected)==set(unsupported)
        value=Occurrence();first=containers[0]
        refuses('slot-range',lib.spv_graph_render_occurrence(graph._get(),first.id,first.members,C.byref(value)))
        refuses('unknown-container',lib.spv_graph_render_occurrence(graph._get(),0xFFFFFFFF,0,C.byref(value)))
        refuses('missing-output',lib.spv_graph_render_occurrence(graph._get(),first.id,0,None))
        refuses('wrong-container-class',lib.spv_graph_render_occurrence(graph._get(),report['placements'][0]['renderable'],0,C.byref(value)))
        repeated={value:seen.count(value) for value in set(seen) if seen.count(value)>1}
        assert repeated=={value['renderable']:value['count'] for value in report['repeated']}
        result={'status':'passed','source':str(args.source),'source_sha256':sha(args.source),
            'native_dll_sha256':sha(dll),'occurrences':len(visited),'distinct_renderables':len(set(seen)),
            'repeated_renderables':len(repeated),'unsupported_members':len(rejected),'guards':guards,
            'scope':'Exact actual support slot/Model mesh/material identity and input matrix, no native visibility/draw-order claim.'}
        args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        print(json.dumps({k:result[k] for k in ('status','occurrences','distinct_renderables','repeated_renderables','unsupported_members')}))
    finally:
        if scene:lib.spv_scene_destroy(scene)
        graph.close()
if __name__=='__main__':main()
