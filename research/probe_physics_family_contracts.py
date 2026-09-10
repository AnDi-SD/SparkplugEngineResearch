#!/usr/bin/env python3
"""Actual PC physics ownership and paired BV identities/constructor prefixes."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from pc_crt_memory_fixtures import install_crt_memory
from ps2_scalar_prefix import Ps2ScalarPrefix
import probe_pc_animation_lifecycle as lifetime


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    started=time.perf_counter();report=dict(kind='physics-ownership-and-bv-prefixes',status='running',inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),pcOwnershipCases=[],pcCloneCases=[],ps2Cases=[],
        scope='Actual PC PhysicsManager/body/contact factories and ordered deregistration,then complete manager disposal. Actual Capsule/Convex clone keeps own factory fields. PS2 getters/null base Clone and post-base constructor prefixes;no constraint solver or full R5900 float/collision pipeline.')
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for names,order in ((['body']*3,[1,0,2]),(['contact']*3,[2,1,0]),(['body','contact','body','contact'],[0,3,2,1])):
            report['pending']=dict(kind='ownership',names=names,order=order);save()
            with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
            p=f.p;install_crt_memory(f);manager=f.call(0x459f00)
            assert f.allocations[manager]==132 and p.uint(manager)==0x6e70bc and p.uint(0x75db80)==manager
            baseline=bytes(p.mu.mem_read(manager,132));objects=[];live={'body':[],'contact':[]};events=[];dirty=0
            def check(label):
                expected=bytearray(baseline);expected[0x21]=dirty
                vectors={}
                for name,offset in (('body',0x2c),('contact',0x3c)):
                    begin,end,limit=(p.uint(manager+offset+i) for i in (0,4,8))
                    assert (begin==end==limit==0) or (begin in f.allocations and begin<=end<=limit and limit-begin==f.allocations[begin])
                    assert end-begin==len(live[name])*4
                    values=[p.uint(begin+4*i) for i in range(len(live[name]))];assert values==live[name],'stable registry order'
                    expected[offset:offset+12]=struct.pack('<3I',begin,end,limit);vectors[name]=values
                assert bytes(p.mu.mem_read(manager,132))==bytes(expected),'whole manager guard outside the two registries and observed byte21'
                events.append(dict(label=label,registries=vectors,byte21=p.mu.mem_read(manager+0x21,1).hex()))
            check('empty')
            for name in names:
                a=f.call(0x49bac0 if name=='body' else 0x48bcf0);objects.append((name,a));live[name].append(a)
                if name=='body':dirty=1
                assert f.allocations[a]==(312 if name=='body' else 228)
                check('created-'+name)
            for index in order:
                name,a=objects[index];f.call(p.uint(p.uint(a)),a,(1,));assert a in f.freed
                live[name].remove(a);check('deleted-'+name)
            f.call(p.uint(p.uint(manager)),manager,(1,));assert p.uint(0x75db80)==0
            assert set(f.allocations)==set(f.freed),'all original objects and registry buffers released'
            report['pcOwnershipCases'].append(dict(names=names,deleteOrder=order,events=events,allAllocationsFreed=True));report.pop('pending');save()
        for name,factory,size,fields in (('spCapsuleBV',0x4884d0,120,[0x28,0x2c,0x30,0x34,0x38,0x6c,0x70,0x74]),('spConvexBV',0x47d8e0,48,[0x14,0x2c])):
            report['pending']=dict(kind='bv-clone',name=name);save()
            with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
            p=f.p;obj=f.call(factory);assert f.allocations[obj]==size;before=bytes(p.mu.mem_read(obj,size))
            for i,offset in enumerate(fields):p.put_uint(obj+offset,0x41100000+i)
            modified=bytes(p.mu.mem_read(obj,size));clone=f.call(p.uint(p.uint(obj)+8),obj)
            assert clone!=obj and bytes(p.mu.mem_read(clone,size))==before,'own BV geometry stays at factory defaults'
            assert bytes(p.mu.mem_read(obj,size))==modified
            for a in (clone,obj):f.call(p.uint(p.uint(a)),a,(1,));assert a in f.freed
            assert set(f.allocations)==set(f.freed)
            report['pcCloneCases'].append(dict(className=name,changedSourceOffsets=fields,cloneEqualsFactoryBytes=True,allAllocationsFreed=True));report.pop('pending');save()
        rows={r['className']:r for r in json.loads((ROOT/'local-data/results/native-cycle-20260910-1900/physics-family/catalog-family.json').read_text())}
        getters={'spBoundingVolume':0x127e40,'spBoxBV':0x127f10,'spCapsuleBV':0x128d30,'spCollisionManager':0x124c80,'spConvexBV':0x129de0,'spMeshBV':0x12a020,'spOBBBV':0x12d2c0,'spSphereBV':0x130940}
        for name,entry in getters.items():
            q=Ps2ScalarPrefix([(entry,12)]);r=q.run(entry,[q.RETURN]);assert q.reg('V0')==rows[name]['ps2Record']
            report['ps2Cases'].append(dict(kind='getter',className=name,entry=entry,record=q.reg('V0'),execution=r));save()
        q=Ps2ScalarPrefix([(0x127f00,8)]);r=q.run(0x127f00,[q.RETURN]);assert q.reg('V0')==0
        report['ps2Cases'].append(dict(kind='base-null-clone',entry=0x127f00,execution=r));save()
        for kind,entry,ranges,stop,writes,target in (
            ('Convex-post-base',0x129fe8,[(0x129fd4,0x3c)],0x12a00c,{0:0x48d280,0x14:6,0x28:0,0x2c:0},None),
            ('Capsule-before-matrix',0x129938,[(0x129938,0x24)],0x109470,{0:0x48d220,0x28:0x3f800000,0x2c:0x3f800000},0x30),
            ('Capsule-after-segment',0x12998c,[(0x129924,0xc),(0x12998c,0x20)],0x1299a8,{0x14:3,0x6c:0,0x70:0,0x74:0},None)):
            report['pending']=dict(kind=kind,entry=entry,stop=stop);save()
            q=Ps2ScalarPrefix(ranges);obj=0x21000000;q.map(obj,4096);q.map(0x22000000,4096);q.map(0x49f000,0x6000)
            q.write(obj,b'\xa5'*120);q.reg('S0',obj);q.reg('SP',0x22000800);q.reg('GP',0x4a4170)
            expected=bytearray(q.read(obj,120))
            for offset,value in writes.items():expected[offset:offset+4]=struct.pack('<I',value)
            r=q.run(entry,[stop]);assert q.read(obj,120)==bytes(expected)
            if target is not None:assert q.reg('A0')==obj+target and q.reg('A1')==1
            report['ps2Cases'].append(dict(kind=kind,entry=entry,stop=stop,writes=writes,execution=r));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error),pending=report.get('pending'))));return 1
    print(json.dumps(dict(status='passed',pcOwnershipCases=len(report['pcOwnershipCases']),pcCloneCases=len(report['pcCloneCases']),ps2Cases=len(report['ps2Cases']),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
