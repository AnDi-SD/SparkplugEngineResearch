"""Bounded CRT qsort replacement for UInt16 vertex-ID lists only.

The sorting algorithm is fixture insertion sort, NOT recovered MSVCRT qsort.
Each comparison executes the original PC comparator by an ordinary guest CALL;
no nested emulator.run(), OS forwarding or change to the global guard/caps.
Ties remain stable in this fixture; native CRT tie ordering is not claimed.
"""
import struct


def install_u16_qsort_fixture(p, entry, comparator=0x4607f0):
    address = 0x34120000
    code = bytearray()
    labels, branches = {}, []

    def emit(raw):
        code.extend(bytes.fromhex(raw))

    def label(name):
        labels[name] = len(code)

    def branch(opcode, target):
        emit(opcode)
        branches.append((len(code), target))
        emit('00')

    emit('55 89 e5 53 56 57')  # push ebp; mov ebp,esp; preserve ebx/esi/edi
    emit('bb 01 00 00 00')  # outer index = 1
    label('outer')
    emit('3b 5d 0c')  # cmp ebx,count
    branch('73', 'done')
    emit('89 de')  # inner index = outer index
    label('inner')
    emit('85 f6')
    branch('74', 'next')
    emit('8d 3c 36 03 7d 08')  # edi = base + index*2
    emit('8d 47 fe 57 50 ff 55 14 83 c4 08')  # comparator(left,right), cdecl
    emit('85 c0')
    branch('7e', 'next')  # signed result <= 0: pair already ordered
    emit('66 8b 47 fe 66 8b 17 66 89 07 66 89 57 fe')  # swap UInt16 pair
    emit('4e')
    branch('eb', 'inner')
    label('next')
    emit('43')
    branch('eb', 'outer')
    label('done')
    emit('5f 5e 5b 5d c3')
    for offset, target in branches:
        code[offset:offset+1] = struct.pack('b', labels[target]-offset-1)
    instructions = list(p.decoder.disasm(bytes(code), address))
    assert sum(ins.size for ins in instructions) == len(code)
    p.mu.mem_map(address, 0x1000)
    p.mu.mem_write(address, bytes(code))
    p.mu.mem_protect(address, 0x1000, p.uc.UC_PROT_READ | p.uc.UC_PROT_EXEC)
    for ins in instructions:
        raw = bytes(ins.bytes)

        def allow_exact(p, at=ins.address, expected=raw):
            assert bytes(p.mu.mem_read(at, len(expected))) == expected
        p.seams[ins.address] = allow_exact

    previous = p.seams[entry]
    calls = []

    def qsort(p):
        base, count, width, compare = (
            p.uint(p.reg('ESP')+4*i) for i in (1, 2, 3, 4))
        if (width, compare) != (2, comparator):
            previous(p)  # retain the other fixture's original strict checks
            return
        assert 0 <= count <= 64, 'bounded geometry vertex-ID sort'
        assert 0x31000000 <= base and base+count*2 <= 0x31010000
        calls.append((base, count, width, compare))
        p.set_reg('EIP', address)

    p.seams[entry] = qsort
    return calls
