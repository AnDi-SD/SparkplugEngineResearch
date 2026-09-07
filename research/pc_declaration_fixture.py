"""Explicit COM device and initialized-empty declaration map, not native startup."""
from probe_pc_dx_buffers import BufferFixture,check

class DeclarationDeviceFixture(BufferFixture):
    def __init__(self,mode='declaration'):
        super().__init__(mode,renderer_extent=0xf364,device_vtable_size=0x160)
        self.declarations={};self.declaration_arrays=[]
        p=self.p;head=p.allocate(24);self.allocations[head]=24
        self.declaration_head=head
        for offset in (0,4,8):p.put_uint(head+offset,head)
        p.mu.mem_write(head+20,b'\x01\x01')
        p.put_uint(self.renderer+0xf35c,head);p.put_uint(self.renderer+0xf360,0)
        p.seams[0x4123f0]=self.allocate
        device_table=p.uint(self.device)
        p.put_uint(device_table+0x158,0x34060090);p.seams[0x34060090]=self.create_declaration
        p.put_uint(device_table+0x15c,0x340600a0);p.seams[0x340600a0]=self.set_declaration
        self.declaration_table=p.allocate(12);p.put_uint(self.declaration_table+8,0x340600b0)
        p.seams[0x340600b0]=self.release_declaration

    def create_declaration(self,p):
        sp=p.reg('ESP');device,elements,out=(p.uint(sp+4+i*4) for i in range(3))
        check(device==self.device,'CreateVertexDeclaration uses explicit device')
        size=self.allocations.get(elements,0)
        check(0<size<=256 and size%8==0,'native declaration builder allocation bound')
        raw=bytes(p.mu.mem_read(elements,size))
        terminals=[i for i in range(0,size,8) if raw[i:i+8]==bytes.fromhex('ff00000011000000')]
        check(len(terminals)==1,'native D3D declaration sentinel inside allocated capacity')
        raw=raw[:terminals[0]+8];self.declaration_arrays.append(raw)
        self.events.append(('create-declaration',raw.hex()))
        if self.mode=='declaration-failure':p.put_uint(out,0);p.fixture_return(12,eax=0x80004005);return
        obj=p.allocate(4);p.put_uint(obj,self.declaration_table);self.declarations[obj]=1
        p.put_uint(out,obj);p.fixture_return(12,eax=0)

    def set_declaration(self,p):
        sp=p.reg('ESP');device,declaration=p.uint(sp+4),p.uint(sp+8)
        check(device==self.device and (not declaration or self.declarations.get(declaration)==1),'SetVertexDeclaration uses live/null explicit COM object')
        self.events.append(('set-declaration',declaration));p.fixture_return(8,eax=0)

    def release_declaration(self,p):
        obj=p.uint(p.reg('ESP')+4);check(self.declarations.get(obj)==1,'declaration COM release exactly once')
        self.declarations[obj]=0;self.events.append(('release-declaration',obj));p.fixture_return(4,eax=0)

    def declaration_objects(self):
        p=self.p
        return [a for a,s in self.allocations.items() if a not in self.freed and s==0x1c and p.uint(a)==0x6f2e58]

    def clear_declarations(self,allow_overwritten_leak=False):
        p=self.p
        for obj in self.declaration_objects():self.call(0x4c9ce0,this=obj,args=(1,))
        self.call(0x4ae140,this=self.renderer+0xf358)
        if not allow_overwritten_leak:
            check(all(refs==0 for refs in self.declarations.values()),'all COM declarations released')
