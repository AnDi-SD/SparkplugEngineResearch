#!/usr/bin/env python3
"""Cold layer cloning with actual CloneManager and native static clone map."""
import hashlib,json,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from pc_crt_memory_fixtures import install_crt_memory
import probe_pc_animation_lifecycle as lifetime


def guest(name,output,dispose='none',word14='default'):
    if dispose not in ('none','identified-contexts'):raise ValueError('Explicit context disposal required')
    if word14!='default' and name!='spDXEnvironmentMapLayer':raise ValueError('Own word14 established only for EnvironmentMapLayer')
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Fresh local report required')
    family='engine-core-remainder' if name in ('spCameraViewLayer','spMovieLayer') else 'engine-platform-remainder'
    catalog=ROOT/f'local-data/results/native-cycle-20260910-1900/{family}/catalog-family.json';record=next(r for r in json.loads(catalog.read_text()) if r['className']==name)
    t=time.perf_counter();r=dict(kind='original-layer-clone-native-context',className=name,record=record,status='running',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),stages=[],scope='Original CloneManager factory,static map constructor/teardown,pair insertion and nested Clone. No pair/lookup or Copy game-method seams. Original optional AnimationManager for MovieLayer. CRT bounded-memory only;no device.')
    with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
    p=f.p;p.seams.pop(0x412f70);p.put_uint(0x74e060,0);install_crt_memory(f)
    def save():
        r.update(seconds=time.perf_counter()-t,lastIp=f'{p.reg("EIP"):08X}',lastTail=[f'{v:08X}' for v in p.tail],allocations=[dict(address=a,size=n,freed=a in f.freed,vtable=f'{p.uint(a):08X}') for a,n in f.allocations.items()]);output.write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8')
    def call(label,address,this=0,args=()):
        r['pending']=dict(label=label,address=f'{address:08X}',this=this,args=args);save();p.execution_profile='protected-block' if name=='spDXMirrorLayer' and label=='factory' else 'micro';v=f.call(address,this,args);r['stages'].append(dict(label=label,address=f'{address:08X}',result=v));r.pop('pending');save();return v
    def snapshot(o):
        n=f.allocations[o];payload=p.uint(o+0x10);return dict(address=o,bytes=n,raw=bytes(p.mu.mem_read(o,n)).hex(),ownedPayload=dict(address=payload,bytes=f.allocations[payload],raw=bytes(p.mu.mem_read(payload,f.allocations[payload])).hex(),vtable=f'{p.uint(payload):08X}') if payload else None)
    try:
        call('original-static-map-construction',0x52fd90,0x755588);manager=call('original-clone-manager-factory',0x412540);assert f.allocations[manager]==24 and p.uint(0x74e060)==manager
        animation=call('original-animation-manager-factory',0x454640) if name=='spMovieLayer' else None
        obj=call('factory',record['pcFactory'])
        if word14!='default':p.put_uint(obj+0x14,int(word14,0));r['declaredWord14']=int(word14,0)
        r['originalBefore']=snapshot(obj);assert call('rtti',p.uint(p.uint(obj)+16),obj)==record['pcRecord']
        clone=call('clone',p.uint(p.uint(obj)+8),obj);assert clone and clone!=obj and clone in f.allocations;r['clone']=snapshot(clone);r['originalAfter']=snapshot(obj);assert r['originalBefore']==r['originalAfter']
        if word14!='default':assert p.uint(clone+0x14)==int(word14,0)
        r['nativeMapAfterClone']=dict(depth=p.uint(manager+0x14),size=p.uint(0x755590));assert p.uint(manager+0x14)==0
        for label,o in [('delete-clone',clone),('delete-original',obj)]:call(label,p.uint(p.uint(o)),o,(1,));assert o in f.freed
        call('original-static-map-teardown',0x6d7db0);call('delete-clone-manager',p.uint(p.uint(manager)),manager,(1,));assert manager in f.freed and p.uint(0x74e060)==0
        if animation:
            assert p.uint(animation+0x24)==p.uint(animation+0x28)==0;call('delete-animation-manager',p.uint(p.uint(animation)),animation,(1,));assert animation in f.freed and p.uint(0x75f880)==0
        if dispose=='identified-contexts':
            for context_name,table in [('spCameraManager',0x6e718c),('spEngineCore',0x6dc318),('spPCRenderTargetManager',0x6f27e4)]:
                objects=[a for a in f.allocations if a not in f.freed and p.uint(a)==table]
                assert len(objects)<=1,'One identified lazy context per class'
                for a in objects:call('delete-identified-context-'+context_name,p.uint(table),a,(1,));assert a in f.freed
        r['remainingContextAllocations']=[a for a in f.allocations if a not in f.freed];r['status']='passed';save()
    except Exception as error:
        r.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',className=name,pending=r.get('pending'),error=str(error))));return 1
    print(json.dumps(dict(status='passed',className=name,bytes=r['originalBefore']['bytes'],ownedPayloadBytes=r['originalBefore']['ownedPayload']['bytes'],remaining=r['remainingContextAllocations'],seconds=r['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
