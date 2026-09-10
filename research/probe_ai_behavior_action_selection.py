#!/usr/bin/env python3
"""Paired original AI behavior map lookup and action selection prefixes."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from capture_native_ranges import EXPECTED,read_window
import probe_pc_animation_lifecycle as lifetime


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    started=time.perf_counter();report=dict(kind='paired-original-ai-behavior-action-selection',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='PC whole lookup/selection after actual Baco factory and original map insertion. PS2 prefixes skip SQ frames, execute original lookup/iterator/action leaves, stop before restoration. Literal action interface objects use pristine wxAIAction vtables;no heavy action callback substitution.',
        limits='Fresh guests per case;PC100k/2s blocks,PS22000/100ms,30s outer;whole receiver/action guards. PC factory allocations bounded64KiB.')
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    base_keys={0:1,7:2,0x80000000:3,0xffffffff:0}
    cases=[]
    for keys,label in [({},'empty'),(base_keys,'four-keys')]:
        for key in (0,1,7,8,0x80000000,0xfffffffe,0xffffffff):cases.append(dict(kind='lookup',label=label,key=key,keys=keys,old=0,gate=0))
    for key,old,gate,label in ((7,1,0,'found'),(7,2,0,'same-action-reentered'),(8,2,0,'missing-falls-back-zero'),(0xffffffff,2,0,'null-value-falls-back-zero'),(0,2,1,'gate-allows-zero'),(7,2,1,'gate-rejects-nonzero'),(0x80000000,0,0,'high-bit-found'),(8,0,0,'missing-from-null')):
        cases.append(dict(kind='select',label=label,key=key,keys=base_keys,old=old,gate=gate))
    cases.extend([dict(kind='select',label='no-fallback',key=8,keys={7:2},old=1,gate=0),dict(kind='select',label='empty-map',key=0,keys={},old=1,gate=0)])
    try:
        for case in cases:
            report['pending']=case;save();pair={};key=case['key'];keys=case['keys'];old=case['old'];gate=case['gate'];kind=case['kind']
            rejected=kind=='select' and bool(gate and key)
            selected=keys.get(key,0)
            if kind=='select' and not selected:selected=keys.get(0,0)
            if rejected:selected=old
            for platform in ('pc','ps2'):
                callbacks=[]
                if platform=='pc':
                    with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
                    p=f.p;f.time(1200)
                    # Resolve the already captured exact factory, not a guessed nearby address.
                    catalog=json.loads((ROOT/'local-data/results/native-cycle-20260910-1900/ai-behavior/catalog-family.json').read_text())
                    obj=f.call(next(r['pcFactory'] for r in catalog if r['className']=='wxBacoAIBehavior'))
                    n=f.allocations[obj];actions={i:p.allocate(0x3a8) for i in (1,2,3)};actions[0]=0
                    current,active,clear=0x124,0x138,0x374
                    write=lambda a,b:p.mu.mem_write(a,bytes(b));read=lambda a,n:bytes(p.mu.mem_read(a,n));put=p.put_uint
                    for i,a in actions.items():
                        if i:write(a,b'\xcc'*0x3a8);put(a,0x702450);put(a+clear,0xabc00000+i)
                    kptr=p.allocate(4)
                    for k,v in keys.items():put(kptr,k);value=f.call(0x562010,obj+0x12c,(kptr,));put(value,actions[v])
                    def watch(mu,address,size,user):
                        if address==0x58ea50:callbacks.append(dict(kind='exit',action=next(i for i,a in actions.items() if a==p.reg('ECX')),current=next(i for i,a in actions.items() if a==p.uint(obj+current))))
                        elif address==0x5b7a00:callbacks.append(dict(kind='enter',action=next(i for i,a in actions.items() if a==p.reg('ECX')),current=next(i for i,a in actions.items() if a==p.uint(obj+current)),parameter=p.uint(p.reg('ESP')+4)))
                    hook=p.mu.hook_add(p.uc.UC_HOOK_CODE,watch)
                else:
                    ranges=[(0x225df0,0x1f0),(0x223890,0x78),(0x2233c0,0x28),(0x223490,0x18),(0x2234e0,12)]
                    p=Ps2ScalarPrefix(ranges);p.map(0x21000000,0x8000);p.map(0x22000000,0x1000)
                    obj,n=0x21000000,0x1c0;actions={0:0,1:0x21001000,2:0x21002000,3:0x21003000};current,active,clear=0x130,0x148,0x378
                    write,read,put=p.write,p.read,p.put_uint;write(obj,b'\xcc'*n)
                    raw,sections=pristine();p.map(0x492060,0x50);write(0x492060,read_window('ps2',raw,0x492060,0x50,sections)[0])
                    for i,a in actions.items():
                        if i:write(a,b'\xcc'*0x3ac);put(a,0x492060);put(a+clear,0xabc00000+i)
                    sorted_keys=sorted(keys);nodes={k:0x21005000+32*i for i,k in enumerate(sorted_keys)}
                    def tree(items,parent):
                        if not items:return 0
                        mid=len(items)//2;k=items[mid];a=nodes[k];put(a,tree(items[:mid],a));put(a+4,tree(items[mid+1:],a));put(a+8,parent);put(a+12,k);put(a+16,actions[keys[k]]);return a
                    put(obj+0x138,len(keys));put(obj+0x13c,tree(sorted_keys,obj+0x13c));put(obj+0x140,nodes[sorted_keys[0]] if keys else obj+0x13c);put(obj+0x144,nodes[sorted_keys[-1]] if keys else obj+0x13c)
                    p.reg('SP',0x22000800);p.reg('A0',obj);p.reg('A1',key if key<0x80000000 else key|0xffffffff00000000)
                    if kind=='lookup':p.reg('A2',0x22000824)
                    else:p.reg('S2',obj);p.reg('A2',0x12345678)
                    def watch(mu,address,size,user):
                        if address==0x223490:callbacks.append(dict(kind='exit',action=next(i for i,a in actions.items() if a==p.reg('A0')),current=next(i for i,a in actions.items() if a==p.uint(obj+current))))
                        elif address==0x2234a0:callbacks.append(dict(kind='enter',action=next(i for i,a in actions.items() if a==p.reg('A0')),current=next(i for i,a in actions.items() if a==p.uint(obj+current)),parameter=p.reg('A1')&0xffffffff))
                    p.u.hook_add(__import__('unicorn').UC_HOOK_CODE,watch)
                put(obj+current,actions[old]);write(obj+active,bytes([gate]));before=read(obj,n)
                sizes={i:(0x3a8 if platform=='pc' else 0x3ac) for i in (1,2,3)};action_before={i:read(actions[i],sizes[i]) for i in sizes}
                if platform=='pc':
                    result=f.call(0x591c40 if kind=='lookup' else 0x591c80,obj,(key,) if kind=='lookup' else (key,0x12345678));r=dict(blocks=sum(p.visits.values()),completion='original return');p.mu.hook_del(hook)
                else:r=p.run(0x225f90 if kind=='lookup' else 0x225e08,[0x225fcc if kind=='lookup' else 0x225ee4]);result=p.reg('V0')&0xffffffff
                expected=bytearray(before)
                if kind=='select':struct.pack_into('<I',expected,current,actions[selected])
                assert read(obj,n)==bytes(expected),'receiver guard'
                for i in sizes:
                    desired=bytearray(action_before[i])
                    if kind=='select' and not rejected and i==old:struct.pack_into('<I',desired,clear,0)
                    assert read(actions[i],sizes[i])==bytes(desired),'action guard'
                expected_calls=[]
                if kind=='select' and not rejected:
                    if old:expected_calls.append(dict(kind='exit',action=old,current=old))
                    if selected:expected_calls.append(dict(kind='enter',action=selected,current=selected,parameter=0x12345678))
                assert callbacks==expected_calls,(callbacks,expected_calls)
                value=next(i for i,a in actions.items() if a==result) if kind=='lookup' else result&255
                assert value==(keys.get(key,0) if kind=='lookup' else int(not rejected))
                r.update(value=value,current=next(i for i,a in actions.items() if a==p.uint(obj+current)),callbacks=callbacks);pair[platform]=r
            assert all(pair['pc'][k]==pair['ps2'][k] for k in ('value','current','callbacks'))
            report['cases'].append(dict(input=case,**pair));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(cases),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
