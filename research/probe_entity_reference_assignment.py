#!/usr/bin/env python3
"""Original spEntity +18 reference assignment and independent PS2 prefixes."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from capture_native_ranges import EXPECTED,read_window
import probe_pc_animation_lifecycle as lifetime


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    output.parent.mkdir(parents=True,exist_ok=True);started=time.perf_counter()
    report=dict(kind='paired-original-sp-entity-reference-assignment',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Literal spEntity-sized receiver, actual PC Node factories/destructors. Original PC setter returns;PS2 scalar prefix14E160 stops before restore or actual Node destructor. No invented setter or successful game callee fixture.',
        limits='Fresh guests;PC micro100k/2s with block tracing;PS2 ordinary2000/100ms;30s process;default64KiB PC arena.')
    def save():
        report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    cases=[dict(old=None,new=None,a=0,b=0),dict(old=None,new='A',a=0,b=0),dict(old=None,new='A',a=65535,b=0),
        dict(old='A',new='A',a=1,b=0),dict(old='A',new='A',a=0,b=0),dict(old='A',new='B',a=2,b=5),
        dict(old='A',new='B',a=0,b=65535),dict(old='A',new=None,a=2,b=0),dict(old='A',new=None,a=1,b=0),dict(old='A',new='B',a=1,b=0)]
    raw,sections=pristine();node_vt=0x4902f0
    node_dtor=int.from_bytes(read_window('ps2',raw,node_vt+8,4,sections)[0],'little')
    report['ps2NodeDestructor']=f'{node_dtor:08X}'
    try:
        for case in cases:
            report['pending']=case;save()
            with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
            p=f.p;nodes={'A':f.call(0x421e20),'B':f.call(0x421e20),None:0};obj=p.allocate(0x28)
            p.mu.mem_write(obj,b'\xa5'*0x28);p.put_uint(obj,0x6dbccc);p.put_uint(obj+0x18,nodes[case['old']])
            for label in ('A','B'):p.mu.mem_write(nodes[label]+8,struct.pack('<H',case[label.lower()]))
            before=bytes(p.mu.mem_read(obj,0x28));dtor=p.uint(p.uint(nodes['A']));events=[]
            def state():return dict(owner=next(k for k,v in nodes.items() if v==p.uint(obj+0x18)),a=p.uint(nodes['A']+8)&65535,b=p.uint(nodes['B']+8)&65535)
            def observe(u,address,size,user):
                assert p.reg('ECX')==nodes['A'] and p.uint(p.reg('ESP')+4)==1
                events.append(state())
            hook=p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=dtor,end=dtor)
            f.call(0x419d00,obj,(nodes[case['new']],));p.mu.hook_del(hook)
            actual=state();expected=dict(owner=case['new'],a=case['a'],b=case['b']);deletes=False
            if case['old']!=case['new']:
                if case['old']:
                    expected['a']=(expected['a']-1)&65535;deletes=expected['a']==0
                if case['new']:
                    k=case['new'].lower();expected[k]=(expected[k]+1)&65535
            assert actual==expected and (nodes['A'] in f.freed)==deletes and len(events)==int(deletes)
            expected_bytes=bytearray(before);expected_bytes[0x18:0x1c]=struct.pack('<I',nodes[case['new']]);assert bytes(p.mu.mem_read(obj,0x28))==expected_bytes
            pc=dict(after=actual,deleteEvents=events,deletedOld=deletes,blocks=sum(p.visits.values()),originalNodeDestructor=f'{dtor:08X}')
            m=Ps2ScalarPrefix([(0x14e160,0x64)]);m.map(0x21000000,0x4000)
            mnodes={'A':0x21001000,'B':0x21002000,None:0};entity=0x21000000
            m.write(entity,b'\xa5'*0x28);m.put_uint(entity,0x48df60);m.put_uint(entity+0x18,mnodes[case['old']])
            m.map(node_vt,12);m.write(node_vt,read_window('ps2',raw,node_vt,12,sections)[0])
            for label in ('A','B'):
                a=mnodes[label];m.write(a,b'\xa5'*0x20);m.put_uint(a,node_vt);m.write(a+8,struct.pack('<H',case[label.lower()]))
            for name,value in (('A0',entity),('A1',mnodes[case['new']])):m.reg(name,value)
            b=m.run(0x14e160,[0x14e1c4,node_dtor])
            after=dict(owner=next(k for k,v in mnodes.items() if v==m.uint(entity+0x18)),a=m.uint(mnodes['A']+8)&65535,b=m.uint(mnodes['B']+8)&65535)
            if deletes:
                assert m.stop==node_dtor and after==events[0] and m.reg('A0')==mnodes['A'] and m.reg('A1')==1
            else:assert m.stop==0x14e1c4 and after==actual
            b['after']=after;b['scope']='before actual destructor entry' if deletes else 'complete setter effects before excluded restore frame'
            desired=bytearray(before);desired[0:4]=struct.pack('<I',0x48df60);desired[0x18:0x1c]=struct.pack('<I',mnodes[after['owner']]);assert m.read(entity,0x28)==desired
            report['cases'].append(dict(input=case,pc=pc,ps2=b));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(cases),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
