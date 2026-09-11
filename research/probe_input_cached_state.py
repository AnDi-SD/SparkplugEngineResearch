"""Original cached keyboard/mouse queries; no OS polling or device callbacks."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/input-device-state'
TABLES={'keyboard':{'pc':(0x6f31d4,0x6f31b8),'ps2':(0x4915e0,0x491604)},'mouse':{'pc':(0x6f3138,0x6f311c),'ps2':(0x491660,0x491684)}}
def sha(b):return hashlib.sha256(b).hexdigest().upper()


def execute(c,k):
    pc=k=='pc';device=c['device'];kind=c['kind'];primary,interface=TABLES[device][k];mappings=dict(json.loads((FOLDER/'key-map-rows.json').read_text())['rows'])
    if pc:
        p=PcBlocks();base=p.allocate(0x6000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu
    else:
        ranges=[(0x1f1480,0x80),(0x1f16c0,0x70)] if device=='keyboard' else [(0x1f1750,0x1e0),(0x1f2030,0x70)]
        p=Ps2ScalarPrefix(ranges,stack_window=(0x22000000,4096),upper64=UPPER);base=0x21000000;p.map(base,0x6000);p.map(0x22000000,4096);read,write,u=p.read,p.write,p.u;p.reg('SP',0x22000800)
        raw,sections=pristine();p.map(primary,0x80);write(primary,read_window('ps2',raw,primary,0x80,sections)[0])
    put=p.put_uint;write(base,bytes([c['fill']])*0x6000);put(base,primary);put(base+0x14,interface);write(base+0x44,bytes([0 if pc else c['active']]));write(base+0x45,bytes([c['active'] if pc else 0]))
    if device=='keyboard' and pc:
        for i in range(256):write(base+0x858+i,bytes([c.get('decoy',c.get('state',0))]));write(base+0x958+i,bytes([c.get('decoy',c.get('changed',0))]))
        if 'decoy' in c:
            index=mappings[c['code']];write(base+0x858+index,bytes([c['state']]));write(base+0x958+index,bytes([c['changed']]))
        for i,(key,value) in enumerate(c.get('records',[])):
            assert 0<=key<256;put(base+0x88+20*i,key);put(base+0x8c+20*i,value)
    elif device=='mouse':
        for i in range(8):
            if pc:write(base+0x50a4+i,bytes([c['state']]));write(base+0x50b8+i,bytes([c['changed']]))
            else:put(base+0x50+4*i,c['state']);put(base+0xb0+4*i,c['changed'])
        if pc:
            for off,value in zip([0x5098,0x509c,0x50a0,0x50c0,0x50c4],[-3,4,-5,1234,567]):put(base+off,value)
        else:write(base+0x74,struct.pack('<3f',10.,20.,30.))
    before=read(base,0x6000);expected=bytearray(before);key=c.get('code',0);map_calls=[]
    if kind=='events':
        assert pc and device=='keyboard';entry=0x4cc980;receiver=base;args=(len(c['records']),);expected_value=1;bool_result=True
        for index,value in c['records']:expected[0x858+index]=int(bool(value&0x80));expected[0x958+index]=1
    elif kind=='initialize':
        assert not pc and device=='keyboard';entry=0x1f14f0;receiver=base;args=();expected[0x44]=1;expected_value=1;bool_result=True
    else:
        slot=c['slot'];entry=p.uint(interface+(0 if pc else 8)+4*slot);receiver=base+0x14;args=(key,);bool_result=slot in (1,2)
        if device=='keyboard':
            if pc and slot in (1,2):
                assert key in mappings;expected_value=int(bool(c['active']) and c['state' if slot==1 else 'changed']==1)
            else:expected_value=0
        elif slot in (1,2):expected_value=int(bool(c['active']) and 100<=key<=107 and c['state' if slot==1 else 'changed']==1)
        elif slot==3:expected_value=([1234,567] if pc else [10,20])[key-108] if c['active'] and key in (108,109) else 0
        elif slot==4:expected_value=([-3,4,-5] if pc else [10,20,30])[key-108] if c['active'] and key in (108,109,110) else 0
        else:raise ValueError('Reviewed slot1..4 only')
    def observe(machine,address,size,user):
        if address==0x4d6470:map_calls.append(p.uint(p.reg('ESP')+4))
    hook=u.hook_add(p.uc.UC_HOOK_CODE,observe,begin=0x4d6470,end=0x4d6470) if pc else None
    if pc:
        p.run(entry,this=receiver,args=args);value=p.reg('EAX')&(255 if bool_result else 0xffffffff);result=dict(entry=f'{entry:08X}',completion='original return',blocks=sum(p.visits.values()))
    else:
        p.reg('A0',receiver);p.reg('A1',key);original=[read(a,n) for a,n in p.ranges];result=p.run(entry,[p.RETURN],count=6000,timeout_us=500000);value=p.reg('V0')&(255 if bool_result else 0xffffffff);assert original==[read(a,n) for a,n in p.ranges]
        assert p.reg('SP')==0x22000800 and p.reg('RA')==p.RETURN and result['initialUpper64']==result['finalUpper64']
        if kind=='query':assert p.reg('A0')==base,'Original secondary-interface thunk must subtract14'
    if hook is not None:u.hook_del(hook)
    assert value==expected_value&0xffffffff,(value,expected_value)
    assert map_calls==([key] if pc and device=='keyboard' and kind=='query' and c['slot'] in (1,2) and c['active'] else [])
    after=read(base,0x6000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:12]
    result.update(value=value,returnScope='low8 Boolean only' if bool_result else 'low32 scalar result',keyMapCalls=map_calls,guardedBytes=0x6000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),interfaceOffset=0x14 if kind=='query' else None,
        scope='Literal cache/input records and original class/interface tables;no COM,OS polling or synthetic device callback.No normal constructor/startup claim.PS2 mouse conversions restricted to exact nonnegative small integers.')
    return result


def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/('routing-cases.json' if selection=='routing' else 'cases.json')
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch','routing'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if selection=='routing' or c['pilot']==(selection=='pilot')];start=time.perf_counter();report=dict(kind='original-cached-input-device-state',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[])
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
