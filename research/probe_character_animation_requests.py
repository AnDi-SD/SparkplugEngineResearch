#!/usr/bin/env python3
"""Original state packed-key requests, real PC lookup and playback boundaries.

PC same-animation paths return completely; changed paths stop at real playback.
PS2 post-SQ bodies stop at real indexed lookup. No lookup/playback replacement.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix
import probe_pc_animation_lifecycle as lifetime

SELECTION=ROOT/'local-data/results/native-cycle-20260911-0730/character-animation-requests/fixed-mask-selection.json'
HANDLE=0x13572468
MODE_ZERO={'MikaelOpenGate','Missile','OpenGate','TrixAttack','WandringNPCWait','YetiAttack'}


def execute(row,c):
    ispc=c['platform']=='pc';name=row['className'][2:-5];size=row['pcBytes' if ispc else 'ps2Bytes']
    masked=(c['packed']&row['pcMask'])|row['pcBits'];same=c['stage']=='same';profile=c['profile']
    if ispc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=lifetime.LifetimeFixture()
        p=f.p;storage=p.allocate(0x6000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
    else:
        p=Ps2ScalarPrefix([(int(row['ps2Entry'],16),0x400)]);storage=0x21000000;p.map(storage,0x6000);p.map(0x22000000,0x1000)
        p.reg('SP',0x22000800);p.reg('GP',0x4a4170);read,write,put=p.read,p.write,p.put_uint
    write(storage,b'\xa5'*0x6000)
    obj,char,move,arg,manager=storage,storage+0x400,storage+0x800,storage+0xc00,storage+0x1000
    put(obj,int(row['pcVtable' if ispc else 'ps2Vtable'],16));put(obj+0x14,char);put(obj+0x24,HANDLE if same else 0)
    put(char+(0x128 if ispc else 0x134),profile);put(char+(0x12c if ispc else 0x138),move);put(arg,c['packed'])
    if ispc:
        put(manager,0x703470);put(0x765adc,manager)
        for i in range(68):
            tree=manager+0x20+12*i;head=storage+0x2000+0x20*i
            put(tree+4,head);put(tree+8,int(i==profile));put(head,head);put(head+4,head);put(head+8,head);write(head+0x14,b'\x01\x01')
            if i==profile:
                node=storage+0x3000;put(node,head);put(node+4,head);put(node+8,head);put(node+0xc,0);put(node+0x10,HANDLE);write(node+0x14,b'\x01\x00');put(head+4,node)
    else:
        put(manager,0x492ff0);p.map(0x49fda0,4);put(0x49fda0,manager)
    before=read(storage,0x6000);expected=bytearray(before);struct.pack_into('<I',expected,arg-storage,masked)
    if ispc and same and name in ('Action','OpenGate'):struct.pack_into('<I',expected,move-storage+4,0)
    if ispc and not same and name in ('MikaelOpenGate','WandringNPCWait'):expected[0x3c]=1
    lookups=[]
    def observe(u,a,n,user):
        if a==0x59a5e0:
            assert p.reg('ECX')==manager;arguments=[p.uint(p.reg('ESP')+i) for i in (4,8)]
            assert arguments==[masked,profile];lookups.append(arguments)
    if ispc:
        p.mu.hook_add(p.uc.UC_HOOK_CODE,observe);entry=int(row['pcEntry'],16)
        p.run(entry,obj,(arg,),stop_at=None if same else 0x512ea0)
        assert lookups==[[masked,profile]]
        result=dict(entry=row['pcEntry'],stop=f'{p.reg("EIP"):08X}',completion='original return' if same else 'original playback entry;guest discarded',blocks=sum(p.visits.values()),lookupArguments=lookups[0])
        if not same:
            mode=0 if name in MODE_ZERO else 1
            if name=='IceWormAttack':mode=int(((masked>>7)&255) not in (8,10))
            arguments=[p.uint(p.reg('ESP')+i) for i in (4,8,12)]
            assert p.reg('ECX')==obj and arguments==[HANDLE,mode,1],arguments
            result['playbackArguments']=arguments
    else:
        p.reg('A0',obj);p.reg('A1',arg)
        for r,v in row['ps2InitialConstants'].items():p.reg(r,v)
        result=p.run(int(row['ps2Start'],16),[0x273d10],timeout_us=500000)
        arguments=[p.reg(r)&0xffffffff for r in ('A0','A1','A2')]
        assert arguments==[manager,masked,profile],arguments
        result.update(originalEntry=row['ps2Entry'],lookupArguments=arguments[1:],initialConstants=row['ps2InitialConstants'])
    after=read(storage,0x6000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:16]
    result.update(inputPacked=c['packed'],outputPacked=masked,borrowedStorageBytes=0x6000,
        beforeSha256=hashlib.sha256(before).hexdigest().upper(),afterSha256=hashlib.sha256(after).hexdigest().upper(),changes=[dict(offset=i,before=a,after=b) for i,(a,b) in enumerate(zip(before,after)) if a!=b])
    return result


def guest(output,selection):
    output=Path(output).resolve();selection=Path(selection).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local evidence required')
    cases=json.loads(selection.read_text())['cases'];assert 0<len(cases)<=150
    rows={r['className']:r for r in json.loads(SELECTION.read_text())['classes']};started=time.perf_counter()
    report=dict(kind='original-character-state-animation-requests',status='running',inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),selectionSha256=hashlib.sha256(selection.read_bytes()).hexdigest().upper(),
        contextSha256=hashlib.sha256(SELECTION.read_bytes()).hexdigest().upper(),cases=[])
    output.parent.mkdir(parents=True,exist_ok=True)
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for c in cases:
        report['pending']=c;save()
        try:result=dict(input=c,status='passed',**execute(rows[c['className']],c))
        except Exception as error:result=dict(input=c,status='blocked',error=str(error),traceback=traceback.format_exc())
        report['cases'].append(result);report.pop('pending');save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='passed' if not failed else 'partial';save()
    print(json.dumps(dict(status=report['status'],cases=len(cases),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
