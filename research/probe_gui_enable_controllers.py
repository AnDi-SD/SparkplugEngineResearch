"""Original GUIObject enable/search/controller writes over declared node records."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_block_emulator import PcBlocks
from pc_instruction_emulator import run_bounded
from pc_stl_fixtures import read_cstring
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER

FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/gui-runtime'
def sha(b):return hashlib.sha256(b).hexdigest().upper()


def execute(c,platform):
    pc=platform=='pc';searches=[];writes=[]
    if pc:
        p=PcBlocks();base=p.allocate(0x5000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));u=p.mu
        callbacks=0x34150000;u.mem_map(callbacks,4096,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
        def strstr(m):
            a=m.uint(m.reg('ESP')+4);b=m.uint(m.reg('ESP')+8);hay=read_cstring(m,a,256);needle=read_cstring(m,b,256);index=hay.find(needle)
            searches.append(dict(haystack=hay.hex(),needle=needle.hex(),index=index));m.fixture_return(eax=a+index if index>=0 else 0)
        p.put_uint(0x6d932c,callbacks+16);p.seams[callbacks+16]=strstr
    else:
        p=Ps2ScalarPrefix([(0x165240,0xf0),(0x1a7160,0x154),(0x40b208,0x84),(0x1163f0,8),(0x14dd40,0x10)],stack_window=(0x22000000,4096),upper64=UPPER)
        base=0x21000000;p.map(base,0x5000);p.map(0x22000000,4096);read,write,u=p.read,p.write,p.u
        raw,sections=pristine()
        for a,n in [(0x4902f0,0x40),(0x447010,16)]:p.map(a,n);p.write(a,read_window('ps2',raw,a,n,sections)[0])
        p.reg('SP',0x22000800);p.reg('A0',base);p.reg('A1',c['value']);p.reg('S0',0x123456789);p.reg('S1',0xabcdef123)
    put=p.put_uint;obj=base;pool=base+0x1000;root=base+0x2000;sentinel=base+0x1800;array=base+0x3000;controls=[base+0x4000+i*0x80 for i in range(4)]
    write(base,b'\xa5'*0x5000);put(obj,0x6dcc70 if pc else 0x48e560);put(obj+0x18,root);put(root,0x6dc4f4 if pc else 0x4902f0)
    assert p.uint(p.uint(root)+(0x2c if pc else 0x30))==(0x421330 if pc else 0x1a7160)
    put(root+0x10,pool if c['name'] is not None else 0)
    if c['name'] is not None:write(pool+9,c['name'].encode('ascii')+b'\0')
    if pc:put(root+0x18,sentinel);put(sentinel,sentinel);put(sentinel+4,sentinel)
    else:put(root+0x18,root+0x18);put(root+0x1c,root+0x18)
    for i,value in enumerate(c['flags']):put(controls[i]+0x18,value)
    for i,index in enumerate(c['indices']):put(array+4*i,controls[index])
    if pc:put(root+0x68,array);put(root+0x6c,array+len(c['indices'])*4);put(root+0x70,array+16)
    else:put(root+0x64,4);put(root+0x68,len(c['indices']));put(root+0x6c,array)
    before=read(base,0x5000);expected=bytearray(before);expected[0x28]=c['value']&255
    found=c['name'] is not None and 'GUICollision' in c['name'];matched=c['indices'] if found else []
    for index in matched:struct.pack_into('<I',expected,controls[index]-base+0x18,4)
    def observe(machine,access,address,size,value,user):
        for i,a in enumerate(controls):
            if address==a+0x18:writes.append(dict(index=i,size=size,value=value))
    hook=u.hook_add((p.uc if pc else __import__('unicorn')).UC_HOOK_MEM_WRITE,observe,begin=controls[0]+0x18,end=controls[-1]+0x18)
    if pc:
        p.run(0x4290b0,this=obj,args=(c['value'],));result=dict(entry='004290B0',completion='original return',blocks=sum(p.visits.values()),externalCrtSearches=searches)
    else:
        original=[p.read(a,n) for a,n in p.ranges];stop=0x40b218 if c['name']=='' else p.RETURN
        result=p.run(0x165240,[stop],timeout_us=500000);assert original==[p.read(a,n) for a,n in p.ranges]
        if stop==p.RETURN:assert [p.reg(n) for n in ('SP','RA','S0','S1')]==[0x22000800,p.RETURN,0x123456789,0xabcdef123]
        else:result['openBoundary']='Before original JR with MOVZ delay slot in empty-haystack strstr;no substituted search result'
    u.hook_del(hook)
    assert read(base,0x5000)==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(read(base,0x5000),expected)) if a!=b][:10]
    assert writes==[dict(index=i,size=4,value=4) for i in matched],(writes,matched)
    result.update(guardedBytes=0x5000,beforeSha256=sha(before),afterSha256=sha(read(base,0x5000)),expectedAfterSha256=sha(bytes(expected)),controllerWrites=writes,
        scope='Borrowed Node/name-pool/controller-vector state,original vtables and full GUIObject/Node search methods. PC external CRT strstr is bounded;PS2 uses original ELF strstr. No reconstructed Node startup,subscription binding,rendering or controller execution.')
    return result


def guest(output,selection):
    output=Path(output).resolve();path=FOLDER/'enable-cases.json'
    if output.exists() or not output.is_relative_to(FOLDER) or selection not in ('pilot','batch'):raise ValueError('Fresh bounded selection')
    cases=[c for c in json.loads(path.read_text()) if c['pilot']==(selection=='pilot')]
    report=dict(kind='paired-original-gui-enable-controller-flags',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),selectionSha256=sha(path.read_bytes()),cases=[]);start=time.perf_counter()
    for c in cases:
        for platform in ('pc','ps2'):
            row=dict(input=c,platform=platform);report['cases'].append(row)
            try:row.update(status='passed',**execute(c,platform))
            except Exception as e:row.update(status='blocked',error=str(e),traceback=traceback.format_exc())
            report['seconds']=time.perf_counter()-start;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
            if row['status']!='passed':break
        if row['status']!='passed':break
    report['status']='passed' if len(report['cases'])==2*len(cases) and all(c['status']=='passed' for c in report['cases']) else 'blocked'
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(status=report['status'],cases=len(report['cases']),seconds=report['seconds'],errors=[c['error'] for c in report['cases'] if 'error' in c])))
    return int(report['status']!='passed')


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
