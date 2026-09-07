#!/usr/bin/env python3
"""Original weighted RGB endpoint optimizer compared at binary32 stores."""
from pathlib import Path
import hashlib,json,random,struct,subprocess,sys,time
from pc_instruction_emulator import PcInstructions,ROOT,run_bounded


def f32(value):return struct.unpack('<f',struct.pack('<f',value))[0]
def words(values):return list(struct.unpack('<'+'I'*len(values),struct.pack('<'+'f'*len(values),*values)))
def cases(random_count=8,seed=0):
    weights=struct.unpack('<3f',struct.pack('<3I',0x3e981530,0x3f800000,0x3dce6734))
    raw=[[[0,0,0]]*16,[[1,1,1]]*16,[[.5,.25,.75]]*16,
         [[i/15,1-i/15,(i%4)/3] for i in range(16)]]
    rng=random.Random(0x64b5c0+seed)
    for _ in range(random_count):raw.append([[rng.randrange(256)/255 for _ in range(3)] for _ in range(16)])
    for index,colors in enumerate(raw):
        points=[[f32(f32(value)*weight) for value,weight in zip(color,weights)] for color in colors]
        for steps in (3,4):yield f'{index}-{steps}',steps,points


def main(label='first',random_count='8',seed='0'):
    assert label.replace('-','').isalnum() and len(label)<=40
    capture=label.startswith('trace')
    random_count=int(random_count);seed=int(seed);assert 1<=random_count<=48 and 0<=seed<=1024
    started=time.monotonic();batch=list(cases(random_count,seed));binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugTextureSerializationTests.exe'
    if capture:batch=[row for row in batch if row[0]=='3-3']
    text=''.join(str(steps)+' '+' '.join(map(str,words([v for p in points for v in p])))+'\n' for _,steps,points in batch)
    result=subprocess.run([str(binary),'--optimize-blocks-trace' if capture else '--optimize-blocks'],input=text,text=True,capture_output=True,timeout=10)
    assert result.returncode==0,result.stderr;expected=[json.loads(line) for line in result.stdout.splitlines()];assert len(expected)==len(batch)
    p=PcInstructions();data=p.allocate(256);a=p.allocate(16);b=p.allocate(16);instructions=0;rows=[];mismatches=[]
    trace=[]
    if capture:
        def observe(mu,address,size,user):
            base=p.reg('EBP');trace.append([p.uint(base+off) for off in (-0x10,-0xc,-8,-0x20,-0x1c,-0x18,-0x40,-0x3c,-0x38,-0x50,-0x4c,-0x48,-0x30,-0x2c,-0x28,0x14,0x10)])
        p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=0x64ba2d,end=0x64ba2d)
    for (name,steps,points),want in zip(batch,expected):
        p.put_floats(data,[v for row in points for v in (*row,1)]);p.mu.mem_write(a,b'\xa5'*16);p.mu.mem_write(b,b'\xa5'*16)
        p.run(0x64b5c0,args=(a,b,data,steps));got=list(struct.unpack('<3I',p.mu.mem_read(a,12)))+list(struct.unpack('<3I',p.mu.mem_read(b,12)))
        assert bytes(p.mu.mem_read(a+12,4))==bytes(p.mu.mem_read(b+12,4))==b'\xa5'*4
        instructions+=sum(p.visits.values());rows.append({'name':name,'native':got,'source':want})
        if capture:rows[-1]['nativeTrace']=trace.copy()
        if got!=(want['result'] if capture else want) or (capture and trace!=want['trace']):mismatches.append(rows[-1])
    report={'kind':'native-source-texture-block-optimizer','status':'mismatch' if mismatches else 'passed','cases':len(rows),
            'nativeInstructions':instructions,'arenaReservedBytes':p.allocated,'seams':len(p.seams),'seconds':time.monotonic()-started,
            'randomBlockCount':random_count,'seedOffset':seed,
            'sourceExecutableSha256':hashlib.sha256(binary.read_bytes()).hexdigest().upper(),'results':rows}
    path=ROOT/f'local-data/results/cycle-20260908-0700/cp112-block-optimizer-{label}.json';path.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='results'}));print('mismatches',json.dumps(mismatches[:8]));return int(bool(mismatches))


if __name__=='__main__':
    raise SystemExit(main(*sys.argv[2:]) if sys.argv[1:2]==['--guest'] else run_bounded(Path(__file__),sys.argv[1:]))
