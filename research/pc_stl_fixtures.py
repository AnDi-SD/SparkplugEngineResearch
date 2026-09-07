"""Explicit bounded MSVCP71 char_traits seams for original PC name-registry probes.

No DLL is loaded and no import is forwarded to the host. Only these elementary
byte/string contracts are supplied. Original in-image std::string/container
algorithms still execute; the fixtures do not count as engine reconstruction.
"""


def read_cstring(machine, address, limit=4096):
    if not address:
        raise ValueError('null C-string is outside this fixture contract')
    result = bytearray()
    for offset in range(limit):
        value = bytes(machine.mu.mem_read(address + offset, 1))[0]
        if not value:
            return bytes(result)
        result.append(value)
    raise ValueError('bounded C-string has no terminator')


def install_char_traits(machine):
    base = 0x34010000
    machine.mu.mem_map(base, 0x1000, machine.uc.UC_PROT_READ | machine.uc.UC_PROT_EXEC)

    def args(p, count):
        return [p.uint(p.reg('ESP') + 4 * i) for i in range(1, count + 1)]

    def assign(p):
        destination, source = args(p, 2)
        p.mu.mem_write(destination, bytes(p.mu.mem_read(source, 1)))
        p.fixture_return()

    def length(p):
        p.fixture_return(eax=len(read_cstring(p, args(p, 1)[0])))

    def copy_move(p):
        destination, source, size = args(p, 3)
        if size > 4096:
            raise ValueError('char_traits copy exceeds fixture bound')
        if size:
            p.mu.mem_write(destination, bytes(p.mu.mem_read(source, size)))
        p.fixture_return(eax=destination)

    def compare(p):
        first, second, size = args(p, 3)
        if size > 4096:
            raise ValueError('char_traits compare exceeds fixture bound')
        a, b = (bytes(p.mu.mem_read(address, size)) if size else b'' for address in (first, second))
        p.fixture_return(eax=((a > b) - (a < b)) & 0xffffffff)

    def equal(p):
        first, second = args(p, 2)
        p.fixture_return(eax=int(bytes(p.mu.mem_read(first, 1)) == bytes(p.mu.mem_read(second, 1))))

    def find(p):
        source, size, char = args(p, 3)
        if size > 4096:
            raise ValueError('char_traits find exceeds fixture bound')
        data = bytes(p.mu.mem_read(source, size)) if size else b''
        position = data.find(bytes(p.mu.mem_read(char, 1)))
        p.fixture_return(eax=source + position if position >= 0 else 0)

    # Imported names recovered directly from pristine PE's MSVCP71 table.
    for index, (iat, handler) in enumerate(((0x6d9200, copy_move), (0x6d9204, length),
            (0x6d9208, copy_move), (0x6d920c, assign), (0x6d9210, equal),
            (0x6d9214, find), (0x6d921c, compare))):
        target = base + 0x10 * (index + 1)
        machine.put_uint(iat, target)  # guest IAT data only, not original code
        machine.seams[target] = handler
