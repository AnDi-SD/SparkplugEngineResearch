#!/usr/bin/env python3
"""Compare shared header/bounds readers with the saved, matching original observations."""
from pathlib import Path
import argparse,ctypes as C,hashlib,json
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def main(directory):
    directory=directory.resolve();directory.relative_to(ROOT/'local-data/results')
    dllpath=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll';dll=C.CDLL(str(dllpath))
    dll.spv_ps2_mesh_header.argtypes=[C.c_void_p,C.c_uint32,C.c_void_p]
    dll.spv_mesh_bounds.argtypes=[C.c_void_p,C.c_uint32,C.c_void_p]
    rows=[]
    for mode in ('header','raw-counters','bounds'):
        path=directory/(mode+'.json');original=json.loads(path.read_text(encoding='utf-8'))
        assert original['status']=='passed' and original['probe_sha256']==sha(ROOT/'research/probe_pc_ps2_mesh_header.py')
        assert original['pc_sha256']==sha(ROOT/'local-data/pc-pristine/WinxClub.exe')
        expected=bytes.fromhex(original['observed_hex'])
        # The original stopped before packet allocation. Only its first40 bytes
        # are compared; the source additionally checks a synthetic opaque extent.
        input_bytes=expected+(bytes(16) if mode=='header' else b'')
        function=dll.spv_mesh_bounds if mode=='bounds' else dll.spv_ps2_mesh_header
        source=C.create_string_buffer(input_bytes);result=C.create_string_buffer(len(expected))
        assert function(source,len(input_bytes),result)==1 and result.raw==expected,mode
        rows.append({'mode':mode,'bytes':len(expected),'original_report_sha256':sha(path)})
    report={'status':'passed','validator_sha256':sha(__file__),'native_dll_sha256':sha(dllpath),'cases':rows,
        'scope':'Exact header/bounds bytes against unchanged original PC observations. Extra opaque packet extent is synthetic; no packet runtime or PS2 execution claim.'}
    (directory/'abi.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS 3 original/source metadata snapshots')
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('directory',type=Path)
    main(parser.parse_args().directory)
