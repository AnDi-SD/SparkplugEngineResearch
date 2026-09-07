#!/usr/bin/env python3
"""Original PC StaticRenderObject and PCPartitionRenderable runtime support.

Actual factories, append, clone and teardown. Device leaves only record the
matrix/fog/mesh boundary; matrices installed literally are not loader proof.
The real tiny Matrix4 CRT initializer runs, never full Windows startup.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_renderer_queues import QueueFixture, IDENTITY

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value: raise AssertionError(label)


class StaticFixture(QueueFixture):
    def __init__(self, matrix_startup=True):
        super().__init__()
        self.objects = []
        p = self.p
        if matrix_startup: self.call(0x6d38c0)
        for record, identity, parent in ((0x75db08,0x56d67170,0x7555f8),
                                         (0x765938,0x94bbca2a,0x755310),
                                         (0x7651f0,0x9cbb56a2,0x765938)):
            p.put_uint(record, identity)
            p.put_uint(record+0x48,parent)

    def object(self, factory):
        obj = self.call(factory)
        self.objects.append(obj)
        return obj

    def append(self, obj, support_offset, model):
        self.call(0x469ed0,this=obj+support_offset,args=(model,))
        if model in self.models: self.models.remove(model)

    def close(self):
        for obj in reversed(self.objects):
            self.call(self.p.uint(self.p.uint(obj)),this=obj,args=(1,))
        super().close()


def lifetime_copy_and_bounds():
    f = StaticFixture()
    p = f.p
    check(p.floats(0x760058,16) == IDENTITY, 'actual6D38C0 shared Matrix4 identity startup')
    static, partition = f.object(0x41a7c0), f.object(0x4cd950)
    check(f.allocations[static] == 0x10c and p.uint(static) == 0x6e65e8 and
          p.uint(static+0x14) == 0x6e6604, 'Static exact10C Named14 + support74 + Scene/matrices')
    check(f.allocations[partition] == 0x8c and p.uint(partition) == 0x6f4540 and
          p.uint(partition+0x10) == 0x6f4528, 'concrete PCPartition exact8C Base10 + support74')
    check(f.call(0x408370,this=partition,args=(0x94bbca2a,)) & 255 == 1 and
          f.call(0x408350,this=partition,args=(0x94bbca2a,)) & 255 == 0 and
          f.call(0x408350,this=partition,args=(0x9cbb56a2,)) & 255 == 1,
          'base factory actually returns derived spPCPartitionRenderable RTTI')
    check(p.uint(static+0x48) == static+0x8c and p.uint(static+0x4c) == static+0xcc and
          p.floats(static+0x8c,32) == IDENTITY*2,
          'Static copies initialized shared Matrix4 into two self-owned matrices')
    check(p.uint(partition+0x44) == p.uint(partition+0x48) == 0x760058 and
          p.uint(partition+0x84) == 0xff000000, 'Partition uses shared identity and DebugColorFF000000')
    check(p.uint(static+0x88) == p.uint(partition+0x88) == 0 and
          p.uint(static+0x84) == static and p.uint(partition+0x80) == partition,
          'both complete objects have Scene88 but different support/self offsets')
    check(p.uint(static+0x80) == 0x01010100 and p.uint(partition+0x7c) == 0x01010101,
          'first support control differs Static0/Partition1; other three true')
    model, mesh = f.model(), f.mesh_record((1.,2.,3.,4.))
    f.call(0x479e20,this=model,args=(mesh,))
    f.append(static,0x14,model)
    f.append(partition,0x10,model)
    check(p.uint(model+8) & 65535 == 2,
          'shared actual Model retained once by each independent support')
    check(p.floats(static+0x28,4) == p.floats(partition+0x24,4) == (1.,2.,3.,4.) and
          p.floats(static+0x38,4) == (1.,2.,3.,4.) and
          p.floats(partition+0x34,4) == (1.,2.,3.,4.),
          'common append updates local/world sphere using initialized identity matrices')
    p.put_uint(partition+0x84,0x11223344)
    p.put_floats(static+0x8c,(*IDENTITY[:12],10.,20.,30.,1.))
    p.put_floats(static+0xcc,(*IDENTITY[:12],-10.,-20.,-30.,1.))
    for obj, own_begin, matrix in ((static,0x1c,0x8c),(partition,0x18,None)):
        clone = f.call(p.uint(p.uint(obj)+8),this=obj)
        f.objects.append(clone)
        check(p.uint(clone+own_begin) == 0,
              'inherited-only actual clone DOES NOT copy renderable list')
        if matrix is not None:
            check(p.floats(clone+matrix,32) == IDENTITY*2,
                  'Static clone keeps constructor default transform/inverse')
        else:
            check(p.uint(clone+0x84) == 0xff000000,
                  'PCPartition clone does not copy DebugColor')
    f.close()
    check(p.uint(mesh+8) == 1, 'all support/model native intrusive ownership released')


def startup_and_partition_ownership():
    f = StaticFixture(matrix_startup=False)
    p = f.p
    cold = f.object(0x41a7c0)
    check(p.floats(cold+0x8c,32) == (0.,)*32,
          'omitting Matrix4 CRT startup copies zero PE globals; not a native application default')
    f.call(0x6d38c0)
    check(p.floats(cold+0x8c,32) == (0.,)*32,
          'later shared startup does not retroactively repair static self-owned matrices')
    root = f.object(0x426910)
    static, payload = f.object(0x41a7c0),f.object(0x4cd950)
    for _ in range(2): f.call(0x426740,this=root,args=(static,))
    f.objects.remove(static)
    check(p.uint(static+8)&65535 == 2 and p.uint(root+0x38)-p.uint(root+0x34)==8,
          'Partition append static retains each duplicate; owns references not direct object')
    p.put_uint(root+0x78,payload)  # decoded own field assignment, not original setter
    p.mu.mem_write(payload+8,(7).to_bytes(2,'little'))
    f.objects.remove(payload)
    scene = p.allocate(0x54)
    f.call(0x4259e0,this=root,args=(scene,))
    check(p.uint(root+0x80)==p.uint(static+0x88)==p.uint(payload+0x88)==scene,
          'actual partition Scene propagation reaches both concrete render-support classes')
    f.call(0x426890,this=root,args=(1,))
    f.objects.remove(root)
    check(static in f.freed and payload in f.freed,
          'Partition dtor releases both static refs but directly deletes payload despite ref7')
    f.close()


def matrix_and_render_protocol():
    f = StaticFixture()
    p = f.p
    for factory, offset, world_offset in ((0x41a7c0,0x14,0x8c),(0x4cd950,0x10,None)):
        obj = f.object(factory)
        support = obj+offset
        models = []
        for _ in range(3):
            model, mesh = f.model(), f.mesh_record((1.,2.,3.,4.))
            f.call(0x479e20,this=model,args=(mesh,))
            f.append(obj,offset,model)
            models.append(model)
        if world_offset:
            world = (*IDENTITY[:12],10.,20.,30.,1.)
            # Intentionally distinct finite inverse proves no recomputation at
            # submission boundary. This is not a claim of acceptable file data.
            inverse = (*IDENTITY[:12],-7.,-8.,-9.,1.)
            p.put_floats(obj+0x8c,world)
            p.put_floats(obj+0xcc,inverse)
            matrices = (obj+0x8c,obj+0xcc)
        else: matrices = (0x760058,0x760058)
        p.put_floats(support+0x24,(5.,6.,7.,8.))
        f.results['matrix'], f.results['mesh'] = 0,1
        f.calls.clear()
        check(f.call(p.uint(p.uint(support)+4),this=support) & 255 == 0 and
              f.calls[0][1] == matrices,
              'matrix setup submits exact independent pointers and propagates device false')
        check(p.uint(f.renderer+0xc190) == support+0x3c,
              'matrix setup publishes light cache before failure')
        f.calls.clear()
        draw = p.uint(p.uint(support))
        check(f.call(draw,this=support,args=(f.camera,0)) & 255 == 1 and
              [x[0] for x in f.calls] == ['matrix','fog','mesh','fog','mesh','fog','mesh'],
              'Static/Partition draw IGNORE matrix failure, still submit every successful Model')
        f.results['matrix'], f.results['mesh'] = 1,0
        f.calls.clear()
        check(f.call(draw,this=support,args=(f.camera,1)) & 255 == 0 and
              [x[0] for x in f.calls] == ['matrix','fog','mesh'],
              'Static/Partition draw STOP on first failed Model, unlike RenderNode')
        check(p.floats(f.renderer+0xc9c8,4) == (5.,6.,7.,8.),
              'successful matrix preparation publishes cached world sphere')
        f.results['mesh'] = 1
        f.calls.clear()
        p.mu.mem_write(f.renderer+0xc050,b'\1')
        p.mu.mem_write(0x75f8e8,b'\0')
        check(f.call(draw,this=support,args=(f.camera,0)) & 255 == 1 and not f.calls,
              'queue branch appends actual Models without direct matrix/mesh submission')
        begin,end = p.uint(f.renderer+0xc058),p.uint(f.renderer+0xc05c)
        check(end-begin == 60 and [p.uint(begin+i*20) for i in range(3)] == models and
              all(p.uint(begin+i*20+4)==support for i in range(3)),
              'three native general records carry exact adjusted support pointer')
        p.mu.mem_write(f.renderer+0xc050,b'\0')
        f.call(0x456b10,this=f.renderer)
        scene = p.allocate(0x54)
        p.put_uint(scene+0x40,77)
        p.put_uint(obj+0x88,scene)
        p.put_uint(support+0x64,77)
        gate = p.uint(p.uint(support)+0x10)
        check(f.call(gate,this=support)&255 == 0, 'support visibility mark equal Scene40 rejects')
        p.put_uint(support+0x64,76)
        check(f.call(gate,this=support)&255 == 1 and
              f.call(p.uint(p.uint(support)+0x14),this=support)&255 == 1,
              'different mark visible; static/partition support enable slot always true')
    f.close()


def main():
    lifetime_copy_and_bounds()
    startup_and_partition_ownership()
    matrix_and_render_protocol()
    print(f'PASS {checks}/{checks}: original PC static/partition render support')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']: raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
