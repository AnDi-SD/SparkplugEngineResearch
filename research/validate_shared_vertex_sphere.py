#!/usr/bin/env python3
"""Seven tiny original-PC cases check the shared extracted sphere producer."""
from pathlib import Path
import hashlib,json,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def main(output):
    output=Path(output).resolve();output.relative_to(ROOT/'local-data/results')
    binary=ROOT/'artifacts/native/viewer/Release/ViewerMeshBVCoreChecks.exe'
    original=ROOT/'research/probe_pc_mesh_bounds.py';rows=[]
    for mode in ('triangle','five','seven','u32','rounding','zero-primitives','point'):
        started=time.perf_counter()
        native=subprocess.run([sys.executable,str(original),mode],capture_output=True,text=True,timeout=35)
        if native.returncode:raise AssertionError(native.stdout+native.stderr)
        line=next(line for line in native.stdout.splitlines() if line.startswith('PC_MESH_BOUNDS_CAPTURE '))
        ib,vb,capture=json.loads(line.removeprefix('PC_MESH_BOUNDS_CAPTURE '))
        result=subprocess.run([str(binary),'--vertex-sphere'],input=vb+'\n',capture_output=True,text=True,timeout=10)
        if result.returncode:raise AssertionError(result.stdout+result.stderr)
        sphere=json.loads(result.stdout);assert sphere==capture[1],(mode,sphere,capture[1])
        rows.append(dict(mode=mode,index_input_sha256=hashlib.sha256(bytes.fromhex(ib)).hexdigest(),
            vertex_input_sha256=hashlib.sha256(bytes.fromhex(vb)).hexdigest(),sphere_bits=sphere,seconds=time.perf_counter()-started))
        print('PASS original/shared vertex sphere',mode,flush=True)
    report=dict(status='passed',scope='Actual pristine-PC mesh bounds producer vs extracted shared sphere helper; exact float bits, no GPU/collision query claim.',
        executable_sha256=sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),original_probe_sha256=sha(original),
        validator_sha256=sha(__file__),source_binary_sha256=sha(binary),cases=rows)
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main(*sys.argv[1:])
