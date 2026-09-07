#!/usr/bin/env python3
"""Original PC engine graphics-frame ordering with explicit renderer/camera seams.

No graphics device, window or game is started. Core41C460 and its original
begin/end hooks41C210/41C2A0 execute on synthetic bounded engine storage.
Camera/scene pointers and backend calls are observations, not actual drawing.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_animation_lifecycle import LifetimeFixture

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


class RenderFixture(LifetimeFixture):
    def __init__(self):
        super().__init__()
        p = self.p
        self.calls = []
        self.results = {}
        self.next_seam = 0x34050010
        p.mu.mem_map(0x34050000, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
        self.engine = p.allocate(0x158)
        engine_table = p.allocate(0x64)
        p.put_uint(self.engine, engine_table)
        p.put_uint(engine_table + 0x40, 0x41c210)
        p.put_uint(engine_table + 0x44, 0x41c2a0)
        p.put_uint(0x755274, self.engine)
        self.renderer = p.allocate(0x1c)
        table = p.allocate(0x1c)
        p.put_uint(self.renderer + 0x18, table)
        p.put_uint(0x75db68, self.renderer)
        for offset, name, pop, args in (
            (4, 'renderer-slot1', 4, (0,)),
            (0xc, 'renderer-slot3', 0, ()),
            (0x14, 'renderer-slot5', 12, (7, 0xff112233, 0)),
            (0x10, 'renderer-slot4', 0, ()),
            (0x18, 'renderer-slot6', 4, (1,)),
        ):
            p.put_uint(table + offset, self.seam(name, self.renderer + 0x18, pop, args))
        self.target_manager = p.allocate(0x44)
        p.put_uint(0x75db88, self.target_manager)
        p.put_uint(self.engine + 0x1c, 0x12345678)  # borrowed default-camera identity, callee seam only
        p.put_uint(self.engine + 0x38, 0xff112233)
        self.seam('target-camera', self.target_manager, 4, (0x12345678,), address=0x45d100)
        self.seam('target-end', self.target_manager, address=0x45cc20)
        self.middle = p.allocate(0x10)
        middle_table = p.allocate(0x28)
        p.put_uint(self.middle, middle_table)
        p.put_uint(self.engine + 0x50, self.middle)
        p.put_uint(middle_table + 0x20, self.seam('field50-begin', self.middle))
        p.put_uint(middle_table + 0x24, self.seam('field50-middle', self.middle))
        self.extra = p.allocate(0x24)
        p.put_uint(0x75dba0, self.extra)
        p.mu.mem_write(self.extra + 0x20, b'\1')
        self.seam('global75DBA0-work', self.extra, address=0x452d40)
        p.put_uint(self.engine + 0x30, self.seam('callback30', None))
        p.put_uint(self.engine + 0x34, self.seam('callback34', None))
        self.cameras = []
        for index, enabled, before_middle in ((0, False, False), (1, True, True),
                                             (2, False, False), (3, True, False),
                                             (4, True, True), (5, False, False)):
            camera = p.allocate(0xb4)
            camera_table = p.allocate(0x44)
            p.put_uint(camera, camera_table)
            p.put_uint(camera_table + 0x40, self.seam(f'camera{index}', camera))
            p.put_uint(camera + 0xb0, 0x100 if enabled else 0)
            if enabled:
                scene = p.allocate(0x28)
                p.put_uint(camera + 0x3c, scene)
                p.mu.mem_write(scene + 0x25, bytes([before_middle]))
            self.cameras.append(camera)
        storage = p.allocate(len(self.cameras) * 4)
        for i, camera in enumerate(self.cameras):
            p.put_uint(storage + i * 4, camera)
        p.put_uint(self.engine + 0x24, storage)
        p.put_uint(self.engine + 0x28, storage + len(self.cameras) * 4)
        p.put_uint(self.engine + 0x2c, storage + len(self.cameras) * 4)

    def seam(self, name, owner, pop=0, args=(), *, address=None):
        if address is None:
            address = self.next_seam
            self.next_seam += 0x10
            if self.next_seam >= 0x34051000:
                raise ValueError('bounded render seam page exhausted')

        def invoke(p):
            if owner is not None:
                check(p.reg('ECX') == owner, name + ' receiver')
            actual = tuple(p.uint(p.reg('ESP') + 4 + i) for i in range(0, pop, 4))
            check(actual == args, name + ' arguments')
            self.calls.append(name)
            # Native callback30/34 return values are deliberately false and
            # ignored; BOOL backend gates default true.
            result = self.results.get(name, 0 if name.startswith('callback') else 1)
            p.fixture_return(pop, eax=result)
        self.p.seams[address] = invoke
        return address

    def frame(self):
        self.calls.clear()
        return self.call(0x41c460, this=self.engine) & 255


BEGIN = ['target-camera', 'renderer-slot1', 'renderer-slot3', 'renderer-slot5',
         'field50-begin', 'callback30']
BODY = ['global75DBA0-work', 'camera1', 'field50-middle', 'camera3', 'camera4']
END = ['target-end', 'callback34', 'renderer-slot4', 'renderer-slot6']


def main():
    f = RenderFixture()
    check(f.frame() == 1 and f.calls == BEGIN + BODY + END, 'full original graphics-frame ordering')
    for stage in ('renderer-slot3', 'renderer-slot5', 'field50-begin'):
        f.results = {stage: 0}
        check(f.frame() == 0 and f.calls == BEGIN[:BEGIN.index(stage) + 1],
              stage + ' short-circuits before callbacks/body/end')
    f.results = {'renderer-slot4': 0}
    check(f.frame() == 0 and f.calls == BEGIN + BODY + END[:-1],
          'failed end skips present but callback34 already ran')
    f.results = {'renderer-slot6': 0}
    check(f.frame() == 0 and f.calls == BEGIN + BODY + END, 'failed present propagates false')
    f.results = {'renderer-slot6': 0x12345680}
    check(f.frame() == 1, 'graphics wrapper normalizes nonzero AL to1')
    f.results = {}
    f.p.mu.mem_write(f.extra + 0x20, b'\0')
    check(f.frame() == 1 and f.calls == BEGIN + BODY[1:] + END,
          'actual global75DBA0 flag getter gates only its work')
    f.p.put_uint(f.engine + 0x28, f.p.uint(f.engine + 0x24))
    check(f.frame() == 1 and f.calls == BEGIN + ['field50-middle'] + END,
          'empty camera vector still performs middle stage')
    f.p.put_uint(f.engine + 0x30, 0)
    f.p.put_uint(f.engine + 0x34, 0)
    check(f.frame() == 1 and f.calls == BEGIN[:-1] + ['field50-middle', 'target-end',
          'renderer-slot4', 'renderer-slot6'], 'optional callbacks may be null')
    check(not f.allocations, 'all synthetic boundary objects, no native renderer allocation or host calls')
    print(f'PASS {checks}/{checks}: original engine graphics frame gates/order with renderer/camera seams')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
