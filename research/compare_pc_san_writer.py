#!/usr/bin/env python3
"""Actual PC field writer versus compiled writer, one bounded asset per call."""
from pathlib import Path
import hashlib
import subprocess
import sys
from inspect_pc_san_keys import DEFAULT

ROOT=Path(__file__).resolve().parents[1]


def output(command):
    result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=30)
    if result.returncode:raise AssertionError(result.stdout+'\n'+result.stderr)
    lines=[line for line in result.stdout.splitlines() if line.startswith('SAN_OUTPUT_HEX ')]
    if len(lines)!=1:raise AssertionError('missing/duplicate output record')
    return bytes.fromhex(lines[0].split(' ',1)[1])


def main(name):
    if name not in {'bbush.san','bflower.san','barrel.san','bw.san'}:raise ValueError('bounded PC SAN specimen only')
    native=output([sys.executable,str(ROOT/'research/probe_pc_san_writer.py'),name,'emit'])
    portable=output([str(ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugSanReaderTests.exe'),
                     '--rewrite-fields',str(DEFAULT/name)])
    if native!=portable:
        differences=[i for i,(a,b) in enumerate(zip(native,portable)) if a!=b]
        raise AssertionError(f'writer mismatch: native{len(native)} portable{len(portable)}; first{differences[:20]}')
    print(f'PASS {name}: {len(native)} exact native/portable field bytes; SHA256 {hashlib.sha256(native).hexdigest().upper()}')
    return 0


if __name__=='__main__':raise SystemExit(main(*sys.argv[1:]))
