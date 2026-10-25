"""Own Microsoft SDK boundary for inspecting original PC shader resources.

No game math or source specialization lives here. Compiler identity/options
must be recorded: a later SDK is not automatically the game's compiler.
"""
from __future__ import annotations
import ctypes as C
from pathlib import Path


class D3dx:
    def __init__(self, dll_name='d3dx9_43.dll'):
        if Path(dll_name).name != dll_name or not dll_name.lower().startswith('d3dx9_'):
            raise ValueError('A system D3DX DLL basename is required')
        system = C.create_unicode_buffer(32768)
        kernel = C.WinDLL('kernel32', use_last_error=True)
        if not kernel.GetSystemDirectoryW(system, len(system)):
            raise C.WinError(C.get_last_error())
        self.path = Path(system.value) / dll_name
        self.dll = C.WinDLL(str(self.path))
        pointer = C.POINTER(C.c_void_p)
        self.dll.D3DXAssembleShader.argtypes = [C.c_void_p,C.c_uint,C.c_void_p,C.c_void_p,C.c_uint,pointer,pointer]
        self.dll.D3DXAssembleShader.restype = C.c_long
        self.dll.D3DXDisassembleShader.argtypes = [C.c_void_p,C.c_int,C.c_char_p,pointer]
        self.dll.D3DXDisassembleShader.restype = C.c_long
        self.dll.D3DXCompileShader.argtypes = [C.c_void_p,C.c_uint,C.c_void_p,C.c_void_p,C.c_char_p,C.c_char_p,C.c_uint,pointer,pointer,pointer]
        self.dll.D3DXCompileShader.restype = C.c_long
        self.dll.D3DXGetShaderConstantTable.argtypes = [C.c_void_p,pointer]
        self.dll.D3DXGetShaderConstantTable.restype = C.c_long

    @staticmethod
    def method(pointer, slot, result, *arguments):
        table = C.cast(pointer,C.POINTER(C.POINTER(C.c_void_p))).contents
        return C.WINFUNCTYPE(result,C.c_void_p,*arguments)(table[slot])

    @classmethod
    def release(cls, pointer):
        if pointer.value:
            cls.method(pointer,2,C.c_ulong)(pointer)

    @classmethod
    def consume(cls, pointer):
        if not pointer.value:
            return b''
        try:
            size=cls.method(pointer,4,C.c_uint)(pointer)
            if size>16*1024*1024:
                raise ValueError('SDK result exceeds research bound')
            return C.string_at(cls.method(pointer,3,C.c_void_p)(pointer),size)
        finally:
            cls.release(pointer)

    def assemble(self, source):
        code,errors=C.c_void_p(),C.c_void_p()
        hr=self.dll.D3DXAssembleShader(C.create_string_buffer(source),len(source),None,None,0,C.byref(code),C.byref(errors))
        return hr,self.consume(code),self.consume(errors).decode('utf-8',errors='replace').rstrip('\0')

    def disassemble(self, code):
        result=C.c_void_p()
        hr=self.dll.D3DXDisassembleShader(C.create_string_buffer(code),0,None,C.byref(result))
        text=self.consume(result).decode('utf-8',errors='replace').rstrip('\0')
        if hr<0:
            raise RuntimeError(f'Disassembly failed: {hr}')
        normalized='\n'.join(s for line in text.splitlines() if (s:=line.split('//',1)[0].strip()))
        return text,normalized

    @classmethod
    def reflection(cls, table):
        class Description(C.Structure):
            _fields_=[('creator',C.c_char_p),('version',C.c_uint),('constants',C.c_uint)]
        class Constant(C.Structure):
            _fields_=[('name',C.c_char_p),('registerSet',C.c_int),('index',C.c_uint),('count',C.c_uint),
                      ('parameterClass',C.c_int),('parameterType',C.c_int),('rows',C.c_uint),('columns',C.c_uint),
                      ('elements',C.c_uint),('structMembers',C.c_uint),('bytes',C.c_uint),('defaultValue',C.c_void_p)]
        if not table.value:
            return None
        description=Description()
        if cls.method(table,5,C.c_long,C.POINTER(Description))(table,C.byref(description))<0 or description.constants>256:
            raise ValueError('Invalid/bounded SDK reflection description')
        constants=[]
        for i in range(description.constants):
            handle=cls.method(table,8,C.c_void_p,C.c_void_p,C.c_uint)(table,None,i)
            row=Constant();count=C.c_uint(1)
            if not handle or cls.method(table,6,C.c_long,C.c_void_p,C.POINTER(Constant),C.POINTER(C.c_uint))(table,handle,C.byref(row),C.byref(count))<0 or count.value!=1:
                raise ValueError('Invalid SDK constant reflection')
            record={name:int(getattr(row,name)) for name,_ in Constant._fields_ if name not in ('name','defaultValue')}
            record['name']=row.name.decode('ascii');constants.append(record)
        return {'creator':description.creator.decode('ascii') if description.creator else None,
                'version':description.version,'constants':constants}

    def compile(self, source, entry, target, flags=0):
        if len(source)>65536:
            raise ValueError('Shader source exceeds research bound')
        code,errors,table=C.c_void_p(),C.c_void_p(),C.c_void_p()
        hr=self.dll.D3DXCompileShader(C.create_string_buffer(source),len(source),None,None,
            entry.encode('ascii'),target.encode('ascii'),flags,C.byref(code),C.byref(errors),C.byref(table))
        bytecode=self.consume(code)
        messages=self.consume(errors).decode('utf-8',errors='replace').rstrip('\0')
        try:
            return hr,bytecode,messages,self.reflection(table)
        finally:
            self.release(table)

    def reflect(self, code):
        table=C.c_void_p()
        hr=self.dll.D3DXGetShaderConstantTable(C.create_string_buffer(code),C.byref(table))
        try:
            return self.reflection(table) if hr>=0 else None
        finally:
            self.release(table)
