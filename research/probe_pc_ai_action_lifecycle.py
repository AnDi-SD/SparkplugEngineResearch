#!/usr/bin/env python3
"""Bounded original action/state/state-machine lifecycle, retaining each stage.

One shared lifecycle runner; each family has its own cached identity catalog.
Earlier AIAction captures retain their exact runner copies beside the evidence.
"""
from pathlib import Path
import hashlib,json,sys,time
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded,execution_limits
from probe_pc_task_timer import TimerFixture
import probe_pc_animation_lifecycle as lifetime


def guest(class_name,output,factory_profile='micro',context='none',family='ai-action',tracer='instruction',lifecycle_profile='micro',crt='none',platform_profile='none'):
    execution_limits(factory_profile)
    execution_limits(lifecycle_profile)
    if tracer not in ('instruction','block'):raise ValueError('Explicit instruction or block tracer required')
    if 'protected-block' in (factory_profile,lifecycle_profile) and tracer!='block':raise ValueError('protected-block requires block tracer')
    if context not in ('none','empty-scene','empty-scene-profile','empty-scene-bound-node','animation-manager'):raise ValueError('Explicit reviewed context required')
    if crt not in ('none','bounded-strings','bounded-char-traits','bounded-char-traits-sync','bounded-memory'):raise ValueError('Explicit CRT fixture required')
    if platform_profile not in ('none','network-clock','network-startup-failure') or (platform_profile!='none' and family!='network-family'):raise ValueError('Explicit reviewed family platform profile required')
    if family not in ('ai-action','character-state','character-state-machine','entity-direct','entity-core','entity-manager','ai-behavior','generic-trigger','gui-object','projectile','projectile-manager','serializer-expansion','timer-family','projection-family','network-family','physics-family'):raise ValueError('Explicit reviewed family required')
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Local output required')
    source=ROOT/f'local-data/results/native-cycle-20260910-1900/{family}/catalog-family.json'
    record=next(x for x in json.loads(source.read_text()) if x['className']==class_name)
    # These verified platform primary interfaces precede CrossPlatform+4.
    # Their Clone returns that secondary BaseObject pointer, while factory and
    # allocator use the complete object. Other reviewed families keep offset0.
    object_interface_offset=4 if (family,class_name) in (('projection-family','spPCProjectionFX'),('network-family','spDXNetwork')) else 0
    output.parent.mkdir(parents=True,exist_ok=True);started=time.perf_counter()
    if tracer=='block':
        from pc_block_emulator import PcBlocks
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
    else:f=TimerFixture()
    p=f.p;f.time(1200)
    if crt=='bounded-strings':
        from pc_crt_string_fixtures import install_crt_string
        install_crt_string(p)
    elif crt=='bounded-memory':
        from pc_crt_memory_fixtures import install_crt_memory
        install_crt_memory(f)
    elif crt in ('bounded-char-traits','bounded-char-traits-sync'):
        from pc_stl_fixtures import install_char_traits
        install_char_traits(p)
    locks=lock_calls=None
    if crt=='bounded-char-traits-sync':
        from pc_locale_input_fixtures import install_locale_inputs
        locks,lock_calls=install_locale_inputs(p,critical_only=True)
    platform_events=[]
    if platform_profile!='none':
        from pc_network_platform_inputs import install_network_platform_inputs
        platform_events=install_network_platform_inputs(p,platform_profile)
    report=dict(kind=f'original-pc-{family}-lifecycle',className=class_name,status='running',record=record,stages=[],
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),catalogSha256=hashlib.sha256(source.read_bytes()).hexdigest().upper(),
        executableSha256=hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper(),
        limits='Explicit factoryProfile and lifecycleProfile;30s child;64KiB heap/32KiB allocation;fresh guest per class',
        boundaries='Existing TimerFixture allocator/SEH,clock1200/divisor1/refreshfalse and clone-map observations. Optional borrowed context is declared separately; no OS or substituted game constructor/search.',factoryReached=False,wholeClassClosed=False)
    report['tracer']=dict(mode=tracer,visitUnit='basic-block-entry' if tracer=='block' else 'instruction-entry',
        scope='Block visits are not instruction coverage; invalid/privileged blocks stop before execution. Native instruction/time/heap/process bounds remain enforced.' if tracer=='block' else 'Per-instruction visits and guards')
    report['factoryProfile']=dict(name=factory_profile,limits=execution_limits(factory_profile),reason='Explicit fresh-guest escalation after recorded micro cap' if factory_profile!='micro' else 'Default bounded operation')
    report['lifecycleProfile']=dict(name=lifecycle_profile,limits=execution_limits(lifecycle_profile),reason='Explicit fresh-guest escalation after recorded lifecycle micro cap' if lifecycle_profile!='micro' else 'Default bounded operation')
    report['crtFixture']=crt
    if object_interface_offset:report['objectInterfaceOffset']=object_interface_offset
    def save():
        if platform_profile!='none':report['platformInput']=dict(profile=platform_profile,events=list(platform_events),scope='Declared literal platform responses;no host clock/network calls and no successful socket startup claim')
        if locks is not None:report['criticalSectionInput']=dict(scope='Existing single-thread recursive lock bookkeeping;no contention or host OS forwarding;only four synchronization imports installed',calls=list(lock_calls),remaining=dict(locks))
        report.update(seconds=time.perf_counter()-started,arenaBytes=p.allocated,lastIp=f'{p.reg("EIP"):08X}',lastTail=[f'{a:08X}' for a in p.tail],
            allocations=[dict(address=a,bytes=n,freed=a in f.freed,firstWord=f'{p.uint(a):08X}') for a,n in f.allocations.items()],
            lastTextAddressesByFirstVisit=[f'{a:08X}' for a in p.visits if 0x408000<=a<0x6d7000][-80:],
            lastHotAddresses=[dict(address=f'{a:08X}',visits=n) for a,n in p.visits.most_common(16)])
        report['lastBlockCount' if tracer=='block' else 'lastInstructionCount']=sum(p.visits.values())
        output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def call(label,address,this=0,args=()):
        p.execution_profile=factory_profile if label=='factory' else lifecycle_profile
        report['pending']=dict(label=label,address=f'{address:08X}',this=this,args=args);save()
        value=f.call(address,this,args);stage=dict(label=label,address=f'{address:08X}',eax=value)
        stage['blocks' if tracer=='block' else 'instructions']=sum(p.visits.values());report['stages'].append(stage)
        report.pop('pending');save();return value
    def snapshot(a):
        n=f.allocations[a];assert n>=0x10;vt=p.uint(a+object_interface_offset)
        slot_count=17 if family=='ai-action' else 7
        return dict(address=a,allocationBytes=n,bytes=bytes(p.mu.mem_read(a,n)).hex(),vtable=f'{vt:08X}',slots=[f'{p.uint(vt+4*i):08X}' for i in range(slot_count)])
    bound_nodes=[]
    def bind_node(label,entity):
        if context!='empty-scene-bound-node':return
        if (family,class_name) not in (('generic-trigger','wxChestTrigger'),('entity-direct','wxBreakableBarrel'),('entity-direct','wxCharacterMoveCtrl')):
            raise ValueError('Node binding reviewed only for these entity dependencies')
        node=call(label+'-node-factory',0x421e20)
        before=int.from_bytes(p.mu.mem_read(node+8,2),'little')
        call(label+'-bind-node',0x419d00,entity,(node,))
        after=int.from_bytes(p.mu.mem_read(node+8,2),'little')
        assert p.uint(entity+0x18)==node and after==(before+1)&0xffff
        bound_nodes.append(node)
        report['declaredContext'].setdefault('nodeBindings',[]).append(dict(entity=entity,node=node,referenceBefore=before,referenceAfter=after,
            scope='Original Node factory and previously verified spEntity419D00 assignment. Each clone gets its own binding after clone returns;no entity+24 binding assertion.'))
        save()
    try:
        if context=='animation-manager':
            if family!='projectile':raise ValueError('Actor dependency reviewed for projectile family')
            manager=call('animation-manager-factory',0x454640)
            assert f.allocations[manager]==0x2c and p.uint(manager)==0x6e6e30 and p.uint(0x75f880)==manager
            assert p.uint(manager+0x24)==p.uint(manager+0x28)==0
            report['declaredContext']=dict(kind=context,originalManager=manager,globalAddress='0075F880',
                scope='Original spAnimationManager factory454640,known controller registration dependency. No borrowed success callback or full engine startup.')
            save()
        if context.startswith('empty-scene'):
            # Actual Node factory/search, with literal external core/scene
            # pointers. No scene initialize, game startup or fake Find result.
            call('matrix-static-initializer',0x6d38e0)
            engine=p.allocate(0x158);scene=p.allocate(0x54)
            p.mu.mem_write(engine,bytes(0x158));p.mu.mem_write(scene,bytes(0x54));p.put_uint(0x755274,engine)
            root=call('borrowed-root-factory',0x421e20)
            p.put_uint(engine+0x18,scene);p.put_uint(scene+0x14,root)
            report['declaredContext']=dict(kind=context,engineStorage=engine,sceneStorage=scene,originalRoot=root,
                scope='Opaque borrowed engine+18->scene+14->actual unnamed empty Node; no original engine/scene factory or initialize claim')
            if context=='empty-scene-profile':
                if class_name!='wxStellaGlyph' or family!='generic-trigger':raise ValueError('Profile record reviewed only for StellaGlyph teardown')
                profile=p.allocate(0x2cb8);profile_before=b'\x5a'*0x2cb8;p.mu.mem_write(profile,profile_before);p.put_uint(0x765ad4,profile)
                report['declaredContext']['borrowedProfile']=dict(globalAddress='00765AD4',storage=profile,extentBytes=0x2cb8,
                    scope='Literal existing profile record covering independently decoded PC/PS2 teardown byte2CB6. Not an original profile constructor or exact profile allocation size. Whole record guarded at completion.')
            save()
        obj=call('factory',record['pcFactory']);report['factoryReached']=True;report['initial']=snapshot(obj)
        report['factoryVisits']={f'{a:08X}':n for a,n in sorted(p.visits.items())};save()
        assert call('rtti',p.uint(p.uint(obj+object_interface_offset)+0x10),obj+object_interface_offset)==record['pcRecord']
        bind_node('original',obj)
        clone_interface=call('clone',p.uint(p.uint(obj+object_interface_offset)+8),obj+object_interface_offset)
        assert clone_interface;clone=clone_interface-object_interface_offset;assert clone!=obj and clone in f.allocations
        report['clone']=snapshot(clone);report['clonePairs']=f.clone_pairs;save()
        bind_node('clone',clone)
        for label,a in [('delete-clone',clone),('delete-original',obj)]:
            call(label,p.uint(p.uint(a+object_interface_offset)),a+object_interface_offset,(1,));assert a in f.freed
        if context=='animation-manager':
            assert p.uint(manager+0x24)==p.uint(manager+0x28)==0,'Projectile-owned Actors must unregister before manager deletion'
            call('delete-animation-manager',p.uint(p.uint(manager)),manager,(1,));assert manager in f.freed and p.uint(0x75f880)==0
            report['declaredContext']['emptyRegistryAndManagerDeletionVerified']=True
        if context.startswith('empty-scene'):
            call('delete-borrowed-root',p.uint(p.uint(root)),root,(1,));assert root in f.freed
        if context=='empty-scene-profile':
            expected=bytearray(profile_before);expected[0x2cb6]=1
            assert bytes(p.mu.mem_read(profile,len(expected)))==bytes(expected),'Borrowed profile record guard'
            report['declaredContext']['borrowedProfile']['verifiedChangedOffsets']=[0x2cb6]
        if bound_nodes:
            report['declaredContext']['boundNodesReleased']=[a in f.freed for a in bound_nodes]
            assert all(a in f.freed for a in bound_nodes),'Entity teardown must release both bound Nodes'
        report['remainingAllocations']=[a for a in f.allocations if a not in f.freed];report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),errorType=type(error).__name__);save()
        print(json.dumps(dict(status='blocked',className=class_name,factoryReached=report['factoryReached'],pending=report.get('pending'),error=str(error))));return 1
    print(json.dumps(dict(status='passed',className=class_name,bytes=report['initial']['allocationBytes'],stages=len(report['stages']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
