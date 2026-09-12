"""Compare original PC mesh loading/color bytes with the shared recovered source.

Real original internal routines execute to return. Only file input, allocation,
initialized renderer backing and COM storage are declared harness boundaries.
This is CPU/declaration evidence, not a rendered-pixel or whole-startup test.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import time

from pc_instruction_emulator import ROOT
from probe_pc_dx_mesh_payload import MeshFixture


def fixtures():
    colors = (0x00336699, 0x8012A5EF, 0xFFE08020)
    for packed in (False, True):
        for uv_count in (1, 2):
            flags = (0x97e if packed else 0x940) | (0x1000 if uv_count == 2 else 0)
            vertices = bytearray()
            for i, position in enumerate(((0, 0, 0), (1, 0, 0), (0, 1, 0))):
                vertices += struct.pack('<3f', *position)
                if packed:
                    vertices += struct.pack('<4f4B', .4, .3, .2, .1, 0, 3, 7, 15)
                vertices += struct.pack('<3fI2f', 0, 2, 0, colors[i], i*.25, i*.125)
                if uv_count == 2:
                    vertices += struct.pack('<2f', .75-i*.125, .5+i*.25)
            indices = struct.pack('<3H', 0, 1, 2)
            # Original helper ignores this planning header. Use conflicting
            # values so comparison cannot merely trust redundant metadata.
            wire = (struct.pack('<IIIIB', 0xdeadbeef, 999, 777, 555, 0xa5)
                    + struct.pack('<III', 2, 1, 0) + indices
                    + struct.pack('<III', flags, 3, 0) + vertices)
            yield dict(name=f'{"packed" if packed else "plain"}-{uv_count}uv',
                       wire=wire, input=bytes(vertices), indices=indices,
                       count=3, flags=flags, color_offset=44 if packed else 24)
    asset = ROOT/'local-data/pc-pristine/Media/Menus/logo_screen.smo'
    raw = asset.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != 'dbd6a1f261008bbf1c2971030517b7c9d60a5e27f58a4a69f7c14eaf10e2e3c7':
        raise AssertionError('Original logo SMO fixture changed')
    yield dict(name='original-logo-field', wire=raw[484:677], input=raw[533:677],
               indices=raw[513:521], count=4, flags=0x940, color_offset=24,
               asset=str(asset.relative_to(ROOT)), assetSha256=digest)


def tools_capture(case, path):
    import ctypes as C
    from validate_tools_mesh_reader import Info, Vertex
    dll = C.CDLL(str(path.resolve()))
    dll.spv_abi_version.restype = C.c_uint32
    dll.spv_last_error.restype = C.c_char_p
    dll.spv_mesh_read.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32, C.c_uint32]
    dll.spv_mesh_read.restype = C.c_void_p
    dll.spv_mesh_info.argtypes = [C.c_void_p, C.POINTER(Info)]
    dll.spv_mesh_vertices.argtypes = [C.c_void_p, C.POINTER(Vertex), C.c_uint32]
    dll.spv_mesh_indices.argtypes = [C.c_void_p, C.POINTER(C.c_uint32), C.c_uint32]
    dll.spv_mesh_destroy.argtypes = [C.c_void_p]
    if dll.spv_abi_version() != 2 or C.sizeof(Info) != 76 or C.sizeof(Vertex) != 64:
        raise AssertionError('Qualified tools ABI required')
    def check(result):
        if not result:
            raise AssertionError(dll.spv_last_error().decode())
    wire = C.create_string_buffer(case['wire'])
    handle = dll.spv_mesh_read(wire, len(case['wire']), 2, 2)
    check(handle)
    try:
        info = Info(); check(dll.spv_mesh_info(handle, C.byref(info)))
        if info.vertices != case['count'] or info.indices > 16:
            raise AssertionError('Tiny tools fixture extent')
        vertices = (Vertex*info.vertices)(); indices = (C.c_uint32*info.indices)()
        check(dll.spv_mesh_vertices(handle, vertices, info.vertices))
        check(dll.spv_mesh_indices(handle, indices, info.indices))
        stride = len(case['input']) // case['count']
        for i, vertex in enumerate(vertices):
            raw = case['input'][i*stride:(i+1)*stride]
            packed = bool(case['flags'] & 0x20)
            normal_at = 32 if packed else 12
            color_at = case['color_offset']
            if tuple(vertex.position) != struct.unpack_from('<3f', raw, 0):
                raise AssertionError('Tools position projection')
            if tuple(vertex.normal) != struct.unpack_from('<3f', raw, normal_at):
                raise AssertionError('Tools must retain unnormalized normal')
            if vertex.color != struct.unpack_from('<I', raw, color_at)[0]:
                raise AssertionError('Tools must retain authored color and alpha')
            if tuple(vertex.uv0) != struct.unpack_from('<2f', raw, color_at+4):
                raise AssertionError('Tools UV0 projection')
            if case['flags'] & 0x1000 and tuple(vertex.uv1) != struct.unpack_from('<2f', raw, color_at+12):
                raise AssertionError('Tools UV1 projection')
            if packed and (tuple(vertex.weights) != struct.unpack_from('<4f', raw, 12)
                           or vertex.bones != struct.unpack_from('<I', raw, 28)[0]):
                raise AssertionError('Tools authored weights/palette indices')
        if list(indices) != list(struct.unpack('<'+'H'*info.indices, case['indices'])):
            raise AssertionError('Tools index projection')
        return dict(info={name: getattr(info, name) for name, _ in Info._fields_},
                    colorARGB=[f'{v.color:08x}' for v in vertices], matched=True)
    finally:
        dll.spv_mesh_destroy(handle)


def compare(case, source, bridge=None):
    f = MeshFixture(case['wire']); p = f.p
    if p.uint(0x763148):
        raise AssertionError('Standalone path requires inactive combiner')
    mesh = f.call(0x4a9e80)
    result = f.call(0x429a40, args=(f.stream, mesh)) & 255
    visits = dict(p.visits)
    required = (0x45fb80, 0x460300, 0x45fea0, 0x4aa000, 0x4b21e0, 0x4ae0e0)
    if result != 1 or f.position != len(case['wire']) or f.errors or not all(a in visits for a in required):
        raise AssertionError('Complete original reader/layout/materialization/declaration must return')
    storage = [f.buffers[p.uint(p.uint(mesh+offset)+0x10)] for offset in (0x58, 0x54)]
    vb, ib = [bytes(p.mu.mem_read(b['data'], b['size'])) for b in storage]
    metadata = [p.uint(mesh+offset) for offset in (0x70, 0x74, 0x64, 0x60, 0x80)]
    if len(f.declaration_arrays) != 1:
        raise AssertionError('One original native vertex declaration required')
    declaration = f.declaration_arrays[0]
    elements = [struct.unpack_from('<HH4B', declaration, i) for i in range(0, len(declaration)-8, 8)]
    color_elements = [e for e in elements if e[4:] == (10, 0)]
    shift = 12 if case['flags'] & 0x20 else 0
    stride = len(case['input']) // case['count']
    color_offset = case['color_offset']
    if len(color_elements) != 1 or color_elements[0] != (0, color_offset+shift, 4, 0, 10, 0):
        raise AssertionError('Original declaration must expose packed D3DCOLOR COLOR0 at its actual offset')
    if metadata[1] != stride+shift or metadata[2:4] != [len(vb), len(ib)] or ib != case['indices']:
        raise AssertionError('Original standalone extents and indices')
    input_colors = []
    for i in range(case['count']):
        before = case['input'][i*stride:(i+1)*stride]
        after = vb[i*(stride+shift):(i+1)*(stride+shift)]
        if shift:
            # Four weights occupy bytes 12..27. Palette indices at 28..31
            # expand independently; every other byte must remain unchanged.
            if after[:28] != before[:28] or after[44:] != before[32:]:
                raise AssertionError('Color/alpha, normal, UV or weight bytes changed')
            if struct.unpack_from('<4f', after, 28) != (0., 3., 7., 15.):
                raise AssertionError('Palette index expansion is unnormalized')
        elif after != before:
            raise AssertionError('Plain vertex copy changed authored bytes')
        input_colors.append(f'{struct.unpack_from("<I", before, color_offset)[0]:08x}')
    native = [metadata, vb.hex(), ib.hex()]
    field = bytes((0xa1, len(case['wire']))) + case['wire'] + b'\0'
    done = subprocess.run([str(source), '--read-payload'], input=field.hex(),
                          capture_output=True, text=True, timeout=10, check=True)
    recovered = json.loads(done.stdout)
    # Complete native destructor and resource cleanup, no interrupted frames.
    f.call(0x4aa350, this=mesh, args=(1,))
    f.clear_declarations()
    resource = p.uint(0x75db78)
    if resource:
        f.call(p.uint(p.uint(resource)), this=resource, args=(1,))
    if any(b['refs'] for b in f.buffers.values()) or set(f.allocations) != set(f.freed):
        raise AssertionError('Original objects/COM storage must be released')
    tool_result = tools_capture(case, bridge) if bridge else None
    if tool_result:
        info = tool_result['info']
        if [info['runtime_stride'], info['runtime_bytes'], info['indices']*info['index_width']] != metadata[1:4]:
            raise AssertionError('Tools metadata differs from original PC result')
    return dict(name=case['name'], flags=case['flags'], native=native, source=recovered,
                match=native == recovered, inputVertexBytes=case['input'].hex(),
                nativePayload=case['wire'].hex(), declaration=declaration.hex(),
                colorOffset=color_offset+shift, colorARGB=input_colors,
                instructions=sum(visits.values()), inputBytes=len(case['wire']),
                visitedRequired=[f'{a:08x}' for a in required], cleanup=True, tools=tool_result,
                **{k: case[k] for k in ('asset', 'assetSha256') if k in case})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--bridge', type=Path)
    parser.add_argument('--guest', action='store_true')
    args = parser.parse_args()
    if not args.guest:
        return subprocess.run([sys.executable, str(Path(__file__)), *sys.argv[1:], '--guest'], timeout=30).returncode
    if not args.output.resolve().is_relative_to(ROOT/'local-data') or args.output.exists():
        raise ValueError('Fresh local evidence output required')
    started = time.perf_counter()
    cases = [compare(case, args.source, args.bridge) for case in fixtures()]
    report = dict(cases=cases, matched=sum(c['match'] for c in cases),
                  sourceSha256=hashlib.sha256(args.source.read_bytes()).hexdigest(),
                  seconds=round(time.perf_counter()-started, 4),
                  scope='Complete original PC CPU mesh helper vs shared C++ reader; declared COM storage, no GPU or lighting inference')
    if args.bridge:
        report['toolsDllSha256'] = hashlib.sha256(args.bridge.read_bytes()).hexdigest()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8') as output:
        json.dump(report, output, indent=2)
    print(json.dumps(dict(cases=len(cases), matched=report['matched'], seconds=report['seconds'])))
    return int(report['matched'] != len(cases))


if __name__ == '__main__':
    raise SystemExit(main())
