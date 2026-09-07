"""Explicit PC startup/container/stream inputs, NOT native startup evidence.

Original manager/FAT construction and CRT RTTI registration are unresolved.
These consumer-derived containers let separately studied native operations run
without substituting their lookup, factory, deserialization or cleanup bodies.
"""
from probe_pc_san_reader import ReaderFixture


def empty_manager(f):
    p=f.p;manager=p.allocate(0x2c);head=p.allocate(0x18)
    p.put_uint(manager,0x6dc838);p.put_uint(manager+0x20,head)
    p.put_uint(head,head);p.put_uint(head+4,head)
    p.seams[0x4123f0]=f.allocate
    return manager,head


def empty_fat(f):
    p=f.p;fat=p.allocate(0x58);p.put_uint(fat+0x10,1)
    for offset in (0x14,0x20,0x2c):
        head=p.allocate(24)
        for field in (0,4,8):p.put_uint(head+field,head)
        p.mu.mem_write(head+20,b'\x01\x01');p.put_uint(fat+offset+4,head)
    for offset in (0x38,0x48):
        head=p.allocate(12);p.put_uint(head,head);p.put_uint(head+4,head)
        p.put_uint(fat+offset+4,head);p.put_uint(fat+offset+12,head)
    return fat


def animation_rtti(f):
    """One-entry valid RB tree, seeded from original initializer/getter evidence.

    No fake success callback for RTTI membership/create. The actual initializer
    6D1C10 capped in its protected registration dependency and is NOT resumed.
    Engine RTTI is Animation->Controller->SubController->Base, NOT NamedObject,
    despite Animation's physical named-object prefix.
    """
    p=f.p;manager=p.allocate(0x20);head=p.allocate(24);node=p.allocate(24)
    p.put_uint(manager+0x18,head);p.put_uint(manager+0x1c,1)
    for offset in (0,4,8):
        p.put_uint(head+offset,node);p.put_uint(node+offset,head)
    p.mu.mem_write(head+20,b'\x01\x01');p.mu.mem_write(node+20,b'\x01\x00')
    p.put_uint(node+12,0x56ee563a);p.put_uint(node+16,0x75d248)
    p.put_uint(0x755378,manager);p.put_uint(0x75d248+0x4c,0x41a090)
    for record,identity,parent in ((0x75d248,0x56ee563a,0x75de50),
          (0x75de50,0x4fad24f1,0x760340),(0x760340,0x062c22ed,0x755310),
          (0x755310,0x415352a1,0)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    p.seams[0x4123f0]=f.allocate
    return manager


class PCFileBytesFixture(ReaderFixture):
    """Byte-backed successful-open PC file contract, no Win32 calls.

    PC stream origin +14 affects seek-start and tell, NOT physical size.
    Nonzero short reads succeed, zero read/EOF fail (6BDB60); source never
    changes. Seeks remain bounded to the supplied file for guest safety.
    Named-object string ownership/diagnostics/math remain ReaderFixture seams.
    """
    def __init__(self,data):
        super().__init__(data)
        p=self.p;p.put_uint(p.uint(self.stream)+0x3c,0x34000040)
        p.seams[0x34000040]=self.get_size
        self.io=[]

    def get_size(self,p):
        p.put_uint(p.uint(p.reg('ESP')+4),len(self.data));p.fixture_return(4,eax=1)

    def tell(self,p):
        p.put_uint(p.uint(p.reg('ESP')+4),self.position-p.uint(self.stream+0x14))
        p.fixture_return(4,eax=1)

    def seek(self,p):
        sp=p.reg('ESP');mode=p.uint(sp+4);offset=p.uint(sp+8)
        signed=offset if offset<0x80000000 else offset-0x100000000
        if mode not in (1,2,4):raise AssertionError('unknown file seek mode')
        position={1:p.uint(self.stream+0x14),2:len(self.data),4:self.position}[mode]+signed
        self.io.append(('seek',mode,signed,position))
        if not 0<=position<=len(self.data):p.fixture_return(8,eax=0);return
        self.position=position;p.fixture_return(8,eax=1)

    def read(self,p):
        sp=p.reg('ESP');destination=p.uint(sp+4);count=p.uint(sp+8)
        actual=min(count,len(self.data)-self.position)
        self.io.append(('read',self.position,count,actual))
        if actual:p.mu.mem_write(destination,self.data[self.position:self.position+actual])
        self.position+=actual;p.fixture_return(8,eax=int(actual!=0))
