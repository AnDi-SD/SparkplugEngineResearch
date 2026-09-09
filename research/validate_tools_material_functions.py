#!/usr/bin/env python3
"""Original read-time scalar bits vs the shared tool field ABI."""
from pathlib import Path
import ctypes as C,json,subprocess,sys
from validate_tools_material_reader import dependencies,sha
from probe_pc_tool_material_functions import MODES
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'local-data/results/tools-core-cycle-20260909-1900/material-functions'
def original():
    OUT.mkdir(parents=True,exist_ok=True);probe=ROOT/'research/probe_pc_tool_material_functions.py';cases=[]
    for mode in MODES:
        result=subprocess.run([sys.executable,str(probe),'--guest',mode],cwd=ROOT,capture_output=True,text=True,timeout=30)
        log=OUT/(mode+'-original.log');log.write_text(result.stdout+result.stderr,encoding='utf-8')
        if result.returncode:raise RuntimeError('Fresh guest failed; no retry: '+str(log))
        marker='TOOL_FUNCTION_CAPTURE ';capture=json.loads(next(line[len(marker):] for line in result.stdout.splitlines() if line.startswith(marker)))
        cases.append({'capture':capture,'log':log.relative_to(ROOT).as_posix(),'sha256':sha(log)});print('PASS original',mode,flush=True)
    report={'status':'passed','cases':cases,'pc_exe_sha256':sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),
        'dependencies':dependencies([Path(__file__),probe]),'limits':'Fresh micro64KiB,100k instructions/2s per call,30s process; capped color factory excluded'}
    (OUT/'original.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def compare():
    report=json.loads((OUT/'original.json').read_text(encoding='utf-8'));assert report['status']=='passed'
    assert report['pc_exe_sha256']==sha(ROOT/'local-data/pc-pristine/WinxClub.exe')
    for entry in report['dependencies']:assert sha(ROOT/entry['path'])==entry['sha256'],entry['path']
    path=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll';dll=C.CDLL(str(path));dll.spv_last_error.restype=C.c_char_p
    for name in ('spv_uv_functions_read','spv_color_functions_read'):getattr(dll,name).argtypes=[C.c_void_p,C.c_uint32,C.c_void_p]
    comparisons=[]
    for entry in report['cases']:
        assert sha(ROOT/entry['log'])==entry['sha256'];capture=entry['capture'];data=bytes.fromhex(capture['body'])
        raw=C.create_string_buffer(data);state=C.create_string_buffer(192 if capture['mode'].startswith('uv-') else 152)
        read=dll.spv_uv_functions_read if capture['mode'].startswith('uv-') else dll.spv_color_functions_read
        assert read(raw,len(data),state),dll.spv_last_error()
        assert state.raw.hex()==capture['state'],(capture['mode'],state.raw.hex(),capture['state'])
        comparisons.append({'mode':capture['mode'],'original_scalar_bytes_exact':True,'bytes':len(state.raw)})
    guards=0
    for name,data in (('uv',b''),('uv',b'\xa0\x01\0\0'),('color',b'\0'*4),('color',b'\0'*6)):
        raw=C.create_string_buffer(data);state=C.create_string_buffer(192)
        read=dll.spv_uv_functions_read if name=='uv' else dll.spv_color_functions_read
        assert not read(raw,len(data),state);guards+=1
    result={'status':'passed','native_dll_sha256':sha(path),'original_report_sha256':sha(OUT/'original.json'),
        'cases':comparisons,'host_refusals':guards,'scope':'Actual nested UV/color reader state, no evaluation, material binding or protected color factory claim'}
    (OUT/'comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('PASS shared material function readers:',len(comparisons),'original states;',guards,'bounded refusals')
if __name__=='__main__':
    if sys.argv[1:]==['original']:original()
    elif sys.argv[1:]==['compare']:compare()
    else:raise SystemExit('original | compare')
