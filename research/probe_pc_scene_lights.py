#!/usr/bin/env python3
"""Original PC light factory/copy/scene lists and render-node light caches.

Actual light/manager/node methods execute. Only existing bounded allocation,
name-owner, clone-pair recording, preinitialized RTTI and absent renderer/startup
storage are fixtures. The clone-manager root transaction is not claimed here.
Scene's light-manager render-list link is the exact45D850 assignment, without
executing its unrelated projection/device/partition initialization.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_scene_render_registry import RenderSceneFixture

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value: raise AssertionError(label)


class LightSceneFixture(RenderSceneFixture):
    def __init__(self):
        super().__init__()
        for record, identity, parent in ((0x75e278, 0x72444900, 0x75dd88),
                                         (0x75d4e8, 0x5e6402df, 0x75e278)):
            self.p.put_uint(record, identity)
            self.p.put_uint(record + 0x48, parent)

    def scene(self):
        result = super().scene()
        self.p.put_uint(self.p.uint(result + 0x34) + 0x1c, result + 0x18)
        return result

    def light(self):
        return self.call(0x41a330)

    def close(self):
        manager = self.p.uint(0x74e060)
        if manager and manager in self.allocations and manager not in self.freed:
            self.call(self.p.uint(self.p.uint(manager)), this=manager, args=(1,))
        super().close()


def light_entries(p, manager):
    previous, current, result = 0, p.uint(manager + 0x10), []
    while current:
        check(len(result) < 16 and p.uint(current + 0xb8) == previous,
              'native light list bounded prev link')
        result.append(current)
        previous, current = current, p.uint(current + 0xbc)
    check(previous == p.uint(manager + 0x14) and len(result) == p.uint(manager + 0x18),
          'native light head/tail/count agree')
    return result


def cached_lights(p, cache):
    count = p.uint(cache + 0x24)
    check(count <= 8, 'fixed ordinary light cache does not exceed8')
    return [p.uint(cache + i * 4) for i in range(count)]


def factory_copy_clone():
    f = LightSceneFixture()
    p = f.p
    source, destination = f.light(), f.light()
    check(f.allocations[source] == f.allocations[destination] == 0xf0 and
          p.uint(source) == 0x6de990 and p.uint(source + 0xb4) == 0x6de98c,
          'actual concrete light factory exactF0 and both vptrs')
    check(p.uint(source + 0xb8) == p.uint(source + 0xbc) == p.uint(source + 0xc0) == 0,
          'light intrusive links/type zero defaults')
    check(p.floats(source + 0xc4, 4) == (1., 1., 1., 1.) and
          p.floats(source + 0xd8, 1) == (1.,) and p.floats(source + 0xe0, 3) == (200., 0., 0.),
          'original white/intensity/range/angle defaults')
    check(p.uint(source + 0xd4) == 0xcccccc00 and p.uint(source + 0xdc) == 0xcccccccc and
          p.uint(source + 0xec) == 0xcccc0100, 'untouched opaque/padding and false shadow/true light-enabled')
    p.put_uint(source + 0xc0, 2)
    p.put_floats(source + 0xc4, (.25, .5, .75, .125))
    p.mu.mem_write(source + 0xd4, b'\1')
    p.put_floats(source + 0xd8, (3.25,))
    p.put_uint(source + 0xdc, 0x12345678)
    p.put_floats(source + 0xe0, (24., .5, 1.))
    p.mu.mem_write(source + 0xec, b'\1\0')
    p.put_floats(destination + 0xd8, (9.5,))
    check(f.call(0x428eb0, this=source, args=(destination,)) & 255 == 1, 'actual PC inherited light copy')
    for offset, size in ((0xc0, 20), (0xd4, 1), (0xdc, 16), (0xec, 2)):
        check(bytes(p.mu.mem_read(source + offset, size)) == bytes(p.mu.mem_read(destination + offset, size)),
              'copy transfers light payload including explicitly initialized opaque bits')
    check(p.floats(destination + 0xd8, 1) == (9.5,),
          'PC independently confirms intensity omission, even for nonfresh destination')
    pairs = []
    def pair(p):
        check(p.reg('ECX') == p.uint(0x74e060), 'explicit clone-pair recorder receiver')
        pairs.append((p.uint(p.reg('ESP') + 4), p.uint(p.reg('ESP') + 8)))
        p.fixture_return(8)
    p.seams[0x412f70] = pair
    clone = f.call(0x41aca0, this=source)
    check(clone and f.allocations[clone] == 0xf0 and p.floats(clone + 0xd8, 1) == (1.,),
          'actual light-data clone retains constructor intensity1')
    check(pairs == [(source, clone)], 'native clone registers source/destination at explicit pair seam')
    check(p.uint(clone + 0xdc) == 0x12345678 and p.uint(clone + 0x3c) == 0 and
          p.uint(clone + 0xb8) == p.uint(clone + 0xbc) == 0,
          'clone copies opaque state, not scene membership links')
    for light in (source, destination, clone): f.call(0x435430, this=light, args=(1,))
    f.close()


def manager_lifetime():
    f = LightSceneFixture()
    p = f.p
    manager = f.call(0x46ab80)
    check(f.allocations[manager] == 0x24 and p.uint(manager) == 0x6e8ca4 and
          all(p.uint(manager + off) == 0 for off in (0x10, 0x14, 0x18, 0x1c)) and
          p.uint(manager + 0x20) == 0xcccccccc,
          'standalone LightManager24 has empty lists but owner20 remains uninitialized')
    light = f.light()
    f.call(0x45a780, this=manager, args=(light,))
    pairs = []
    def pair(p):
        pairs.append((p.uint(p.reg('ESP') + 4), p.uint(p.reg('ESP') + 8)))
        p.fixture_return(8)
    p.seams[0x412f70] = pair
    p.put_uint(manager + 0x20, 0x12345678)  # borrowed literal state, not dereferenced by native clone/dtor
    clone = f.call(0x46abf0, this=manager)
    check(pairs == [(manager, clone)] and f.allocations[clone] == 0x24 and
          all(p.uint(clone + off) == 0 for off in (0x10, 0x14, 0x18, 0x1c)) and
          p.uint(clone + 0x20) == 0xcccccccc,
          'LightManager clone is fresh, list not copied and owner left untouched')
    f.call(0x46a830, this=manager, args=(1,))
    check(light not in f.freed and p.uint(light + 8) & 65535 == 0,
          'manager dtor does not destroy borrowed registered light')
    f.call(0x46a830, this=clone, args=(1,))
    f.call(0x435430, this=light, args=(1,))
    f.close()


def registration_reparent_refresh():
    f = LightSceneFixture()
    p = f.p
    scenes = [f.scene(), f.scene()]
    roots = [p.uint(scene + 0x14) for scene in scenes]
    managers = [p.uint(scene + 0x34) for scene in scenes]
    lights = [f.light() for _ in range(3)]
    node = f.render_node()
    f.call(0x421a60, this=roots[0], args=(node,))
    p.put_floats(node + 0xc8, (0., 0., 0., 2.))
    for light in lights:
        f.call(0x421a60, this=roots[0], args=(light,))
        check(p.uint(light + 8) & 65535 == 1, 'scene light list adds no owning reference')
    check(light_entries(p, managers[0]) == lights and light_entries(p, managers[1]) == [],
          'actual typed light branch appends in native order')
    visited = []
    def observe(mu, address, size, _):
        if address == 0x428dd0:
            visited.append(p.reg('ECX'))
    handle = p.mu.hook_add(p.uc.UC_HOOK_CODE, observe)
    f.call(0x46aaf0, this=managers[0], args=(0,))
    p.mu.hook_del(handle)
    check(visited == lights, 'native debug traversal invokes each actual light virtual38 in list order')
    f.call(0x45a7d0, this=p.uint(0x75db90))
    check(cached_lights(p, node + 0xf0) == lights, 'scene world builds actual nonempty light cache')
    f.call(0x421a60, this=roots[1], args=(lights[1],))
    check(light_entries(p, managers[0]) == [lights[0], lights[2]] and
          light_entries(p, managers[1]) == [lights[1]], 'typed reparent unlinks middle light and appends destination')
    check(cached_lights(p, node + 0xf0) == [lights[0], lights[2]],
          'unregister removes old-scene cached light using original swap-last cache helper')
    light = lights[0]
    p.put_uint(light + 0xc0, 1)
    p.put_floats(light + 0xe0, (1.,))
    p.put_floats(light + 0x20, (100., 0., 0.))
    p.put_uint(light + 0xb0, p.uint(light + 0xb0) | 9)
    f.call(0x428c30, this=light, args=(0,))
    check(p.floats(light + 0x74, 3) == (100., 0., 0.) and not p.uint(light + 0xb0) & 8,
          'light world updates PRS then consumes light-dirty8')
    check(cached_lights(p, node + 0xf0) == [lights[2]],
          'actual LightManager refresh removes newly out-of-range light from render-node cache')
    p.put_floats(light + 0x20, (0., 0., 0.))
    p.put_uint(light + 0xb0, p.uint(light + 0xb0) | 1)
    f.call(0x428c30, this=light, args=(8,))
    check(cached_lights(p, node + 0xf0) == [lights[2], lights[0]],
          'inherited dirty8 refresh adds eligible light at current cache tail')
    f.call(0x420de0, this=roots[0], args=(0,))
    check(all(not p.uint(obj + 0xb0) & 0x100 for obj in (roots[0], node, lights[0], lights[2])) and
          all(p.uint(obj + 0xb0) & 0x200 for obj in (roots[0], node, lights[0], lights[2])),
          'original420DE0 recursively clears hierarchy100 while preserving Enabled200')
    check(p.uint(node + 0x3c) == scenes[0] and light_entries(p, managers[0]) == [lights[0], lights[2]],
          'hierarchy activation flag does not unregister scene links/lists')
    f.call(0x46ac40, this=managers[0], args=(node,))
    check(cached_lights(p, node + 0xf0) == [], 'explicit rebuild filters hierarchy-inactive lights')
    f.call(0x420de0, this=roots[0], args=(1,))
    f.call(0x46ac40, this=managers[0], args=(node,))
    check(cached_lights(p, node + 0xf0) == [lights[0], lights[2]],
          'recursive hierarchy reactivation plus rebuild restores stable manager selection order')
    f.close()


def eligibility():
    f = LightSceneFixture()
    p = f.p
    light, node, static = f.light(), f.render_node(), p.allocate(0x80)
    p.put_floats(node + 0xd8, (0., 0., 0., 2.))
    p.put_floats(static + 0x34, (0., 0., 0., 2.))
    p.put_uint(light + 0xb0, 0x100)  # deliberately no Node Enabled200
    p.put_floats(light + 0x74, (100., 0., 0.))
    p.put_floats(light + 0xe0, (3.,))
    for kind in (0, 3):
        p.put_uint(light + 0xc0, kind)
        check(f.call(0x46a850, args=(light, node)) & 255 == 1 and
              f.call(0x46a950, args=(light, static)) & 255 == 1,
              'directional/ambient ignore distance and Node Enabled200')
    for kind in (1, 2, 4):  # unknown positive values follow the same literal range branch
        p.put_uint(light + 0xc0, kind)
        for distance, expected in ((5., 1), (5.25, 0), (0., 1)):
            p.put_floats(light + 0x74, (distance, 0., 0.))
            check(f.call(0x46a850, args=(light, node)) & 255 == expected and
                  f.call(0x46a950, args=(light, static)) & 255 == expected,
                  'point/spot/other type uses inclusive combined sphere range')
    p.put_uint(light + 0xc0, 0)
    p.mu.mem_write(light + 0xec, b'\1')
    check(f.call(0x46a850, args=(light, node)) & 255 == 0, 'default node121 excludes shadow-volume light')
    p.mu.mem_write(node + 0x121, b'\0')
    check(f.call(0x46a850, args=(light, node)) & 255 == 1 and
          f.call(0x46a950, args=(light, static)) & 255 == 0,
          'node121 can allow shadow light, static-object test always rejects it')
    p.mu.mem_write(light + 0xec, b'\0\0')
    check(f.call(0x46a850, args=(light, node)) & 255 == 0, 'separate light-enabledED false rejects')
    p.mu.mem_write(light + 0xed, b'\1')
    p.put_uint(light + 0xb0, 0x200)
    check(f.call(0x46a850, args=(light, node)) & 255 == 0,
          'Enabled200 does not replace required active-hierarchy100')
    f.call(0x435430, this=light, args=(1,))
    f.call(0x4255d0, this=node, args=(1,))
    f.close()


def fixed_cache():
    f = LightSceneFixture()
    p = f.p
    cache = p.allocate(0x28)
    f.call(0x490b20, this=cache)
    check(bytes(p.mu.mem_read(cache, 0x28)) == b'\0' * 0x28, 'native cache ctor exactly8 pointers/ambient/count')
    lights = [f.light() for _ in range(11)]
    f.call(0x490b50, this=cache, args=(lights[0],))  # resolves protected constant before capacity test
    check(p.uint(0x13b3688) == 8, 'native append capacity constant verified before ninth insertion')
    for light in lights[1:9]: f.call(0x490b50, this=cache, args=(light,))
    check(cached_lights(p, cache) == lights[:8] and p.uint(cache + 0x20) == 0,
          'safe native capacity guard rejects ninth ordinary light')
    f.call(0x490b50, this=cache, args=(lights[0],))
    check(cached_lights(p, cache) == lights[:8], 'full cache and duplicate do not append')
    for light in lights[9:]:
        p.put_uint(light + 0xc0, 3)
        f.call(0x490b50, this=cache, args=(light,))
    check(p.uint(cache + 0x20) == lights[9] and p.uint(cache + 0x24) == 8,
          'first ambient wins independently of full ordinary cache')
    f.call(0x490ba0, this=cache, args=(lights[1],))
    check(cached_lights(p, cache) == [lights[0], lights[7], *lights[2:7]] and
          p.uint(cache + 7 * 4) == lights[7], 'middle remove swaps last and leaves stale unused tail pointer')
    f.call(0x490b50, this=cache, args=(lights[8],))
    check(cached_lights(p, cache)[-1] == lights[8], 'freed logical capacity accepts a new source')
    f.call(0x490ba0, this=cache, args=(lights[8],))
    check(p.uint(cache + 7 * 4) == 0 and p.uint(cache + 0x24) == 7,
          'last-element remove explicitly zeroes that slot')
    f.call(0x490ba0, this=cache, args=(lights[10],))
    check(p.uint(cache + 0x20) == lights[9], 'removing unselected ambient preserves current')
    f.call(0x490ba0, this=cache, args=(lights[9],))
    check(p.uint(cache + 0x20) == 0, 'ambient remove clears only matching ambient slot')
    check(all(p.uint(light + 8) & 65535 == 0 for light in lights), 'cache pointers are borrowed without refs')
    for light in lights: f.call(0x435430, this=light, args=(1,))
    f.close()


def partition_payload_walk():
    f = LightSceneFixture()
    p = f.p
    light = f.light()
    p.put_uint(light + 0xb0, 0x100)
    p.put_uint(light + 0xc0, 1)
    p.put_floats(light + 0xe0, (3.,))
    manager = f.call(0x46ab80)
    # Literal consumer layout only: no claim these are initialized concrete
    # partition-system classes or decoded SMO partition resources.
    nodes = [p.allocate(0x80) for _ in range(3)]
    payloads = [p.allocate(0x80) for _ in range(2)]
    children = p.allocate(8)
    p.put_uint(children, nodes[1]); p.put_uint(children + 4, nodes[2])
    p.put_uint(nodes[0] + 0x58, children); p.put_uint(nodes[0] + 0x5c, 2)
    for node, payload, distance in zip(nodes[1:], payloads, (5., 5.25)):
        p.put_uint(node + 0x78, payload)
        p.put_floats(payload + 0x34, (distance, 0., 0., 2.))
        f.call(0x490b20, this=payload + 0x4c)
    f.call(0x46ab20, this=manager, args=(nodes[0], light))
    check(cached_lights(p, payloads[0] + 0x4c) == [light] and
          cached_lights(p, payloads[1] + 0x4c) == [],
          'original recursive partition consumer skips absent payload and selects child payload by range')
    p.put_floats(light + 0x74, (10., 0., 0.))
    f.call(0x46ab20, this=manager, args=(nodes[0], light))
    check(cached_lights(p, payloads[0] + 0x4c) == [light] and
          cached_lights(p, payloads[1] + 0x4c) == [light], 'inclusive radius on both sides adds second payload')
    f.call(0x46a7e0, this=manager, args=(nodes[0], light))
    check(all(cached_lights(p, payload + 0x4c) == [] for payload in payloads),
          'original partition-unregister consumer recursively removes cached borrowed light')
    f.call(0x46a830, this=manager, args=(1,))
    f.call(0x435430, this=light, args=(1,))
    f.close()


def main():
    factory_copy_clone()
    manager_lifetime()
    registration_reparent_refresh()
    eligibility()
    fixed_cache()
    partition_payload_walk()
    print(f'PASS {checks}/{checks}: original PC light lifecycle/scene selection/cache')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
