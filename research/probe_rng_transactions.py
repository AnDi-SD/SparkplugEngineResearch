#!/usr/bin/env python3
"""Original PC/PS2 RNG default initialization, output and regeneration.

Returned native state is transferred verbatim into fresh bounded guests.
One explicitly separate boundary experiment sets index623 in the already
generated state; this is not claimed to be a625-output serial replay.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_block_emulator import PcBlocks
from pc_instruction_emulator import run_bounded
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine

BASE=ROOT/'local-data/results/native-cycle-20260911-0730/rng-transactions'


def execute(platform,state):
    ispc=platform=='pc';index,array,stride=(0x73fe8c,0x755658,4) if ispc else (0x49c230,0x4a0e80,8)
    if ispc:
        p=PcBlocks();read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b))
    else:
        p=Ps2ScalarPrefix([(0x108350,0xb48)]);read,write=p.read,p.write
        for a,n in ((0x49c000,4096),(0x4a0000,0x3000),(0x444000,4096)):p.map(a,n)
        raw,sections=pristine();write(0x444170,read_window('ps2',raw,0x444170,16,sections)[0]);p.reg('GP',0x4a4170)
    raw_state=bytes.fromhex(state['arrayHex'])
    assert len(raw_state)==624*stride
    write(array-16,b'\xa5'*(len(raw_state)+32));write(array,raw_state);p.put_uint(index,state['index'])
    if ispc:
        p.run(0x4132b0);execution=dict(entry='004132B0',completion='original return',blocks=sum(p.visits.values()));value=p.reg('EAX')
    else:
        execution=p.run(0x108350,[p.RETURN],count=100000,timeout_us=2000000);value=p.reg('V0')&0xffffffff
    assert read(array-16,16)==read(array+len(raw_state),16)==b'\xa5'*16,'array extent guards'
    result=read(array,len(raw_state));words=list(struct.unpack('<'+str(624)+('I' if ispc else 'Q'),result))
    if not ispc:assert all(w<=0xffffffff for w in words),'PS2 state upper halves after original initialization/twist'
    execution.update(output=value,index=p.uint(index),arrayHex=result.hex(),normalizedWords=words,arraySha256=hashlib.sha256(result).hexdigest().upper())
    return execution


def guest(output,selection='cold-pilot'):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    if selection not in ('cold-pilot','continuations'):raise ValueError('Explicit selection')
    output.parent.mkdir(parents=True,exist_ok=True);started=time.perf_counter()
    report=dict(kind='original-pc-ps2-rng-transactions',status='running',selection=selection,inputs=EXPECTED,cases=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Actual complete generator calls including cold default seed and array regeneration. Independent native bytes;state copied only after normal return. Separate index623 experiment is explicit,not a serial625-output replay.')
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    if selection=='cold-pilot':
        initial={k:dict(index=625,arrayHex=(b'\xcc'*(624*(4 if k=='pc' else 8))).hex()) for k in ('pc','ps2')}
        labels=['cold-default'];parents={}
    else:
        source=BASE/'cold-pilot-run1.json';prior=json.loads(source.read_text());assert prior['status']=='passed'
        initial={k:{a:prior['cases'][0]['platforms'][k][a] for a in ('index','arrayHex')} for k in ('pc','ps2')}
        labels=['next-original-output','explicit-boundary-index623','next-regeneration','next-after-regeneration'];parents=initial
        report['priorReturnedState']=dict(path=source.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(source.read_bytes()).hexdigest().upper())
    current=initial
    for label in labels:
        result=dict(label=label,platforms={});report['cases'].append(result)
        if label=='explicit-boundary-index623':current={k:dict(index=623,arrayHex=v['arrayHex']) for k,v in parents.items()}
        for platform in ('pc','ps2'):
            report['pending']=dict(label=label,platform=platform,inputIndex=current[platform]['index']);save()
            try:result['platforms'][platform]=dict(status='passed',**execute(platform,current[platform]))
            except Exception as error:
                result['platforms'][platform]=dict(status='blocked',error=str(error),traceback=traceback.format_exc());report['status']='blocked';save();print(json.dumps(dict(status='blocked',label=label,platform=platform,error=str(error))));return 1
            report.pop('pending');save()
        a,b=result['platforms']['pc'],result['platforms']['ps2']
        try:
            assert (a['output'],a['index'],a['normalizedWords'])==(b['output'],b['index'],b['normalizedWords']),'independent platform outputs and all624 words'
            if label=='cold-default':assert a['index']==1 and a['output']==3499211612
            if label=='next-original-output':assert a['index']==2 and a['output']==581869302
            if label=='explicit-boundary-index623':assert a['index']==624
            if label=='next-regeneration':assert a['index']==1
            if label=='next-after-regeneration':assert a['index']==2
        except Exception as error:report.update(status='mismatch',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='mismatch',label=label,error=str(error))));return 1
        current={k:{a:result['platforms'][k][a] for a in ('index','arrayHex')} for k in ('pc','ps2')};save()
    report['status']='passed';save();print(json.dumps(dict(status='passed',pairedTransactions=len(labels),seconds=report['seconds'],outputs=[c['platforms']['pc']['output'] for c in report['cases']])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
