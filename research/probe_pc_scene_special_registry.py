#!/usr/bin/env python3
"""PC exact-ID scene dispatch and actual SkyBox/Projection/LensFlare lists.

Native scenes, managers, render nodes and SkyBox are constructed. Renderable
IDs/storage used for dispatch are explicit borrowed fixtures, not loaded models.
No D3D query/renderer initialization runs. Limits are the common bounded guest.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_scene_lights import LightSceneFixture

checks = 0

def check(value, label):
    global checks
    checks += 1
    if not value: raise AssertionError(label)

class SpecialSceneFixture(LightSceneFixture):
    def __init__(self):
        super().__init__()
        p = self.p
        p.put_uint(0x762e10, 0x7a7124af)
        p.put_uint(0x762e10 + 0x48, 0x75e150)
        p.mu.mem_map(0x340a0000, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
        self.borrowed_nodes = []
        self.records = []

    def sky(self):
        return self.call(0x49e4c0)

    def borrowed_renderables(self, node, identities):
        p = self.p
        pointers = p.allocate(max(4, 4*len(identities)))
        result = []
        for identity in identities:
            obj, table, record = p.allocate(0x100), p.allocate(0x40), p.allocate(0x50)
            p.put_uint(obj, table)
            p.put_uint(record, identity)
            address = 0x340a0100 + len(self.records)*0x10
            self.records.append(record)
            p.put_uint(table + 0x10, address)
            def get_record(p, obj=obj, record=record):
                check(p.reg('ECX') == obj, 'actual exact-ID dispatch leaf receiver')
                p.fixture_return(eax=record)
            p.seams[address] = get_record
            p.put_uint(pointers + 4*len(result), obj)
            result.append(obj)
        for off, value in ((0xbc, pointers), (0xc0, pointers+4*len(result)),
                           (0xc4, pointers+4*len(result))): p.put_uint(node+off, value)
        self.borrowed_nodes.append(node)
        return result

    def close(self):
        # Explicit borrowed renderables never acquire native intrusive ownership.
        for node in self.borrowed_nodes:
            for off in (0xbc, 0xc0, 0xc4): self.p.put_uint(node+off, 0)
        super().close()

def list_entries(p, manager, offset=0x18):
    sentinel = p.uint(manager+offset+4)
    current, previous, result = p.uint(sentinel), sentinel, []
    while current != sentinel:
        check(len(result)<16 and p.uint(current+4)==previous, 'bounded doubly linked list')
        result.append(p.uint(current+8))
        previous, current = current, p.uint(current)
    check(p.uint(sentinel+4)==previous and p.uint(manager+offset+8)==len(result),
          'sentinel previous and count agree')
    return result

def flare_entries(p, manager):
    current, previous, result = p.uint(manager+0x18), 0, []
    while current:
        check(len(result)<16 and p.uint(current+0x58)==previous, 'borrowed flare prev link')
        result.append(current)
        previous, current = current, p.uint(current+0x5c)
    check(p.uint(manager+0x1c)==previous and p.uint(manager+0x20)==len(result),
          'flare list tail/count agree')
    return result

def sky_registration():
    f = SpecialSceneFixture()
    p = f.p
    first, second = f.scene(), f.scene()
    roots = [p.uint(scene+0x14) for scene in (first, second)]
    managers = [p.uint(scene+0x30) for scene in (first, second)]
    skies = [f.sky() for _ in range(3)]
    check(all(f.allocations[sky]==0x1d4 and p.uint(sky)==0x6eecec and
              p.uint(sky+0xb4)==0x6eecd4 for sky in skies), 'actual SkyBox exact1D4 and two tables')
    check(all(f.call(0x408370, this=sky, args=(0x603625d0,)) & 255 for sky in skies),
          'original SkyBox RTTI derives RenderNode, not directly Node')
    for sky in skies: f.call(0x421a60, this=roots[0], args=(sky,))
    check(list_entries(p, managers[0])==skies and p.uint(first+0x20)==3,
          'sky registers both render-node and separate skybox manager lists')
    check(all(p.uint(sky+8)&65535==1 for sky in skies), 'sky manager list adds no intrusive refs')
    f.call(0x421a60, this=roots[1], args=(skies[1],))
    check(list_entries(p, managers[0])==[skies[0],skies[2]] and
          list_entries(p, managers[1])==[skies[1]], 'native SkyBox scene reparent list transfer')
    # Direct support draw is deliberate success/no-op: separate sky pass owns it.
    for sky in skies:
        check(f.call(0x4d74a0, this=sky+0xb4, args=(0,0)) & 255 == 1,
              'SkyBox regular render-support path succeeds without camera/device access')
    f.close()

def exact_renderable_dispatch():
    f = SpecialSceneFixture()
    p = f.p
    first, second = f.scene(), f.scene()
    roots = [p.uint(scene+0x14) for scene in (first,second)]
    node = f.render_node()
    # Unknown58DA4026 is tested as literal matching ID, never assigned a class name.
    ids = (0x435370b5, 0x1cca7732, 0x32bb2f56, 0x58da4026, 0x750f73d9,
           0x5cb4145d, 0x435370b6, 0x435370b5)
    objects = f.borrowed_renderables(node, ids)
    f.call(0x421a60, this=roots[0], args=(node,))
    projection, flare = p.uint(first+0x2c), p.uint(first+0x28)
    check(list_entries(p,projection)==objects[1:5], 'all four exact projection IDs, base/neighbor ignored')
    check(flare_entries(p,flare)==[objects[0],objects[7]], 'two distinct exact LensFlare objects registered')
    check(all(p.uint(obj+8)==0 for obj in objects), 'manager registration leaves borrowed refs unchanged')
    # Projection native add has a membership scan, unlike light/flare/sky append.
    f.call(0x4cdf60, this=projection, args=(objects[2],))
    check(list_entries(p,projection)==objects[1:5], 'projection duplicate registration ignored')
    f.call(0x421a60, this=roots[1], args=(node,))
    check(list_entries(p,projection)==[] and flare_entries(p,flare)==[],
          'old scene specialized lists empty after native reparent')
    check(list_entries(p,p.uint(second+0x2c))==objects[1:5] and
          flare_entries(p,p.uint(second+0x28))==[objects[0],objects[7]],
          'new scene keeps exact renderable order')
    # Must unregister borrowed payloads before clearing vector/destroying node.
    f.call(0x45a8c0, args=(node,second))
    check(list_entries(p,p.uint(second+0x2c))==[] and
          flare_entries(p,p.uint(second+0x28))==[], 'actual specialized unregistration all matches')
    f.close()

def main():
    sky_registration()
    exact_renderable_dispatch()
    print(f'PASS {checks}/{checks}: PC actual specialized scene registry')
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']: raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
