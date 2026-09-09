#!/usr/bin/env python3
"""Fresh original PC header writer vs tools ABI; unsupported native quirks stay explicit."""
from pathlib import Path
import ctypes as C
import hashlib,json,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_block_writer import BlockFixture

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def main(output):
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    dllpath=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll';dll=C.CDLL(str(dllpath));dll.spv_last_error.restype=C.c_char_p
    dll.spv_write_field_header.argtypes=[C.c_uint32]*4+[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32)]
    def encode(identity,size,code=None,extended=False):
        data=C.create_string_buffer(6);count=C.c_uint32()
        ok=dll.spv_write_field_header(identity,size,0xffffffff if code is None else code,int(extended),data,6,C.byref(count))
        return ok,data.raw[:count.value],None if ok else dll.spv_last_error().decode()
    cases=[(0,1,None),(3,2,None),(7,4,None),(30,8,None),(32,3,None),(200,255,None),(255,256,None),
        (42,65535,None),(8,65536,None),(42,0xffffffff,None),(8,1,5),(8,255,6),(200,65536,7),(5,4,3)]
    f=BlockFixture();records=[]
    for identity,size,code in cases:
        start=len(f.data)
        if code is not None:f.p.put_uint(f.block+0x24,code)
        result=f.call(0x4727b0,this=f.block,args=(identity,size,int(code is not None)))&255
        instructions=sum(f.p.visits.values());actual=f.data[start:]
        ok,encoded,error=encode(identity,size,code)
        assert result==1 and ok and encoded==actual,(identity,size,code,actual.hex(),encoded.hex(),error)
        records.append({'field':identity,'size':size,'preferred_code':code,'header_hex':actual.hex(),'native_instructions':instructions})
    quirks=[]
    for identity,size in ((31,3),(5,0)):
        start=len(f.data);result=f.call(0x4727b0,this=f.block,args=(identity,size,0))&255
        assert result==1
        quirks.append({'field':identity,'size':size,'native_header_hex':f.data[start:].hex()})
    assert quirks[0]['native_header_hex']=='bf03' and quirks[1]['native_header_hex']==''
    start=len(f.data);assert f.call(0x472b00,this=f.block,args=(0,))&255
    assert encode(0,0)[1]==f.data[start:]==b'\0'
    rejected=[]
    for args in ((31,3,None,False),(5,0,5,False),(7,4,3,True)):
        ok,_,error=encode(*args);assert not ok;rejected.append({'arguments':args,'error':error})
    # Resize fallback uses the original automatic selector if the old capacity
    # is too small. A mismatched forced-escape preference is then discarded.
    assert encode(7,256,5,True)[1]==encode(7,256)[1]
    assert not f.errors;f.cleanup()
    report={'status':'passed','native_dll_sha256':sha(dllpath),'pc_exe_sha256':sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),
        'cases':records,'native_quirks':quirks,'rejected_cases':rejected,'terminator_hex':'00',
        'dependencies':sorted(({'path':str(Path(m.__file__).resolve().relative_to(ROOT)).replace('\\','/'),'sha256':sha(m.__file__)}
            for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda x:x['path']),
        'scope':'Original header/terminator functions with explicit writer constructor/stream fixture. No payload allocation for large sizes. Fixture list cleanup is not a native destructor claim. ID31, real empty fields and forced escapes are paused editor adaptations.'}
    target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS original/tools headers: 14 exact cases, original terminator, 2 native quirks, 3 explicit rejections');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
