#!/usr/bin/env python3
"""Seal CharacterState lifecycle captures and independent PS2 static contracts."""
from pathlib import Path
import hashlib,json,struct
from capture_native_ranges import ROOT,PC,PS2,EXPECTED,read_elf_sections,read_window,pefile
from inspect_executable_architecture import apply_mips_constant


def main():
    base=ROOT/'local-data/results/native-cycle-20260910-1900/character-state'
    output=ROOT/'research/character-state-construction-contracts-2026-09-10.json'
    if output.exists():raise ValueError('Do not overwrite sealed contracts')
    def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
    def ref(p):return dict(path=p.relative_to(ROOT).as_posix(),sha256=sha(p))
    family=json.loads((base/'catalog-family.json').read_text())
    catalog=json.loads((base.parent/'catalog/ps2-architecture.json').read_text())['registered_types']
    assert {x['className'] for x in family}=={x['class_name'] for x in catalog if x['class_name']=='wxCharacterState' or x['base_class_name']=='wxCharacterState'}
    maps={x['className']:x for x in json.loads((base/'ps2-constructor-map.json').read_text())}
    vt={x['className']:x for x in json.loads((base/'ps2-vtable-candidates.json').read_text())}
    paths=['catalog-family.json','ps2-constructor-map.json','ps2-vtable-candidates.json','ps2-factories/capture.json',
           'ps2-factories-extended/capture.json','ps2-constructors/capture.json','extended-and-failure-windows/capture.json']
    factories={};constructors={}
    for folder in ('ps2-factories','ps2-factories-extended','ps2-constructors','extended-and-failure-windows'):
        for c in json.loads((base/folder/'capture.json').read_text())['ranges']:
            if c['platform']!='ps2':continue
            c=dict(c,sourceFolder=folder)
            if folder.startswith('ps2-factories'):factories[c['name']]=c
            else:constructors[c['address']]=c
    def body(c):
        ins=c['instructions'];end=next(i+2 for i,x in enumerate(ins) if x['text']=='jr $ra');return ins[:end]
    psraw=PS2.read_bytes();pcraw=PC.read_bytes()
    assert hashlib.sha256(psraw).hexdigest().upper()==EXPECTED['ps2'] and hashlib.sha256(pcraw).hexdigest().upper()==EXPECTED['pc']
    sections=read_elf_sections(psraw);pe=pefile.PE(data=pcraw,fast_load=True)
    result=dict(kind='reviewed-paired-character-state-construction',schemaVersion=1,executableSha256=EXPECTED,collectorSha256=sha(Path(__file__)),
        scope='97 independently matched registered classes. PC original factory/RTTI/default clone;95 complete factory-only lifetimes. PS2 static sizes, constructor stores and exact vtable/getter identity. Active derived state algorithms remain open.',
        sources=[ref(base/p) for p in paths],classes=[])
    base_tables={}
    for row in family:
        name=row['className'];pcpath=base/f'pc-{name}-run{2 if name=="wxBookMovingState" else 1}.json';pc=json.loads(pcpath.read_text());m=maps[name]
        assert pc['factoryReached'];ops=[s for s in pc['stages'] if s['label'] in ('factory','rtti','clone','delete-clone','delete-original')]
        assert len(ops)==(5 if pc['status']=='passed' else 3)
        assert pc['status']=='passed' or name in ('wxBeforeTrollFightState','wxGoopMonsterMovingState')
        pctable,pcoffset=read_window('pc',pcraw,int(pc['initial']['vtable'],16),68,pe);pcslots=[f'{x:08X}' for x in struct.unpack('<17I',pctable)]
        assert pcslots[:7]==pc['initial']['slots'] and pcslots[3]=='0040ECE0'
        v=vt[name];table,offset=read_window('ps2',psraw,v['vtable'],76,sections);words=struct.unpack('<19I',table)
        assert words[:2]==(0,0) and list(words[2:15])==v['slots'] and words[5]==0x100320
        getter,goffset=read_window('ps2',psraw,v['getter'],12,sections);g=struct.unpack('<3I',getter)
        assert g[0]>>16==0x3c02 and g[1]==0x03e00008 and g[2]>>16==0x2442
        record=((g[0]&0xffff)<<16)+struct.unpack('<h',getter[8:10])[0];assert record==v['registration']==row['ps2Record']
        fi=body(factories[name]);alloc=next(i for i,x in enumerate(fi) if x['text']=='jal 0x10d850')
        literals=[x for x in fi[:alloc+2] if x['text'].startswith('addiu $a0, $zero,')];assert len(literals)==1
        size=int(literals[0]['text'].split(',')[-1].strip(),0);assert size==m['allocationBytes']==pc['initial']['allocationBytes']
        # Twelve factories contain their own derived initialization after the
        # base call. Never label the common base constructor as their full body.
        own=factories[name] if m['constructor']==0x2c9150 and name!='wxCharacterState' else constructors[m['constructor']]
        ins=body(own);assert name=='wxCharacterState' or any(x['text']=='jal 0x2c9150' for x in ins)
        selector=0;stores=[]
        for i,x in enumerate(ins):
            w=int.from_bytes(bytes.fromhex(x['bytes']),'little')
            if w>>26==0x2b and w&0xffff==0x10 and (w>>21)&31==16:
                constants={0:0}
                start=max([j+2 for j,p in enumerate(ins[:i]) if p['text'].startswith('jal ')],default=0)
                for prior in ins[start:i]:
                    pw=int.from_bytes(bytes.fromhex(prior['bytes']),'little')
                    if pw>>26==3:constants={k:v for k,v in constants.items() if k==0 or 16<=k<=23}
                    else:apply_mips_constant(pw,constants)
                value=constants.get((w>>16)&31);assert value is not None,(name,x)
                selector=value&0xffffffff;stores.append(dict(address=f'{x["va"]:08X}',value=selector))
        initial=bytes.fromhex(pc['initial']['bytes']);assert selector==int.from_bytes(initial[16:20],'little'),name
        item=dict(row,pc=dict(status=pc['status'],source=ref(pcpath),returnedClassOperations=len(ops),
            allocationBytes=size,constructedBytes=pc['initial']['bytes'],defaultCloneBytes=pc['clone']['bytes'],
            vtable=pc['initial']['vtable'],vtableFileOffset=pcoffset,vtableBytes=pctable.hex(),slots=pcslots,
            context=pc.get('declaredContext',dict(kind='none')),stateSelector10=selector),
            ps2=dict(status='static-construction-reviewed',allocationBytes=size,allocationLiteralAddress=f'{literals[0]["va"]:08X}',
                vtable=f'{v["vtable"]:08X}',vtableFileOffset=offset,vtableBytes=table.hex(),slots=[f'{x:08X}' for x in words[2:]],
                getter=f'{v["getter"]:08X}',getterFileOffset=goffset,getterBytes=getter.hex(),registration=f'{record:08X}',
                constructor=f'{m["constructor"]:08X}',ownInitializationRange=own['name'],ownInitializationFolder=own['sourceFolder'],
                stateSelector10=selector,selectorStores=stores,selectorSource='own literal store' if stores else 'inherited base zero; no own selector write in captured body',
                directCalls=[x for x in ins if x['text'].startswith('jal ')],
                storeInstructions=[x for x in ins if x['text'].startswith(('sw ','sb ','swc1 ')) and '$sp' not in x['text']]))
        if pc['status']!='passed':item['pc'].update(error=pc['error'],pending=pc['pending'],lastIp=pc['lastIp'])
        result['classes'].append(item)
        if name=='wxCharacterState':base_tables={p:item[p]['slots'] for p in ('pc','ps2')}
    assert len(result['classes'])==97
    result['inheritedBaseSlots']={p:{str(i):sum(c[p]['slots'][i]==base_tables[p][i] for c in result['classes']) for i in range(17)} for p in ('pc','ps2')}
    result['counts']=dict(classes=97,pcFactoriesReturned=97,pcLifecyclePassed=95,pcTeardownBlocked=2,
        pcReturnedClassOperations=sum(c['pc']['returnedClassOperations'] for c in result['classes']),ps2IndependentIdentities=97,
        pairedAllocationSizes=97,pairedSelectorDefaults=97,emptyInheritedCopySlots=97)
    output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result['counts']));print(json.dumps(result['inheritedBaseSlots']))


if __name__=='__main__':main()
