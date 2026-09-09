"""Selected C ABI material-clock guards and cross-consumer playback checks.

Actual controller/pass algorithms reuse the original-PC proofs. This validates
the host boundary, not a complete AnimationManager or game frame.
"""
from pathlib import Path
import argparse, ctypes as C, hashlib, json, sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/SanToVmd'))
import sparkplug_native as native
from validate_tools_loaded_materials import Material, Pass, Layer
U, F, H = C.c_uint32, C.c_float, C.c_void_p
class Clock(C.Structure):
    _fields_ = [('class_id', U), ('accumulated', F), ('applied', F), ('playback', F), ('has_playback', U), ('enabled', U)]
class Submission(C.Structure):
    _fields_ = [('stage', U), ('matrix', F * 9)]
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()

def main():
    parser = argparse.ArgumentParser(); parser.add_argument('managed', type=Path); parser.add_argument('output', type=Path)
    args = parser.parse_args(); report = json.loads(args.managed.read_text(encoding='utf-8'))
    assert report['status'] == 'passed' and sha(report['source']) == report['source_sha256']
    assert sha(ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll') == report['native_dll_sha256']
    lib = native.library()
    signatures = {
        'spv_graph_controller_clock': [H,U,C.POINTER(Clock)],
        'spv_graph_apply_controllers': [H,C.POINTER(U),U,F],
        'spv_graph_update_material_color': [H,U,U,U,C.POINTER(U)],
        'spv_graph_update_material_pass': [H,U,U,C.POINTER(Submission),U,C.POINTER(U)],
        'spv_graph_material': [H,U,C.POINTER(Material)],
        'spv_graph_pass': [H,U,U,C.POINTER(Pass)],
        'spv_graph_layer': [H,U,U,U,C.POINTER(Layer)]}
    for name, params in signatures.items():
        function = getattr(lib,name); function.argtypes = params; function.restype = C.c_int
    guards = checks = 0
    def reject(value):
        nonlocal guards
        assert value == 0 and lib.spv_last_error(); guards += 1
    def clock(handle, identity):
        result=Clock(); native.check(lib.spv_graph_controller_clock(handle,identity,C.byref(result))); return result
    data=Path(report['source']).read_bytes()
    with native.Graph(data) as graph:
        h=graph._get()
        controllers=[entry.id for _,entry,_ in graph.objects if entry.runtime_class in (0x16FB0E47,0x1C0053D6)]
        materials=[entry.id for _,entry,_ in graph.objects if entry.runtime_class == 0x797B39EC]
        assert controllers and materials
        controller=controllers[0]; material=materials[0]; wrong=0xFFFFFFFF
        output=Clock(); count=U(); evaluated=U(); submissions=(Submission*8)()
        reject(lib.spv_graph_controller_clock(None,controller,C.byref(output)))
        reject(lib.spv_graph_controller_clock(h,controller,None))
        reject(lib.spv_graph_controller_clock(h,wrong,C.byref(output)))
        reject(lib.spv_graph_controller_clock(h,material,C.byref(output)))
        before=bytes(clock(h,controller)); selected=(U*1)(controller)
        for identities,length,delta in [(None,1,1),(selected,4097,1),(selected,1,float('nan')),(selected,1,float('inf')),
                ((U*2)(controller,controller),2,1),((U*2)(controller,wrong),2,1)]:
            reject(lib.spv_graph_apply_controllers(h,identities,length,delta))
            assert bytes(clock(h,controller)) == before; checks += 1
        native.check(lib.spv_graph_apply_controllers(h,None,0,0)); checks += 1
        reject(lib.spv_graph_update_material_color(h,material,1,2,C.byref(evaluated)))
        reject(lib.spv_graph_update_material_color(h,material,1,0,None))
        reject(lib.spv_graph_update_material_color(h,controller,1,0,C.byref(evaluated)))
        reject(lib.spv_graph_update_material_pass(h,material,0,submissions,8,None))
        reject(lib.spv_graph_update_material_pass(h,material,0xFFFFFFFF,submissions,8,C.byref(count)))
        reject(lib.spv_graph_update_material_pass(h,material,0,submissions,9,C.byref(count)))
        reject(lib.spv_graph_update_material_pass(h,material,0,None,8,C.byref(count)))
        reject(lib.spv_graph_update_material_pass(h,wrong,0,submissions,8,C.byref(count)))
        # Pick one actual owning UV layer; capture its original callback matrix.
        selected_uv=None
        for material in materials:
            info=Material();native.check(lib.spv_graph_material(h,material,C.byref(info)))
            for p in range(info.passes):
                info_pass=Pass();native.check(lib.spv_graph_pass(h,material,p,C.byref(info_pass)))
                for layer_index in range(info_pass.layers):
                    layer=Layer();native.check(lib.spv_graph_layer(h,material,p,layer_index,C.byref(layer)))
                    if layer.uv_controller and layer.uv_bound_here: selected_uv=(material,p,layer_index,layer.uv_controller);break
                if selected_uv:break
            if selected_uv:break
        if selected_uv:
            material,p,index,controller=selected_uv
            native.check(lib.spv_graph_apply_controllers(h,(U*1)(controller),1,F(.125)))
            reject(lib.spv_graph_update_material_pass(h,material,p,None,0,C.byref(count)))
            assert clock(h,controller).applied == 0;checks += 1
            native.check(lib.spv_graph_update_material_pass(h,material,p,submissions,8,C.byref(count)))
            layer=Layer();native.check(lib.spv_graph_layer(h,material,p,index,C.byref(layer)))
            actual=next(value for value in submissions[:count.value] if value.stage == index)
            assert bytes(actual.matrix) == bytes(layer.uv);checks += 1
            assert clock(h,controller).applied == F(.125).value;checks += 1
    # Replay the managed AnimTex input sequence through an independent binding.
    events=report['animation_events']
    for identity in dict.fromkeys(row['controller'] for row in events):
        with native.Graph(data) as graph:
            h=graph._get(); target=None
            for _,entry,_ in graph.objects:
                if entry.runtime_class != 0x797B39EC:continue
                info=Material();native.check(lib.spv_graph_material(h,entry.id,C.byref(info)))
                for p in range(info.passes):
                    pass_info=Pass();native.check(lib.spv_graph_pass(h,entry.id,p,C.byref(pass_info)))
                    for index in range(pass_info.layers):
                        layer=Layer();native.check(lib.spv_graph_layer(h,entry.id,p,index,C.byref(layer)))
                        if layer.animation == identity and layer.animation_bound_here:target=(entry.id,p,index)
            assert target
            for row in (value for value in events if value['controller'] == identity):
                native.check(lib.spv_graph_apply_controllers(h,(U*1)(identity),1,F(row['delta'])))
                submissions=(Submission*8)(); count=U()
                native.check(lib.spv_graph_update_material_pass(h,target[0],target[1],submissions,8,C.byref(count)))
                state=clock(h,identity)
                for field in ('Accumulated','Applied','Playback'):
                    assert getattr(state,field.lower()) == F(row[field]).value;checks += 1
                layer=Layer();native.check(lib.spv_graph_layer(h,*target,C.byref(layer)))
                assert layer.texture == (row['texture'] or 0);checks += 1
    result={'status':'passed','managed_report':str(args.managed),'managed_sha256':sha(args.managed),
            'guards':guards,'checks':checks,'animation_events':len(events),'native_dll_sha256':report['native_dll_sha256']}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result))

if __name__ == '__main__':main()
