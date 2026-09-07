#!/usr/bin/env python3
"""Exact binary32 comparison for the three original DXT block decoders."""
from pathlib import Path
import hashlib
import json
import random
import struct
import subprocess
import sys
import time
from pc_instruction_emulator import PcInstructions,ROOT,run_bounded


def cases():
    for flags in (1,2,3):
        for first,second in ((0,0xffff),(0xffff,0),(0,0),(0xffff,0xffff),(0xf800,0x1f),(0x7bef,0x8410)):
            colors=struct.pack('<HHI',first,second,0xe4e4e4e4)
            for alpha in (b'\0'*8,b'\xff'*8,bytes.fromhex('ff00fac688fac688'),bytes.fromhex('00ff88c6fa88c6fa')) if flags>1 else (b'',):
                yield flags,alpha+colors
    rng=random.Random(0x64c493)
    for flags in (1,2,3):
        for _ in range(128):yield flags,bytes(rng.randrange(256) for _ in range(8 if flags==1 else 16))


def main():
    started=time.monotonic();batch=list(cases())
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugTextureSerializationTests.exe'
    source=subprocess.run([str(binary),'--decode-blocks'],input=''.join(f'{flags} {data.hex()}\n' for flags,data in batch),
                          text=True,capture_output=True,timeout=10)
    assert source.returncode==0,source.stderr
    expected=[json.loads(line) for line in source.stdout.splitlines()];assert len(expected)==len(batch)
    p=PcInstructions();packed=p.allocate(16);output=p.allocate(256+16);instructions=0;results=[]
    for index,((flags,data),want) in enumerate(zip(batch,expected)):
        p.mu.mem_write(packed,data);p.mu.mem_write(output,b'\xa5'*272)
        p.run({1:0x64c493,2:0x64c5d5,3:0x64c65a}[flags],args=(output,packed))
        observed=list(struct.unpack('<64I',p.mu.mem_read(output,256)))
        assert p.reg('EAX')==0 and bytes(p.mu.mem_read(output+256,16))==b'\xa5'*16
        assert observed==want,(index,flags,data.hex(),[(i,a,b) for i,(a,b) in enumerate(zip(observed,want)) if a!=b])
        instructions+=sum(p.visits.values());results.append({'flags':flags,'packedHex':data.hex(),'words':observed})
    report={'kind':'original-source-texture-block-decode','status':'passed','cases':len(batch),'exactFloatWords':len(batch)*64,
            'nativeInstructions':instructions,'executionProfile':'micro','seams':len(p.seams),'arenaReservedBytes':p.allocated,
            'sourceExecutableSha256':hashlib.sha256(binary.read_bytes()).hexdigest().upper(),'seconds':time.monotonic()-started,'results':results}
    path=ROOT/'local-data/results/cycle-20260908-0700/cp111-block-decode-comparison.json';path.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='results'},sort_keys=True));return 0


if __name__=='__main__':
    raise SystemExit(main() if sys.argv[1:2]==['--guest'] else run_bounded(Path(__file__),sys.argv[1:]))
