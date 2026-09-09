#!/usr/bin/env python3
"""Reuse sealed original material captures for shared inspector consumers."""
from pathlib import Path
import ctypes as C,json,struct
import validate_tools_material_reader as runner
ROOT=Path(__file__).resolve().parents[1]
def main():
    old=runner.OUT;runner.OUT=ROOT/'local-data/results/tools-core-cycle-20260909-1900/material-inspection'
    runner.OUT.mkdir(parents=True,exist_ok=True)
    (runner.OUT/'original.json').write_bytes((old/'original.json').read_bytes());runner.compare()
    report=json.loads((runner.OUT/'original.json').read_text(encoding='utf-8'))
    dll=C.CDLL(str(ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll'))
    dll.spv_material_read.argtypes=[C.c_void_p,C.c_uint32];dll.spv_material_read.restype=C.c_void_p
    dll.spv_material_destroy.argtypes=[C.c_void_p];dll.spv_material_info.argtypes=[C.c_void_p,C.POINTER(runner.Info)]
    class Pass(C.Structure):_fields_=[('blend',C.c_uint32),('layers',C.c_uint32)]
    dll.spv_material_passes.argtypes=[C.c_void_p,C.POINTER(Pass),C.c_uint32]
    passes=[]
    # A valid actual field3 can create a pass with no layer. This accessor must
    # expose its state independently of the old one-layer CSharp DTO.
    for blend in (0,2,6):
        raw=C.create_string_buffer(b'\xa3\x04'+struct.pack('<I',blend)+b'\0')
        handle=dll.spv_material_read(raw,7);assert handle
        try:
            info=runner.Info();assert dll.spv_material_info(handle,C.byref(info))
            values=(Pass*info.passes)();assert dll.spv_material_passes(handle,values,info.passes)
            assert info.passes==1 and info.layers==0 and values[0].blend==blend and values[0].layers==0
            passes.append({'blend':blend,'empty_pass_preserved':True})
        finally:dll.spv_material_destroy(handle)
    result=json.loads((runner.OUT/'comparison.json').read_text(encoding='utf-8'))
    result['pass_accessor_cases']=passes
    result['scope']='Reuse nine sealed original captures; shared reader state plus three source pass-accessor checks. No new original runtime or factory claims.'
    (runner.OUT/'comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('PASS shared material pass accessor:',len(passes),'empty passes')
if __name__=='__main__':main()
