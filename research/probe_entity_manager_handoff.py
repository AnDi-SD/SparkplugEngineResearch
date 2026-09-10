#!/usr/bin/env python3
"""Observe original engine-to-game entity-list handoff, without replacement."""
import hashlib,json,sys,time
from pathlib import Path
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
import probe_pc_animation_lifecycle as lifetime


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local evidence required')
    started=time.perf_counter();report=dict(kind='original-pc-entity-manager-handoff',status='running',cases=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Two fresh original entity factory/deleting destructor guests;observe original add/remove entries and concrete manager lists. No replacement callback or normal scene binding.')
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for name,entry in (('spEntity',0x419d40),('wxEntity',0x401110)):
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
        p=f.p;f.time(1200);events=[]
        def observe(u,a,n,user):events.append(dict(operation={0x4200d0:'engine-add',0x420070:'engine-remove',0x574890:'game-add'}[a],receiver=p.reg('ECX'),entity=p.uint(p.reg('ESP')+4)))
        for address in (0x4200d0,0x420070,0x574890):p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=address,end=address)
        obj=f.call(entry);factory_events=events[:]
        assert [e['operation'] for e in factory_events]==(['engine-add'] if name=='spEntity' else ['engine-add','engine-remove','game-add'])
        assert all(e['entity']==obj for e in factory_events)
        def manager(address,head_offset):
            if not address:return None
            head=p.uint(address+head_offset);link=p.uint(head);items=[]
            while link!=head:
                if len(items)>=16:raise RuntimeError('List observation cap')
                items.append(p.uint(link+8));link=p.uint(link)
            return dict(address=address,allocationBytes=f.allocations[address],vtable=f'{p.uint(address):08X}',head=head,count=p.uint(address+head_offset+4),items=items)
        engine=p.uint(0x75ce98);game=p.uint(0x765acc);before=dict(engine=manager(engine,0x18),game=manager(game,0x24))
        assert before['engine']['items']==([obj] if name=='spEntity' else [])
        if name=='wxEntity':assert before['game']['items']==[obj]
        assert p.uint(obj+0x18)==0 and p.uint(obj+0x24)==0
        events.clear();f.call(p.uint(p.uint(obj)),obj,(1,));assert obj in f.freed
        after=dict(engine=manager(engine,0x18),game=manager(game,0x24));assert after['engine']['items']==[]
        if game:assert after['game']['items']==[]
        report['cases'].append(dict(className=name,factory=f'{entry:08X}',object=obj,allocationBytes=f.allocations[obj],factoryEvents=factory_events,
            afterFactory=before,teardownObservedEvents=events[:],afterTeardown=after,fields18and24NullAfterFactory=True));save()
    report['status']='passed';save();print(json.dumps(dict(status='passed',classes=2,seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
