#!/usr/bin/env python3
"""Seal projectile/manager families with independently checked platform identities."""
import hashlib,json,struct
from pathlib import Path
from capture_native_ranges import ROOT,PC,PS2,EXPECTED,read_elf_sections,read_window,pefile


def main():
    cycle=ROOT/'local-data/results/native-cycle-20260910-1900';out=ROOT/'research/projectile-contracts-2026-09-10.json'
    if out.exists():raise ValueError('Sealed evidence is immutable')
    def sha(a):return hashlib.sha256(a.read_bytes()).hexdigest().upper()
    def ref(a):return dict(path=a.relative_to(ROOT).as_posix(),sha256=sha(a))
    raw={'pc':PC.read_bytes(),'ps2':PS2.read_bytes()};assert all(hashlib.sha256(raw[k]).hexdigest().upper()==EXPECTED[k] for k in raw)
    containers={'pc':pefile.PE(data=raw['pc'],fast_load=True),'ps2':read_elf_sections(raw['ps2'])}
    report=dict(kind='paired-projectile-and-manager-contracts',schemaVersion=1,inputs=EXPECTED,collectorSha256=sha(Path(__file__)),classes=[],sources=[],
        boundaries='22 paired identities;21 new PC full class lifetimes,one manager base reused. PC original AnimationManager resolves Projectile Actor dependency. PS2 separate own-constructor identities and bounded active prefixes;no full PS2 lifecycle or trajectory/damage closure.')
    for family,root_name,count in [('projectile','wxProjectile',10),('projectile-manager','wxProjectileManager',12)]:
        p=cycle/family;rows=json.loads((p/'catalog-family.json').read_text());assert len(rows)==count
        maps={r['className']:r for r in json.loads((p/'ps2-constructor-map.json').read_text())};vt={r['className']:r for r in json.loads((p/'ps2-vtable-candidates.json').read_text())};ctors={r['name']:r for r in json.loads((p/'ps2-constructors/capture.json').read_text())['ranges']}
        report['sources'] += [ref(p/x) for x in ('catalog-family.json','ps2-factories/capture.json','ps2-constructors/capture.json','ps2-constructor-map.json','ps2-vtable-candidates.json')]
        for row in rows:
            name=row['className'];item=dict(row,family=family)
            path=cycle/'entity-direct/pc-wxProjectileManager-run2.json' if name=='wxProjectileManager' else p/f'pc-{name}-run{2 if name=="wxProjectile" else 1}.json'
            pc=json.loads(path.read_text());assert pc['factoryReached'] and pc['status']=='passed';initial=pc['initial'];n=11 if family=='projectile' else 18
            table,off=read_window('pc',raw['pc'],int(initial['vtable'],16),n*4,containers['pc']);slots=[f'{x:08X}' for x in struct.unpack('<'+'I'*n,table)];assert slots[:7]==initial['slots']
            m,v=maps[name],vt[name];t,offset=read_window('ps2',raw['ps2'],v['vtable'],(n+2)*4,containers['ps2']);words=struct.unpack('<'+'I'*(n+2),t);assert words[:2]==(0,0) and list(words[2:9])==v['slots'][:7]
            g,goff=read_window('ps2',raw['ps2'],v['getter'],12,containers['ps2']);gs=struct.unpack('<3I',g);assert gs[0]>>16==0x3c02 and gs[1]==0x03e00008 and gs[2]>>16==0x2442
            rec=((gs[0]&0xffff)<<16)+struct.unpack('<h',g[8:10])[0];assert rec==row['ps2Record']==v['registration']
            first=next(x for x in ctors[name]['instructions'] if x['text'].startswith('jal '));base=int(first['text'].split()[1],16)
            assert base==(0x102bf0 if family=='projectile' else 0x286ca0) if name==root_name else base==maps[root_name]['constructor']
            ops=[s for s in pc['stages'] if s['label'] in ('factory','rtti','clone','delete-clone','delete-original')];assert len(ops)==5
            item['pc']=dict(source=ref(path),reused=name=='wxProjectileManager',status='passed',allocationBytes=initial['allocationBytes'],vtable=initial['vtable'],vtableFileOffset=off,vtableBytes=table.hex(),slots=slots,
                constructedBytes=initial['bytes'],cloneBytes=pc['clone']['bytes'],returnedClassOperations=5,remainingTrackedAllocations=len(pc['remainingAllocations']),declaredContext=pc.get('declaredContext',dict(kind='none')))
            item['ps2']=dict(status='static-construction-reviewed',allocationBytes=m['allocationBytes'],constructor=f'{m["constructor"]:08X}',firstConstructorCall=f'{base:08X}',constructorWindowSource=ref(p/'ps2-constructors/capture.json'),
                vtable=f'{v["vtable"]:08X}',vtableBytes=t.hex(),vtableFileOffset=offset,slots=[f'{x:08X}' for x in words[2:]],getter=f'{v["getter"]:08X}',getterBytes=g.hex(),getterFileOffset=goff,registration=f'{rec:08X}')
            item['interfaceScope']=f'First{n} common family slots;derived additions not enumerated'
            if family=='projectile':assert item['pc']['allocationBytes']==item['ps2']['allocationBytes'];assert pc['declaredContext']['emptyRegistryAndManagerDeletionVerified']
            report['classes'].append(item)
    for f,name,count in [('projectile-manager','common-run1.json',35),('projectile','copy-run2.json',6)]:
        d=json.loads((cycle/f/name).read_text());assert d['status']=='passed' and len(d['cases'])==count;report['sources'].append(ref(cycle/f/name))
    assert len(json.loads((cycle/'projectile/copy-run2.json').read_text())['actorCases'])==2
    report['counts']=dict(registeredRows=22,pairedFactories=22,pcFullLifetimes=22,newPcFullLifetimes=21,reusedPcFullLifetimes=1,newPcReturnedClassOperations=105,reusedPcReturnedClassOperations=5,pairedManagerCases=35,pairedProjectileCopyCases=6,pcPopulatedActorCopyCases=2)
    report['limitations']=['Projectile registered parent wxEntity but actual construction spBaseObject on both platforms was proved earlier;not a new discrepancy.','space-dependency-windows scout labels said Collision before RTTI review;the member is spActor and5A1600 configures its playback capacity,not a collision-space binding. Raw windows remain preserved;identities verified via vtables/getters.','New manager-base micro run repeated an already known protected cap;previous passed entity-direct run2 reused,without rerunning base.','Projectile copy-run1 range omitted final allocator JAL/delay words;run2 explicitly includes original2C367C/2C3680. No instructions or game results patched.']
    out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report['counts']))


if __name__=='__main__':main()
