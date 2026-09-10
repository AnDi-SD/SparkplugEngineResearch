#!/usr/bin/env python3
"""Seal independently checked GUI construction and paired active contracts."""
import hashlib,json,struct
from pathlib import Path
from capture_native_ranges import ROOT,PC,PS2,EXPECTED,read_elf_sections,read_window,pefile


def main():
    p=ROOT/'local-data/results/native-cycle-20260910-1900/gui-object';out=ROOT/'research/gui-object-contracts-2026-09-10.json'
    if out.exists():raise ValueError('Sealed evidence is immutable')
    def sha(a):return hashlib.sha256(a.read_bytes()).hexdigest().upper()
    def ref(a):return dict(path=a.relative_to(ROOT).as_posix(),sha256=sha(a))
    rows=json.loads((p/'catalog-family.json').read_text());assert len(rows)==6 and not any(r['priorAssessments'] for r in rows)
    maps={r['className']:r for r in json.loads((p/'ps2-constructor-map.json').read_text())};vt={r['className']:r for r in json.loads((p/'ps2-vtable-candidates.json').read_text())}
    raw={'pc':PC.read_bytes(),'ps2':PS2.read_bytes()};assert all(hashlib.sha256(raw[k]).hexdigest().upper()==EXPECTED[k] for k in raw)
    containers={'pc':pefile.PE(data=raw['pc'],fast_load=True),'ps2':read_elf_sections(raw['ps2'])}
    report=dict(kind='paired-gui-object-construction-and-active-contracts',schemaVersion=1,inputs=EXPECTED,collectorSha256=sha(Path(__file__)),classes=[],
        boundaries='Six PC factory/default clone/RTTI/two teardowns;PS2 independent construction identity. Widget/Button selection and GUI notify/setter contracts. No rendered GUI,normal scene binding,text editing or full PS2 lifecycle claim.',
        sources=[ref(p/x) for x in ('catalog-family.json','ps2-constructor-map.json','ps2-vtable-candidates.json','ps2-factories/capture.json','ps2-factories-extended/capture.json','ps2-constructors/capture.json','base-constructor-windows/capture.json','pc-base-method-windows/capture.json','ps2-state-method-windows/capture.json','notify-windows/capture.json','state-selection-run1.json','state-selection-run2.json','dispatch-run1.json')])
    for row in rows:
        name=row['className'];item=dict(row);path=p/f'pc-{name}-run{2 if name=="wxButton" else 1}.json';pc=json.loads(path.read_text());assert pc['factoryReached'] and pc['status']=='passed'
        count=11 if name=='spGUIObject' else 12;initial=pc['initial'];table,off=read_window('pc',raw['pc'],int(initial['vtable'],16),count*4,containers['pc']);slots=[f'{x:08X}' for x in struct.unpack('<'+'I'*count,table)];assert slots[:7]==initial['slots']
        m,v=maps[name],vt[name];t,offset=read_window('ps2',raw['ps2'],v['vtable'],(count+2)*4,containers['ps2']);words=struct.unpack('<'+'I'*(count+2),t);assert words[:2]==(0,0) and list(words[2:])==v['slots'][:count]
        g,goff=read_window('ps2',raw['ps2'],v['getter'],12,containers['ps2']);gs=struct.unpack('<3I',g);assert gs[0]>>16==0x3c02 and gs[1]==0x03e00008 and gs[2]>>16==0x2442
        rec=((gs[0]&0xffff)<<16)+struct.unpack('<h',g[8:10])[0];assert rec==row['ps2Record']==v['registration']
        ops=[s for s in pc['stages'] if s['label'] in ('factory','rtti','clone','delete-clone','delete-original')];assert len(ops)==5
        item['pc']=dict(source=ref(path),status='passed',allocationBytes=initial['allocationBytes'],vtable=initial['vtable'],vtableFileOffset=off,vtableBytes=table.hex(),slots=slots,
            constructedBytes=initial['bytes'],cloneBytes=pc['clone']['bytes'],returnedClassOperations=5,remainingTrackedAllocations=len(pc['remainingAllocations']),crt=pc.get('crt'))
        item['ps2']=dict(status='static-construction-reviewed',allocationBytes=m['allocationBytes'],constructionEntry=f'{m["constructor"]:08X}',constructionKind=m['constructionKind'],factoryCalls=m['factoryCalls'],
            vtable=f'{v["vtable"]:08X}',vtableBytes=t.hex(),vtableFileOffset=offset,slots=[f'{x:08X}' for x in words[2:]],getter=f'{v["getter"]:08X}',getterBytes=g.hex(),getterFileOffset=goff,registration=f'{rec:08X}')
        assert item['pc']['allocationBytes']-4==item['ps2']['allocationBytes']
        item['interfaceScope']='11 inherited GUI slots' if count==11 else 'First12 GUI/Widget slots;wxButton has additional slots not enumerated here'
        report['classes'].append(item)
    for name,count in [('state-selection-run2.json',18),('dispatch-run1.json',27)]:
        d=json.loads((p/name).read_text());assert d['status']=='passed' and len(d['cases'])==count
    report['counts']=dict(registeredRows=6,pairedFactories=6,pcFullLifetimes=6,pcReturnedClassOperations=30,pairedStateSelectionCases=18,pairedDispatchCases=27)
    report['boundariesAndFailures']=['wxButton run1 clone stopped at unbound CRT strncpy;run2 uses existing bounded-strings fixture.','PS2 GUIObject and TextWidget factory inline their own vtable/defaults;first parent constructor call is not their entire construction. GUIObject separately has own constructor165860.','PS2 selection run1 stopped at original MOVZ16BC28 in default R4000;run2 uses opt-in5KC integer allowlist with9 targeted wrapper tests. Both runs and source versions preserved.','PS2 selection prefixes stop before original Node enable1A5B00 or SQ/LQ restoration;post-hide prefix declares previously verified original Node enable contract.']
    out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report['counts']))


if __name__=='__main__':main()
