#!/usr/bin/env python3
"""Exact source/metadata/code semantics; native SDK object lifetime checked too."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_shader_compile import main as original,CASES

def main(mode):
    case=CASES[mode];lines=[' '.join(case[k].encode().hex() or '-' for k in ('code','decl','header','insert'))+f" {int(case['hlsl'])} {case.get('hresult',0)}"]
    for parameters in (case.get('parameters',[]),case.get('constants',[('MatDiffuse',4,1),('view_proj_matrix',8,4)])):
        lines.append(str(len(parameters)));lines.extend(f'{name.encode().hex() or "-"} {reg} {count}' for name,reg,count in parameters)
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugShaderCompileTests.exe'
    capture=original(mode,True);native=[capture[i] for i in (0,1,2,4,5,6,7)]
    source=json.loads(subprocess.run([str(binary),'--input',mode],input='\n'.join(lines)+'\n',capture_output=True,text=True,timeout=10,check=True).stdout)
    if source!=native:raise AssertionError(f'{mode}: source={source!r}; native={native!r}')
    print('PASS exact PC shader compiler consumer',mode);return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
