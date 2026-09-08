#!/usr/bin/env python3
"""Exact original region dispatch, positions and entire MT19937 state; no seams."""
from pathlib import Path
import hashlib,json,struct,subprocess,sys,time
from pc_instruction_emulator import PcInstructions,ROOT,run_bounded

def region_input(tag,variant):
    parameters={1:[],2:[4,6,8],3:[2],4:[0,1,0,4,6],5:[2],6:[4,2],7:[1,2,3]}
    origin=[1.25,-2.5,3.75];values=parameters[tag]
    if variant=='translated':
        origin=[1024.25,-.125,-8192.5]
        values={1:[],2:[1.234567,3.1415927,6.54321],3:[1.234567],4:[7,-8,9,1.234567,6.54321],5:[1.234567],6:[6.54321,1.234567],7:[.75,1.234567,2.345678]}[tag]
    elif variant=='zero':origin=[-0.,0.,-0.];values=[0.]*len(values)
    elif variant=='negative':values=[-v for v in values]
    elif variant!='base':raise ValueError('Explicit region variant required')
    return struct.pack('<'+'f'*(len(origin)+len(values)),*(origin+values))

def main(tag=1):
    if not 1<=tag<=7:raise ValueError('Region tag1..7 required')
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugParticleSerializationTests.exe'
    p=PcInstructions(execution_profile='file');obj=p.allocate(264);region=p.allocate(32);guard=p.allocate(128*12+32);output=guard+16
    p.put_uint(obj+0x70,tag);p.put_uint(obj+0x74,region)
    cases=[];started=time.monotonic()
    report={'kind':'native-particle-sampling-comparison','regionTag':tag,'executionProfile':'file','arenaLimitBytes':p.arena_size,
        'sourceExecutableSha256':hashlib.sha256(binary.read_bytes()).hexdigest().upper(),'cases':cases}
    try:
        for variant in ('base','translated','zero','negative'):
            data=region_input(tag,variant);p.mu.mem_write(region,data)
            for seed in (0,5489,0xffffffff):
                for count in (0,1,128):
                    result=subprocess.run([str(binary),'--sample',str(tag),str(seed),str(count)],input=(data.hex()+'\n').encode(),capture_output=True,timeout=5)
                    assert result.returncode==0,result.stderr.decode(errors='replace')
                    expected=json.loads(result.stdout)
                    p.run(0x413270,args=(seed,),callee_pop=False)
                    p.mu.mem_write(guard,b'\xa5'*(128*12+32))
                    p.run(0x48c100,this=obj,args=(count,output));instructions=sum(p.visits.values())
                    assert not p.seams
                    positions=bytes(p.mu.mem_read(output,count*12))
                    observed={'positionsHex':positions.hex(),'randomIndex':p.uint(0x73fe8c),
                        'randomStateHex':bytes(p.mu.mem_read(0x755658,624*4)).hex()}
                    assert observed==expected,dict(tag=tag,variant=variant,seed=seed,count=count,observed=observed,expected=expected)
                    assert bytes(p.mu.mem_read(guard,16))==b'\xa5'*16
                    assert bytes(p.mu.mem_read(output+count*12,(128-count)*12+16))==b'\xa5'*((128-count)*12+16)
                    cases.append(dict(variant=variant,seed=seed,count=count,positionsSha256=hashlib.sha256(positions).hexdigest().upper(),
                        randomIndex=observed['randomIndex'],randomStateSha256=hashlib.sha256(bytes.fromhex(observed['randomStateHex'])).hexdigest().upper(),
                        instructions=instructions,randomCalls=p.visits[0x4132b0]))
        report.update(status='passed',caseCount=len(cases),exactPositionBytes=sum(c['count']*12 for c in cases),
            maxInstructions=max(c['instructions'] for c in cases),totalInstructions=sum(c['instructions'] for c in cases),
            exactRandomStateBytes=len(cases)*(624*4+4),maxRandomCalls=max(c['randomCalls'] for c in cases))
    except (AssertionError,ValueError) as e:report.update(status='failed',error=str(e));raise
    finally:
        report.update(seconds=time.monotonic()-started,arenaReservedBytes=p.allocated)
        path=ROOT/'local-data/results/cycle-20260908-0700'/f'cp121-sampling-{tag}.json';path.write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps({k:v for k,v in report.items() if k!='cases' and k!='error'}),flush=True)
    return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(int(sys.argv[2]) if len(sys.argv)>2 else 1))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
