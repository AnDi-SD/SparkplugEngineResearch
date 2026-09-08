#!/usr/bin/env python3
"""Compare common pixel input through original and reconstructed DX upload."""
from pathlib import Path
import hashlib,json,subprocess,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_texture_cross_upload import specimen,main as original


def main(fmt=0,shape='2x2',pattern='random',label='first',reader='dx'):
    data=specimen(fmt,tuple(map(int,shape.split('x'))),pattern)
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugTextureSerializationTests.exe'
    assert reader in ('dx','common')
    suffix='-common' if reader=='common' else ''
    at=time.monotonic();result=subprocess.run([str(binary),'--cross-upload'+suffix],input=data.hex(),capture_output=True,text=True,timeout=10)
    if result.returncode:raise AssertionError('source cross upload: '+result.stderr[-1500:])
    source=json.loads(result.stdout);native=original(fmt,shape,pattern,'compare-'+label,reader,return_capture=True)
    observed={'state':native['state'],'mips':native['mips']}
    expected={'state':source['state'],'mips':[row['packedHex'] for row in source['levels']]}
    report=dict(kind='native-source-cross-texture-upload',format=fmt,shape=shape,pattern=pattern,reader=reader,
        status='passed' if observed==expected else 'failed',inputSha256=hashlib.sha256(data).hexdigest().upper(),
        sourceExecutableSha256=hashlib.sha256(binary.read_bytes()).hexdigest().upper(),
        nativeInstructions=native['instructions'],releasedAllocations=native['releasedAllocations'],
        arenaReservedBytes=native['arenaReservedBytes'],exactMipBytes=sum(len(mip)//2 for mip in native['mips']),
        observed=observed,expected=expected,elapsedSeconds=time.monotonic()-at)
    (ROOT/f'local-data/results/cycle-20260908-0700/cp115-cross-comparison-{fmt}-{shape}-{pattern}-{label}{suffix}.json').write_text(json.dumps(report,indent=2)+'\n')
    if observed!=expected:raise AssertionError('exact cross state/mip mismatch; preserved JSON')
    print('PASS',json.dumps({k:v for k,v in report.items() if k not in ('observed','expected')}),flush=True);return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(int(sys.argv[2]),*sys.argv[3:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
