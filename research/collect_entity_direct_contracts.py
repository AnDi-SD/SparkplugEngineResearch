#!/usr/bin/env python3
"""Seal construction evidence; registered ancestry is not C++ layout proof."""
import hashlib,json,struct
from pathlib import Path
from capture_native_ranges import ROOT,PC,PS2,EXPECTED,read_elf_sections,read_window,pefile


def main():
    p=ROOT/'local-data/results/native-cycle-20260910-1900/entity-direct'
    output=ROOT/'research/entity-direct-construction-contracts-2026-09-10.json'
    if output.exists():raise ValueError('Do not overwrite sealed evidence')
    def sha(a):return hashlib.sha256(a.read_bytes()).hexdigest().upper()
    def ref(a):return dict(path=a.relative_to(ROOT).as_posix(),sha256=sha(a))
    family=json.loads((p/'catalog-family.json').read_text());names={x['className'] for x in family}
    assert len(family)==71
    for platform in ('pc','ps2'):
        cat=json.loads((p.parent/f'catalog/{platform}-architecture.json').read_text())['registered_types']
        assert names=={'wxEntity'}|{x['class_name'] for x in cat if x['base_class_name']=='wxEntity'}
    vtables={x['className']:x for x in json.loads((p/'ps2-vtable-candidates.json').read_text())}
    maps={x['className']:x for x in json.loads((p/'ps2-constructor-map.json').read_text())}
    ctors={}
    for folder in ('ps2-constructors','ps2-constructors-extended'):
        for c in json.loads((p/folder/'capture.json').read_text())['ranges']:
            ins=c['instructions'];end=next((i+2 for i,x in enumerate(ins) if x['text']=='jr $ra'),None)
            if end:ctors[c['name']]=dict(source=ref(p/folder/'capture.json'),body=ins[:end])
    pcraw=PC.read_bytes();psraw=PS2.read_bytes()
    assert hashlib.sha256(pcraw).hexdigest().upper()==EXPECTED['pc'] and hashlib.sha256(psraw).hexdigest().upper()==EXPECTED['ps2']
    pe=pefile.PE(data=pcraw,fast_load=True);sections=read_elf_sections(psraw)
    result=dict(kind='reviewed-paired-entity-registration-family-construction',schemaVersion=1,executableSha256=EXPECTED,
        collectorSha256=sha(Path(__file__)),scope='71 registered rows;67 callable factories including previously assessed CharacterStateMachine. Seven common interface slots only. Own PS2 constructor windows are static, not full execution. RTTI parent is not automatically physical base.',
        sources=[ref(p/x) for x in ('catalog-family.json','ps2-constructor-map.json','ps2-vtable-candidates.json','ps2-factories/capture.json','ps2-constructors/capture.json','ps2-constructors-extended/capture.json','pc-construction-tables-run1.json','construction-identity-windows/capture.json','registration-layout-windows/capture.json','copy-and-base-windows/capture.json')],classes=[])
    for row in family:
        item=dict(row);name=row['className']
        if not row['pcFactory']:
            assert not row['ps2Factory'];item.update(pc=dict(status='no-registered-factory'),ps2=dict(status='no-registered-factory'));result['classes'].append(item);continue
        paths=sorted(p.glob(f'pc-{name}-run*.json'),key=lambda a:int(a.stem.rsplit('run',1)[1]))
        if name=='wxCharacterStateMachine':paths=[p.parent/'character-state-machine/pc-wxCharacterStateMachine-run1.json']
        pcpath=paths[-1];pc=json.loads(pcpath.read_text());assert pc['factoryReached']
        initial=pc['initial'];vt=int(initial['vtable'],16);b,offset=read_window('pc',pcraw,vt,28,pe);slots=[f'{x:08X}' for x in struct.unpack('<7I',b)];assert slots==initial['slots']
        ops=[s for s in pc['stages'] if s['label'] in ('factory','rtti','clone','delete-clone','delete-original')]
        item['pc']=dict(status=pc['status'],source=ref(pcpath),history=[ref(a) for a in paths],factoryReturned=True,returnedClassOperations=len(ops),
            allocationBytes=initial['allocationBytes'],vtable=initial['vtable'],vtableFileOffset=offset,vtableBytes=b.hex(),slots=slots,
            constructedBytes=initial['bytes'],defaultCloneBytes=pc.get('clone',{}).get('bytes'),declaredContext=pc.get('declaredContext'),
            crtFixture=pc.get('crtFixture','none'),remainingAllocations=pc.get('remainingAllocations'))
        assert len(ops)==(5 if pc['status']=='passed' else 2 if pc['pending']['label']=='clone' else 3)
        if pc['status']!='passed':item['pc'].update(error=pc['error'],pending=pc['pending'],lastIp=pc['lastIp'])
        m=maps[name];v=vtables[name];table,toffset=read_window('ps2',psraw,v['vtable'],36,sections);words=struct.unpack('<9I',table)
        assert words[:2]==(0,0) and list(words[2:])==v['slots'][:7]
        getter,goffset=read_window('ps2',psraw,v['getter'],12,sections);g=struct.unpack('<3I',getter)
        assert g[0]>>16==0x3c02 and g[1]==0x03e00008 and g[2]>>16==0x2442
        record=((g[0]&0xffff)<<16)+struct.unpack('<h',getter[8:10])[0];assert record==v['registration']==row['ps2Record']
        calls=[x for x in ctors[name]['body'] if x['text'].startswith('jal ')]
        item['ps2']=dict(status='static-construction-reviewed',allocationBytes=m['allocationBytes'],constructor=f'{m["constructor"]:08X}',
            constructorSource=ctors[name]['source'],vtable=f'{v["vtable"]:08X}',vtableFileOffset=toffset,vtableBytes=table.hex(),slots=[f'{x:08X}' for x in words[2:]],
            getter=f'{v["getter"]:08X}',getterFileOffset=goffset,getterBytes=getter.hex(),registration=f'{record:08X}',constructorCalls=calls,
            constructorStores=[x for x in ctors[name]['body'] if x['text'].startswith(('sw ','sb ','swc1 ')) and '$sp' not in x['text']])
        item['physicalBaseObservation']=('spBaseObject' if name=='wxProjectile' else 'wxGenericTrigger' if name in ('wxKickableIce','wxRockProjectile','wxSavePoint','wxStellaRingTrigger') else 'spEntity' if name=='wxEntity' else 'wxEntity')
        result['classes'].append(item)
    fresh=[x for x in result['classes'] if x['pcFactory'] and not x['priorAssessments']]
    result['counts']=dict(registeredRows=71,noRegisteredFactory=4,callablePaired=67,reusedCharacterStateMachine=1,newCallableClasses=len(fresh),
        newPcFullLifetimes=sum(x['pc']['status']=='passed' for x in fresh),newPcCloneBlocked=sum(x['pc'].get('pending',{}).get('label')=='clone' for x in fresh),
        newPcTeardownBlocked=sum(x['pc'].get('pending',{}).get('label')=='delete-clone' for x in fresh),newPcReturnedClassOperations=sum(x['pc']['returnedClassOperations'] for x in fresh),
        independentlyObservedPhysicalBaseExceptions=5)
    assert result['counts']['newPcFullLifetimes']==61 and result['counts']['newPcReturnedClassOperations']==318
    result['inheritedGenericTriggerEvidence']=dict(pc=ref(p/'pc-construction-tables-run1.json'),ps2=ref(p/'base-scout-windows/capture.json'),
        scope='Intermediate wxGenericTrigger constructor and original vtable/getter identity seen independently. No own factory, own allocation size or complete lifetime established.')
    output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result['counts']))


if __name__=='__main__':main()
