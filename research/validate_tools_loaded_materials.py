"""Validate the C ABI upload projection against managed consumers on chosen files.

The original reader/constructor proofs are reused from the preceding blocks;
this test checks the host bridge and never substitutes game resource factories.
"""
from pathlib import Path
import argparse, ctypes as C, hashlib, json, sqlite3

ROOT = Path(__file__).resolve().parents[1]
U, F, H = C.c_uint32, C.c_float, C.c_void_p

class Model(C.Structure):
    _fields_ = [(name, U) for name in ('mesh', 'material', 'fog', 'alpha', 'priority', 'projection')]
class Material(C.Structure):
    _fields_ = [('states', U * 11), ('alpha', U), ('power_initialized', U),
                ('colors', F * 16), ('power', F), ('controller', U), ('passes', U)]
class Pass(C.Structure):
    _fields_ = [('blend', U), ('layers', U)]
class Layer(C.Structure):
    _fields_ = [(name, U) for name in ('class_id', 'texture', 'animation', 'uv_controller',
                                    'uv_enabled', 'animation_bound_here', 'uv_bound_here')] + [('states', U * 12), ('uv', F * 9)]
class Texture(C.Structure):
    _fields_ = [(name, U) for name in ('width', 'height', 'format', 'mips')]
class Key(C.Structure):
    _fields_ = [('time', F), ('texture', U)]

