"""Bounded imported MSVCR71 memory contracts; never forwarded to host APIs.

Only the pristine PE's malloc/realloc/free/memset/memmove IAT entries below are
supplied. This is external-library input, not recovered engine behavior. A
successful realloc moves storage; callers must obey the ordinary CRT contract.
"""

def install_crt_memory(fixture):
    from pc_stl_fixtures import read_cstring
    p=fixture.p;base=0x340b0000;limit=0x8000
    p.mu.mem_map(base,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
    def arguments(machine,n):return [machine.uint(machine.reg('ESP')+4+4*i) for i in range(n)]
    def allocate(size):
        if size>limit:raise AssertionError('CRT allocation exceeds unchanged 32KiB request cap')
        address=p.allocate(max(1,size));p.mu.mem_write(address,b'\xcc'*max(1,size))
        fixture.allocations[address]=size;fixture.requests.append((address,size));return address
    def realloc(machine):
        old,size=arguments(machine,2)
        if old and (old not in fixture.allocations or old in fixture.freed):raise AssertionError('CRT realloc requires live tracked allocation')
        if not size:
            if old:fixture.freed.append(old)
            machine.fixture_return(eax=0);return
        new=allocate(size)
        if old:
            count=min(size,fixture.allocations[old])
            if count:machine.mu.mem_write(new,bytes(machine.mu.mem_read(old,count)))
            fixture.freed.append(old)
        machine.fixture_return(eax=new)
    def memset(machine):
        destination,value,size=arguments(machine,3)
        if size>limit:raise AssertionError('bounded CRT memset')
        if size:machine.mu.mem_write(destination,bytes([value&255])*size)
        machine.fixture_return(eax=destination)
    def memmove(machine):
        destination,source,size=arguments(machine,3)
        if size>limit:raise AssertionError('bounded CRT memmove')
        if size:machine.mu.mem_write(destination,bytes(machine.mu.mem_read(source,size)))
        machine.fixture_return(eax=destination)
    def strdup(machine):
        data=read_cstring(machine,arguments(machine,1)[0])+b'\0'
        target=allocate(len(data));machine.mu.mem_write(target,data);machine.fixture_return(eax=target)
    for index,(iat,handler) in enumerate(((0x6d9330,fixture.allocate),(0x6d9334,realloc),(0x6d9338,fixture.free),(0x6d927c,memset),(0x6d9300,memmove),(0x6d9260,strdup))):
        target=base+16*(index+1);p.put_uint(iat,target);p.seams[target]=handler
