#!/usr/bin/env python3
"""Original GameFlowState navigation over explicit borrowed HUD/input records.

Records are literal method inputs, not claimed original HUD/widget factories.
Only CRT strstr and widget virtual effect observations are supplied; navigation,
HUD lookup, selection helper, timer and null-target message dispatch execute.
"""
from pathlib import Path
import hashlib,json,struct,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_task_timer import TimerFixture
from pc_stl_fixtures import read_cstring


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Local output required')
    output.parent.mkdir(parents=True,exist_ok=True)
    started=time.perf_counter();f=TimerFixture();p=f.p;f.time(1200)
    report=dict(kind='original-pc-game-flow-navigation',status='running',cases=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        executableSha256=hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper(),
        profile='micro:100000 instructions/2s each original call;30s child',
        scope='Original game flow over explicit borrowed records, not HUD/UserInput/widget startup or rendering',
        boundaries=['Existing TimerFixture allocator/SEH/clock boundaries',
            'MSVCR71 strstr at IAT6D932C: bounded byte-string search; import name read from pristine PE',
            'Borrowed widget vslots2C/30 only record effects, do not implement or mutate native widget state',
            'Explicit null event receiver at borrowed game global+2B4; original null-target dispatch executes'])
    def data(n):
        a=p.allocate(n);p.mu.mem_write(a,bytes(n));return a
    def string(value):
        b=value.encode('ascii')+b'\0';a=data(len(b));p.mu.mem_write(a,b);return a
    input_owner=data(0x24);input_state=data(16)
    p.put_uint(input_owner+0x18,input_state);p.put_uint(0x7552a0,input_owner)
    hud=data(0x140);window=data(0x150);game=data(0x2b8);view=3
    p.put_uint(0x755284,hud);p.put_uint(hud+0x18+view*4,window);p.put_uint(0x765ad4,game)
    callbacks=0x34140000;p.mu.mem_map(callbacks,4096,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
    effects=[];searches=[]
    def strstr(m):
        a=m.uint(m.reg('ESP')+4);b=m.uint(m.reg('ESP')+8)
        hay,needle=read_cstring(m,a,256),read_cstring(m,b,256);index=hay.find(needle)
        searches.append(dict(haystack=hay.decode('ascii'),needle=needle.decode('ascii'),index=index))
        m.fixture_return(eax=a+index if index>=0 else 0)
    def state(m):
        effects.append(dict(kind='state',this=m.reg('ECX'),value=m.uint(m.reg('ESP')+4)));m.fixture_return(4)
    def refresh(m):
        effects.append(dict(kind='refresh',this=m.reg('ECX')));m.fixture_return()
    for offset,fn in ((0x10,strstr),(0x20,state),(0x30,refresh)):p.seams[callbacks+offset]=fn
    p.put_uint(0x6d932c,callbacks+0x10)
    vtable=data(0x34);p.put_uint(vtable+0x30,callbacks+0x20);p.put_uint(vtable+0x2c,callbacks+0x30)
    names=['start','d0','d1','d2','d3'];widgets=[]
    for name in names:
        a=data(0x1a4);widgets.append(a);p.put_uint(a,vtable);p.put_uint(a+0x54,string(name));p.mu.mem_write(a+0x28,b'\x01')
    links=(0xa4,0xe4,0x124,0x164);array=data(20)
    for i,a in enumerate(widgets):p.put_uint(array+4*i,a)
    def save():
        report.update(seconds=time.perf_counter()-started,arenaBytes=p.allocated,
            allocations=[dict(address=a,bytes=n,freed=a in f.freed) for a,n in f.allocations.items()],
            lastIp=f'{p.reg("EIP"):08X}',lastTail=[f'{a:08X}' for a in p.tail])
        output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    obj=0
    try:
        obj=f.call(0x5d6dc0);timer=p.uint(obj+0x2c)
        report['objects']=dict(state=obj,timer=timer,borrowedInput=input_state,borrowedHUD=hud,borrowedWindow=window,widgets=widgets)
        report['factoryBytes']=bytes(p.mu.mem_read(obj,0x3c)).hex();save()
        def frame(label,angle_bits=0,magnitude_bits=0x3f800000,elapsed=1000,deadline=0,released=1,mode='directions',expect=None):
            effects.clear();searches.clear();p.mu.mem_write(input_state+4,struct.pack('<II',magnitude_bits,angle_bits))
            f.time((1200+elapsed)&0xffffffff);p.put_uint(obj+0x34,deadline);p.mu.mem_write(obj+0x38,bytes([released]))
            p.put_uint(window+0x134,widgets[0]);p.put_uint(window+0x138,widgets[0])
            p.put_uint(window+0x128,array);p.put_uint(window+0x12c,array+(4 if mode=='self-cycle' else 20))
            for i,off in enumerate(links):
                value=names[i+1] if mode=='directions' else {'self-cycle':'start','empty-substring':'xemptyx','missing':'absent','empty':'empty'}[mode]
                p.mu.mem_write(widgets[0]+off,(value.encode()+b'\0').ljust(0x40,b'\0'))
            p.mu.mem_write(widgets[0]+0x28,b'\0' if mode=='self-cycle' else b'\x01')
            before=bytes(p.mu.mem_read(obj,0x3c));f.call(0x5d6e70,obj,(view,));after=bytes(p.mu.mem_read(obj,0x3c))
            selected=p.uint(window+0x138);current=p.uint(timer+0x1c)
            row=dict(label=label,angleBits=f'{angle_bits:08X}',magnitudeBits=f'{magnitude_bits:08X}',elapsed=elapsed,
                oldDeadline=deadline,oldReleased=released,mode=mode,before=before.hex(),after=after.hex(),
                selected=names[widgets.index(selected)],deadline=p.uint(obj+0x34),released=bytes(p.mu.mem_read(obj+0x38,1))[0],
                timerCurrent=current,effects=list(effects),searches=list(searches),instructions=sum(p.visits.values()),
                helperCalls=p.visits.get(0x5d6be0,0),lookupCalls=p.visits.get(0x55d3f0,0))
            report['cases'].append(row);save()
            assert current==elapsed
            if expect:
                for k,v in expect.items():assert row[k]==v,(label,k,row[k],v)
            return row
        for bits in (0,0x3effffff,0x3f000000,0x7fc00000):
            frame('released-magnitude',magnitude_bits=bits,expect=dict(selected='start',released=1,helperCalls=0))
        for elapsed in (999,1000):frame('repeat-gate',elapsed=elapsed,deadline=1000,released=0,expect=dict(selected='start',released=0,helperCalls=0))
        for label,elapsed,deadline,released in [('repeat-after',1001,1000,0),('fresh-input',900,1000,1),('zero-deadline',900,0,0),('deadline-wrap',0xffffff00,0,1)]:
            frame(label,elapsed=elapsed,deadline=deadline,released=released,expect=dict(selected='d0',released=0,deadline=(elapsed+500)&0xffffffff))
        q=struct.unpack('<f',bytes.fromhex('db0f493f'))[0]
        for bits in (0,0x3f490fda,0x3f490fdb,0x3f490fdc,0xbf490fda,0xbf490fdb,0xbf490fdc,
                     0x4016cbe3,0x4016cbe4,0x4016cbe5,0xc016cbe3,0xc016cbe4,0xc016cbe5,0x40490fdb,0xc0490fdb):
            angle=struct.unpack('<f',struct.pack('<I',bits))[0]
            direction=0 if -q<=angle<=q else 1 if angle<=-3*q or angle>=3*q else 2 if -3*q<=angle<=-q else 3
            frame('direction-boundary',angle_bits=bits,expect=dict(selected='d'+str(direction),released=0,deadline=1500))
        for mode in ('empty','empty-substring','missing'):
            frame('link-stop',mode=mode,expect=dict(selected='start',deadline=0,released=0,helperCalls=1 if mode=='missing' else 0))
        frame('unselectable-self-cycle',mode='self-cycle',expect=dict(selected='start',deadline=0,released=0,helperCalls=20,lookupCalls=40))
        save();f.call(0x5d6bc0,obj,(1,));assert set(f.allocations)==set(f.freed)
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',errorType=type(error).__name__,error=str(error));save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',cases=len(report['cases']),seconds=report['seconds'],freed=len(f.freed))));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(args[1]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