def sha(data): return hashlib.sha256(data).hexdigest().upper()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--managed-report', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    managed = json.loads(args.managed_report.read_text(encoding='utf-8'))
    source = Path(managed['file']); data = source.read_bytes()
    assert managed['status'] == 'passed' and sha(data) == managed['file_sha256']
    dll = ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll'
    lib = C.CDLL(str(dll))
    signatures = {
        'last_error': (C.c_char_p, []), 'graph_load': (H, [C.POINTER(C.c_uint8), U]), 'graph_destroy': (None, [H]),
        'graph_model': (C.c_int, [H, U, C.POINTER(Model)]), 'graph_material': (C.c_int, [H, U, C.POINTER(Material)]),
        'graph_pass': (C.c_int, [H, U, U, C.POINTER(Pass)]), 'graph_layer': (C.c_int, [H, U, U, U, C.POINTER(Layer)]),
        'graph_texture': (C.c_int, [H, U, C.POINTER(Texture)]),
        'graph_texture_bgra': (C.c_int, [H, U, C.POINTER(C.c_uint8), U]),
        'graph_texture_track': (C.c_int, [H, U, C.POINTER(U), C.POINTER(F)]),
        'graph_texture_keys': (C.c_int, [H, U, C.POINTER(Key), U]),
    }
    for name, (result, arguments) in signatures.items():
        function = getattr(lib, 'spv_' + name); function.restype = result; function.argtypes = arguments
    assert [C.sizeof(cls) for cls in (Model, Material, Pass, Layer, Texture, Key)] == [24, 128, 8, 112, 16, 8]
    def check(value):
        if not value: raise ValueError(lib.spv_last_error().decode('utf-8'))
        return value
    with sqlite3.connect((ROOT/'local-data/results/smo-corpus-v2.sqlite').as_uri()+'?mode=ro', uri=True) as db:
        file_id, file_sha = db.execute('select id,sha256 from files where corpus_id=2 and relative_path=? collate nocase',
                             (source.relative_to(ROOT/'local-data/pc-pristine/Media').as_posix(),)).fetchone()
        assert file_sha.upper() == sha(data)
        entries = {row[0]: {'index': row[1], 'name': row[2], 'class_id': row[3]} for row in
                   db.execute('select object_id,object_index,name,type_hash from objects where file_id=?', (file_id,))}
    pointer = check(lib.spv_graph_load((C.c_uint8 * len(data)).from_buffer_copy(data), len(data)))
    models, materials, tracks, guards, textures = [], {}, {}, [], []
    try:
        # Model/Skin IDs come from the SQLite class registry, not name patterns.
        with sqlite3.connect((ROOT/'local-data/results/smo-corpus-v2.sqlite').as_uri()+'?mode=ro', uri=True) as db:
            model_classes = {row[0] for row in db.execute("select type_hash from classes where engine_name in ('spModel','spSkin')")}
        for identifier, entry in entries.items():
            if entry['class_id'] not in model_classes: continue
            model = Model(); check(lib.spv_graph_model(pointer, identifier, C.byref(model)))
            models.append({'id': identifier, 'mesh': model.mesh, 'material': model.material})
            if not model.material or model.material in materials: continue
            material = Material(); check(lib.spv_graph_material(pointer, model.material, C.byref(material)))
            passes = []
            for p in range(material.passes):
                native_pass = Pass(); check(lib.spv_graph_pass(pointer, model.material, p, C.byref(native_pass)))
                layers = []
                for l in range(native_pass.layers):
                    layer = Layer(); check(lib.spv_graph_layer(pointer, model.material, p, l, C.byref(layer)))
                    layers.append({name: getattr(layer, name) for name in ('class_id', 'texture', 'animation', 'uv_controller', 'uv_enabled', 'animation_bound_here', 'uv_bound_here')})
                    if layer.animation and layer.animation not in tracks:
                        count, duration = U(), F(); check(lib.spv_graph_texture_track(pointer, layer.animation, C.byref(count), C.byref(duration)))
                        keys = (Key * count.value)(); check(lib.spv_graph_texture_keys(pointer, layer.animation, keys, count))
                        tracks[layer.animation] = {'duration': duration.value, 'keys': [{'time': key.time, 'texture': key.texture} for key in keys]}
                passes.append({'blend': native_pass.blend, 'layers': layers})
            materials[model.material] = {'index': entries[model.material]['index'], 'states': list(material.states),
                'colors': list(material.colors), 'alpha': material.alpha, 'power_initialized': material.power_initialized,
                'power': material.power if material.power_initialized else None, 'passes': passes}
        assert len(models) == managed['models'] and len(materials) == managed['materials']
        for expected in managed['textures']:
            value = Texture(); check(lib.spv_graph_texture(pointer, expected['id'], C.byref(value)))
            pixels = (C.c_uint8 * (value.width * value.height * 4))()
            result = lib.spv_graph_texture_bgra(pointer, expected['id'], pixels, len(pixels))
            if expected['issue']:
                assert not result
                textures.append({'id': expected['id'], 'guard': lib.spv_last_error().decode('utf-8')})
            else:
                check(result)
                assert (value.width, value.height, sha(bytes(pixels))) == (expected['width'], expected['height'], expected['sha256'])
                textures.append({'id': expected['id'], 'format': value.format, 'mips': value.mips, 'sha256': sha(bytes(pixels))})
        def refuses(name, result):
            assert not result, name
            guards.append({'case': name, 'error': lib.spv_last_error().decode('utf-8')})
        first_model, first_material = models[0]['id'], next(iter(materials))
        refuses('missing-model-output', lib.spv_graph_model(pointer, first_model, None))
        refuses('unknown-resource-id', lib.spv_graph_model(pointer, 0xFFFFFFFF, C.byref(Model())))
        refuses('wrong-runtime-class', lib.spv_graph_material(pointer, first_model, C.byref(Material())))
        refuses('pass-range', lib.spv_graph_pass(pointer, first_material, 0xFFFFFFFF, C.byref(Pass())))
        refuses('layer-range', lib.spv_graph_layer(pointer, first_material, 0, 0xFFFFFFFF, C.byref(Layer())))
        if managed['textures']:
            tiny = (C.c_uint8 * 1)(0xA5)
            refuses('pixel-extent', lib.spv_graph_texture_bgra(pointer, managed['textures'][0]['id'], tiny, 1))
            assert tiny[0] == 0xA5
        if tracks:
            identifier = next(iter(tracks))
            refuses('track-key-count', lib.spv_graph_texture_keys(pointer, identifier, None, 0xFFFFFFFF))
    finally:
        lib.spv_graph_destroy(pointer)
    report = {'status': 'passed', 'file': str(source), 'file_sha256': sha(data), 'native_dll_sha256': sha(dll.read_bytes()),
              'managed_report': str(args.managed_report), 'managed_report_sha256': sha(args.managed_report.read_bytes()),
              'models': models, 'materials': materials, 'tracks': tracks, 'textures': textures, 'guards': guards}
    args.output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'models': len(models), 'materials': len(materials), 'textures': len(textures), 'tracks': len(tracks), 'guards': len(guards)}))

if __name__ == '__main__': main()
