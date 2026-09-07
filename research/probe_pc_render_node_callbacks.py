#!/usr/bin/env python3
"""Original PC borrowed render-node callback removal/drain semantics.

Callbacks are explicit recording/mutating leaves, not fabricated partition
implementations. Native vector edits and destruction run with original code.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_scene_render_registry import RenderSceneFixture

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


def main():
    f = RenderSceneFixture()
    p = f.p
    node = f.render_node()
    p.mu.mem_map(0x34090000, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
    entries, observed = [], []
    mutation = False
    for i in range(3):
        obj, table = p.allocate(0x10), p.allocate(0x30)
        entry = 0x34090010 + i * 0x10
        p.put_uint(obj, table)
        p.put_uint(table + 0x2c, entry)
        entries.append(obj)

        def callback(p, obj=obj):
            check(p.reg('ECX') == obj and p.uint(p.reg('ESP') + 4) == node and
                  p.uint(p.reg('ESP') + 8) == 0, 'callback receives owner node and false argument')
            observed.append(obj)
            if mutation:
                # Literal leaf drops the last record itself. Native drain
                # rereads this end, then may pop another remaining entry.
                p.put_uint(node + 0x1cc, p.uint(node + 0x1cc) - 4)
            p.fixture_return(8)
        p.seams[entry] = callback

    def install(values):
        storage = p.allocate(max(4, len(values) * 4))
        f.allocations[storage] = max(4, len(values) * 4)
        for i, value in enumerate(values): p.put_uint(storage + i * 4, value)
        p.put_uint(node + 0x1c8, storage)
        p.put_uint(node + 0x1cc, storage + len(values) * 4)
        p.put_uint(node + 0x1d0, storage + len(values) * 4)
        return storage

    storage = install(entries)
    f.call(0x424d60, this=node, args=(entries[0], 1))
    check(observed == [entries[0]] and p.uint(storage) == entries[2] and
          p.uint(node + 0x1cc) == storage + 8,
          'remove-one replaces first match with last, shrinks before notification')
    observed.clear()
    f.call(0x424d60, this=node, args=(entries[0], 1))
    check(not observed and p.uint(node + 0x1cc) == storage + 8,
          'missing callback removal is no-op without notification')
    f.call(0x424dd0, this=node, args=(1,))
    check(observed == [entries[1], entries[2]] and p.uint(node + 0x1cc) == storage and
          p.uint(node + 0x1c8) == storage and storage not in f.freed,
          'notifying drain works in current reverse order and retains capacity/storage')
    observed.clear()
    f.call(0x424dd0, this=node, args=(0,))
    check(not observed and storage in f.freed and all(p.uint(node + o) == 0 for o in (0x1c8, 0x1cc, 0x1d0)),
          'non-notifying clear frees vector and zeros all pointers')
    storage = install(entries)
    f.call(0x424dd0, this=node, args=(0,))
    check(not observed and storage in f.freed, 'non-notifying populated clear skips every callback')
    storage = install(entries)
    mutation = True
    f.call(0x424dd0, this=node, args=(1,))
    check(observed == [entries[2], entries[0]] and p.uint(node + 0x1cc) == storage,
          'self-removing leaf demonstrates native reread then extra pop; middle callback is skipped')
    mutation = False
    observed.clear()
    f.call(0x424dd0, this=node, args=(0,))
    storage = install(entries)
    f.call(0x4255d0, this=node, args=(1,))
    check(observed == list(reversed(entries)) and storage in f.freed and node in f.freed,
          'actual render-node destructor drains then frees borrowed callback vector')
    f.close()
    print(f'PASS {checks}/{checks}: original PC render-node callback removal/drain/teardown')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']:
        raise SystemExit(main())
    raise SystemExit(run_bounded(Path(__file__), sys.argv[1:]))
