#!/usr/bin/env python3
"""Seal reviewed AIAction construction evidence and independent PS2 identities."""
from pathlib import Path
import hashlib,json,struct
from capture_native_ranges import ROOT,PS2,EXPECTED,read_elf_sections,read_window


def main():
    base=ROOT/'local-data/results/native-cycle-20260910-1900/ai-action'
    output=ROOT/'research/ai-action-construction-contracts-2026-09-10.json'
    if output.exists():raise ValueError('Do not overwrite sealed contracts')
    def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest().upper()
    def ref(path):return dict(path=path.relative_to(ROOT).as_posix(),sha256=sha(path))
    family=json.loads((base/'catalog-family.json').read_text())
    vt={x['className']:x for x in json.loads((base/'ps2-vtable-candidates.json').read_text())}
    factories={x['name']:x for x in json.loads((base/'ps2-factories/capture.json').read_text())['ranges']}
    raw=PS2.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()==EXPECTED['ps2'];sections=read_elf_sections(raw)
    result=dict(kind='reviewed-paired-ai-action-construction',schemaVersion=1,executableSha256=EXPECTED,collectorSha256=sha(Path(__file__)),
        scope='PC original construction/RTTI/clone and35 successful teardowns over explicit empty scene context; PS2 static original construction, vtable/getter and allocation identity. Derived active gameplay and modified-source clone equivalence untested.',
        sources=[ref(base/x) for x in ('catalog-family.json','ps2-vtable-candidates.json','ps2-factories/capture.json','ps2-factories-extended/capture.json','ps2-constructors/capture.json','embedded-pathfinder-identity.json')],classes=[])
    for row in family:
        name=row['className'];pc_path=base/f'pc-{name}-empty-scene-run1.json';pc=json.loads(pc_path.read_text())
        assert pc['factoryReached'] and pc['declaredContext']['kind']=='empty-scene'
        class_ops=[s for s in pc['stages'] if s['label'] in ('factory','rtti','clone','delete-clone','delete-original')]
        if pc['status']=='passed':assert len(class_ops)==5
        else:assert name=='wxIcyAttackAIAction' and len(class_ops)==3
        item=dict(row);item['pc']=dict(status=pc['status'],source=ref(pc_path),factoryReturned=True,returnedClassOperations=len(class_ops),
            allocationBytes=pc['initial']['allocationBytes'],vtable=pc['initial']['vtable'],slots=pc['initial']['slots'],
            constructedBytes=pc['initial']['bytes'],cloneBytes=pc['clone']['bytes'],remainingAllocationCount=len([a for a in pc['allocations'] if not a['freed']]))
        if pc['status']!='passed':item['pc'].update(error=pc['error'],lastIp=pc['lastIp'],pending=pc['pending'])
        v=vt[name];table,offset=read_window('ps2',raw,v['vtable'],76,sections);words=struct.unpack('<19I',table)
        assert words[:2]==(0,0) and list(words[2:15])==v['slots']
        getter,getter_offset=read_window('ps2',raw,v['getter'],12,sections);g=struct.unpack('<3I',getter)
        assert g[0]>>16==0x3c02 and g[1]==0x03e00008 and g[2]>>16==0x2442
        registration=((g[0]&0xffff)<<16)+struct.unpack('<h',getter[8:10])[0];assert registration==v['registration']==row['ps2Record']
        ins=factories[name]['instructions'];a=next(i for i,x in enumerate(ins) if x['text']=='jal 0x10d850')
        values=[x for x in ins[:a+2] if x['text'].startswith('addiu $a0, $zero,')];assert len(values)==1 and a<10
        size=int(values[0]['text'].split(',')[-1].strip(),0);assert size==item['pc']['allocationBytes']+4
        item['ps2']=dict(status='static-construction-reviewed',allocationBytes=size,allocationLiteralAddress=f'{values[0]["va"]:08X}',
            vtable=f'{v["vtable"]:08X}',vtableFileOffset=offset,vtableBytes=table.hex(),slots=[f'{a:08X}' for a in words[2:]],
            getter=f'{v["getter"]:08X}',getterFileOffset=getter_offset,getterBytes=getter.hex(),registration=f'{registration:08X}',
            storeAddress=f'{v["storeAddress"]:08X}',captureFolder=v['captureFolder'],captureRange=v['range'])
        result['classes'].append(item)
    assert len(result['classes'])==36
    result['counts']=dict(classes=36,pcFactoriesReturned=36,pcLifecyclePassed=35,pcTeardownBlocked=1,
        pcReturnedClassOperations=sum(c['pc']['returnedClassOperations'] for c in result['classes']),ps2StaticIdentities=36)
    output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result['counts']))


if __name__=='__main__':main()
