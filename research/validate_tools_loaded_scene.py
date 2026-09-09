"""Compare loaded Node/Skin/support views across C# and Python C ABI consumers.

This is a bridge/lifetime/pose test, not a whole original-game frame test.
Original Node/RenderNode/Skin algorithms reuse their established proofs.
"""
from pathlib import Path
import argparse,ctypes as C,hashlib,json,struct,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'tools/SanToVmd'),str(ROOT/'research'),str(ROOT/'local-data/research-cache/python')]
import sparkplug_native as native
from inspect_serializer_manager import read_pe,image_slice,PC_SHA256
U,F,H=C.c_uint32,C.c_float,C.c_void_p
class Container(C.Structure):
    _fields_=[('id',U),('kind',U),('members',U),('world',F*16),('inverse',F*16)]
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def main():
    parser=argparse.ArgumentParser();parser.add_argument('source',type=Path);parser.add_argument('output',type=Path);args=parser.parse_args()
    report=json.loads(args.source.read_text(encoding='utf-8'));assert report['status']=='passed' and report['file_sha256']==sha(report['file'])
    dll=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll';assert report['native_dll_sha256']==sha(dll)
    lib=native.library()
    signatures={
        'spv_graph_scene_all':(H,[H]),'spv_scene_node_count':(C.c_int,[H,C.POINTER(U)]),
        'spv_scene_graph_node_ids':(C.c_int,[H,C.POINTER(U),U]),
        'spv_scene_sample':(C.c_int,[H,F,C.POINTER(F),U]),
        'spv_scene_graph_skin_info':(C.c_int,[H,U,C.POINTER(U),C.POINTER(U)]),
        'spv_scene_graph_skin_palette':(C.c_int,[H,U,C.POINTER(F),U]),
        'spv_graph_render_containers':(C.c_int,[H,C.POINTER(Container),U,C.POINTER(U)]),
        'spv_graph_render_members':(C.c_int,[H,U,C.POINTER(U),U]),
    }
    for name,(result,arguments) in signatures.items():getattr(lib,name).restype=result;getattr(lib,name).argtypes=arguments
    assert C.sizeof(Container)==140
    guards=[]
    def refuses(name,result):
        assert not result,name
        guards.append({'case':name,'error':lib.spv_last_error().decode('utf-8')})
    graph=native.Graph(Path(report['file']).read_bytes());scene=None;animation=None
    try:
        scene=native.check(lib.spv_graph_scene_all(graph._get()))
        count=U();native.check(lib.spv_scene_node_count(scene,C.byref(count)))
        ids=(U*count.value)();native.check(lib.spv_scene_graph_node_ids(scene,ids,count))
        assert list(ids)==[node['id'] for node in report['nodes']]
        names={entry.id:name for name,entry,_ in graph.objects}
        matrices=(F*(count.value*16))();native.check(lib.spv_scene_sample(scene,0,matrices,len(matrices)))
        def compare_worlds(expected):
            raw=bytes(matrices)
            assert len(expected)==len(ids)
            for i,row in enumerate(expected):assert row['id']==ids[i] and row['world_hex']==raw[i*64:(i+1)*64].hex().upper(),row['id']
        compare_worlds(report['nodes'])
        containers_count=U();native.check(lib.spv_graph_render_containers(graph._get(),None,0,C.byref(containers_count)))
        containers=(Container*containers_count.value)();native.check(lib.spv_graph_render_containers(graph._get(),containers,len(containers),C.byref(containers_count)))
        assert len(containers)==len(report['containers'])
        membership_count=0
        for actual,expected in zip(containers,report['containers']):
            assert actual.id==expected['id'] and ('RenderNode','StaticRenderObject','PartitionRenderable')[actual.kind]==expected['kind']
            assert bytes(actual.world).hex().upper()==expected['world_hex'] and bytes(actual.inverse).hex().upper()==expected['inverse_hex']
            members=(U*actual.members)();native.check(lib.spv_graph_render_members(graph._get(),actual.id,members,len(members)))
            assert list(members)==expected['members'];membership_count+=len(members)
        refuses('membership-count',lib.spv_graph_render_members(graph._get(),containers[0].id,None,0xFFFFFFFF))
        refuses('unknown-container',lib.spv_graph_render_members(graph._get(),0xFFFFFFFF,None,0))
        refuses('container-capacity',lib.spv_graph_render_containers(graph._get(),containers,len(containers)+1,C.byref(containers_count)))
        refuses('node-id-count',lib.spv_scene_graph_node_ids(scene,None,0xFFFFFFFF))
        refuses('missing-count-output',lib.spv_scene_node_count(scene,None))
        # Actual scene owns all resources after the public graph handle closes.
        graph.close()
        native.check(lib.spv_scene_sample(scene,0,matrices,len(matrices)));compare_worlds(report['nodes'])
        palette_count=0
        def compare_palettes(rows):
            nonlocal palette_count
            for row in rows:
                weights,bones=U(),U();native.check(lib.spv_scene_graph_skin_info(scene,row['id'],C.byref(weights),C.byref(bones)))
                output=(F*(bones.value*16))();native.check(lib.spv_scene_graph_skin_palette(scene,row['id'],output,len(output)))
                assert bones.value==len(row['matrices'])
                assert bytes(output).hex().upper()==''.join(row['matrices'])
                palette_count+=bones.value
        compare_palettes(report['skins'])
        if report['skins']:
            refuses('skin-palette-count',lib.spv_scene_graph_skin_palette(scene,report['skins'][0]['id'],None,0xFFFFFFFF))
        refuses('unknown-skin',lib.spv_scene_graph_skin_info(scene,0xFFFFFFFF,C.byref(U()),C.byref(U())))
        if report['san']:
            assert sha(report['san']['path'])==report['san']['sha256']
            animation=native.Animation(Path(report['san']['path']).read_bytes())
            # This selected profile has no competing curves for a name/role.
            # Binding conflict policies are not reimplemented by this test.
            by_name={}
            for track in animation.tracks:
                for role,channel in track.channels.items():
                    if channel.source_keys:
                        assert (track.name,role) not in by_name
                        by_name[track.name,role]=track.ordinal
            roles=(C.c_int32*(len(ids)*3))(*(by_name.get((names[identifier],role),-1) for identifier in ids for role in range(3)))
            assert any(value>=0 for value in roles)
            native.check(lib.spv_scene_bind(scene,animation._get(),roles,len(roles)))
            for frame in report['frames']:
                native.check(lib.spv_scene_sample(scene,frame['time'],matrices,len(matrices)))
                compare_worlds(frame['worlds']);compare_palettes(frame['palettes'])
    finally:
        if scene:lib.spv_scene_destroy(scene)
        if animation:animation.close()
        graph.close()
    # Bounded static PC anchors for the derived-node dispatch used above.
    exe=ROOT/'local-data/pc-pristine/WinxClub.exe';assert sha(exe)==PC_SHA256
    data=exe.read_bytes();base,sections=read_pe(data)
    def read(address,size):return image_slice(data,sections,address-base,size)
    anchors=[]
    for name,slot,target in [('spZone',0x6EBAFC,0x421420),('spZonePortalNode',0x6EBB74,0x421420),('spPartitionSystem',0x6EC558,0x48E710)]:
        raw=read(slot,4);assert struct.unpack('<I',raw)[0]==target
        anchors.append({'class':name,'world_vtable_slot_va':hex(slot),'target_va':hex(target),'bytes':raw.hex()})
    thunk=read(0x48E710,5);assert thunk[0]==0xE9 and 0x48E715+struct.unpack('<i',thunk[1:])[0]==0x4250F0
    anchors[-1]['tail_jump_bytes']=thunk.hex();anchors[-1]['world_implementation_va']='0x4250f0'
    result={'status':'passed','source':str(args.source),'source_sha256':sha(args.source),'native_dll_sha256':sha(dll),
        'pc_sha256':sha(exe),'static_derived_world_anchors':anchors,'nodes':len(report['nodes']),
        'supports':len(report['containers']),'members':membership_count,'palette_matrices':palette_count,
        'san_frames':len(report['frames']),'guards':guards,
        'scope':'Exact Node/Skin/support matrix bits, including signed zero; graph-handle teardown before sampling; null camera/no Scene visibility/gameplay tick.'}
    args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({key:result[key] for key in ('nodes','supports','members','palette_matrices','san_frames')}))
if __name__=='__main__':main()
