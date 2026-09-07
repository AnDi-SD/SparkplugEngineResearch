#!/usr/bin/env python3
"""Compare original generated mip bytes and base state to the compiled reader."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_texture_missing_mips import main as native,specimen


def main(pattern='corpus',shape='16x16'):
    dimensions=tuple(map(int,shape.split('x')))
    assert len(dimensions)==2 and all(n in (1,2,4,8,16) for n in dimensions)
    data,_=specimen(pattern,dimensions)
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugTextureSerializationTests.exe'
    result=subprocess.run([str(binary),'--missing-native'],input=data.hex()+'\n',text=True,capture_output=True,timeout=10)
    assert result.returncode==0,result.stderr
    source=json.loads(result.stdout)
    observed=native(f'compare-{pattern}-{shape}',pattern,shape,return_capture=True)
    state=observed['state']
    expected={'state':[state[0],state[1]&255,state[2],state[3]&255,state[4],state[5]],'levels':observed['levels']}
    assert source==expected,(pattern,shape,source,expected)
    report={'status':'passed','pattern':pattern,'dimensions':dimensions,'payloadSha256':observed['payloadSha256'],
            'sourceExecutableSha256':hashlib.sha256(binary.read_bytes()).hexdigest().upper(),
            'exactMipBytes':sum(len(row['packedHex'])//2 for row in observed['levels']),
            'generatedMipBytes':sum(len(row['packedHex'])//2 for row in observed['levels'][1:]),
            'original':{k:v for k,v in observed.items() if k not in ('levels','events','coefficientTables')}}
    path=ROOT/f'local-data/results/cycle-20260908-0700/cp108-comparison-{pattern}-{shape}.json'
    path.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS',pattern,shape,report['exactMipBytes'],'exact mip bytes')
    return 0


if __name__=='__main__':
    raise SystemExit(main(*sys.argv[2:]) if sys.argv[1:2]==['--guest'] else run_bounded(Path(__file__),sys.argv[1:]))
