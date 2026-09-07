#!/usr/bin/env python3
"""Original PC serializer registration/lookup/clear and FAT file-index evidence.

The complete manager/FAT constructor and full FFPS loader are NOT substituted.
These operations receive explicit valid consumer-derived empty container input.
Allocator/byte-stream/diagnostic boundaries are fixtures; no OS/game/GPU runs.
"""
from pathlib import Path
import struct
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_animation_lifecycle import LifetimeFixture
from probe_pc_san_reader import ReaderFixture, cstring
from pc_loader_fixtures import empty_manager, empty_fat

checks = 0


def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)


def registry_nodes(p, manager):
    head = p.uint(manager + 0x20)
    node, prior, result = p.uint(head), head, []
    while node != head:
        check(len(result) < 16 and p.uint(node + 4) == prior, 'bounded reciprocal registry links')
        result.append((node, *(p.uint(node + offset) for offset in (8, 12, 16, 20))))
        prior, node = node, p.uint(node)
    check(p.uint(head + 4) == prior and p.uint(manager + 0x24) == len(result),
          'registry tail/count agree')
    return result


def registry():
    f = LifetimeFixture(); p = f.p
    manager, head = empty_manager(f)
    a, b = f.call(0x43dab0), f.call(0x43dab0)
    rows = [(0x56ee563a,a,2,1), (0x56ee563a,b,1,1), (0x56ee563a,a,2,2),
            (0x12345678,b,0xff,3), (0x80000001,b,0x80000000,0x80000000),
            (0x12340000,0,1,1), (0x12340000,b,1,1)]
    for count, (identity, serializer, platform, operation) in enumerate(rows, 1):
        f.call(0x422d90, this=manager, args=(identity,serializer,platform,operation))
        nodes = registry_nodes(p, manager)
        check([r[1:] for r in nodes] == [(i,pm,om,s) for i,s,pm,om in rows[:count]],
              'original registration preserves insertion order and four argument roles')
        check(all(f.allocations[node[0]] == 0x18 for node in nodes), 'native registry node extent18')
    queries = [(0x56ee563a,3,1,a), (0x56ee563a,1,1,b), (0x56ee563a,2,2,a),
               (0x56ee563a,2,3,a), (0x56ee563a,8,1,0), (0x56ee563a,0,1,0),
               (0x56ee563a,3,0,0), (0x12345678,8,2,b), (0x999,3,1,0),
               (0x80000001,0x80000000,0x80000000,b),
               (0x12340000,1,1,0)]
    before = bytes(p.mu.mem_read(manager, 0x2c))
    for identity, platform, operation, expected in queries:
        p.put_uint(manager + 0x10, platform); p.put_uint(manager + 0x14, operation)
        result = f.call(0x4224f0, this=manager, args=(identity,))
        check(result == expected, 'PC first match uses class/platform/operation, including null match')
        check(p.uint(0x13b1f20) == 0x42c9f0, 'original lookup bridge resolves to42C9F0')
    check(bytes(p.mu.mem_read(manager + 0x1c, 16)) == before[0x1c:],
          'lookup preserves registry storage')
    f.call(0x4228a0, this=manager)
    check(registry_nodes(p,manager) == [], 'original clear empties registry')
    check([address for address in f.freed if address in (a,b)] == [a,b],
          'shared serializer pointers destroyed once, in first-occurrence order')
    check(set(f.freed) == set(f.allocations), 'all actual serializers and registration allocations freed')
    p.put_uint(manager + 0x10,3); p.put_uint(manager + 0x14,1)
    check(f.call(0x4224f0,this=manager,args=(0x56ee563a,)) == 0, 'empty lookup after clear')
    f.call(0x4228a0,this=manager)
    check(p.uint(head) == p.uint(head+4) == head, 'repeat empty clear preserves sentinel')


def file_index_case(records, suffix=b'', truncate=None):
    data = struct.pack('<I',len(records))
    for identity, name in records:
        data += struct.pack('<IH',identity,len(name)) + name
    data += suffix
    if truncate is not None: data = data[:truncate]
    f = ReaderFixture(data); p = f.p; fat = empty_fat(f)
    before = bytes(p.mu.mem_read(fat,0x58))
    result = f.call(0x465cd0,this=fat,args=(f.stream,)) & 255
    complete = truncate is None
    check(result == int(complete), 'file-index return follows declared exact-read stream success')
    check(bytes(p.mu.mem_read(fat,0x58)) == before, 'PC file-index does not store entries in FAT')
    check(p.uint(0x13b2264) == 0x13bca90, 'original file-index bridge resolves to13BCA90')
    check(not f.freed, 'native file-index does not clean its allocations on this path')
    if complete:
        check(f.position == len(data)-len(suffix) and not f.errors, 'only declared file-index bytes consumed')
        objects = [address for address,size in f.requests if size == 12]
        check(len(objects) == len(records), 'one native0C file entry per record')
        for obj, (identity,name) in zip(objects,records):
            check(p.uint(obj+4) == identity, 'file ID preserved, duplicates not deduplicated')
            pointer = p.uint(obj+8)
            check((pointer == 0 if not name else bytes(p.mu.mem_read(pointer,len(name))) == name),
                  'length-prefixed file-name bytes preserved')
    else:
        check(bool(f.errors), 'failed exact-read index produces diagnostic')
    # Explicit fixture cleanup, NOT native leak/rollback repair or whole FAT dtor.
    for address in tuple(f.allocations):
        p.run(0x412420,args=(address,),callee_pop=False)
    check(set(f.allocations) == set(f.freed), 'fixture releases all deliberately leaked native allocations')


def file_index():
    file_index_case([])
    file_index_case([],suffix=b'ignored')
    file_index_case([(7,b'ab\0')])
    file_index_case([(7,b''),(7,b'alias\0'),(0,b'z\0')])
    file_index_case([],truncate=0)
    file_index_case([(7,b'ab\0')],truncate=4)
    file_index_case([(7,b'ab\0')],truncate=8)
    file_index_case([(7,b'ab\0')],truncate=11)


def main(mode):
    if mode == 'registry': registry()
    elif mode == 'file-index': file_index()
    else: raise ValueError('known bounded profile required')
    print(f'PASS {checks}/{checks}: original PC loader {mode} contracts')
    return 0


if __name__ == '__main__':
    if sys.argv[1:2] == ['--guest']: raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
