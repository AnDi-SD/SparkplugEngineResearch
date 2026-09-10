#!/usr/bin/env python3
"""Original MoveCtrl copy between two objects already bound to original Nodes."""
import hashlib,json,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
import probe_pc_animation_lifecycle as lifetime


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    started=time.perf_counter();report=dict(kind='original-pc-move-controller-bound-copy',status='running',stages=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='Original factories,Node assignment and own Copy method between two existing bound objects. This does not execute or repair Clone;normal clone target binding remains a separate question.')
    with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
    p=f.p;f.time(1200)
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def call(label,address,this=0,args=()):
        report['pending']=dict(label=label,address=f'{address:08X}',this=this,args=args);save();value=f.call(address,this,args)
        report['stages'].append(dict(label=label,address=f'{address:08X}',value=value,blocks=sum(p.visits.values())));report.pop('pending');save();return value
    try:
        call('matrix-initialize',0x6d38e0);engine=p.allocate(0x158);scene=p.allocate(0x54);p.mu.mem_write(engine,bytes(0x158));p.mu.mem_write(scene,bytes(0x54));p.put_uint(0x755274,engine)
        root=call('root-factory',0x421e20);p.put_uint(engine+0x18,scene);p.put_uint(scene+0x14,root)
        catalog=json.loads((ROOT/'local-data/results/native-cycle-20260910-1900/entity-direct/catalog-family.json').read_text());row=next(r for r in catalog if r['className']=='wxCharacterMoveCtrl')
        objects=[];nodes=[]
        for label in ('source','destination'):
            obj=call(label+'-factory',row['pcFactory']);node=call(label+'-node-factory',0x421e20);call(label+'-assign-node',0x419d00,obj,(node,));objects.append(obj);nodes.append(node)
            assert p.uint(obj+0x18)==node
        source,destination=objects;source_node,destination_node=nodes;events=[]
        def observe(mu,address,size,user):
            if address==0x4e6630:events.append(dict(receiver=p.reg('ECX'),node=p.uint(p.reg('ECX')+0x18),source=source,destination=destination))
        hook=p.mu.hook_add(p.uc.UC_HOOK_CODE,observe,begin=0x4e6630,end=0x4e6630)
        result=call('copy-between-bound-objects',0x4f6300,source,(destination,));p.mu.hook_del(hook)
        assert result&255==1 and events==[dict(receiver=destination,node=destination_node,source=source,destination=destination)]
        assert p.uint(source+0x18)==source_node and p.uint(destination+0x18)==destination_node
        lookups=[p.uint(destination+o) for o in (0x2b4,0x2b8,0x2bc)];assert lookups==[0,0,0]
        report.update(objects=objects,nodes=nodes,lookupEvents=events,destinationLookups=lookups)
        for label,obj,node in [('delete-destination',destination,destination_node),('delete-source',source,source_node)]:
            call(label,p.uint(p.uint(obj)),obj,(1,));assert obj in f.freed and node in f.freed
        call('delete-root',p.uint(p.uint(root)),root,(1,));assert root in f.freed
        report.update(status='passed',bothEntityNodesReleased=True,remainingTrackedAllocations=[a for a in f.allocations if a not in f.freed]);save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc(),lastIp=f'{p.reg("EIP"):08X}',lastTail=[f'{a:08X}' for a in p.tail]);save();print(json.dumps(dict(status='blocked',pending=report.get('pending'),error=str(error))));return 1
    print(json.dumps(dict(status='passed',stages=len(report['stages']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
