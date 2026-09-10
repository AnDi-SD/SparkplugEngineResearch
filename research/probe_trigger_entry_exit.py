#!/usr/bin/env python3
"""Original trigger entry/exit and empty HUD-window state, with explicit boundaries."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix
FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/trigger-entry-exit'
def sha(v):return hashlib.sha256(v).hexdigest().upper()
def bits(v):return struct.unpack('<I',struct.pack('<f',v))[0]

def execute(c):
    pc=c['platform']=='pc';kind=c['kind'];cls=c['className'];stage=c.get('stage','head')
    if pc:
        p=PcBlocks();base=p.allocate(0xa000);write=lambda a,b:p.mu.mem_write(a,bytes(b));read=lambda a,n:bytes(p.mu.mem_read(a,n));put=p.put_uint
    else:
        p=Ps2ScalarPrefix([(0x3ac730,0x100),(0x3b3c10,0xc0),(0x3b5a00,0xb0),(0x379470,0x10),(0x3794d0,0x10),(0x37e7a4,0x188)])
        base=0x21000000;p.map(base,0xa000);p.map(0x49f000,4096);p.map(0x22000000,4096);p.reg('GP',0x4a4170);p.reg('SP',0x22000800)
        write,read,put=p.write,p.read,p.put_uint
    obj,profile,hud,window,node,modal,widget,flow,player=base,base+0x1000,base+0x4000,base+0x5000,base+0x6000,base+0x7000,base+0x8000,base+0x9000,base+0x9400
    write(base,b'\xa5'*0xa000);put(obj,int(c['vtable'],16));put(0x765ad4 if pc else 0x49fc7c,profile);put(profile+0x2b4,player)
    put(0x755284 if pc else 0x49fda4,hud);put(hud+0x60,window);put(hud+0x6c,modal if c['modalPresent'] else 0);put(hud+0x294,widget if c['cachedWidget'] else 0)
    put(0x755294 if pc else 0x49fd4c,flow);put(flow+0x1b0,c['gameState'])
    put(window+0x18,node);put(window+(0x13c if pc else 0x144),c['windowType']);put(window+(0x140 if pc else 0x148),bits(c['height']))
    write(window+(0x144 if pc else 0x14c),bytes([c['hudVisible']]));write(modal+(0x144 if pc else 0x14c),bytes([c['modalVisible']]))
    put(window+(0x128 if pc else 0x130),0);put(window+(0x12c if pc else 0x134),0);put(node+(0xb0 if pc else 0xb4),c['dirty'])
    shift=0 if pc else 12
    put(obj+0x130+shift,c['tag']);write(obj+0x125+shift,bytes([c['single'],c['active'],c['fired']]))
    write(obj+0x138+shift,bytes([c['suppressed']]));write(obj+0x144+shift,bytes([c['requested']]))
    write(obj+(0x188 if pc else 0x1a0),bytes([c['bypass']]));put(profile+0x504,c['profile504'])
    before=read(base,0xa000);expected=bytearray(before)
    def byte(a,v):expected[a-base]=v
    def word(a,v):struct.pack_into('<I',expected,a-base,v&0xffffffff)
    def state_effect(visible):
        byte(window+(0x144 if pc else 0x14c),visible);word(node+0x20,0);word(node+0x24,0);word(node+0x28,bits(c['height'])^(0 if visible else 0x80000000));word(node+(0xb0 if pc else 0xb4),c['dirty']|1)
    eligible=bool(c['tag'] and not(c['single'] and c['fired']) and not c['hudVisible'] and not c['suppressed'])
    show_allowed=c['gameState'] not in (70,71,72) and not(c['modalPresent'] and c['modalVisible'])
    boundary=None;message=None
    if pc:
        if kind=='window':
            entry=0x55d1c0;this=window;args=(c['visible'],);stop=0x435d30 if c['windowType']==18 and c['visible'] else None
            if stop:boundary='window-message';message=[player,0x27d1,6,0]
            else:state_effect(c['visible'])
        else:
            this=obj;args=();stop=None
            if kind=='leave':
                entry=0x590420 if cls=='wxGenericTrigger' else 0x53d360;state_effect(0);byte(obj+0x144,0);byte(obj+0x138,0)
                if cls!='wxGenericTrigger':byte(obj+0x126,0);word(profile+0x504,0)
            else:
                entry={'wxGenericTrigger':0x5903b0,'wxPivotingDoor':0x539bc0,'wxPushButton':0x53b130}[cls]
                call_parent=cls!='wxPivotingDoor' or not c['bypass']
                if call_parent and eligible:
                    if not c['cachedWidget']:stop=0x55aedd;boundary='uncached-widget-lookup'
                    elif show_allowed:state_effect(1);stop=0x55af40;boundary='cached-widget-operation'
                    if stop is None:byte(obj+0x144,1)
                if stop is None and cls!='wxGenericTrigger':byte(obj+0x126,1);word(profile+0x504,8 if cls=='wxPivotingDoor' else 16)
        p.run(entry,this=this,args=args,stop_at=stop)
        if boundary=='window-message':assert p.reg('ECX')==window and [p.uint(p.reg('ESP')+i*4) for i in range(1,5)]==message
        if boundary=='cached-widget-operation':assert p.reg('ECX')==widget
        result=dict(entry=f'{entry:08X}',stop=f'{p.reg("EIP"):08X}',boundary=boundary,completion='whole original PC return' if stop is None else 'original '+boundary+' boundary;guest discarded',messageArguments=message,blocks=sum(p.visits.values()))
    else:
        regs=dict(A0=obj)
        if kind=='window':
            # Original37E794 sets V1=18 before the omitted SQ frame stores.
            entry=0x37e7a4;regs=dict(A0=window,A1=c['visible'],V1=18);stop=0x37e818 if c['windowType']==18 and c['visible'] else 0x37e888
            if stop==0x37e818:boundary='window-message';message=[0x27d1,0,0,0,window,0,6,0]
            else:state_effect(c['visible'])
        elif cls=='wxGenericTrigger':
            if kind=='enter':entry=0x3ac78c;stop=0x3794e0 if eligible else 0x3ac81c;boundary='HUD-enter' if eligible else None
            elif stage=='after-HUD':entry=0x3ac750;regs=dict(S0=obj);stop=0x3ac76c;byte(obj+0x150,0);byte(obj+0x144,0)
            else:entry=0x3ac73c;stop=0x37e790;boundary='window-state'
        elif kind=='enter':
            if cls=='wxPivotingDoor':
                entry=0x3b3c78 if stage=='after-parent' else 0x3b3c6c
                if stage=='after-parent':regs=dict(S0=obj)
                done=stage=='after-parent' or c['bypass'];stop=0x3b3cbc if done else 0x3ac780
            else:
                entry=0x3b5a64 if stage=='after-parent' else 0x3b5a5c
                if stage=='after-parent':regs=dict(S0=obj)
                done=stage=='after-parent';stop=0x3b5a98 if done else 0x3ac780
            if done:byte(obj+0x132,1);word(profile+0x504,8 if cls=='wxPivotingDoor' else 16)
            else:boundary='parent-enter'
        else:
            if cls=='wxPivotingDoor':entry=0x3b3c24 if stage=='after-parent' else 0x3b3c1c;stop=0x3b3c50 if stage=='after-parent' else 0x3ac730
            else:entry=0x3b5a14 if stage=='after-parent' else 0x3b5a0c;stop=0x3b5a40 if stage=='after-parent' else 0x3ac730
            if stage=='after-parent':regs=dict(S0=obj);byte(obj+0x132,0);word(profile+0x504,0)
            else:boundary='parent-leave'
        for name,value in regs.items():p.reg(name,value)
        original=[read(a,n) for a,n in p.ranges];result=p.run(entry,[stop],timeout_us=500000);assert original==[read(a,n) for a,n in p.ranges]
        if boundary=='window-message':assert p.reg('A0')==player and [p.uint(p.reg('SP')+0x30+i*4) for i in range(8)]==message
        if boundary in ('parent-enter','parent-leave'):assert p.reg('A0')==obj
        if boundary=='HUD-enter':assert p.reg('A0')==hud
        if boundary=='window-state':assert p.reg('A0')==window and p.reg('A1')==0
        result.update(boundary=boundary,initialRegisters=regs,messageEnvelope=message,completion='independent original PS2 component;SQ frames/parent/HUD/virtual bodies not silently joined')
    after=read(base,0xa000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:14]
    result.update(guardedBytes=0xa000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),effects=[dict(offset=f'{i:X}',before=a,after=b) for i,(a,b) in enumerate(zip(before,after)) if a!=b],scope='Borrowed cached profile/HUD/window/Node and empty widget list;no game UI startup or rendering. Actual PC handlers,explicit PS2 components and pre-virtual boundaries. No substitute game return.')
    return result

def guest(output,selection='pilot'):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Fresh local result')
    if selection not in ('pilot','batch','repair-window'):raise ValueError('Explicit selection')
    rows=[c for c in json.loads((FOLDER/'entry-exit-cases.json').read_text()) if (c['pilot'] and c['platform']=='ps2' and c['kind']=='window') if selection=='repair-window'] if selection=='repair-window' else [c for c in json.loads((FOLDER/'entry-exit-cases.json').read_text()) if c['pilot']==(selection=='pilot')]
    report=dict(kind='original-trigger-entry-exit',status='running',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha((FOLDER/'entry-exit-cases.json').read_bytes()),cases=[]);started=time.perf_counter()
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for c in rows:
        row=dict(input=c);report['cases'].append(row);save()
        try:row.update(status='passed',**execute(c))
        except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
        save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='partial' if failed else 'passed';save();print(json.dumps(dict(status=report['status'],cases=len(rows),failed=failed,seconds=report['seconds'])));return int(failed!=0)

if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
