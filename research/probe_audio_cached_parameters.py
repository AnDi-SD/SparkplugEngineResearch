"""Original audio cache/parameter methods and explicit device-call boundaries."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER
FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/audio-runtime'
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def bits(f):return struct.unpack('<I',struct.pack('<f',f))[0]

def execute(c,k):
    pc=k=='pc';kind=c['kind'];voice=kind=='voice';table=(0x6f321c if voice else 0x6f26d8) if pc else (0x491230 if voice else 0x4911b0)
    if pc:
        p=PcBlocks();base=p.allocate(4096);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b))
    else:
        p=Ps2ScalarPrefix([(0x1ec630,0x88),(0x1eda40,0xb8),(0x1ee360,0x24),(0x1de410,0x2c),(0x1ed820,8)],stack_window=(0x22000000,4096),upper64=UPPER)
        base=0x21000000;p.map(base,4096);p.map(0x22000000,4096);p.reg('SP',0x22000800);read,write=p.read,p.write
        raw,sections=pristine();p.map(table,0x90);write(table,read_window('ps2',raw,table,0x90,sections)[0]);p.map(0x4b0670,48)
    put=p.put_uint;write(base,bytes([c['fill']])*4096);put(base,table);write(base+0x14,bytes([c['active']]));put(base+0xf0,base+0x500);put(base+0x500,base+0x600)
    if voice:
        if pc:put(base+0x28,0)
        else:put(base+0x10,c['voiceIndex']);write(0x4b0670,bytes([c['voiceDecoy']])*48)
        if not pc and 0<=c['voiceIndex']<48:write(0x4b0670+c['voiceIndex'],bytes([c['voiceState']]))
    before=read(base,4096);expected=bytearray(before);expected_return=None;stop=None;args=();device_args=None
    if kind=='parameters':
        entry=0x4c4160 if pc else 0x1ec630;args=(bits(c['first']),bits(c['second']));first=min(1.,max(0.,c['first']));second=min(1.,max(0.,c['second']))
        if pc:struct.pack_into('<II',expected,0xcc,bits(second),bits(first));expected[0x154]=1
        else:struct.pack_into('<II',expected,0xbc,bits(second),bits(struct.unpack('<f',struct.pack('<I',0x3d4ccccd))[0]*first));expected[0x130]=1
    elif kind=='flag':
        assert pc;entry=0x4c4140;args=(c['flag'],);expected[0xc8]=c['flag']&255;expected[0x154]=1
    elif kind=='capacity':entry=0x4c3e50 if pc else 0x1ed820;expected_return=32 if pc else 48
    elif kind=='voice':
        entry=0x4ccc70 if pc else 0x1ee360;expected_return=0 if pc else int(0<=c['voiceIndex']<48 and bool(c['voiceState']))
    elif kind=='volume':
        entry=0x4c3df0 if pc else 0x1eda40;args=(bits(c['value']),)
        if c['active']:
            struct.pack_into('<I',expected,0x18,bits(c['value']));stop=(0x4c3e47 if c['value']>0 else 0x4c3e21) if pc else 0x1df400
    else:raise ValueError(kind)
    if pc:
        p.run(entry,this=base,args=args,stop_at=stop);value=p.reg('EAX')&(255 if voice else 0xffffffff)
        result=dict(entry=f'{entry:08X}',completion='declared original prefix boundary;guest discarded' if stop else 'original return',blocks=sum(p.visits.values()))
        if stop:
            result['stop']=f'{stop:08X}';sp=p.reg('ESP');device_args=[p.uint(sp),p.uint(sp+4)];assert device_args==[base+0x500,c['expectedPcDb']&0xffffffff],device_args
    else:
        p.reg('A0',base)
        if kind=='parameters':p.reg('F12',args[0]);p.reg('F13',args[1])
        elif kind=='volume':p.reg('F12',args[0])
        original=[read(a,n) for a,n in p.ranges];saved={r:p.reg(r) for r in ('S0','S1','S2','SP','RA')};sdk_before=read(0x4b0670,48)
        result=p.run(entry,[stop or p.RETURN],count=6000,timeout_us=500000);value=p.reg('V0')&0xffffffff
        assert original==[read(a,n) for a,n in p.ranges] and sdk_before==read(0x4b0670,48)
        if stop:device_args=[p.reg(r)&0xffffffff for r in ('A0','A1','A2')];assert device_args==[0,c['expectedPs2Level'],c['expectedPs2Level']]
        else:assert saved=={r:p.reg(r) for r in saved} and result['initialUpper64']==result['finalUpper64']
    if expected_return is not None:assert value==expected_return,(kind,value,expected_return)
    after=read(base,4096);assert after==bytes(expected),[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:12]
    result.update(returnValue=value if expected_return is not None else None,deviceArguments=device_args,guardedBytes=4096,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),scope='Literal original audio objects/cache inputs.Original PS2 voice state helper reads bounded48-byte array.Original parameter clamp methods.No OS/device call completion;volume active paths stop before COM call or SDK entry.No guessed audible setting names or game callback results.')
    return result

def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'parameter-cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];start=time.perf_counter();report=dict(kind='original-audio-cached-parameters',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[])
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
