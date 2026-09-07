"""Declared D3DX9 compiler/COM result contracts; no shader library or GPU runs.

PC611241/611324 match the7/10-argument stdcall D3DXAssembleShader/CompileShader
contracts, including output buffers and reflection table; engine4CFFE0 is
never replaced. Fixture bytecode/reflection are explicit SDK outputs, not
claimed recovered compiler implementation or GPU-valid generated shaders.
"""
from pc_stl_fixtures import read_cstring

class ShaderCompilerOutput:
    def __init__(self,p,bytecode=b'\x10\x20\x30\x40\x50',constants=(),hresult=0,warning=False,compact_storage=False):
        self.p=p;self.bytecode=bytecode;self.constants=list(constants);self.hresult=hresult;self.calls=[];self.events=[];self.released=[]
        base=0x340e0000;p.mu.mem_map(base,0x1000,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC);self.next=base+16
        external_cursor=base+0x400
        def allocate(size):
            nonlocal external_cursor
            if not compact_storage:return p.allocate(size)
            # Pure SDK output/COM data share this already-declared fixture page.
            # Engine allocations stay in the unchanged64KiB arena and still
            # use the ordinary tracked allocator. No additional page is mapped.
            rounded=(size+15)&~15;address=external_cursor;external_cursor+=rounded
            if external_cursor>base+0x1000:raise AssertionError('bounded SDK fixture page exhausted')
            return address
        def method(vtable,offset,handler):
            target=self.next;self.next+=16;p.put_uint(vtable+offset,target);p.seams[target]=handler
        def raw(data):
            address=allocate(max(1,len(data)));p.mu.mem_write(address,data or b'\0');return address
        self.buffer_data=raw(bytecode);self.warning_data=raw(b'Fixture warning\0')
        vt=allocate(20);self.buffer=allocate(4);p.put_uint(self.buffer,vt);self.warning=allocate(4) if warning else 0
        if self.warning:p.put_uint(self.warning,vt)
        def release(machine):
            obj=self.args(1)[0]
            if obj in self.released:raise AssertionError('duplicate SDK object Release')
            self.released.append(obj);self.events.append(['Release',self.label(obj)]);machine.fixture_return(4,eax=0)
        def pointer(machine):
            obj=self.args(1)[0];self.events.append(['Pointer',self.label(obj)])
            machine.fixture_return(4,eax=self.buffer_data if obj==self.buffer else self.warning_data)
        def size(machine):
            obj=self.args(1)[0]
            if obj!=self.buffer:raise AssertionError('unexpected warning size query')
            self.events.append(['Size','code']);machine.fixture_return(4,eax=len(self.bytecode))
        method(vt,8,release);method(vt,12,pointer);method(vt,16,size)
        tv=allocate(40);self.table=allocate(4);p.put_uint(self.table,tv);self.names=[raw(name.encode()+b'\0') for name,_,_ in self.constants]
        method(tv,8,release)
        def desc(machine):
            obj,output=self.args(2)
            if obj!=self.table:raise AssertionError('table GetDesc self')
            machine.mu.mem_write(output,b'\0'*12);machine.put_uint(output+8,len(self.constants));self.events.append(['Desc',len(self.constants)]);machine.fixture_return(8,eax=0)
        def constant(machine):
            obj,parent,index=self.args(3)
            if obj!=self.table or parent or index>=len(self.constants):raise AssertionError('bounded table GetConstant')
            self.events.append(['Constant',index]);machine.fixture_return(12,eax=index+1)
        def constant_desc(machine):
            obj,handle,output,count=self.args(4);index=handle-1
            if obj!=self.table or not 0<=index<len(self.constants) or machine.uint(count)!=16:raise AssertionError('original requests up to16 constant descriptions')
            machine.mu.mem_write(output,b'\0'*48);machine.put_uint(output,self.names[index]);machine.put_uint(output+8,self.constants[index][1]);machine.put_uint(output+12,self.constants[index][2]);machine.put_uint(count,1)
            self.events.append(['ConstantDesc',index]);machine.fixture_return(16,eax=0)
        method(tv,20,desc);method(tv,24,constant_desc);method(tv,32,constant)
        p.seams[0x611241]=lambda machine:self.compile(machine,False)
        p.seams[0x611324]=lambda machine:self.compile(machine,True)
    def args(self,count):return [self.p.uint(self.p.reg('ESP')+4+4*i) for i in range(count)]
    def label(self,obj):
        if obj==self.buffer:return 'code'
        if obj==self.table:return 'table'
        if obj==self.warning:return 'warning'
        raise AssertionError('unknown SDK object')
    def compile(self,p,hlsl):
        args=self.args(10 if hlsl else 7);source,length,macros,include=args[:4]
        if length>0x8000 or macros or include:raise AssertionError('bounded source/no include-macro contract')
        text=bytes(p.mu.mem_read(source,length)).decode('latin1')
        if hlsl:
            entry,target,flags,code,errors,table=args[4:];p.put_uint(table,self.table)
            values=[read_cstring(p,entry).decode('latin1'),read_cstring(p,target).decode('latin1')]
        else:flags,code,errors=args[4:];values=[]
        if flags:raise AssertionError('original flags zero')
        self.calls.append(['hlsl' if hlsl else 'assembly',text,values,flags,list(bytes(p.mu.mem_read(0x73fe6b,1)))])
        p.put_uint(code,self.buffer);p.put_uint(errors,self.warning);p.fixture_return(len(args)*4,eax=self.hresult)
