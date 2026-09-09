#!/usr/bin/env python3
"""Fresh original PC scalar Node reader vs the tool's common scalar field body."""
from pathlib import Path
import ctypes as C
import hashlib,json,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_node_serializer import main as original
from analyze_smo_texture_data import read_header

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
class Field(C.Structure):_fields_=[(n,C.c_uint32) for n in ('field','offset','size')]
class Values(C.Structure):_fields_=[('position',C.c_float*3),('rotation',C.c_float*4),('scale',C.c_float*3),
    ('orientation',C.c_float*9)]+[(n,C.c_uint32) for n in ('flags','billboard','bone','is_static','animated')]
def main(output):
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    path=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll';dll=C.CDLL(str(path));dll.spv_last_error.restype=C.c_char_p
    dll.spv_node_values.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(Field),C.c_uint32,C.POINTER(Values)]
    assert C.sizeof(Values)==96 and C.sizeof(Field)==12
    records=[]
    for mode in ('empty','transforms','false-flags','repeat-flags','unknown-repeat'):
        captured=original(mode,True);data=bytes.fromhex(captured[1]);state=bytes.fromhex(captured[4]);offset=0;fields=[]
        while offset<len(data):
            identity,size,header=read_header(data,offset)
            if size and identity<=8 and identity not in (5,7):fields.append(Field(identity,offset+header,size))
            offset+=header+size
        descriptors=(Field*len(fields))(*fields);buf=C.create_string_buffer(data);result=Values()
        ok=dll.spv_node_values(buf,len(data),descriptors,len(fields),C.byref(result))
        assert ok,dll.spv_last_error().decode()
        assert bytes(result.position)+bytes(result.scale)+bytes(result.orientation)==state[:60],mode
        assert result.flags==captured[3],(mode,hex(result.flags),hex(captured[3]))
        if mode=='transforms':assert tuple(result.rotation)==(0,0,.5,.5) and result.bone==0xa5 and result.animated==0
        if mode=='repeat-flags':assert not result.bone and not result.is_static and not result.animated and result.flags&0xc00==0xc00
        records.append({'mode':mode,'input_hex':data.hex(),'native_flags':captured[3],
            'local_float_state_hex':state[:60].hex(),'raw_rotation':list(result.rotation),
            'authored_flags':[result.bone,result.is_static,result.animated]})
    # Descriptor safety is the host boundary; these are not native malformed
    # field outcomes. The original full reader has separately bounded probes.
    rejected=[];buf=C.create_string_buffer(bytes(20));result=Values()
    for f in (Field(0,16,12),Field(0,0,4),Field(5,0,8)):
        ok=dll.spv_node_values(buf,20,C.byref(f),1,C.byref(result));assert not ok
        rejected.append(dll.spv_last_error().decode())
    report={'status':'passed','pc_exe_sha256':sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),
        'native_dll_sha256':sha(path),'cases':records,'host_rejections':rejected,
        'dependencies':sorted(({'path':str(Path(m.__file__).resolve().relative_to(ROOT)).replace('\\','/'),'sha256':sha(m.__file__)}
            for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda x:x['path']),
        'scope':'Original scalar Node reader local P/S/R float bits and effective flags. Tool descriptors select only known scalar fields; references, derived constructors and whole graph are not inferred from this isolated Node.'}
    target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS Node scalar ABI: 5 original cases, exact local float bits/effective flags, 3 host rejections');return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
