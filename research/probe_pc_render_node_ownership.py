#!/usr/bin/env python3
"""Actual PC SMO append, support copy and root clone transaction, no GPU.

The isolated52FD90 initializer and6D7DB0 destructor manage the native static
clone map. No clone-pair/lookup/copy seams are used. Mesh records are borrowed
fixtures; constructed models, render nodes, vectors and map nodes are owned.
"""
from pathlib import Path
import math
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_model_runtime import ModelFixture

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


def near(a, b):
    return all(math.isclose(x, y, rel_tol=2e-5, abs_tol=2e-5) for x, y in zip(a, b))


def entries(p, node):
    begin, end = p.uint(node + 0xbc), p.uint(node + 0xc0)
    check(0 <= end - begin <= 32 and (end - begin) % 4 == 0,
          'bounded native owned-renderable vector')
    return [p.uint(i) for i in range(begin, end, 4)]


class OwnedFixture(ModelFixture):
    def __init__(self):
        super().__init__()
        self.nodes = []
        # Exact startup6D14C0 calls this with global map; do not execute atexit.
        self.call(0x52fd90, this=0x755588)

    def node(self):
        obj = self.render_node()
        self.nodes.append(obj)
        return obj

    def append_model(self, node, model):
        self.call(0x469ed0, this=node + 0xb4, args=(model,))
        if model in self.models:
            self.models.remove(model)  # native append now owns original ref

    def close(self):
        for node in reversed(self.nodes):
            self.call(0x4255d0, this=node, args=(1,))
        self.call(0x6d7db0)  # actual static map destructor, before allocation audit
        super().close()


