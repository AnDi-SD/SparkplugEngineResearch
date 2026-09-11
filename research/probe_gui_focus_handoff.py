"""Original GUIManager focus order, real Notify handlers and refresh boundaries."""
import hashlib,json,sys,time,traceback,struct
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/gui-focus'
CATALOG={c['className']:c for c in json.loads((ROOT/'research/gui-object-contracts-2026-09-10.json').read_text())['classes']}
def sha(b):return hashlib.sha256(b).hexdigest().upper()


def execute(c,k):
    pc=k=='pc';calls=[]
    if pc:
        p=PcBlocks();base=p.allocate(0x4000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu
    else:
        p=Ps2ScalarPrefix([(0x163fd0,0xe8),(0x169b80,0x72c),(0x16be70,0x274),(0x1655d0,0x4c)],stack_window=(0x22000000,4096),upper64=UPPER)
        base=0x21000000;p.map(base,0x4000);p.map(0x22000000,4096);read,write,u=p.read,p.write,p.u
        raw,sections=pristine()
        for name in set(c['types']):
            a=int(CATALOG[name]['ps2']['vtable'],16);p.map(a,56);write(a,read_window('ps2',raw,a,56,sections)[0])
        p.map(0x4772c0,60);write(0x4772c0,read_window('ps2',raw,0x4772c0,60,sections)[0]);p.reg('SP',0x22000800)
        for i in (16,17):p.reg(str(i),0x12340000+i)
    manager=base;objects=[base+0x1000,base+0x2000];write(base,b'\xa5'*0x4000);put=p.put_uint
    old=0 if c['old'] is None else objects[c['old']];new=0 if c['new'] is None else objects[c['new']];put(manager+0x4c,old)
    for i,a in enumerate(objects):
        put(a,int(CATALOG[c['types'][i]][k]['vtable'],16));write(a+0x28,bytes([c['enabled'][i]]));write(a+(0x3d if pc else 0x39),bytes([c['highlight'][i]]))
    before=read(base,0x4000);expected=bytearray(before);expected_calls=[];boundary=None;current=old
    for index,code in [(c['old'],0x17),(c['new'],0x16)]:
        if code==0x16:
            current=new;struct.pack_into('<I',expected,0x4c,new)
        if index is None:continue
        expected_calls.append(dict(receiver=index,message=[code,0,0,0,manager,0,0,0],focusAtEntry=current))
        if c['types'][index]=='spEditBox' and c['enabled'][index]:
            expected[objects[index]-base+(0x3d if pc else 0x39)]=int(code==0x16);boundary='old-refresh' if code==0x17 else 'new-refresh';break
    entries={objects[i]:int(CATALOG[t][k]['slots'][1],16) for i,t in enumerate(c['types'])}
    def observe(machine,address,size,user):
        receiver=p.reg('ECX' if pc else 'A0')
        if receiver in entries and entries[receiver]==address:
            arg=p.uint(p.reg('ESP')+4) if pc else p.reg('A1')
            calls.append(dict(receiver=objects.index(receiver),message=[p.uint(arg+i*4) for i in range(8)],focusAtEntry=p.uint(manager+0x4c)))
    hooks=[u.hook_add((p.uc if pc else __import__('unicorn')).UC_HOOK_CODE,observe,begin=a,end=a) for a in set(entries.values())]
    stop=(0x4369a0 if pc else 0x169820) if boundary else None
    if pc:
        p.run(0x451ad0,this=manager,args=(new,),stop_at=stop);result=dict(entry='00451AD0',stop=f'{p.reg("EIP"):08X}',completion='original return' if stop is None else 'original EditBox refresh boundary;guest discarded',blocks=sum(p.visits.values()))
    else:
        p.reg('A0',manager);p.reg('A1',new);original=[p.read(a,n) for a,n in p.ranges];result=p.run(0x163fd0,[stop or p.RETURN],timeout_us=500000);assert original==[p.read(a,n) for a,n in p.ranges]
        if stop is None:assert [p.reg(n) for n in ('SP','RA','S0','S1')]==[0x22000800,p.RETURN,0x12340010,0x12340011]
    for h in hooks:u.hook_del(h)
    assert calls==expected_calls,(calls,expected_calls)
    if boundary:assert p.reg('ECX' if pc else 'A0')==objects[c['old'] if boundary=='old-refresh' else c['new']]
    after=read(base,0x4000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:12]
    result.update(boundary=boundary,notifications=calls,finalFocus=p.uint(manager+0x4c),guardedBytes=0x4000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)),scope='Direct original GUIManager focus method over literal manager/GUI records. Real class Notify bodies execute;enabled EditBox stops only at actual refresh after highlight write. No substitute Notify return,manager startup or subsequent refresh completion.')
    return result


def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'focus-cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cs=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];report=dict(kind='original-gui-focus-notification-order',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[]);start=time.perf_counter()
    for c in cs:
        for k in ('pc','ps2'):
            row=dict(input=c,platform=k);report['cases'].append(row)
            try:row.update(status='passed',**execute(c,k))
            except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
            report['seconds']=time.perf_counter()-start;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
            if row['status']!='passed':break
        if row['status']!='passed':break
    report['status']='passed' if len(report['cases'])==2*len(cs) and all(c['status']=='passed' for c in report['cases']) else 'blocked'
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(status=report['status'],cases=len(report['cases']),seconds=report['seconds'],errors=[c['error'] for c in report['cases'] if 'error' in c])))
    return int(report['status']!='passed')


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
