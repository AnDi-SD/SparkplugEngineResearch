#!/usr/bin/env python3
"""Compare original no-dither block encoders to reconstructed C++ output."""
from pathlib import Path
import hashlib,json,random,struct,subprocess,sys,time
from pc_instruction_emulator import PcInstructions,ROOT,run_bounded


def words(values):return list(struct.unpack('<'+'I'*len(values),struct.pack('<'+'f'*len(values),*values)))
def cases(count=8,seed=0):
    for color in ((0,0,0,0),(0,0,0,1),(1,1,1,1),(1,0,0,1),(0,0,1,1),(.7,.2,.6,.25),(.5,.5,.5,.5)):
        yield [color]*16
    yield [(i/15,1-i/15,(i%4)/3,(i%3)/2) for i in range(16)]
    yield [(i/15,(i%3)/2,1-i/15,float(i%2)) for i in range(16)]
    for alpha_bits in (0x3effffff,0x3f000001):
        alpha=struct.unpack('<f',struct.pack('<I',alpha_bits))[0]
        yield [(i/15,1-i/15,(i%4)/3,alpha) for i in range(16)]
    rng=random.Random(0x64bb40+seed)
    for index in range(count):
        yield [[rng.random() if index%2 else rng.randrange(256)/255 for _ in range(4)] for _ in range(16)]


def main(flags='1',label='first',count='8',seed='0'):
    flags=int(flags);count=int(count);seed=int(seed)
    assert flags in (1,2,3) and 1<=count<=48 and 0<=seed<=1024 and label.replace('-','').isalnum() and len(label)<=40
    started=time.monotonic();batch=list(cases(count,seed));binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugTextureSerializationTests.exe'
    result=subprocess.run([str(binary),'--encode-blocks'],input=''.join(str(flags)+' '+' '.join(map(str,words([v for row in block for v in row])))+'\n' for block in batch),text=True,capture_output=True,timeout=10)
    assert result.returncode==0,result.stderr;expected=[json.loads(line) for line in result.stdout.splitlines()];assert len(expected)==len(batch)
    p=PcInstructions();data=p.allocate(256);out=p.allocate(32);instructions=0;rows=[];mismatches=[]
    for index,(block,want) in enumerate(zip(batch,expected)):
        p.put_floats(data,[v for row in block for v in row]);p.mu.mem_write(out,b'\xa5'*32)
        p.run({1:0x64c798,2:0x64c8bc,3:0x64c9eb}[flags],args=(out,data,0));size=8 if flags==1 else 16
        observed=bytes(p.mu.mem_read(out,size)).hex();assert p.reg('EAX')==0 and bytes(p.mu.mem_read(out+size,32-size))==b'\xa5'*(32-size)
        instructions+=sum(p.visits.values());row={'index':index,'nativeHex':observed,'sourceHex':want};rows.append(row)
        if observed!=want:mismatches.append(row)
    report={'kind':'native-source-texture-block-encode','status':'mismatch' if mismatches else 'passed','flags':flags,'dither':False,'cases':len(rows),
            'nativeInstructions':instructions,'arenaReservedBytes':p.allocated,'seams':len(p.seams),'seconds':time.monotonic()-started,
            'sourceExecutableSha256':hashlib.sha256(binary.read_bytes()).hexdigest().upper(),'randomBlockCount':count,'seedOffset':seed,'results':rows}
    path=ROOT/f'local-data/results/cycle-20260908-0700/cp113-block-encode-{flags}-{label}.json';path.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='results'}));print('mismatches',json.dumps(mismatches[:12]));return int(bool(mismatches))


if __name__=='__main__':
    raise SystemExit(main(*sys.argv[2:]) if sys.argv[1:2]==['--guest'] else run_bounded(Path(__file__),sys.argv[1:]))
