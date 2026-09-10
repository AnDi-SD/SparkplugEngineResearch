#!/usr/bin/env python3
"""Original PC/PS2 state transition predicates and their consuming dependency.

Borrowed, guarded records are explicit inputs. No game callback is replaced.
Cases are grouped by native implementation and then qualify distinct PS2 copies.
The oracle describes reviewed machine code; it is not an application runtime.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix
import probe_pc_animation_lifecycle as lifetime

CATALOG=ROOT/'research/character-state-construction-contracts-2026-09-10.json'
HANDLE=0x13572468
OTHER=0x24681357
# Original post-SQ entries and pre-LQ stops. PickFlower's branch has SQ in
# its delay slot, so select the branch successor from the declared input.
PREFIX={0x2cc1f0:(0x2cc200,0x2cc2a0),0x2cd590:(0x2cd59c,0x2cd5d4),
        0x2d04e0:(0x2d04f0,0x2d0540),0x305bb0:(0x305bc0,0x305c08)}


def consume(first,second,handle,clear=1):
    if not handle:return 1,first,second
    if second and second==handle:
        if clear:
            second=0
            if first==handle:first=0
        return 1,first,second
    if first and first==handle:return 1,0 if clear else first,second
    return 0,first,second


def expectation(pc_entry,slot,c):
    a=c.get('target',0);h=c.get('handle',HANDLE);x=c.get('first',OTHER);y=c.get('second',OTHER)
    current=c.get('current',0);f=c.get('state3c',0);word40=c.get('state40',0);bits=c.get('characterBits',0)
    changes={};calls=0;boundary=None
    def poll():
        nonlocal x,y,calls
        calls+=1;answer,x,y=consume(x,y,h);return answer
    if slot==14:
        if pc_entry==0x5144f0:answer=int(a not in (0,4,8,10,18))
        elif pc_entry==0x513480:answer=1
        elif pc_entry==0x516c40:answer=int(a==1)
        elif pc_entry==0x534580:answer=int(a==2)
        elif pc_entry==0x5175e0:answer=int(a==9)
        elif pc_entry==0x51ad20:answer=2 if a in (0,3) else 1
        elif pc_entry==0x5159f0:answer=int(a not in (0,19,20,21,22,23,24,29,30) and not(a==4 and c.get('machineState',0)==4))
        elif pc_entry==0x5a7dc0:answer=0
        else:raise ValueError(hex(pc_entry))
    elif pc_entry==0x5a7db0:answer=1
    elif pc_entry==0x518dc0:answer=0
    elif pc_entry==0x518780:answer=f&255
    elif pc_entry==0x513e70:answer=0 if a==1 and bits&15==3 else f&255
    elif pc_entry==0x5144c0:answer=0 if a==1 and bits&15==3 else int(f>=word40)
    elif pc_entry==0x514720:answer=int(a not in (27,29,32) and not(a==1 and bits&15==3))
    elif pc_entry==0x513bb0:answer=int(a not in (1,3,4,5,8))
    elif pc_entry in (0x5a6a00,0x51aa60):answer=int(c.get('gameState',0)!=(70 if pc_entry==0x5a6a00 else 71))
    elif pc_entry==0x5238d0:answer=int(a==10 or f&255!=0)
    elif pc_entry==0x521970:answer=int(f&255==0)
    elif pc_entry==0x516500:answer=int(a in (0,10,14))
    elif pc_entry==0x5161a0:
        answer=int(a!=39)
        if not answer:changes['move61']=0
    elif pc_entry==0x520630:answer=c.get('state4c',0)&255 if c.get('otherMachinePresent',0) else 1
    elif pc_entry==0x516c70:
        answer=int(not(c.get('profile512',0) and a in (3,4,5)) and a!=23 and (word40&255!=0 or a in (14,16,17)))
    elif pc_entry==0x517790:answer=poll() if a not in (10,11) and current==9 and h else 1
    elif pc_entry==0x51f7a0:answer=poll() if current and h else 1
    elif pc_entry in (0x5203d0,0x5a6770):answer=poll() if h else 1
    elif pc_entry==0x523770:answer=poll()
    elif pc_entry==0x520e20:answer=poll() if a!=10 and h else 1
    elif pc_entry==0x5179c0:answer=poll() if a not in (9,10) and h else 1
    elif pc_entry==0x51a810:answer=poll() if a!=28 and h else 1
    elif pc_entry==0x51ad00:answer=0 if a==23 else poll()
    elif pc_entry==0x5139f0:answer=0 if a in (3,4,5,23,31,41) else poll()
    elif pc_entry==0x522990:answer=1 if a==10 else poll()
    elif pc_entry==0x520100:answer=poll() if a not in (10,17,28) and h else 1
    elif pc_entry==0x51a290:answer=poll() if a not in (22,28) and h else 1
    elif pc_entry==0x51a600:answer=poll() if h and f&255 else 1
    elif pc_entry==0x51a050:answer=poll() if h and (f>>8)&255 else 1
    elif pc_entry==0x51fd70:answer=f&255 if current==28 else poll() if h else 1
    elif pc_entry==0x519e50:answer=poll() if h and (current==10 or a not in (10,33)) else 1
    elif pc_entry==0x5194d0:answer=poll() if bits&0x7f80 in (0x400,0x500) else 1
    elif pc_entry==0x534550:answer=int(poll()!=0 and c.get('state1d',0)==0)
    elif pc_entry==0x5a6ae0:
        answer=1 if a==10 or not h else poll()
        if answer:changes['profile504']=0
    elif pc_entry==0x521b20:
        if a==11 and not poll():answer=None;boundary='stop-action'
        else:answer=poll()
    elif pc_entry==0x5a77c0:
        answer=poll()
        if answer:answer=None;boundary='message'
    else:raise ValueError(hex(pc_entry))
    return dict(answer=answer,first=x,second=y,calls=calls,changes=changes,boundary=boundary)


def execute(row,platform,slot,c):
    pc_entry=int(row['pc']['slots'][slot],16);original=int(row[platform]['slots'][slot],16)
    ispc=platform=='pc';size=row[platform]['allocationBytes'];expected=expectation(pc_entry,slot,c)
    if ispc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=lifetime.LifetimeFixture()
        p=f.p;storage=p.allocate(0x4000);read=lambda a,n:bytes(p.mu.mem_read(a,n))
        write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
    else:
        p=Ps2ScalarPrefix([(original,0x100),(0x2a6c30,0x70)])
        storage=0x21000000;p.map(storage,0x4000);p.map(0x22000000,0x2000)
        p.reg('SP',0x22000800);p.reg('GP',0x4a4170);read,write,put=p.read,p.write,p.put_uint
    definitions={'state':(0,size),'character':(0x400,0x300),'consumer':(0x800,0x200),
        'machine':(0xc00,0x300),'otherCharacter':(0x1000,0x200),'move':(0x1400,0x300),
        'profile':(0x1800,0x600),'game':(0x2000,0x300)}
    addresses={k:storage+o for k,(o,n) in definitions.items()}
    for k,(o,n) in definitions.items():write(storage+o,bytes([0xa5])*n)
    obj,char,consumer,machine,other,move,profile,game=addresses.values()
    put(obj,int(row[platform]['vtable'],16));put(obj+0x10,c.get('current',0));put(obj+0x14,char);put(obj+0x18,consumer);put(obj+0x24,c.get('handle',HANDLE))
    write(obj+0x1d,bytes([c.get('state1d',0)]))
    for off,key in ((0x3c,'state3c'),(0x40,'state40'),(0x4c,'state4c')):
        if off+4<=size:put(obj+off,c.get(key,0))
    delta=0 if ispc else 12
    put(char+0x140+delta,c.get('characterBits',0));put(char+0x124+delta,machine);put(char+0x12c+delta,move)
    put(machine+0x14c+delta,c.get('machineState',0));put(machine+0x138+delta,other);put(other+0x124+delta,c.get('otherMachinePresent',0))
    put(consumer+0x170+delta,c.get('first',OTHER));put(consumer+0x174+delta,c.get('second',OTHER))
    put(game+0x1b0,c.get('gameState',0));write(profile+0x512,bytes([c.get('profile512',0)]))
    for global_address,value in (((0x765ad4 if ispc else 0x49fc7c),profile),((0x755294 if ispc else 0x49fd4c),game)):
        if not ispc:p.map(global_address,4)
        put(global_address,value)
    before={k:read(a,definitions[k][1]) for k,a in addresses.items()};wanted={k:bytearray(b) for k,b in before.items()}
    struct.pack_into('<II',wanted['consumer'],0x170+delta,expected['first'],expected['second'])
    if 'move61' in expected['changes']:wanted['move'][0x61]=0
    if 'profile504' in expected['changes']:struct.pack_into('<I',wanted['profile'],0x504,0)
    calls=[]
    def observe(u,a,n,user):
        if a==(0x4fb330 if ispc else 0x2a6c30):
            receiver=p.reg('ECX') if ispc else p.reg('A0');h=p.uint(p.reg('ESP')+4) if ispc else p.reg('A1')&0xffffffff
            flag=p.uint(p.reg('ESP')+8) if ispc else p.reg('A2')
            assert (receiver,h,flag)==(consumer,c.get('handle',HANDLE),1)
            calls.append(dict(handle=h,first=p.uint(consumer+0x170+delta),second=p.uint(consumer+0x174+delta)))
    (p.mu if ispc else p.u).hook_add(p.uc.UC_HOOK_CODE if ispc else __import__('unicorn').UC_HOOK_CODE,observe)
    if ispc:
        stop={'stop-action':0x512e40,'message':0x40ec00}.get(expected['boundary'])
        p.run(original,obj,(c.get('target',0),),stop_at=stop)
        result=dict(entry=f'{original:08X}',stop=f'{p.reg("EIP"):08X}',completion='consumer boundary;guest discarded' if stop else 'original return',blocks=sum(p.visits.values()))
        actual=p.reg('EAX')&(0xffffffff if slot==14 else 255)
        if stop:
            assert p.reg('ECX')==obj
            actual_args=[p.uint(p.reg('ESP')+4+4*i) for i in range(4 if expected['boundary']=='message' else 1)]
            assert actual_args==([0x27de,0x12,0,0] if expected['boundary']=='message' else [0])
            result['consumerArguments']=actual_args
    else:
        entry,end=PREFIX.get(original,(original,p.RETURN));p.reg('A0',obj);p.reg('A1',c.get('target',0))
        if original==0x305bb0:p.reg('V0',11)
        if original==0x2d1ce0:entry=0x2d1cf4 if c.get('target',0)==10 else 0x2d1d10;end=0x2d1d9c
        stop={'stop-action':0x2c8d60,'message':0x1007a0}.get(expected['boundary'],end)
        result=p.run(entry,[stop],timeout_us=500000);actual=p.reg('V0')&0xffffffff
        if expected['boundary']:
            assert p.reg('A0')==obj
            actual_args=[p.reg(r)&0xffffffff for r in (('A1','A2','A3','T0') if expected['boundary']=='message' else ('A1',))]
            assert actual_args==([0x27de,0x12,0,0] if expected['boundary']=='message' else [0])
            result['consumerArguments']=actual_args
        result['originalEntry']=f'{original:08X}'
    assert len(calls)==expected['calls'],(len(calls),expected)
    if expected['answer'] is not None:assert actual==expected['answer'],(hex(actual),expected)
    after={k:read(a,definitions[k][1]) for k,a in addresses.items()}
    for k,b in after.items():assert b==wanted[k],(k,[(hex(i),a,z) for i,(a,z) in enumerate(zip(b,wanted[k])) if a!=z][:12])
    result.update(expected=expected,returned=actual if expected['answer'] is not None else None,dependencyCalls=calls,
        buffers={k:dict(size=len(before[k]),beforeSha256=hashlib.sha256(before[k]).hexdigest().upper(),afterSha256=hashlib.sha256(b).hexdigest().upper(),
            changes=[dict(offset=i,before=a,after=z) for i,(a,z) in enumerate(zip(before[k],b)) if a!=z]) for k,b in after.items()})
    return result


def guest(output,selection):
    output=Path(output).resolve();selection=Path(selection).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local result required')
    cases=json.loads(selection.read_text())['cases'];catalog=json.loads(CATALOG.read_text());rows={r['className']:r for r in catalog['classes']}
    if not 0<len(cases)<=180:raise ValueError('Bounded adjacent batch of1..180 platform cases')
    report=dict(kind='original-character-state-transition-permissions',status='running',inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),selectionSha256=hashlib.sha256(selection.read_bytes()).hexdigest().upper(),
        catalogSha256=hashlib.sha256(CATALOG.read_bytes()).hexdigest().upper(),cases=[])
    started=time.perf_counter();output.parent.mkdir(parents=True,exist_ok=True)
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for c in cases:
        report['pending']=c;save()
        try:result=dict(input=c,status='passed',**execute(rows[c['className']],c['platform'],c['slot'],c['values']))
        except Exception as error:result=dict(input=c,status='blocked',error=str(error),traceback=traceback.format_exc())
        report['cases'].append(result);report.pop('pending');save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='partial' if failed else 'passed';save()
    print(json.dumps(dict(status=report['status'],cases=len(cases),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
