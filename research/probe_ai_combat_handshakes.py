#!/usr/bin/env python3
"""Paired original combat handshakes, using the actual warmed RNG output path.

Original PC v7 executes to return; PS2 executes dispatcher/state bodies from
the declared post-SQ entry to the common completion boundary. No game callee
is replaced. Borrowed character-state and clock records are explicit inputs.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED
from pc_block_emulator import PcBlocks
from pc_instruction_emulator import run_bounded
from ps2_scalar_prefix import Ps2ScalarPrefix

CATALOG=ROOT/'research/ai-action-construction-contracts-2026-09-10.json'
GOLDENS={0:0,0x12345678:0x2b7e4a2d,0x80000000:0x88102204,0xffffffff:0x6fe01bf8}
SPECS={
 'GolemAttack':dict(pc=0x5b80c0,ps2=0x24ddc4,size=0x2c8,end=0x24e088,table=None,states={1:(3,0x20),2:(4,0x21)}),
 'SpiderAttack':dict(pc=0x5b3cb0,ps2=0x262910,size=0x3c8,end=0x262cd4,table=(0x478490,24),states={1:(3,0x21),2:(3,0x20)}),
 'ShadowBeastAttack':dict(pc=0x5ba600,ps2=0x261c24,size=0x400,end=0x262020,table=(0x478470,32),states={3:(3,0x22)}),
}


def machine(platform,ranges=()):
    if platform=='pc':
        p=PcBlocks();read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));storage=p.allocate(0x3000)
    else:
        p=Ps2ScalarPrefix(list(ranges)+[(0x108350,0x1c),(0x108e30,0x68)]);storage=0x21000000
        for a,n in ((storage,0x3000),(0x22000000,4096),(0x49c000,4096),(0x49f000,4096),(0x4a0000,8192)):p.map(a,n)
        read,write=p.read,p.write;p.reg('GP',0x4a4170);p.reg('SP',0x22000800)
    return p,read,write,storage


def warmed_rng(p,platform,write,word):
    index,array,stride=(0x73fe8c,0x755658,4) if platform=='pc' else (0x49c230,0x4a0e80,8)
    write(array,bytes.fromhex('A5')*(stride*4));write(array,word.to_bytes(stride,'little'));p.put_uint(index,0)
    return index,array,stride


def rng_case(platform,word,timeout_us=100000):
    p,read,write,storage=machine(platform);index,array,stride=warmed_rng(p,platform,write,word);before=read(array,stride*4)
    if platform=='pc':p.run(0x4132b0);execution=dict(entry='004132B0',completion='original return',blocks=sum(p.visits.values()));value=p.reg('EAX')
    else:execution=p.run(0x108350,[p.RETURN],timeout_us=timeout_us);value=p.reg('V0')&0xffffffff
    assert value==GOLDENS[word] and p.uint(index)==1 and read(array,stride*4)==before
    return dict(**execution,output=value,indexAfter=1,arrayUnchanged=True)


def handshake(row,platform,state,latch,observed,word,now):
    name=row['className'][2:-8];spec=SPECS[name];ispc=platform=='pc';d=0 if ispc else 4
    ranges=[(spec['ps2'],spec['size'])]+([spec['table']] if spec['table'] else [])
    p,read,write,storage=machine(platform,ranges);index,array,stride=warmed_rng(p,platform,write,word)
    sizes={'action':row[platform]['allocationBytes'],'owner':0x300,'character':0x300,'command':0x100,'stateMachine':0x300,'timer':0x88}
    addresses={k:storage+off for k,off in zip(sizes,(0,0x1000,0x1400,0x1800,0x1c00,0x2000))}
    for k,a in addresses.items():write(a,b'\xa5'*sizes[k])
    obj,owner,char,cmd,sm,timer=addresses.values();put=p.put_uint
    put(obj,int(row[platform]['vtable'],16));put(obj+0x20+d,owner);put(owner+(0x144 if ispc else 0x154),char)
    put(char+(0x124 if ispc else 0x130),sm);put(char+(0x130 if ispc else 0x13c),cmd)
    put(sm+(0x130 if ispc else 0x13c),observed);write(obj+0x3bc+d,bytes([latch]));put(obj+0x3ac+d,state)
    put(timer+0x1c,now);put(0x755298 if ispc else 0x49fc80,timer)
    before={k:read(a,sizes[k]) for k,a in addresses.items()};expected={k:bytearray(v) for k,v in before.items()}
    wanted,command=spec['states'][state];consume=False
    if name=='ShadowBeastAttack':expected['command'][0x1d]=1
    if latch==0:
        expected['command'][command]=int(observed!=wanted)
        if observed==wanted:expected['action'][0x3bc+d]=1
    elif observed!=wanted:
        expected['action'][0x3bc+d]=0
        if name=='GolemAttack':
            consume=True;struct.pack_into('<I',expected['action'],0x3ac+d,0)
            struct.pack_into('<I',expected['action'],0x3b0+d,(now+GOLDENS[word]%301+200)&0xffffffff)
        elif name=='SpiderAttack' and state==1:
            consume=True;struct.pack_into('<I',expected['action'],0x3ac+d,4);struct.pack_into('<I',expected['action'],0x3b0+d,0)
        else:
            struct.pack_into('<I',expected['action'],0x3ac+d,0);struct.pack_into('<I',expected['action'],0x3b4+d,0)
            if name=='ShadowBeastAttack':struct.pack_into('<I',expected['action'],0x3b0+d,0)
    rng_before=read(array,stride*4)
    if ispc:
        p.run(spec['pc'],this=obj);assert p.reg('EAX')&255==1
        execution=dict(entry=f"{spec['pc']:08X}",completion='original v7 return',blocks=sum(p.visits.values()))
    else:
        p.reg('A0',obj);p.reg('A1',1);p.reg('A3',3)
        execution=p.run(spec['ps2'],[spec['end']])
    after={k:read(a,sizes[k]) for k,a in addresses.items()}
    for k in sizes:assert after[k]==expected[k],(k,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after[k],expected[k])) if a!=b][:12])
    assert p.uint(index)==int(consume) and read(array,stride*4)==rng_before,'actual RNG consumption and unchanged warm array'
    execution.update(rngOutputsConsumed=p.uint(index),latchAfter=after['action'][0x3bc+d],stateAfter=p.uint(obj+0x3ac+d),
        deadlineAfter=p.uint(obj+0x3b0+d),commandAfter=after['command'][command],buffers={k:dict(bytes=sizes[k],
        beforeSha256=hashlib.sha256(before[k]).hexdigest().upper(),afterSha256=hashlib.sha256(after[k]).hexdigest().upper(),
        changes=[dict(offset=i,before=a,after=b) for i,(a,b) in enumerate(zip(before[k],after[k])) if a!=b]) for k in sizes})
    return execution


def guest(output,selection='batch'):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    if selection not in ('rng-pilot','retry-ps2-zero','handshake-pilot','batch'):raise ValueError('Explicit selection')
    output.parent.mkdir(parents=True,exist_ok=True);started=time.perf_counter();rows={c['className']:c for c in json.loads(CATALOG.read_text())['classes']}
    report=dict(kind='paired-original-ai-combat-handshakes',status='running',selection=selection,inputs=EXPECTED,cases=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='PC full original v7 selected state paths;PS2 post-SQ original dispatcher/state body until explicit completion boundary. Borrowed state/clock inputs;actual warm RNG entry. No full RNG seed/twist,character startup or unselected attack-state claim.')
    cases=[dict(kind='rng',word=w) for w in GOLDENS] if selection=='rng-pilot' else []
    if selection=='retry-ps2-zero':cases=[dict(kind='rng',word=0)]
    if selection in ('batch','handshake-pilot'):
        for name,spec in SPECS.items():
            for state,(wanted,command) in spec['states'].items():
                for latch in (0,1,255):
                    for observed in (0,wanted,0xffffffff):cases.append(dict(kind='handshake',name=name,state=state,latch=latch,observed=observed,word=0x12345678,now=1200))
        cases += [dict(kind='handshake',name='GolemAttack',state=state,latch=1,observed=0,word=w,now=0xfffffff0) for state in (1,2) for w in (0,0x80000000,0xffffffff)]
        cases=cases[:1] if selection=='handshake-pilot' else cases[1:]
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for case in cases:
        result=dict(input=case,platforms={});report['cases'].append(result)
        for platform in ('pc','ps2'):
            if selection=='retry-ps2-zero' and platform!='ps2':continue
            report['pending']=dict(**case,platform=platform);save()
            try:
                v=rng_case(platform,case['word'],500000 if selection=='retry-ps2-zero' else 100000) if case['kind']=='rng' else handshake(rows['wx'+case['name']+'AIAction'],platform,**{k:v for k,v in case.items() if k not in ('kind','name')})
                result['platforms'][platform]=dict(status='passed',**v)
            except Exception as error:result['platforms'][platform]=dict(status='blocked',error=str(error),traceback=traceback.format_exc())
            report.pop('pending');save()
    failed=sum(v['status']!='passed' for c in report['cases'] for v in c['platforms'].values())
    report['status']='passed' if not failed else 'partial';save();print(json.dumps(dict(status=report['status'],cases=len(cases),platformCases=sum(len(c['platforms']) for c in report['cases']),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
