"""Bounded writable byte stream and diagnostic output, not native I/O proof."""
from pc_loader_fixtures import PCFileBytesFixture


class PCWriteBytesFixture(PCFileBytesFixture):
    MaximumBytes=0x10000

    def __init__(self,data=b''):
        super().__init__(data)
        p=self.p;p.put_uint(p.uint(self.stream)+0x38,0x34000050)
        p.seams[0x34000050]=self.write
        p.seams[0x4135e0]=self.trace
        self.messages=[];self.write_calls=0;self.fail_write_call=None

    def trace(self,p):
        # Variadic cdecl trace formatting/output is an external boundary.
        self.messages.append(p.uint(p.reg('ESP')+4));p.fixture_return()

    def write(self,p):
        sp=p.reg('ESP');source=p.uint(sp+4);count=p.uint(sp+8)
        if count>self.MaximumBytes or self.position+count>self.MaximumBytes:
            raise AssertionError('bounded writer output')
        self.write_calls+=1
        if self.write_calls==self.fail_write_call:
            self.io.append(('write-failed',self.position,count));p.fixture_return(8,eax=0);return
        if len(self.data)<self.position+count:
            self.data+=bytes(self.position+count-len(self.data))
        self.data=self.data[:self.position]+bytes(p.mu.mem_read(source,count))+self.data[self.position+count:]
        self.io.append(('write',self.position,count));self.position+=count;p.fixture_return(8,eax=1)
