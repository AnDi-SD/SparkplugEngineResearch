#!/usr/bin/env python3
"""Original native DXData copied rows/base state vs shared compiled reader.

Strip only declared4-byte per-row COM fixture padding from captured ORIGINAL
surface memory; compare actual packed data, not invented host COM event traces.
"""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_texture_native_mip import main as native
def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugTextureSerializationTests.exe'
    result=subprocess.run([str(binary),'--native',mode],capture_output=True,text=True,timeout=10,check=True)
    source=json.loads(result.stdout);original=native(mode,True)
    parts=mode.split('-');dimension=int(parts[1]) if len(parts)>1 else 1
    row_size={'raw':4,'dxt1':8,'dxt3':16,'dxt5':16}[parts[0]];compressed=parts[0]!='raw'
    actual=[original[2]] if isinstance(original[2],str) else original[2];packed=[]
    for level in actual:
        stride=max(1,dimension>>2)*row_size if compressed else dimension*row_size
        rows=max(1,dimension>>2) if compressed else dimension;captured=bytes.fromhex(level);pitch=stride+4
        packed.append(b''.join(captured[row*pitch:row*pitch+stride] for row in range(rows)).hex());dimension=max(1,dimension>>1)
    expected=[original[0],original[1],packed,original[3]]
    if source!=expected:raise AssertionError(f'{mode}: source={source!r};original={expected!r}')
    print(f'PASS exact PC native mip {mode}: source/wire/actual copied packed pixels/base state')
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
