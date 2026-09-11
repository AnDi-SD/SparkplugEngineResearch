"""Original light update and bounded nondeleting reference transactions."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER
FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/light-runtime'

def sha(b):return hashlib.sha256(b).hexdigest().upper()
def bits(f):return struct.unpack('<I',struct.pack('<f',f))[0]
def f32(f):return struct.unpack('<f',struct.pack('<f',f))[0]

def execute(c,k):
    pc=k=='pc';kind=c['kind'];controller=kind=='controller'
    if pc:
        p=PcBlocks();base=p.allocate(4096);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu;uc=p.uc
    else:
        p=Ps2ScalarPrefix([(0x11a170,0x188),(0x11c470,0x490),(0x11caa0,0x2c),(0x11cd3c,0x3c),(0x150b50,0xb0),(0x14e150,0x88)],stack_window=(0x22000000,4096),upper64=UPPER)
        base=0x21000000;p.map(base,4096);p.map(0x22000000,4096);p.reg('SP',0x22000800);p.reg('F20',bits(13.5));read,write=p.read,p.write;u=p.u
        import unicorn as uc
        raw,sections=pristine()
        for a,n in [(0x48db40,0x90),(0x48dbd0,0x30),(0x48cd90,0x90)]:p.map(a,n);write(a,read_window('ps2',raw,a,n,sections)[0])
    put=p.put_uint;write(base,bytes([c['fill']])*4096);target=base+0x400;refs=[0,base+0x800,base+0x900]
    put(base,(0x6de664 if controller else 0x6de714) if pc else (0x48dbd0 if controller else 0x48db40))
    args=();calls=[];packed=[]
    if controller:
        put(base+0x10,c['enabled']);put(base+0x1c,0x6de684 if pc else 0x48cd90);put(base+0x34,0x6ea9ac if pc else 0x48cdf0)
        put(base+0x2c,c['color1']);put(base+0x30,c['color2']);put(base+0x44,bits(.125));put(base+0x58,bits(c['value']));put(base+0x68,c['functionType']);put(base+0x6c,target if c['target'] else 0)
        # The direct method only writes this buffer's proven fields; no guessed PS2 Light vtable.
        put(target,0);put(target+(0xb0 if pc else 0xb4),c['flags']);entry=0x42f930 if pc else 0x11a170;args=(bits(.25),)
        observed={0x478990:'color',0x478680:'function'} if pc else {0x11c470:'color',0x11caa0:'function'}
        for a,name in observed.items():
            def observe(u,address,size,user,name=name):calls.append(name)
            u.hook_add(uc.UC_HOOK_CODE,observe,begin=a,end=a)
        after_color=0x42f950 if pc else 0x11a1a4
        def capture(u,address,size,user):packed.append(p.uint(p.reg('EAX') if pc else p.reg('SP')+0x2c))
        u.hook_add(uc.UC_HOOK_CODE,capture,begin=after_color,end=after_color)
    else:
        for i,count in enumerate((c['ref1'],c['ref2']),1):write(refs[i]+8,struct.pack('<H',count))
        put(base+0x14,refs[c['release']]);put(base+0x18,refs[c['old']])
        for a in (0x30,0x40,0x50):put(base+a,0)
        entry=(0x431050 if pc else 0x150b50) if kind=='release' else (0x419d00 if pc else 0x14e150)
        if kind=='bind':args=(refs[c['new']],)
    before=read(base,4096);expected=bytearray(before);expect_packed=[]
    if controller and c['target']:
        w=max(0.,min(1.,(c['value']+1.)/2));channels=[]
        for shift in (0,8,16,24):
            a=((c['color1']>>shift)&255)*w;b=((c['color2']>>shift)&255)*(1-w)
            assert a==int(a) and b==int(b),'PS2 fractional conversion outside this experiment'
            channels.append((int(a)+int(b))&255)
        expect_packed=[sum(b<<(8*i) for i,b in enumerate(channels))]
        scale=struct.unpack('<f',read(0x6dca9c,4))[0] if pc else None
        values=[f32(channels[i]*scale if pc else channels[i]/255.) for i in (2,1,0,3)]
        struct.pack_into('<4f',expected,0x400+(0xc4 if pc else 0xd0),*values)
        struct.pack_into('<I',expected,0x400+(0xb0 if pc else 0xb4),c['flags']|8)
    elif not controller:
        if kind=='release':
            if c['release']:
                off=refs[c['release']]-base+8;count=struct.unpack_from('<H',expected,off)[0];assert count>1;struct.pack_into('<H',expected,off,count-1)
            struct.pack_into('<I',expected,0x14,0)
        elif c['old']!=c['new']:
            for index,delta in [(c['old'],-1),(c['new'],1)]:
                if index:
                    off=refs[index]-base+8;count=struct.unpack_from('<H',expected,off)[0];assert delta>0 or count>1;struct.pack_into('<H',expected,off,count+delta)
            struct.pack_into('<I',expected,0x18,refs[c['new']])
    if pc:
        p.run(entry,this=base,args=args);result=dict(entry=f'{entry:08X}',completion='original return',blocks=sum(p.visits.values()))
    else:
        p.reg('A0',base)
        if controller:p.reg('F12',args[0])
        elif kind=='bind':p.reg('A1',args[0])
        original=[read(a,n) for a,n in p.ranges];saved={r:p.reg(r) for r in ('S0','S1','S2','SP','RA','F20')}
        result=p.run(entry,[p.RETURN],count=6000,timeout_us=500000)
        assert original==[read(a,n) for a,n in p.ranges]
        assert saved=={r:p.reg(r) for r in saved} and result['initialUpper64']==result['finalUpper64']
    assert calls==(['color','function'] if controller and c['target'] else []),calls
    assert packed==expect_packed,(packed,expect_packed)
    after=read(base,4096);assert after==bytes(expected),[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:16]
    result.update(originalCallbacks=calls,packedColor=packed,guardedBytes=4096,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),scope='Literal proven fields/tables; actual ColorFuncEval and constant FunctionEval callbacks.No callback return seams.Light target is a layout buffer; no target factory/rendering claim.Reference cases avoid zero-after-decrement destruction and use null Node slots.')
    return result

def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];start=time.perf_counter();report=dict(kind='original-light-runtime',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[])
    for c in cases:
        for k in c['platforms']:
            row=dict(input=c,platform=k);report['cases'].append(row)
            try:row.update(status='passed',**execute(c,k))
            except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
            report['seconds']=time.perf_counter()-start;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
            if row['status']!='passed':break
        if row['status']!='passed':break
    report['status']='passed' if len(report['cases'])==sum(len(c['platforms']) for c in cases) and all(c['status']=='passed' for c in report['cases']) else 'blocked'
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(status=report['status'],cases=len(report['cases']),seconds=report['seconds'],errors=[c['error'] for c in report['cases'] if 'error' in c])))
    return int(report['status']!='passed')

if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
