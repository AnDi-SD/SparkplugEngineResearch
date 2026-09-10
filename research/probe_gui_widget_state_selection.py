#!/usr/bin/env python3
"""Original widget/button selection; PS2 boundaries surround known Node enable."""
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
    started=time.perf_counter();report=dict(kind='paired-original-widget-button-state-selection',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='PC complete selection with four actual original Nodes and original recursive EnabledMask method. PS2 fresh scalar prefixes stop at real Node enable consumer;when an old visible Node is hidden,a second fresh prefix explicitly starts after that previously verified Node contract. No resumed guest,substituted game return or rendering claim.',
        limits='PC100k/2s blocks,PS22000/100ms per fresh prefix,outer30s;whole widget and PC Node records guarded.')
    defaults=dict(kind='widget',enabled=1,highlight=0,visible=1,old=0,oldEnabled=1,normal=1,disabled=2,highlighted=3,pressed=0,pressedNode=4)
    definitions=[('normal',{}),('disabled',dict(enabled=0)),('highlight',dict(highlight=1)),('disabled-over-highlight',dict(enabled=0,highlight=1)),
        ('raw-bools',dict(enabled=255,highlight=128,visible=2)),('missing-highlight',dict(highlight=1,highlighted=0)),('missing-disabled',dict(enabled=0,disabled=0)),
        ('all-null',dict(normal=0,disabled=0,highlighted=0)),('selected-hidden',dict(visible=0)),('hide-and-show-same',dict(old=1)),
        ('hide-and-show-different',dict(old=1,highlight=1)),('old-hidden-early-return',dict(old=1,oldEnabled=0,enabled=0)),
        ('button-pressed',dict(kind='button',pressed=1)),('pressed-over-disabled',dict(kind='button',pressed=1,enabled=0)),
        ('missing-pressed',dict(kind='button',pressed=1,pressedNode=0,enabled=0)),('button-highlight',dict(kind='button',highlight=1)),
        ('pressed-same-old',dict(kind='button',pressed=255,pressedNode=1,old=1)),('button-old-hidden',dict(kind='button',old=1,oldEnabled=0,pressed=1))]
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for label,changes in definitions:
            case=dict(defaults,**changes);case['label']=label;report['pending']=case;save();button=case['kind']=='button';skip=bool(case['old'] and not case['oldEnabled'])
            index=7 if button and case['pressed'] and case['pressedNode'] else 1 if not case['enabled'] else 2 if case['highlight'] else 0
            selected=case['pressedNode'] if button and case['pressed'] and case['pressedNode'] else [case['normal'],case['disabled'],case['highlighted']][index]
            if not selected:selected=case['normal']
            if skip:index,selected=7,case['old']
            calls=[] if skip else ([dict(node=case['old'],enabled=0,recursive=1)] if case['old'] else [])+([dict(node=selected,enabled=case['visible'],recursive=1)] if selected else [])
            with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
            p=f.p;nodes={0:0};node_before={}
            for i in (1,2,3,4):
                a=f.call(0x421e20);nodes[i]=a;p.put_uint(a+0xb0,(p.uint(a+0xb0)&~0x200)|(0 if i==case['old'] and not case['oldEnabled'] else 0x200));node_before[i]=bytes(p.mu.mem_read(a,f.allocations[a]))
            size=0x60 if button else 0x54;obj=p.allocate(size);p.mu.mem_write(obj,b'\xa5'*size);p.put_uint(obj,0x6deac0 if button else 0x6dea6c)
            for o,v in [(0x28,case['enabled']),(0x3c,case['visible']),(0x3d,case['highlight'])]+([(0x58,case['pressed'])] if button else []):p.mu.mem_write(obj+o,bytes([v]))
            for o,v in ((0x40,case['old']),(0x44,case['normal']),(0x48,case['disabled']),(0x4c,case['highlighted'])):p.put_uint(obj+o,nodes[v])
            if button:p.put_uint(obj+0x5c,nodes[case['pressedNode']])
            p.put_uint(obj+0x50,7);before=bytes(p.mu.mem_read(obj,size));entry=p.uint(p.uint(nodes[1])+0x34);observed=[]
            def observe(mu,address,n,user):
                if address==entry:observed.append(dict(node=next(i for i,a in nodes.items() if a==p.reg('ECX')),enabled=p.uint(p.reg('ESP')+4),recursive=p.uint(p.reg('ESP')+8)))
            hook=p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=entry,end=entry);f.call(0x435cb0 if button else 0x4358c0,obj);p.mu.hook_del(hook)
            expected=bytearray(before);struct.pack_into('<I',expected,0x40,nodes[selected]);struct.pack_into('<I',expected,0x50,index)
            assert bytes(p.mu.mem_read(obj,size))==bytes(expected) and observed==calls,(case,observed,calls)
            for i,a in nodes.items():
                if not i:continue
                expected_node=bytearray(node_before[i]);flags=struct.unpack_from('<I',expected_node,0xb0)[0]
                for call in calls:
                    if call['node']==i:flags=(flags|0x200) if call['enabled'] else (flags&~0x200)
                struct.pack_into('<I',expected_node,0xb0,flags);assert bytes(p.mu.mem_read(a,len(expected_node)))==bytes(expected_node),'whole original Node guard'
            pc=dict(selected=selected,index=index,calls=observed,blocks=sum(p.visits.values()),completion='original return',nodeEnableAddress=f'{entry:08X}')
            prefixes=[];ps_calls=[]
            for phase in (['hide','after-hide'] if not skip and case['old'] else ['entry']):
                q=Ps2ScalarPrefix([(0x168d4c,0xf0)] if button else [(0x16bbcc,0xc0)],profile='integer-movz');q.map(0x21000000,0x4000);q.map(0x22000000,4096);q.map(0x4902f0,0x40)
                raw,sections=pristine();q.write(0x4902f0,read_window('ps2',raw,0x4902f0,0x40,sections)[0]);psnodes={0:0,1:0x21001000,2:0x21001800,3:0x21002000,4:0x21002800}
                psobj,pssize=0x21000000,0x5c if button else 0x50;q.write(psobj,b'\xa5'*pssize)
                for i,a in psnodes.items():
                    if not i:continue
                    q.put_uint(a,0x4902f0);q.put_uint(a+0xb4,0 if i==case['old'] and (not case['oldEnabled'] or phase=='after-hide') else 0x200)
                for o,v in [(0x28,case['enabled']),(0x38,case['visible']),(0x39,case['highlight'])]+([(0x54,case['pressed'])] if button else []):q.write(psobj+o,bytes([v]))
                for o,v in ((0x3c,case['old']),(0x40,case['normal']),(0x44,case['disabled']),(0x48,case['highlighted'])):q.put_uint(psobj+o,psnodes[v])
                if button:q.put_uint(psobj+0x58,psnodes[case['pressedNode']])
                q.put_uint(psobj+0x4c,7);psbefore=q.read(psobj,pssize);q.reg('SP',0x22000800);q.reg('A0',psobj);q.reg('S0',psobj)
                start=(0x168d90 if button else 0x16bc10) if phase=='after-hide' else (0x168d4c if button else 0x16bbcc)
                restore=0x168e3c if button else 0x16bc8c;consumer=phase=='hide' or not skip and selected!=0
                r=q.run(start,[0x1a5b00 if consumer else restore]);desired=bytearray(psbefore)
                if phase!='hide':struct.pack_into('<I',desired,0x3c,psnodes[selected]);struct.pack_into('<I',desired,0x4c,index)
                assert q.read(psobj,pssize)==bytes(desired),'PS2 widget guard'
                if consumer:
                    call=dict(node=next(i for i,a in psnodes.items() if a==q.reg('A0')),enabled=q.reg('A1')&0xffffffff,recursive=q.reg('A2')&0xffffffff);ps_calls.append(call)
                r.update(phase=phase,selected=next(i for i,a in psnodes.items() if a==q.uint(psobj+0x3c)),index=q.uint(psobj+0x4c));prefixes.append(r)
            assert ps_calls==calls and prefixes[-1]['selected']==selected and prefixes[-1]['index']==index
            report['cases'].append(dict(input=case,pc=pc,ps2=dict(selected=selected,index=index,calls=ps_calls,prefixes=prefixes)));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(definitions),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
