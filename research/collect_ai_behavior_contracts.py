#!/usr/bin/env python3
"""Seal paired AI behavior construction identities and active-base evidence."""
import hashlib,json,struct
from pathlib import Path
from capture_native_ranges import ROOT,PC,PS2,EXPECTED,read_elf_sections,read_window,pefile


def main():
    p=ROOT/'local-data/results/native-cycle-20260910-1900/ai-behavior';out=ROOT/'research/ai-behavior-contracts-2026-09-10.json'
    if out.exists():raise ValueError('Sealed evidence is immutable')
    def sha(a):return hashlib.sha256(a.read_bytes()).hexdigest().upper()
    def ref(a):return dict(path=a.relative_to(ROOT).as_posix(),sha256=sha(a))
    rows=json.loads((p/'catalog-family.json').read_text());assert len(rows)==22 and not any(r['priorAssessments'] for r in rows)
    maps={r['className']:r for r in json.loads((p/'ps2-constructor-map.json').read_text())};vt={r['className']:r for r in json.loads((p/'ps2-vtable-candidates.json').read_text())}
    ctors={r['name']:r for r in json.loads((p/'ps2-constructors/capture.json').read_text())['ranges']}
    raw={'pc':PC.read_bytes(),'ps2':PS2.read_bytes()};assert all(hashlib.sha256(raw[k]).hexdigest().upper()==EXPECTED[k] for k in raw)
    containers={'pc':pefile.PE(data=raw['pc'],fast_load=True),'ps2':read_elf_sections(raw['ps2'])}
    report=dict(kind='paired-ai-behavior-construction-and-base-contracts',schemaVersion=1,inputs=EXPECTED,collectorSha256=sha(Path(__file__)),classes=[],
        boundaries='Registered family has22 rows and21 paired factories. PC factory/default clone/teardown execution;PS2 own constructor/getter/vtable independently decoded. No complete derived active behavior or normal binding claim.',
        sources=[ref(p/x) for x in ('catalog-family.json','ps2-constructor-map.json','ps2-vtable-candidates.json','ps2-factories/capture.json','ps2-constructors/capture.json','pc-construction-tables-run1.json','base-scout-windows/capture.json','corrected-base-method-windows/capture.json','base-active-windows/capture.json','base-map-windows/capture.json','map-helper-windows/capture.json','action-selection-run1.json','action-gate-run1.json')])
    for row in rows:
        item=dict(row);name=row['className']
        if not row['pcFactory']:
            assert name=='wxBaseAIBehavior' and not row['ps2Factory'];item['registeredFactory']='zero on each platform;not a C++ abstractness proof';report['classes'].append(item);continue
        path=p/f'pc-{name}-run1.json';pc=json.loads(path.read_text());assert pc['factoryReached']
        initial=pc['initial'];table,off=read_window('pc',raw['pc'],int(initial['vtable'],16),28,containers['pc']);slots=[f'{x:08X}' for x in struct.unpack('<7I',table)];assert slots==initial['slots']
        m,v=maps[name],vt[name];t,offset=read_window('ps2',raw['ps2'],v['vtable'],36,containers['ps2']);words=struct.unpack('<9I',t);assert words[:2]==(0,0) and list(words[2:])==v['slots'][:7]
        g,goff=read_window('ps2',raw['ps2'],v['getter'],12,containers['ps2']);gs=struct.unpack('<3I',g);assert gs[0]>>16==0x3c02 and gs[1]==0x03e00008 and gs[2]>>16==0x2442
        rec=((gs[0]&0xffff)<<16)+struct.unpack('<h',g[8:10])[0];assert rec==row['ps2Record']==v['registration']
        body=ctors[name]['instructions'];end=next(i+2 for i,r in enumerate(body) if r['text']=='jr $ra');body=body[:end]
        assert next(r['text'] for r in body if r['text'].startswith('jal '))=='jal 0x2272a0'
        ops=[s for s in pc['stages'] if s['label'] in ('factory','rtti','clone','delete-clone','delete-original')]
        item['pc']=dict(source=ref(path),status=pc['status'],allocationBytes=initial['allocationBytes'],vtable=initial['vtable'],vtableFileOffset=off,vtableBytes=table.hex(),slots=slots,
            constructedBytes=initial['bytes'],cloneBytes=pc['clone']['bytes'],returnedClassOperations=len(ops),remainingTrackedAllocations=len(pc.get('remainingAllocations',[])) if pc['status']=='passed' else None)
        if pc['status']!='passed':item['pc'].update(pending=pc['pending'],error=pc['error'],lastIp=pc['lastIp'])
        item['ps2']=dict(status='static-construction-reviewed',allocationBytes=m['allocationBytes'],constructor=f'{m["constructor"]:08X}',constructorBody=body,
            vtable=f'{v["vtable"]:08X}',vtableBytes=t.hex(),vtableFileOffset=offset,slots=[f'{x:08X}' for x in words[2:]],getter=f'{v["getter"]:08X}',getterBytes=g.hex(),getterFileOffset=goff,registration=f'{rec:08X}')
        report['classes'].append(item)
    base={}
    for platform,address,count,skip in [('pc',0x702708,25,0),('ps2',0x496780,27,2)]:
        t,off=read_window(platform,raw[platform],address,count*4,containers[platform]);words=struct.unpack('<'+'I'*count,t)
        base[platform]=dict(vtable=f'{address:08X}',vtableBytes=t.hex(),vtableFileOffset=off,slots=[f'{w:08X}' for w in words[skip:]],constructor='00592A00' if platform=='pc' else '002272A0',
            scope='Intermediate original constructor and25 slot interface;no standalone factory/allocation size claim')
    report['base']=base
    callable_rows=[r for r in report['classes'] if r['pcFactory']]
    report['counts']=dict(registeredRows=22,pairedFactories=21,pcFullLifetimes=sum(r['pc']['status']=='passed' for r in callable_rows),pcReturnedClassOperations=sum(r['pc']['returnedClassOperations'] for r in callable_rows),pairedLookupSelectionCases=24,pairedGateCases=13)
    assert report['counts']['pcFullLifetimes']==20 and report['counts']['pcReturnedClassOperations']==103
    for name,count in [('action-selection-run1.json',24),('action-gate-run1.json',13)]:
        d=json.loads((p/name).read_text());assert d['status']=='passed' and len(d['cases'])==count
    report['scoutLabelCorrections']=[dict(path='base-scout-windows/capture.json',range='IceGargoyle-dtor',platform='ps2',correction='231780 was a scouting guess,not the destructor. Actual original-vtable target232100 is captured in corrected-base-method-windows.'),dict(path='corrected-base-method-windows/capture.json',range='BaseAIBehavior-copy',platform='ps2',correction='227140 scout is not the copy entry. Actual vtable slot3 points226200;captured in base-active-windows,with copy tail in base-map-windows.')]
    out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report['counts']))


if __name__=='__main__':main()
