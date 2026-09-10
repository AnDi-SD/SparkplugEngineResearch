#!/usr/bin/env python3
"""Seal paired generic trigger construction identities and active-base evidence."""
import hashlib,json,struct
from pathlib import Path
from capture_native_ranges import ROOT,PC,PS2,EXPECTED,read_elf_sections,read_window,pefile


def main():
    p=ROOT/'local-data/results/native-cycle-20260910-1900/generic-trigger';out=ROOT/'research/generic-trigger-contracts-2026-09-10.json'
    if out.exists():raise ValueError('Sealed evidence is immutable')
    def sha(a):return hashlib.sha256(a.read_bytes()).hexdigest().upper()
    def ref(a):return dict(path=a.relative_to(ROOT).as_posix(),sha256=sha(a))
    rows=json.loads((p/'catalog-family.json').read_text());assert len(rows)==43 and [r['className'] for r in rows if r['priorAssessments']]==['wxGenericTrigger']
    maps={r['className']:r for r in json.loads((p/'ps2-constructor-map.json').read_text())};vt={r['className']:r for r in json.loads((p/'ps2-vtable-candidates.json').read_text())}
    ctors={r['name']:r for r in json.loads((p/'ps2-constructors/capture.json').read_text())['ranges']}
    raw={'pc':PC.read_bytes(),'ps2':PS2.read_bytes()};assert all(hashlib.sha256(raw[k]).hexdigest().upper()==EXPECTED[k] for k in raw)
    containers={'pc':pefile.PE(data=raw['pc'],fast_load=True),'ps2':read_elf_sections(raw['ps2'])}
    report=dict(kind='paired-generic-trigger-construction-and-base-contracts',schemaVersion=1,inputs=EXPECTED,collectorSha256=sha(Path(__file__)),classes=[],
        boundaries='Registered recursive family has43 rows and41 paired factories. PC original factory/clone/teardown with explicit context;PS2 own constructor/getter/vtable independently decoded. No complete derived active behavior or normal binding claim.',
        sources=[ref(p/x) for x in ('catalog-family.json','ps2-constructor-map.json','ps2-vtable-candidates.json','ps2-factories/capture.json','ps2-constructors/capture.json','pc-construction-tables-run1.json','base-context-windows/capture.json','base-method-windows/capture.json','base-active-correct-windows/capture.json','base-identity-windows/capture.json','remaining-context-identity-windows/capture.json','tick-dependency-windows/capture.json','common-run1.json','tick-run1.json')])
    for row in rows:
        item=dict(row);name=row['className']
        if not row['pcFactory']:
            assert name in ('wxGenericTrigger','wxGenericSideQuest') and not row['ps2Factory'];item['registeredFactory']='zero on each platform;not a C++ abstractness proof';report['classes'].append(item);continue
        history=sorted(p.glob(f'pc-{name}-run*.json'),key=lambda a:int(a.stem.rsplit('run',1)[1]));path=history[-1];pc=json.loads(path.read_text());assert pc['factoryReached'] and pc['status']=='passed'
        initial=pc['initial'];table,off=read_window('pc',raw['pc'],int(initial['vtable'],16),28,containers['pc']);slots=[f'{x:08X}' for x in struct.unpack('<7I',table)];assert slots==initial['slots']
        m,v=maps[name],vt[name];t,offset=read_window('ps2',raw['ps2'],v['vtable'],36,containers['ps2']);words=struct.unpack('<9I',t);assert words[:2]==(0,0) and list(words[2:])==v['slots'][:7]
        g,goff=read_window('ps2',raw['ps2'],v['getter'],12,containers['ps2']);gs=struct.unpack('<3I',g);assert gs[0]>>16==0x3c02 and gs[1]==0x03e00008 and gs[2]>>16==0x2442
        rec=((gs[0]&0xffff)<<16)+struct.unpack('<h',g[8:10])[0];assert rec==row['ps2Record']==v['registration']
        body=ctors[name]['instructions'];end=next(i+2 for i,r in enumerate(body) if r['text']=='jr $ra');body=body[:end]
        first_call=next(r['text'] for r in body if r['text'].startswith('jal '));assert first_call.startswith('jal ')
        ops=[s for s in pc['stages'] if s['label'] in ('factory','rtti','clone','delete-clone','delete-original')]
        item['pc']=dict(source=ref(path),history=[ref(a) for a in history],declaredContext=pc.get('declaredContext'),status=pc['status'],allocationBytes=initial['allocationBytes'],vtable=initial['vtable'],vtableFileOffset=off,vtableBytes=table.hex(),slots=slots,
            constructedBytes=initial['bytes'],cloneBytes=pc['clone']['bytes'],returnedClassOperations=len(ops),remainingTrackedAllocations=len(pc.get('remainingAllocations',[])) if pc['status']=='passed' else None)
        if pc['status']!='passed':item['pc'].update(pending=pc['pending'],error=pc['error'],lastIp=pc['lastIp'])
        item['ps2']=dict(status='static-construction-reviewed',allocationBytes=m['allocationBytes'],constructor=f'{m["constructor"]:08X}',constructorBody=body,
            vtable=f'{v["vtable"]:08X}',vtableBytes=t.hex(),vtableFileOffset=offset,slots=[f'{x:08X}' for x in words[2:]],getter=f'{v["getter"]:08X}',getterBytes=g.hex(),getterFileOffset=goff,registration=f'{rec:08X}')
        report['classes'].append(item)
    base={}
    for platform,address,count,skip in [('pc',0x7024b0,25,0),('ps2',0x498030,27,2)]:
        t,off=read_window(platform,raw[platform],address,count*4,containers[platform]);words=struct.unpack('<'+'I'*count,t)
        base[platform]=dict(vtable=f'{address:08X}',vtableBytes=t.hex(),vtableFileOffset=off,slots=[f'{w:08X}' for w in words[skip:]],constructor='00590110' if platform=='pc' else '003AD760',
            scope='Intermediate original constructor and25 slot interface;no standalone factory/allocation size claim')
    report['base']=base
    report['genericSideQuestIdentity']={}
    for platform,address,skip in [('pc',0x702588,0),('ps2',0x496fa0,2)]:
        table,off=read_window(platform,raw[platform],address,(7+skip)*4,containers[platform]);words=struct.unpack('<'+'I'*(7+skip),table)
        getter=words[4+skip];gb,goff=read_window(platform,raw[platform],getter,6 if platform=='pc' else 12,containers[platform])
        record=int.from_bytes(gb[1:5],'little') if platform=='pc' else (int.from_bytes(gb[:2],'little')<<16)+struct.unpack('<h',gb[8:10])[0]
        expected=next(r[platform+'Record'] for r in rows if r['className']=='wxGenericSideQuest');assert record==expected
        report['genericSideQuestIdentity'][platform]=dict(vtable=f'{address:08X}',vtableBytes=table.hex(),vtableFileOffset=off,getter=f'{getter:08X}',getterBytes=gb.hex(),getterFileOffset=goff,registration=f'{record:08X}')
    callable_rows=[r for r in report['classes'] if r['pcFactory']]
    report['counts']=dict(registeredRows=43,pairedFactories=41,pcFullLifetimes=sum(r['pc']['status']=='passed' for r in callable_rows),pcReturnedClassOperations=sum(r['pc']['returnedClassOperations'] for r in callable_rows),pairedCommonCases=21,pairedTickCases=14)
    assert report['counts']['pcFullLifetimes']==41 and report['counts']['pcReturnedClassOperations']==205
    for name,count in [('common-run1.json',21),('tick-run1.json',14)]:
        d=json.loads((p/name).read_text());assert d['status']=='passed' and len(d['cases'])==count
    report['registeredVsConstructionExceptions']=[dict(className='wxSideQuestMarcusCersei',registeredBase='wxGenericSideQuest',constructorBase='wxOutfitArtsSideQuest',
        pc=ref(p/'pc-construction-tables-marcus-run2.json'),ps2=ref(p/'ps2-constructors/capture.json'),registration=ref(p/'marcus-registration-windows/capture.json'),
        scope='Original registration names GenericSideQuest on both platforms;PC intermediate vtable writes and PS2 direct ctor call3D8310 independently include OutfitArtsSideQuest. Do not change game registration.')]
    report['unusedScoutLabels']=['base-method-windows:stella-deletion-call is an unused guessed window. Actual deleting wrapper58ABF0 and body58A590 are in the corrected active/context captures.',
        'Unimported draft used590100 for the base constructor;address validation showed this is a getter,whereas constructor starts590110. Draft retained locally before corrected collection.']
    out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report['counts']))


if __name__=='__main__':main()
