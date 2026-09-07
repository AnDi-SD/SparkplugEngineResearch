"""Decoded attributes at the original XML-library/engine callback boundary.

CP52 executes the whole native XML library and proves the attribute +4 value
record passed through6B1720 to engine4D3A80. This provider replaces only that
library query; every engine callback/vector/string/ownership method executes.
It is not a substitute-success fixture for unresolved engine instructions.
"""
from pc_stl_fixtures import read_cstring
from pc_crt_numeric_fixtures import install_crt_numeric

class DecodedXmlAttributes:
    def __init__(self,p):
        self.p=p;self.attributes={};self.queries=[];self.strings={}
        self.handle=p.allocate(16)
        p.seams[0x6b1720]=self.query
        install_crt_numeric(p)
    def string(self,value):
        if isinstance(value,str):value=value.encode('latin1')
        if len(value)>4095 or b'\0' in value:raise ValueError('bounded XML attribute input')
        if value not in self.strings:
            address=self.p.allocate(len(value)+1);self.p.mu.mem_write(address,value+b'\0');self.strings[value]=address
        return self.strings[value]
    def set(self,attributes):
        self.attributes={}
        for key,value in attributes.items():
            address=self.p.allocate(8);self.p.put_uint(address,self.string(key));self.p.put_uint(address+4,self.string(value));self.attributes[key]=address
    def query(self,p):
        stack=p.reg('ESP');handle=p.uint(stack+4);key=read_cstring(p,p.uint(stack+8)).decode('latin1')
        if handle!=self.handle:raise AssertionError('XML query used another parser handle')
        self.queries.append(key);p.fixture_return(eax=self.attributes.get(key,0))
