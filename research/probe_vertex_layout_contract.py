"""Original PC/PS2 vertex-layout calls; explicit object backing, no renderer/GPU."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
SEQUENCES=[[mask] for mask in (0,0x840,0x940,0x93e,0x2104,0x814,0x2000,0x1800,0x4800,0x19fe,0x1fffff)] + [
    [0x1fffff,0x940,0x2104,0], [0x93e,0x900,0x93e]]


def native(platform):
    if platform=='pc':
        from pc_instruction_emulator import PcInstructions
        machine=PcInstructions();address=machine.allocate(0x5c)
    else:
        from ps2_scalar_prefix import Ps2ScalarPrefix
        address=0x21000000
    rows=[];counts=[]
    for sequence in SEQUENCES:
        state=bytes(0x5c);result=[]
        for mask in sequence:
            incoming=bytearray(state);struct.pack_into('<I',incoming,0x14,mask)
            if platform=='pc':
                machine.mu.mem_write(address,bytes(incoming));machine.run(0x45fea0,this=address)
                state=bytes(machine.mu.mem_read(address,0x5c));counts.append(sum(machine.visits.values()))
            else:
                machine=Ps2ScalarPrefix([(0x15c8e0,0x250)])
                machine.map(address,4096);machine.write(address,incoming);machine.reg('A0',address)
                execution=machine.run(0x15c8e0,[machine.RETURN],count=2000,timeout_us=100000)
                if execution['stop']!=f'{machine.RETURN:08X}':raise AssertionError('Original PS2 call must return')
                state=machine.read(address,0x5c);counts.append(len(machine.trace))
            # Only the three proven layout regions can change.
            for i,(a,b) in enumerate(zip(incoming,state)):
                if a!=b and i not in (*range(0x10,0x12),*range(0x18,0x1a),*range(0x24,0x50)):
                    raise AssertionError('Unexpected object mutation')
            result.append([struct.unpack_from('<H',state,0x10)[0],struct.unpack_from('<H',state,0x18)[0],
                           *struct.unpack_from('<22H',state,0x24)])
        rows.append(result)
    return dict(platform=platform,sequences=SEQUENCES,layouts=rows,instructions=counts,
                calls=len(counts),scope='Original complete layout routine only; explicit zeroed initial backing and prior completed output on reinitialization')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform',choices=['pc','ps2'],required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--source',type=Path)
    parser.add_argument('--guest',action='store_true')
    args=parser.parse_args()
    if not args.guest:
        return subprocess.run([sys.executable,str(Path(__file__)),*sys.argv[1:],'--guest'],timeout=30).returncode
    if not args.output.resolve().is_relative_to(ROOT/'local-data'):raise ValueError('Local evidence output required')
    started=time.perf_counter();result=native(args.platform)
    if args.source:
        request=str(len(SEQUENCES))+'\n'+'\n'.join(str(len(s))+' '+' '.join(map(str,s)) for s in SEQUENCES)+'\n'
        done=subprocess.run([str(args.source),'--vertex-layouts'],input=request,capture_output=True,text=True,timeout=10,check=True)
        source=json.loads(done.stdout);result['sourceLayouts']=source
        result['sourceSha256']=hashlib.sha256(args.source.read_bytes()).hexdigest()
        result['mismatches']=[dict(sequence=s,call=i,mask=SEQUENCES[s][i],native=n,source=c)
            for s,(nr,cr) in enumerate(zip(result['layouts'],source)) for i,(n,c) in enumerate(zip(nr,cr)) if n!=c]
        if len(source)!=len(SEQUENCES) or any(len(a)!=len(b) for a,b in zip(source,SEQUENCES)):raise AssertionError('Incomplete source output')
    result['seconds']=round(time.perf_counter()-started,4)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
    print(json.dumps({k:result[k] for k in ('platform','calls','seconds')}|dict(mismatches=len(result.get('mismatches',[])))))
    return int(bool(result.get('mismatches')))


if __name__=='__main__':raise SystemExit(main())
