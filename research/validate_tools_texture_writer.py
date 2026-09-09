#!/usr/bin/env python3
"""Exact source writer/ABI bytes from sealed fresh native dimension probes."""
from pathlib import Path
import ctypes as C
import hashlib,json,struct,sys
from pc_instruction_emulator import ROOT
from analyze_smo_texture_data import parse_sections

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def main(directory):
    folder=Path(directory).resolve();folder.relative_to(ROOT/'local-data/results')
    dllpath=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll';dll=C.CDLL(str(dllpath))
    dll.spv_last_error.restype=C.c_char_p
    dll.spv_texture_write_bgra.argtypes=[C.c_void_p]+[C.c_uint32]*5;dll.spv_texture_write_bgra.restype=C.c_void_p
    dll.spv_serialized_bytes_destroy.argtypes=[C.c_void_p]
    dll.spv_serialized_bytes_size.argtypes=[C.c_void_p,C.POINTER(C.c_uint32)]
    dll.spv_serialized_bytes_copy.argtypes=[C.c_void_p,C.c_void_p,C.c_uint32]
    def check(value):
        if not value:raise AssertionError(dll.spv_last_error().decode())
    def encode(pixels,width,height,flag,kind):
        data=C.create_string_buffer(pixels);handle=dll.spv_texture_write_bgra(data,len(pixels),width,height,flag,kind);check(handle)
        try:
            count=C.c_uint32();check(dll.spv_serialized_bytes_size(handle,C.byref(count)))
            output=C.create_string_buffer(count.value);check(dll.spv_serialized_bytes_copy(handle,output,count.value));return output.raw
        finally:dll.spv_serialized_bytes_destroy(handle)
    records=[]
    for mode in ('1x1','13x7','1x9-field2','3x2-field0'):
        path=folder/(mode+'.json');proof=json.loads(path.read_text());assert proof['status']=='passed'
        assert proof['pc_exe_sha256']==sha(ROOT/'local-data/pc-pristine/WinxClub.exe')
        for dependency in proof['dependencies']:assert sha(ROOT/dependency['path'])==dependency['sha256']
        original=bytes.fromhex(proof['output_hex']);pixels=bytes.fromhex(proof['pixels_hex'])
        native=next(f.payload for f in parse_sections(original) if f.section==1 and f.type==1)
        original_mip=parse_sections(native)[0].payload
        mip=encode(pixels,proof['width'],proof['height'],proof['field1c'],1);assert mip==original_mip
        whole=encode(pixels,proof['width'],proof['height'],proof['field1c'],0)
        # Original object-header operation and separately compared42E5F0 field3
        # frame around the complete original42BF40 output. No guessed mip bytes.
        expected=struct.pack('<II',0x78ea082b,0x4f4f4253)+b'\xe3'+struct.pack('<I',len(original))+original+b'\0'
        assert whole==expected
        records.append({'mode':mode,'native_evidence_sha256':sha(path),'raw_record_bytes':len(mip),'object_bytes':len(whole),
            'raw_record_sha256':hashlib.sha256(mip).hexdigest().upper(),'object_sha256':hashlib.sha256(whole).hexdigest().upper()})
    embedded=[]
    for mode in ('start','positioned'):
        path=folder/('embedded-'+mode+'.json');proof=json.loads(path.read_text());assert proof['status']=='passed'
        for dependency in proof['dependencies']:assert sha(ROOT/dependency['path'])==dependency['sha256']
        data=bytes.fromhex(proof['input_hex']);assert bytes.fromhex(proof['output_hex'])==b'\xe3'+struct.pack('<I',len(data))+data+b'\0'
        embedded.append({'path':path.name,'sha256':sha(path),'source_consumed':proof['native_source_deleted']})
    data=C.create_string_buffer(b'abcd')
    assert not dll.spv_texture_write_bgra(data,3,1,1,1,0)
    assert not dll.spv_texture_write_bgra(data,4,1,1,256,0)
    assert not dll.spv_texture_write_bgra(data,4,1,1,1,2)
    # A failure must not poison later independent host calls.
    assert encode(b'abcd',1,1,1,1).endswith(b'abcd')
    report={'status':'passed','native_dll_sha256':sha(dllpath),'validator_sha256':sha(__file__),
        'cases':records,'embedded_source_evidence':embedded,'invalid_inputs_rejected':3,
        'scope':'Exact original writer bytes for one supplied BGRA mip and object/source framing. No original vector-construction or runtime upload claim.'}
    (folder/'abi.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print('PASS exact original/tools texture writer: 4 dimensions/flags, 2 source cases, 3 input guards');return 0
if __name__=='__main__':raise SystemExit(main(*sys.argv[1:]))
