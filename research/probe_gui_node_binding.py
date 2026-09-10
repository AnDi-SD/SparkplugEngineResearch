"""Original GUIObject node binding, retained nodes and saved controller flags."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_block_emulator import PcBlocks
from pc_instruction_emulator import run_bounded
from probe_pc_task_timer import TimerFixture
import probe_pc_animation_lifecycle as lifetime
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/gui-runtime'
def sha(b):return hashlib.sha256(b).hexdigest().upper()


def execute(c,platform):
    pc=platform=='pc'
    if pc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
        p=f.p;nodes=[f.call(0x421e20) for _ in range(2)];base=p.allocate(0x4000)
        read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu
    else:
        p=Ps2ScalarPrefix([(0x165330,0x298),(0x100820,0x40),(0x1163f0,8),(0x14dd40,0x10)],stack_window=(0x22000000,4096),upper64=UPPER)
        base=0x21000000;nodes=[base+0x2000,base+0x2800];p.map(base,0x4000);p.map(0x22000000,4096)
        raw,sections=pristine();p.map(0x4902f0,0x40);p.write(0x4902f0,read_window('ps2',raw,0x4902f0,0x40,sections)[0])
        read,write,u=p.read,p.write,p.u;p.reg('SP',0x22000800)
        for i in range(16,21):p.reg(str(i),0x123450000+i)
    write(base,b'\xa5'*0x4000);obj=base;save=base+0x400;arrays=[base+0x800,base+0xa00];controls=[[base+0x1000+j*0x200+i*0x40 for i in range(4)] for j in range(2)]
    put=p.put_uint;put(obj,0x6dcc70 if pc else 0x48e560);put(obj+0x18,nodes[c['initial']] if c['initial'] is not None else 0)
    if pc:put(obj+0x2c,0);put(obj+0x30,save);put(obj+0x34,save+4*len(c['saved']));put(obj+0x38,save+64)
    else:put(obj+0x2c,16);put(obj+0x30,len(c['saved']));put(obj+0x34,save)
    for i,v in enumerate(c['saved']):put(save+4*i,v)
    for j,node in enumerate(nodes):
        if not pc:
            put(node,0x4902f0);put(node+4,0);put(node+0x18,node+0x18);put(node+0x1c,node+0x18)
        write(node+8,c['references'][j].to_bytes(2,'little'))
        for i,v in enumerate(c['flags'][j]):put(controls[j][i]+0x18,v);put(arrays[j]+4*i,controls[j][i])
        if pc:put(node+0x68,arrays[j]);put(node+0x6c,arrays[j]+4*len(c['flags'][j]));put(node+0x70,arrays[j]+16)
        else:put(node+0x64,4);put(node+0x68,len(c['flags'][j]));put(node+0x6c,arrays[j])
    expected_flags=[list(v) for v in c['flags']];expected_saved=list(c['saved']);expected_refs=list(c['references']);current=c['initial'];events=[]
    def observe(machine,access,address,size,value,user):
        for j,cc in enumerate(controls):
            for i,a in enumerate(cc):
                if address==a+0x18:events.append(dict(node=j,index=i,size=size,value=value))
    hook=u.hook_add((p.uc if pc else __import__('unicorn')).UC_HOOK_MEM_WRITE,observe,begin=controls[0][0]+0x18,end=controls[1][-1]+0x18)
    outputs=[]
    for target in c['targets']:
        before=read(base,0x4000);expected=bytearray(before);expected_events=[]
        if current is not None:
            for i,v in enumerate(expected_saved[:len(expected_flags[current])]):
                expected_flags[current][i]=v;expected_events.append(dict(node=current,index=i,size=4,value=v))
                struct.pack_into('<I',expected,controls[current][i]+0x18-base,v)
        deleting=current is not None and current!=target and expected_refs[current]==1
        if current!=target:
            if current is not None:expected_refs[current]-=1
            if not deleting:
                if target is not None:expected_refs[target]+=1
                current=target;struct.pack_into('<I',expected,0x18,nodes[current] if current is not None else 0)
        if not pc:
            for j,n in enumerate(nodes):struct.pack_into('<H',expected,n+8-base,expected_refs[j])
        if pc and not deleting and current is not None:
            expected_saved+=expected_flags[current]
            for i,v in enumerate(expected_flags[current]):
                expected_flags[current][i]=4;expected_events.append(dict(node=current,index=i,size=4,value=4))
                struct.pack_into('<I',expected,controls[current][i]+0x18-base,4)
            struct.pack_into('<I',expected,0x34,save+4*len(expected_saved))
            for i,v in enumerate(expected_saved):struct.pack_into('<I',expected,save-base+4*i,v)
        start_event=len(events)
        if pc:
            assert not deleting
            node_before=[read(n,f.allocations[n]) for n in nodes]
            f.call(0x4294b0,obj,(nodes[target] if target is not None else 0,))
            # Actual subscriptions may change Node+4; reference count changes
            # are allowed. No other Node fields may change in this empty tree.
            for j,n in enumerate(nodes):
                nb=bytearray(node_before[j]);nb[4:8]=read(n+4,4);struct.pack_into('<H',nb,8,expected_refs[j]);assert read(n,len(nb))==nb
            result=dict(completion='original return',blocks=sum(p.visits.values()))
        else:
            assert len(c['targets'])==1
            stop=0x1a8cf0 if deleting else 0x100860 if target is not None else p.RETURN
            p.reg('A0',obj);p.reg('A1',nodes[target] if target is not None else 0);original=[p.read(a,n) for a,n in p.ranges]
            result=p.run(0x165330,[stop],timeout_us=500000);assert original==[p.read(a,n) for a,n in p.ranges]
            if stop==p.RETURN:
                assert p.reg('SP')==0x22000800 and p.reg('RA')==p.RETURN
                assert [p.reg(str(i)) for i in range(16,21)]==[0x123450000+i for i in range(16,21)]
            else:assert p.reg('A0')==(nodes[c['initial']] if deleting else nodes[target]) and p.reg('A1')==(1 if deleting else obj)
            result['boundary']='node-deleting-destructor' if deleting else 'subscribe-new-node' if target is not None else None
        after=read(base,0x4000);assert after==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(after,expected)) if a!=b][:12]
        assert events[start_event:]==expected_events,(events[start_event:],expected_events)
        result.update(target=target,node=current,savedFlags=expected_saved.copy(),controllerFlags=[v.copy() for v in expected_flags],references=expected_refs.copy(),controllerWrites=events[start_event:],guardedBytes=0x4000,beforeSha256=sha(before),afterSha256=sha(after),expectedAfterSha256=sha(bytes(expected)))
        outputs.append(result)
    u.hook_del(hook)
    return dict(calls=outputs,scope='PC actual Node factories and actual subscribe/unsubscribe with explicit retained references,borrowed controller records/preallocated saved-vector storage;empty child lists. PS2 whole detach or pre-subscribe/destructor boundary with declared empty observer lists. No successful replacement game callback or whole UI startup.')


def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'binding-cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')];report=dict(kind='original-gui-node-binding-controller-history',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[]);start=time.perf_counter()
    for c in cases:
        row=dict(input=c);report['cases'].append(row)
        try:row.update(status='passed',**execute(c,c['platform']))
        except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
        report['seconds']=time.perf_counter()-start;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        if row['status']!='passed':break
    report['status']='passed' if len(report['cases'])==len(cases) and all(c['status']=='passed' for c in report['cases']) else 'blocked'
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(status=report['status'],cases=len(report['cases']),seconds=report['seconds'],errors=[c['error'] for c in report['cases'] if 'error' in c])))
    return int(report['status']!='passed')


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
