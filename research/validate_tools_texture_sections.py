#!/usr/bin/env python3
"""Fresh original texture read/copy vs shared tools section reader on its bytes."""
from pathlib import Path
import ctypes as C
import hashlib,json,sys
from pc_instruction_emulator import ROOT,run_bounded
from analyze_smo_texture_data import parse_sections
from probe_pc_texture_native_mip import main as native_mip
from probe_pc_texture_cross import main as native_cross

class Info(C.Structure):
    _fields_=[(x,C.c_uint32) for x in ('kind','width','height','format','auxiliary','bits','present','mips')]
class Mip(C.Structure):
    _fields_=[(x,C.c_uint32) for x in ('width','height','descriptor0','descriptor1','descriptor2','offset','size')]
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()

def main(mode,output):
    if mode not in ('cross-rgba','cross-gray','cross-rgb16','raw-4','dxt1-4','dxt3-4','dxt5-4'):
        raise ValueError('Explicit tiny specimen required')
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    cross=mode.startswith('cross-');capture=native_cross(mode[6:],True) if cross else native_mip(mode,True)
    whole=bytes.fromhex(capture[1]);field_type=0 if cross else 1
    leaf=next(field.payload for field in parse_sections(whole) if field.section==1 and field.type==field_type)
    dllpath=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll';dll=C.CDLL(str(dllpath))
    dll.spv_last_error.restype=C.c_char_p
    dll.spv_texture_section_read.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32];dll.spv_texture_section_read.restype=C.c_void_p
    dll.spv_texture_section_destroy.argtypes=[C.c_void_p]
    dll.spv_texture_section_info.argtypes=[C.c_void_p,C.POINTER(Info)]
    dll.spv_texture_section_mips.argtypes=[C.c_void_p,C.POINTER(Mip),C.c_uint32]
    assert dll.spv_abi_version()==2 and C.sizeof(Info)==32 and C.sizeof(Mip)==28
    def check(value):
        if not value:raise AssertionError(dll.spv_last_error().decode())
    data=C.create_string_buffer(leaf);handle=dll.spv_texture_section_read(data,len(leaf),field_type);check(handle)
    try:
        info=Info();check(dll.spv_texture_section_info(handle,C.byref(info)))
        mips=(Mip*info.mips)();check(dll.spv_texture_section_mips(handle,mips,info.mips))
        pixels=[leaf[m.offset:m.offset+m.size] for m in mips]
        if cross:
            assert (info.width,info.height,info.format,info.bits)==(capture[2][0],capture[2][1],capture[2][3],capture[2][4]*8)
            assert [v.hex() for v in pixels]==[capture[3]] and info.mips==1
        else:
            assert (info.width,info.height,info.format,info.present,info.mips)==(capture[3][4],capture[3][5],capture[3][2],capture[3][1],capture[3][0])
            for mip,actual,expected in zip(mips,capture[2],pixels,strict=True):
                captured=bytes.fromhex(actual);stride=mip.descriptor1;rows=mip.descriptor2
                assert len(captured)==(stride+4)*rows
                packed=b''.join(captured[r*(stride+4):r*(stride+4)+stride] for r in range(rows))
                assert packed==expected and mip.size==stride*rows
        report={'status':'passed','mode':mode,'native_capture':capture,'input_sha256':hashlib.sha256(whole).hexdigest().upper(),
            'native_dll_sha256':sha(dllpath),'info':{k:getattr(info,k) for k,_ in Info._fields_},
            'mips':[{k:getattr(m,k) for k,_ in Mip._fields_} for m in mips],
            'dependencies':sorted(({'path':str(Path(m.__file__).resolve().relative_to(ROOT)).replace('\\','/'),'sha256':sha(m.__file__)}
                for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda x:x['path']),
            'scope':'Original PC whole CPU/native reader, actual pixels, balanced teardown. Tools ABI reads identical explicit representation through the shared engine reader. No GPU or whole-SMO claim.'}
    finally:dll.spv_texture_section_destroy(handle)
    target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS original/tools texture section',mode,flush=True);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