def append_and_world():
    f = OwnedFixture()
    p, node = f.p, f.node()
    mesh = f.mesh_record((1., 2., 3., 2.))
    model = f.model()
    f.call(0x479e20, this=model, args=(mesh,))
    flags = p.uint(node + 0xb0)
    f.append_model(node, model)
    check(entries(p, node) == [model] and p.uint(model + 8) & 65535 == 1,
          'SMO reader append retains actual model once')
    check(p.floats(node + 0xc8, 4) == p.floats(node + 0xd8, 4) == (1., 2., 3., 2.) and
          p.uint(node + 0xb0) == flags and p.uint(node + 0x134) == 0,
          'append immediately rebuilds local/cached-world sphere, not dirty flags')
    # Geometry remains readable despite mesh28=0: no invented validity gate.
    check(f.call(0x479da0, this=model) == 0, 'borrowed mesh has bounds-valid false')
    p.put_floats(node + 0x20, (10., 20., 30.))
    p.put_floats(node + 0x30, (2., -3., 4.))
    p.put_floats(node + 0x40, (0., 1., 0., -1., 0., 0., 0., 0., 1.))
    f.call(0x4250f0, this=node, args=(1,))
    check(near(p.floats(node + 0xd8, 4), (16., 22., 42., 8.)),
          'actual model getter feeds actual RenderNode PRS sphere')
    p.mu.mem_map(0x340c0000, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
    table = p.allocate(0x40)
    p.put_uint(f.renderer + 0x18, table)
    p.put_uint(table + 0x38, 0x340c0010)
    p.seams[0x340c0010] = lambda p: p.fixture_return(8, eax=1)  # matrix device boundary only
    f.call(0x4248d0, this=node + 0xb4)
    f.append_model(node, model)
    check(entries(p, node) == [model, model] and p.uint(model + 8) & 65535 == 2,
          'duplicate relationship stored twice and retained twice')
    check(near(p.floats(node + 0xd8, 4), (16., 22., 42., 4.)) and
          p.uint(node + 0xb0) == flags and p.uint(node + 0x134) == 0,
          'later append uses cached X-radius rule, not max-scale world update')
    f.close()
    check(p.uint(mesh + 8) == 1, 'node teardown releases model and its mesh exactly once')


def native_copy_and_root_clone():
    f = OwnedFixture()
    p = f.p
    source, destination = f.node(), f.node()
    model, old = f.model(), f.model()
    mesh = f.mesh_record((0., 0., 0., 5.))
    f.call(0x479e20, this=model, args=(mesh,))
    f.append_model(source, model)
    f.append_model(source, model)
    f.append_model(destination, old)
    p.put_floats(source + 0xc8, (1., 2., 3., 4., 5., 6., 7., 8.))
    p.put_floats(destination + 0xc8, (-1., -2., -3., -4., -5., -6., -7., -8.))
    p.mu.mem_write(source + 0x120, b'\1\0\0\0')
    p.mu.mem_write(source + 0x130, b'\1')
    p.put_uint(source + 0x118, 123)
    p.put_uint(destination + 0x118, 456)
    p.put_uint(source + 0x134, 1)
    source_world = tuple(float(100 + i) for i in range(16))
    destination_world = tuple(float(200 + i) for i in range(16))
    p.put_floats(source + 0x138, source_world)
    p.put_floats(destination + 0x138, destination_world)
    check(f.call(0x424980, this=source, args=(destination,)) & 255,
          'original nonempty Node/support/RenderNode copy returns true')
    copied = entries(p, destination)
    check(len(copied) == 3 and copied[0] == old and copied[1] != copied[2] and
          all(obj != model for obj in copied[1:]),
          'PC copy APPENDS and makes independent model clone for EACH occurrence')
    check(all(p.uint(obj + 0x58) == mesh for obj in copied[1:]) and p.uint(mesh + 8) == 4,
          'models are separate while geometry relationship remains shared')
    check(p.floats(destination + 0xc8, 8) == p.floats(source + 0xc8, 8),
          'copy restores source local/world spheres after append recomputation')
    check(p.floats(destination + 0x138, 16) == destination_world and
          p.uint(destination + 0x134) == 0 and p.uint(destination + 0x118) == 456,
          'copy leaves destination render matrices/dirty/word118 untouched')
    check(bytes(p.mu.mem_read(destination + 0x120, 4)) == b'\1\0\0\0' and
          bytes(p.mu.mem_read(destination + 0x130, 1)) == b'\1',
          'four support control bytes and cull bypass copied')
    manager = p.uint(0x74e060)
    check(manager and p.uint(manager + 0x14) == 0 and p.uint(0x755590) == 0,
          'direct copy child clone transactions drain native static map')
    clone = f.call(0x412be0, this=manager, args=(source,))
    f.nodes.append(clone)
    cloned = entries(p, clone)
    check(len(cloned) == 2 and cloned[0] != cloned[1] and
          all(obj not in copied and obj != model for obj in cloned),
          'root transaction also clones duplicate renderables separately, not map alias reuse')
    check(p.uint(manager + 0x14) == 0 and p.uint(0x755590) == 0,
          'native root clone returns depth0 and clears temporary map entries')
    check(p.floats(clone + 0xc8, 8) == p.floats(source + 0xc8, 8) and
          p.uint(clone + 0x134) == 0, 'fresh clone keeps source spheres but default dirty cache')
    f.close()
    check(p.uint(mesh + 8) == 1, 'all separate model clones release shared geometry correctly')


def clone_map_lookup_vs_always():
    f = OwnedFixture()
    p = f.p
    source = f.model()
    first = f.call(0x412c40, args=(source,))
    check(0x4d3810 in p.visits and p.uint(0x13b342c) == 0x74e060,
          'public protected412C40 resolves body4D3810 and its original lazy-singleton address')
    f.models.append(first)
    manager = p.uint(0x74e060)
    check(first != source and first in f.allocations and manager in f.allocations and
          p.uint(manager + 0x14) == 0 and p.uint(0x755590) == 0,
          'map-aware miss lazily constructs manager, clones model and drains root transaction')
    second = f.model()
    f.call(0x412f70, this=manager, args=(source, first))
    allocation_count = len(f.requests)
    check(p.uint(0x755590) == 1 and f.call(0x412c40, args=(source,)) == first and
          len(f.requests) == allocation_count, 'actual map hit returns existing clone without allocation')
    f.call(0x412f70, this=manager, args=(source, second))
    check(p.uint(0x755590) == 1 and f.call(0x412c40, args=(source,)) == second,
          'registering same source overwrites mapped clone, does not duplicate key')
    check(p.uint(first + 8) & 65535 == 0 and p.uint(second + 8) & 65535 == 0,
          'static clone map borrows both source and result without intrusive ownership')
    check(f.call(0x412be0, this=manager, args=(0,)) == 0 and p.uint(0x755590) == 1,
          'null always-clone input does not enter depth or clear preexisting map')
    third = f.call(0x412be0, this=manager, args=(source,))
    f.models.append(third)
    check(third not in (source, first, second) and p.uint(0x755590) == 0 and
          p.uint(manager + 0x14) == 0,
          'always-clone ignores existing mapping and clears map only on root exit')
    f.close()


def main():
    append_and_world()
    native_copy_and_root_clone()
    clone_map_lookup_vs_always()
    print(f'PASS {checks}/{checks}: original PC render-node ownership/copy/root-clone transaction')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
