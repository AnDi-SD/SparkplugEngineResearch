#!/usr/bin/env python3
"""Real SAN reader + original shared animation-name registry, bounded guest only.

The stream, named-object string ownership, diagnostic and char_traits boundaries
remain explicit fixtures. SAN parsing, std::map, binding/unbinding, constructor
and destructor code are original. This is not a Windows loader/game test.
"""
from collections import Counter
from pathlib import Path
import sys
from inspect_pc_san_keys import DEFAULT, inspect, u32
from pc_instruction_emulator import run_bounded
from pc_stl_fixtures import install_char_traits, read_cstring
from probe_pc_animation_manager import registry_entries
from probe_pc_san_reader import ReaderFixture

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


def shared_asset(name, *, quiet=False):
    raw = (DEFAULT / name).read_bytes()
    summary, tracks = inspect(DEFAULT / name)
    f = ReaderFixture(raw[u32(raw, 20) + 8:])
    p = f.p
    del p.seams[0x454370]
    del p.seams[0x453b10]
    install_char_traits(p)
    manager = f.call(0x454640)
    serializer = f.call(0x43dab0)
    names = [track['name'].encode('latin1') for track in tracks]
    expected_ids = {text: index + 1 for index, text in enumerate(dict.fromkeys(names))}
    references = Counter(names)
    objects = []
    snapshots = []

    def snapshot(ids=()):
        values = registry_entries(p, manager)
        snapshots.append({'slots': list(ids), 'registry': [
            [name.decode('latin1'), *binding] for name, binding in sorted(values.items())
        ], 'next': p.uint(manager + 0x14)})

    def load():
        f.position = 0
        animation = f.call(0x41a090)
        result = f.call(0x43ecc0, this=serializer + 0x10, args=(f.stream, animation))
        check(result & 255 == 1 and not f.errors, 'original reader succeeds with real registry')
        check(f.position == len(f.data), 'complete object field stream consumed')
        check(p.uint(animation + 0x20) == len(tracks), 'track count unchanged')
        ids = []
        for index, text in enumerate(names):
            track = p.uint(animation + 0x1c) + index * 0x44
            check(read_cstring(p, p.uint(track + 0x10) + 9) == text, 'track name bytes preserved')
            ids.append(p.uint(track + 0x14))
        return animation, ids

    for count in (1, 2):
        animation, ids = load()
        objects.append(animation)
        check(ids == [expected_ids[text] for text in names], 'simultaneous resources share original slot IDs')
        check(registry_entries(p, manager) == {
            text: (expected_ids[text], refs * count) for text, refs in references.items()
        }, 'registry retains one reference per live track across animations')
        snapshot(ids)
    for count, animation in zip((1, 0), objects):
        f.call(p.uint(p.uint(animation)), this=animation, args=(1,))
        check(registry_entries(p, manager) == ({
            text: (expected_ids[text], refs) for text, refs in references.items()
        } if count else {}), 'destruction releases only that animation name references')
        snapshot()
    animation, ids = load()
    check(ids == [expected_ids[text] + len(expected_ids) for text in names],
          'reload after last release gets fresh monotonic slots')
    snapshot(ids)
    f.call(p.uint(p.uint(animation)), this=animation, args=(1,))
    check(registry_entries(p, manager) == {}, 'registry empty after third animation destruction')
    snapshot()
    f.call(p.uint(p.uint(serializer)), this=serializer, args=(1,))
    debug = p.uint(0x75526c)
    if debug:
        f.call(p.uint(p.uint(debug)), this=debug, args=(1,))
    f.call(0x4545d0, this=manager, args=(1,))
    check(set(f.freed) == set(f.allocations), 'all original resource/registry allocations released')
    if not quiet:
        print(f'ASSET {name}: {len(tracks)} tracks; simultaneous shared IDs, release and reload PASS', flush=True)
    return snapshots


def main(names):
    if len(names) > 1 or any(name not in {'bbush.san', 'bflower.san'} for name in names):
        raise ValueError('one bounded pristine asset per guest process')
    shared_asset(names[0] if names else 'bbush.san')
    print(f'PASS {checks}/{checks}: original SAN/manager integration checks')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main(sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
