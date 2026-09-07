#!/usr/bin/env python3
"""Original PC alpha queue construction/comparator/flush and model queue.

Renderer storage is a zeroed fixture, not a full renderer constructor. Device
calls are recorders. qsort is an explicit no-op boundary ONLY for already
ordered flush input; the original comparator is executed separately. No claim
of CRT tie ordering, GPU output or whole-scene success is made.
"""
from pathlib import Path
import math
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_render_node_ownership import OwnedFixture

checks = 0
IDENTITY = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.)


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


class QueueFixture(OwnedFixture):
    RX = 0x340f0000

    def __init__(self):
        super().__init__()
        p = self.p
        self.calls = []
        self.results = {'matrix': 1, 'mesh': 1, 'fog': 1}
        p.mu.mem_map(self.RX, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
        table = p.allocate(0x80)
        p.put_uint(self.renderer + 0x18, table)
        for offset, slot, name, words in ((0x10, 0x38, 'matrix', 2),
                                          (0x20, 0x24, 'mesh', 1),
                                          (0x30, 0x68, 'fog', 1)):
            p.put_uint(table + slot, self.RX + offset)
            p.seams[self.RX + offset] = lambda p, n=name, w=words: self.device(p, n, w)
        self.camera = p.allocate(0x238)  # explicit camera cache fixture, not ctor
        p.put_floats(self.camera + 0xcc, IDENTITY)
        p.mu.mem_write(self.renderer + 0x45, b'\1')
        p.mu.mem_write(self.renderer + 0xc9d8, b'\1')

    def device(self, p, name, words):
        check(p.reg('ECX') == self.renderer + 0x18, 'native secondary renderer receiver')
        args = tuple(p.uint(p.reg('ESP') + 4 + i * 4) for i in range(words))
        self.calls.append((name, args, p.uint(self.renderer + 0x44) & 255))
        p.fixture_return(words * 4, eax=self.results[name])

    def model_with_node(self, node=None, sphere=(1., 2., 3., 1.), alpha=False):
        p = self.p
        node = self.node() if node is None else node
        model, mesh = self.model(), self.mesh_record(sphere)
        self.call(0x479e20, this=model, args=(mesh,))
        self.append_model(node, model)
        if alpha:
            material, pass_record = p.allocate(0x70), p.allocate(0x20)
            p.put_uint(material + 8, 2)  # model plus external ref
            p.put_uint(material + 0x4c, pass_record)
            p.put_uint(pass_record + 0x10, 1)
            p.put_uint(model + 0x20, material)
        return node, model, mesh

    def enqueue(self, node, model, priority=0):
        return self.call(0x454c30, this=self.renderer,
                         args=(model, node + 0xb4, self.camera, priority)) & 255

    def record(self, index):
        p = self.p
        address = self.renderer + 0x50 + index * 24
        return (p.uint(address), p.uint(address + 4), p.uint(address + 8),
                p.floats(address + 12, 1)[0], p.uint(address + 16),
                p.uint(address + 20) & 255)

    def already_sorted_qsort_boundary(self):
        p = self.p
        check(bytes(p.mu.mem_read(0x454864, 2)) == b'\xff\x15',
              'flush qsort boundary is an actual IAT call')
        iat = p.uint(0x454866)
        p.put_uint(iat, self.RX + 0x40)  # explicit import resolution fixture

        def qsort(p):
            values = tuple(p.uint(p.reg('ESP') + 4 + 4 * i) for i in range(4))
            check(values[0] == self.renderer + 0x50 and values[1] <= 4 and
                  values[2:] == (24, 0x454800), 'qsort receives fixed record24 and original comparator')
            self.calls.append(('qsort', values[1]))
            p.fixture_return()

        p.seams[self.RX + 0x40] = qsort


def alpha_metric_and_capacity():
    f = QueueFixture()
    p = f.p
    node, model, mesh = f.model_with_node()
    p.put_floats(node + 0x138, (*IDENTITY[:12], 10., 20., 30., 1.))
    p.put_floats(f.camera + 0xcc, (*IDENTITY[:12], -1., -2., -3., 1.))
    p.put_uint(f.renderer + 0x48, 0xfffffff9)
    model_refs, mesh_refs = p.uint(model + 8), p.uint(mesh + 8)
    check(f.enqueue(node, model, 13) == 1 and p.uint(f.renderer + 0x4c) == 1,
          'original alpha queue appends one record')
    check(f.record(0) == (f.camera, node + 0xb4, model, 1400., 6, 0),
          'record(camera,support,renderable,distance2,wrapped priority,exact-particle flag)')
    p.mu.mem_write(f.camera + 0x231, b'\1')
    check(f.enqueue(node, model, 0) == 1 and f.record(1)[3] == 900.,
          'camera231 uses z squared, not serialized Is2D at C8')
    check(p.uint(model + 8) == model_refs and p.uint(mesh + 8) == mesh_refs,
          'queue borrows model/support/camera without retaining references')
    p.mu.mem_write(f.renderer + 0x45, b'\0')
    check(f.call(0x454c30, this=f.renderer, args=(0, 0, 0, 0)) & 255 == 1 and
          p.uint(f.renderer + 0x4c) == 2, 'disabled45 returns true without reading null inputs')
    p.mu.mem_write(f.renderer + 0x45, b'\1')
    p.put_uint(f.renderer + 0x4c, 2048)
    before = bytes(p.mu.mem_read(f.renderer + 0x50, 48))
    check(f.enqueue(node, model) == 0 and p.uint(f.renderer + 0x4c) == 2048 and
          bytes(p.mu.mem_read(f.renderer + 0x50, 48)) == before,
          'capacity2048 returns false with no record/count overwrite')
    p.put_uint(f.renderer + 0x4c, 0)
    f.close()


def alpha_comparator():
    f = QueueFixture()
    p = f.p
    first, second = p.allocate(24), p.allocate(24)

    def compare(flag_a, priority_a, distance_a, flag_b, priority_b, distance_b):
        for address, flag, priority, distance in ((first, flag_a, priority_a, distance_a),
                                                  (second, flag_b, priority_b, distance_b)):
            p.mu.mem_write(address + 20, bytes([flag]))
            p.put_uint(address + 16, priority)
            p.put_floats(address + 12, (distance,))
        p.run(0x454800, args=(first, second), callee_pop=False)
        raw = p.reg('EAX')
        return raw if raw < 0x80000000 else raw - 0x100000000

    check(compare(0, 1, 1., 1, 100, 100.) == -1 and
          compare(1, 100, 100., 0, 1, 1.) == 1, 'non-particle records sort before exact particle')
    check(compare(0, 0xffffffff, 1., 0, 2, 100.) == -1 and
          compare(0, 2, 100., 0, 0xffffffff, 1.) == 1, 'priority is descending UNSIGNED')
    check(compare(0, 5, 9., 0, 5, 4.) == -1 and
          compare(0, 5, 4., 0, 5, 9.) == 1, 'equal ordinary priority sorts distance descending')
    check(compare(1, 0, 9., 1, 100, 4.) == -1 and
          compare(1, 100, 4., 1, 0, 9.) == 1, 'two particle records ignore priority')
    check(compare(0, 5, 4., 0, 5, 4.) == 1 and compare(1, 0, 4., 1, 0, 4.) == 1,
          'native comparator returns +1 for equality, NEVER zero; CRT tie order unproved')
    check(compare(0, 0, math.nan, 0, 0, 4.) == 1 and
          compare(0, 0, 4., 0, 0, math.nan) == 1, 'unordered x87 distance also yields +1')
    f.close()


def alpha_full_dispatch():
    f = QueueFixture()
    p = f.p
    a, first, _ = f.model_with_node(alpha=True)
    _, second, _ = f.model_with_node(a, alpha=True)
    b, third, _ = f.model_with_node(alpha=True)
    for model, node, priority in ((first, a, 30), (second, a, 20), (third, b, 10)):
        p.put_uint(model + 0x1c, priority)
        check(f.call(0x479dc0, this=model, args=(f.camera, node + 0xb4)) & 255 == 1,
              'actual Model pre enqueues and successfully skips draw')
    check(p.uint(f.renderer + 0x4c) == 3 and not f.calls,
          'three native pre passes produce ordered borrowed queue, no device submission')
    f.already_sorted_qsort_boundary()
    f.results['matrix'], f.results['mesh'] = 0, 0
    check(f.call(0x454850, this=f.renderer) & 255 == 1,
          'alpha flush succeeds despite both matrix and model draw failures')
    check([c[0] for c in f.calls] == ['qsort', 'matrix', 'fog', 'mesh',
                                      'fog', 'mesh', 'matrix', 'fog', 'mesh'],
          'flush reuses consecutive support; all entries visited despite failures')
    check(all(c[-1] == 1 for c in f.calls if c[0] != 'qsort'),
          'renderer44 is set during entire flush, preventing re-enqueue')
    check(p.uint(f.renderer + 0x4c) == 0 and p.uint(f.renderer + 0x44) & 255 == 0,
          'flush clears count and active byte, leaving record storage borrowed/stale')
    check(f.record(0)[2] == first and f.record(2)[2] == third,
          'flush resets count but does not clear record bytes')
    f.calls.clear()
    check(f.call(0x454850, this=f.renderer) & 255 == 1 and f.calls == [('qsort', 0)],
          'empty flush still invokes qsort boundary')
    f.close()


def general_model_queue():
    f = QueueFixture()
    p = f.p
    a, first, mesh_a = f.model_with_node()
    _, second, mesh_b = f.model_with_node(a)
    b, third, mesh_c = f.model_with_node()
    p.mu.mem_write(0x75f8e8, b'\0')  # explicit normal-vs-nine-bucket startup setting
    check(f.call(0x456310, this=f.renderer, args=(0, 0, 0)) & 255 == 0,
          'C050-disabled model queue returns false before reading null inputs')
    p.mu.mem_write(f.renderer + 0xc050, b'\1')
    for model, node in ((third, b), (first, a), (second, a)):
        check(f.call(0x456310, this=f.renderer,
                     args=(model, node + 0xb4, f.camera)) & 255 == 1,
              'normal queue append executes original protected vector helper')
    begin, end = p.uint(f.renderer + 0xc058), p.uint(f.renderer + 0xc05c)
    check(end - begin == 60 and begin in f.allocations,
          'normal queue contains three owned compiler-vector records of20 bytes')
    records = [tuple(p.uint(begin + i * 20 + j * 4) for j in range(5)) for i in range(3)]
    check(records == [(third, b + 0xb4, f.camera, 0, mesh_c),
                      (first, a + 0xb4, f.camera, 0, mesh_a),
                      (second, a + 0xb4, f.camera, 0, mesh_b)],
          'normal records preserve insertion: renderable/support/camera/material-key/mesh')
    check(all(p.uint(model + 8) & 65535 == 1 for model in (first, second, third)),
          'normal queue borrows model references')
    f.results['matrix'], f.results['mesh'] = 0, 0
    check(f.call(0x456910, this=f.renderer, args=(f.camera,)) & 255 == 1,
          'normal native sort/flush ignores setup/draw failures')
    check([c[0] for c in f.calls] == ['matrix', 'fog', 'mesh', 'fog', 'mesh',
                                      'matrix', 'fog', 'mesh'] and
          [c[1][0] for c in f.calls if c[0] == 'mesh'] == [mesh_a, mesh_b, mesh_c],
          'actual normal queue sorting groups ascending material-key then mesh address')
    check(p.uint(f.renderer + 0xc058) == p.uint(f.renderer + 0xc05c) and
          p.uint(f.renderer + 0xc058) != 0 and
          p.uint(f.renderer + 0xc050) & 255 == 1,
          'flush clears logical vector but retains capacity and does not reset C050')
    f.call(0x456b10, this=f.renderer)
    check(p.uint(f.renderer + 0xc058) == p.uint(f.renderer + 0xc060) == 0,
          'actual renderer queue cleanup frees retained vector allocation')
    f.close()


def general_material_key():
    f = QueueFixture()
    p = f.p
    node, model, mesh = f.model_with_node()
    material, pass_record, layer, owner, table = [p.allocate(size) for size in (0x70, 0x20, 0x20, 4, 0x20)]
    p.put_uint(material + 8, 2)
    p.put_uint(material + 0x4c, pass_record)
    p.put_uint(pass_record + 0x18, layer)
    p.put_uint(layer + 0x10, owner)
    p.put_uint(owner, table)
    p.put_uint(table + 0x1c, f.RX + 0x50)
    p.put_uint(model + 0x20, material)
    key_calls = []
    def key(p):
        check(p.reg('ECX') == owner, 'material-key virtual1C receives nested owner')
        key_calls.append(1)
        p.fixture_return(eax=0x81234567)
    p.seams[f.RX + 0x50] = key
    p.mu.mem_write(0x75f8e8, b'\0')
    p.mu.mem_write(f.renderer + 0xc050, b'\1')
    check(f.call(0x456310, this=f.renderer, args=(model, node + 0xb4, f.camera)) & 255 == 1,
          'normal queue consumes nested material/pass/layer owner key')
    begin = p.uint(f.renderer + 0xc058)
    check(p.uint(begin + 12) == 0x81234567 and p.uint(begin + 16) == mesh and key_calls == [1],
          'material-key and model mesh are separate sorting fields')
    f.call(0x456b10, this=f.renderer)
    f.close()


def nine_bucket_mode():
    f = QueueFixture()
    p = f.p
    node, model, _ = f.model_with_node()
    p.mu.mem_write(0x75f8e8, b'\1')
    p.mu.mem_write(f.renderer + 0xc050, b'\1')
    for mode in (0, 2, 8):
        p.put_uint(model + 0x14, mode)
        check(f.call(0x456310, this=f.renderer,
                     args=(model, node + 0xb4, f.camera)) & 255 == 1,
              'nine-bucket mode append accepted for in-range mode')
        vector = f.renderer + 0xc064 + mode * 16
        begin, end = p.uint(vector + 4), p.uint(vector + 8)
        check(end - begin == 8 and (p.uint(begin), p.uint(begin + 4)) ==
              (model, node + 0xb4), 'bucket record8 omits per-item camera/material/mesh')
    p.put_uint(node + 0xb0, p.uint(node + 0xb0) & ~0x200)
    check(f.call(0x456310, this=f.renderer,
                 args=(model, node + 0xb4, f.camera)) & 255 == 1 and
          p.uint(f.renderer + 0xc0ec) - p.uint(f.renderer + 0xc0e8) == 8,
          'disabled support slot14 skips bucket insertion but returns true')
    p.put_uint(f.renderer, 0x6f2918)  # original concrete PC table, no forged pass leaves
    check(f.call(0x456910, this=f.renderer, args=(f.camera,)) & 255 == 1 and not f.calls,
          'actual PC selected pass slots are shared5B7A00 no-ops, not a functional draw mode')
    check(0x5b7a00 in p.visits and all(
          p.uint(f.renderer + 0xc064 + mode * 16 + 8) -
          p.uint(f.renderer + 0xc064 + mode * 16 + 4) == 8 for mode in (0, 2, 8)),
          'bucket flush does not drain buckets; Scene clears them on next queue pass')
    # Actual compiler-vector destructor is storage-only for borrowed records.
    for mode in (0, 2, 8):
        f.call(0x456ac0, this=f.renderer + 0xc064 + mode * 16)
    f.close()


def main():
    alpha_metric_and_capacity()
    alpha_comparator()
    alpha_full_dispatch()
    general_model_queue()
    general_material_key()
    nine_bucket_mode()
    print(f'PASS {checks}/{checks}: original PC renderer alpha/model queues/comparator/flush')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
