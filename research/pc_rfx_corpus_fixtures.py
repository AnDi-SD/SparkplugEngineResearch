"""Size-validated library contracts for explicit original RFX attribute strings.

No generic bound is raised: large strings must exactly match whitelist values
from the hash-verified input document; transfers must remain inside registered
input or live native allocation extents, with the unchanged32KiB request cap.
Existing short-string contracts are retained for other addresses/operations.
"""
from pc_xml_event_fixtures import DecodedXmlAttributes

class CorpusXmlAttributes(DecodedXmlAttributes):
    def __init__(self,f,large_values):
        self.large_values={value.encode('latin1') for value in large_values};super().__init__(f.p)
        p=f.p
        def args(n):return [p.uint(p.reg('ESP')+4+4*i) for i in range(n)]
        def extent(address):
            for data,start in self.strings.items():
                if start<=address<start+len(data)+1:return start+len(data)+1-address
            for start,size in f.allocations.items():
                if start not in f.freed and start<=address<start+size:return start+size-address
            return 0
        def long_range(address,size):
            if size>0x8000 or extent(address)<size:raise AssertionError('RFX library transfer leaves explicit live storage')
        for iat in (0x6d9200,0x6d9208):
            target=p.uint(iat);short=p.seams[target]
            def copy(machine,short=short):
                destination,source,size=args(3)
                if size<=4096:short(machine);return
                long_range(source,size);long_range(destination,size)
                machine.mu.mem_write(destination,bytes(machine.mu.mem_read(source,size)));machine.fixture_return(eax=destination)
            p.seams[target]=copy
        target=p.uint(0x6d9204);short_length=p.seams[target]
        def length(machine):
            address=args(1)[0];size=extent(address)
            if size<=4096:short_length(machine);return
            if size>0x8000:raise AssertionError('explicit string backing exceeds request cap')
            data=bytes(machine.mu.mem_read(address,size));end=data.find(b'\0')
            if end<0:raise AssertionError('known string backing has no terminator')
            machine.fixture_return(eax=end)
        p.seams[target]=length
    def string(self,value):
        if isinstance(value,str):value=value.encode('latin1')
        if len(value)<=4095:return super().string(value)
        if value not in self.large_values or len(value)+1>0x8000 or b'\0' in value:raise ValueError('non-whitelisted corpus string')
        if value not in self.strings:
            address=self.p.allocate(len(value)+1);self.p.mu.mem_write(address,value+b'\0');self.strings[value]=address
        return self.strings[value]
