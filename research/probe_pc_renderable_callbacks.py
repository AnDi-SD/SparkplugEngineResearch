#!/usr/bin/env python3
"""Original PC callback vectors, material save/restore and alpha-queue gates.

Only user callbacks and platform leaves are recording fixtures. Actual native
group traversal/erase, pre/post and Model render run unchanged. Borrowed material
records have explicit external refs; they are not constructed GPU materials.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_model_runtime import ModelFixture

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


class ProtocolFixture(ModelFixture):
    RX = 0x340e0000

    def __init__(self):
        super().__init__()
        p = self.p
        self.calls = []
        self.results = {'pre': 1, 'post': 1, 'mesh': 1, 'fog': 0}
        self.group_results = {}
        self.camera, self.support = p.allocate(0x238), p.allocate(0x40)
        self.active = 0
        p.mu.mem_map(self.RX, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
        table = p.allocate(0x80)
        p.put_uint(self.renderer + 0x18, table)
        for slot, offset, name in ((0x24, 0x10, 'mesh'), (0x68, 0x20, 'fog')):
            p.put_uint(table + slot, self.RX + offset)
            p.seams[self.RX + offset] = lambda p, name=name: self.device(p, name)
        for offset, name in ((0x30, 'pre'), (0x40, 'post')):
            p.seams[self.RX + offset] = lambda p, name=name: self.direct(p, name)
        p.seams[self.RX + 0x50] = self.group

    def arguments(self, count):
        p = self.p
        return tuple(p.uint(p.reg('ESP') + 4 + i * 4) for i in range(count))

    def device(self, p, name):
        check(p.reg('ECX') == self.renderer + 0x18,
              'platform callback uses secondary renderer receiver')
        self.calls.append((name, self.arguments(1)[0],
                           p.uint(self.renderer + 0xc1c4) & 255))
        p.fixture_return(4, eax=self.results[name])

    def direct(self, p, name):
        check(self.arguments(3) == (self.active, self.camera, self.support),
              'direct callback receives cdecl model/camera/support')
        self.calls.append((name, p.uint(self.renderer + 0xc1c4) & 255))
        p.fixture_return(eax=self.results[name])

    def group(self, p):
        obj, camera, support, ordinal, user = self.arguments(5)
        check((obj, camera, support) == (self.active, self.camera, self.support),
              'group callback cdecl five-argument prefix')
        self.calls.append(('group', ordinal, user,
                           p.uint(self.renderer + 0xc1c4) & 255))
        p.fixture_return(eax=self.group_results.get(user, 1) & 0xffffffff)

    def prepare(self):
        obj = self.model()
        mesh = self.mesh_record()
        self.call(0x479e20, this=obj, args=(mesh,))
        self.p.put_uint(obj + 0x2c, self.RX + 0x30)
        self.p.put_uint(obj + 0x30, self.RX + 0x40)
        self.active = obj
        return obj

    def group_buffer(self, obj, offset, users):
        p = self.p
        data = p.allocate(max(8, len(users) * 8))
        self.allocations[data] = max(8, len(users) * 8)
        for index, user in enumerate(users):
            p.put_uint(data + index * 8, self.RX + 0x50)
            p.put_uint(data + index * 8 + 4, user)
        for field, value in ((4, data), (8, data + len(users) * 8),
                             (12, data + max(8, len(users) * 8))):
            p.put_uint(obj + offset + field, value)
        return data

    def material(self, obj):
        p = self.p
        material, pass_record = p.allocate(0x70), p.allocate(0x20)
        p.put_uint(material + 8, 2)  # one external + one model reference
        p.put_uint(material + 0x4c, pass_record)
        p.put_uint(obj + 0x20, material)
        return material, pass_record

    def render(self, obj):
        self.active = obj
        return self.call(0x479dc0, this=obj, args=(self.camera, self.support)) & 255


def group_erase_and_stop():
    f = ProtocolFixture()
    p = f.p
    for offset, entry in ((0x44, 0x423e30), (0x34, 0x423ea0)):
        obj = f.prepare()
        data = f.group_buffer(obj, offset, (11, 22, 33, 44, 55))
        f.group_results = {11: -1, 22: 0x100, 33: -1, 44: 0, 55: 1}
        f.calls.clear()
        f.call(entry, this=obj, args=(f.camera, f.support))
        check([(c[1], c[2]) for c in f.calls] == [(0, 11), (1, 22), (2, 33), (3, 44)],
              'ordinal increments even after erase; full EAX100 continues, EAX0 stops')
        check(p.uint(obj + offset + 8) == data + 24 and
              [p.uint(data + i * 8 + 4) for i in range(3)] == [22, 44, 55],
              'minus-one removal is stable left shift, stop record retained')
        check(p.uint(obj + offset + 12) == data + 40 and data not in f.freed,
              'group dispatch neither shrinks allocation nor releases user records')
        f.group_results = {22: -2, 44: -1, 55: -1}
        f.calls.clear()
        f.call(entry, this=obj, args=(f.camera, f.support))
        check([(c[1], c[2]) for c in f.calls] == [(0, 22), (1, 44), (2, 55)] and
              p.uint(obj + offset + 8) == data + 8,
              'negative non-minus-one continues; last removals keep first record')
        f.group_results = {22: -1}
        f.call(entry, this=obj, args=(f.camera, f.support))
        check(p.uint(obj + offset + 8) == data, 'final record erased without freeing capacity')
    f.close()


def wrapper_order_and_material_state():
    f = ProtocolFixture()
    p = f.p
    obj = f.prepare()
    material, _ = f.material(obj)
    p.mu.mem_write(material + 0x6c, b'\1')
    f.group_buffer(obj, 0x44, (10, 20))
    f.group_buffer(obj, 0x34, (30, 40))
    f.group_results = {10: 0, 30: 0}
    p.mu.mem_write(obj + 0x54, b'\1\1')
    p.mu.mem_write(f.renderer + 0xc1c4, b'\x7b')
    check(f.render(obj) == 1, 'group stop does not abort Model dispatch')
    check([c[0] for c in f.calls] == ['group', 'pre', 'fog', 'mesh', 'group', 'post'],
          'pre group/direct, state setup, mesh, state restore, post group/direct')
    check(f.calls[0][-1] == f.calls[1][-1] == 0x7b and
          f.calls[2][-1] == f.calls[3][-1] == 0 and
          f.calls[4][-1] == f.calls[5][-1] == 0x7b,
          'material6C state is cleared after pre and restored before post callbacks')
    check(p.uint(0x7400fc) & 255 == 0x7b, 'saved renderer byte lives in shared global7400FC')
    f.calls.clear()
    f.results['mesh'] = 0
    p.mu.mem_write(f.renderer + 0xc1c4, b'\x45')
    check(f.render(obj) == 0 and p.uint(f.renderer + 0xc1c4) & 255 == 0 and
          [c[0] for c in f.calls] == ['group', 'pre', 'fog', 'mesh'],
          'mesh failure skips post and leaves temporary renderer state cleared')
    f.results['mesh'], f.results['pre'] = 1, 0x100
    f.calls.clear()
    p.mu.mem_write(f.renderer + 0xc1c4, b'\x67')
    check(f.render(obj) == 1 and [c[0] for c in f.calls] == ['group', 'pre'] and
          p.uint(f.renderer + 0xc1c4) & 255 == 0x67,
          'direct callback tests AL unlike group EAX; pre skip makes no state change')
    f.results['pre'] = 1
    p.mu.mem_write(obj + 0x54, b'\0\0')
    f.calls.clear()
    check(f.render(obj) == 1 and [c[0] for c in f.calls] == ['pre', 'fog', 'mesh', 'post'],
          'disabled group flags do not drain the vectors')
    # Two legal sequential pre calls demonstrate the single shared save slot.
    other = f.prepare()
    other_material, _ = f.material(other)
    p.mu.mem_write(other_material + 0x6c, b'\1')
    p.put_uint(obj + 0x2c, 0)
    p.put_uint(other + 0x2c, 0)
    p.mu.mem_write(f.renderer + 0xc1c4, b'\x55')
    for model in (obj, other):
        f.call(0x423fd0, this=model, args=(f.camera, f.support))
    p.put_uint(obj + 0x30, 0)
    f.call(0x4240d0, this=obj, args=(f.camera, f.support))
    check(p.uint(0x7400fc) & 255 == 0 and p.uint(f.renderer + 0xc1c4) & 255 == 0,
          'nested/interleaved pre overwrites shared saved byte; no stack restoration guarantee')
    f.close()
    check(p.uint(material + 8) == p.uint(other_material + 8) == 1,
          'actual model teardown releases borrowed material relationship once')


def alpha_pre_gates():
    f = ProtocolFixture()
    p = f.p
    obj = f.prepare()
    _, pass_record = f.material(obj)
    p.put_uint(pass_record + 0x10, 1)
    p.mu.mem_write(f.renderer + 0xc9d8, b'\1')
    queue_calls = []

    def queue(p):
        check(p.reg('ECX') == f.renderer and
              f.arguments(4) == (obj, f.support, f.camera, 13),
              'original pre queue call is renderer(model,support,camera,priority)')
        queue_calls.append(1)
        p.fixture_return(16, eax=0)  # failed queue is intentionally ignored

    p.seams[0x454c30] = queue
    p.put_uint(obj + 0x1c, 13)
    check(f.render(obj) == 1 and queue_calls == [1] and not f.calls,
          'alpha enqueue failure still becomes pre skip and successful Model render')
    for address, disabled, enabled in ((pass_record + 0x10, 0, 1),
                                      (obj + 0x18, 0, 1),
                                      (f.renderer + 0x44, 1, 0),
                                      (f.renderer + 0xc9d8, 0, 1)):
        p.mu.mem_write(address, bytes([disabled]))
        f.calls.clear()
        check(f.render(obj) == 1 and queue_calls == [1] and
              [c[0] for c in f.calls] == ['pre', 'fog', 'mesh', 'post'],
              'one disabled alpha gate switches to immediate draw')
        p.mu.mem_write(address, bytes([enabled]))
    f.close()


def main():
    group_erase_and_stop()
    wrapper_order_and_material_state()
    alpha_pre_gates()
    print(f'PASS {checks}/{checks}: original PC Renderable callback/material/alpha gates')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
