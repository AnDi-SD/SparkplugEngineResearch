#!/usr/bin/env python3
"""Compare external-source path, owned stream lifetime and exact texture state."""
from pathlib import Path
import hashlib,json,subprocess,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_texture_external_source import specimen,main as original


def main(kind='rgba',path_case='drive',reader='dx',label='first'):
    outer,data,parent,reference,resolved=specimen(kind,path_case)
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugTextureSerializationTests.exe'
    option='--external-source-common' if reader=='common' else '--external-source'
    at=time.monotonic();result=subprocess.run([str(binary),option,parent.decode('ascii'),resolved.decode('ascii')],
        input=outer.hex()+'\n'+data.hex()+'\n',capture_output=True,text=True,timeout=10)
    if result.returncode:raise AssertionError('source external read: '+result.stderr[-1500:])
    source=json.loads(result.stdout);native=original(kind,path_case,reader,'compare-'+label,return_capture=True)
    expected={key:source[key] for key in ('state','mips')};observed={key:native[key] for key in ('state','mips')}
    equal=expected==observed and source['resolvedHex']==resolved.hex() and source['closeCalls']==source['destroyCalls']==1
    report=dict(kind='native-source-external-texture-comparison',textureKind=kind,pathCase=path_case,reader=reader,
        status='passed' if equal else 'failed',inputSha256=native['inputSha256'],externalSha256=native['externalSha256'],
        sourceExecutableSha256=hashlib.sha256(binary.read_bytes()).hexdigest().upper(),nativeInstructions=native['instructions'],
        releasedAllocations=native['releasedAllocations'],fixtureFreedFilenameBytes=native['fixtureFreedFilenameBytes'],
        arenaReservedBytes=native['arenaReservedBytes'],exactMipBytes=sum(len(row)//2 for row in native['mips']),
        observed=observed,expected=expected,resolved=native['resolved'],elapsedSeconds=time.monotonic()-at)
    (ROOT/f'local-data/results/cycle-20260908-0700/cp118-external-comparison-{kind}-{path_case}-{reader}-{label}.json').write_text(json.dumps(report,indent=2)+'\n')
    assert equal,'external state/pixels/path/lifetime mismatch; preserved report'
    print('PASS',json.dumps({k:v for k,v in report.items() if k not in ('observed','expected')}),flush=True);return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
