#!/usr/bin/env python3
"""Seal independent CharacterStateMachine construction and identity evidence."""
from pathlib import Path
import hashlib,json,struct
from capture_native_ranges import ROOT,PC,PS2,EXPECTED,read_elf_sections,read_window,pefile
from inspect_executable_architecture import apply_mips_constant


def main():
    p=ROOT/'local-data/results/native-cycle-20260910-1900/character-state-machine'
    output=ROOT/'research/character-machine-construction-contracts-2026-09-10.json'
    if output.exists():raise ValueError('Do not overwrite sealed evidence')
    def sha(a):return hashlib.sha256(a.read_bytes()).hexdigest().upper()
    def ref(a):return dict(path=a.relative_to(ROOT).as_posix(),sha256=sha(a))
    family=json.loads((p/'catalog-family.json').read_text());byname={x['className']:x for x in family}
    assert len(family)==36
    for platform in ('pc','ps2'):
        cat=json.loads((p.parent/f'catalog/{platform}-architecture.json').read_text())['registered_types'];names={'wxCharacterStateMachine'}
        while True:
            expanded=names|{x['class_name'] for x in cat if x['base_class_name'] in names}
            if expanded==names:break
            names=expanded
        assert names==set(byname)
    vt={x['className']:x for x in json.loads((p/'ps2-vtable-candidates.json').read_text())}
    maps={x['className']:x for x in json.loads((p/'ps2-constructor-map.json').read_text())}
    ctors={x['name']:x for x in json.loads((p/'ps2-constructors/capture.json').read_text())['ranges']}
    own_codes={};own_stores={}
    for name,c in ctors.items():
        ins=c['instructions'];ins=ins[:next(i+2 for i,x in enumerate(ins) if x['text']=='jr $ra')];c['reviewedBody']=ins
        for i,x in enumerate(ins):
            w=int.from_bytes(bytes.fromhex(x['bytes']),'little')
            if w>>26!=0x2b or w&0xffff!=0x134 or (w>>21)&31!=16:continue
            constants={0:0};start=max([j+2 for j,q in enumerate(ins[:i]) if q['text'].startswith('jal ')],default=0)
            for q in ins[start:i]:apply_mips_constant(int.from_bytes(bytes.fromhex(q['bytes']),'little'),constants)
            value=constants.get((w>>16)&31);assert value is not None,(name,x)
            own_codes[name]=value;own_stores[name]=x
    def kind_code(name):return own_codes[name] if name in own_codes else kind_code(byname[name]['base'])
    pcraw=PC.read_bytes();psraw=PS2.read_bytes();assert hashlib.sha256(pcraw).hexdigest().upper()==EXPECTED['pc'] and hashlib.sha256(psraw).hexdigest().upper()==EXPECTED['ps2']
    pe=pefile.PE(data=pcraw,fast_load=True);sections=read_elf_sections(psraw)
    result=dict(kind='reviewed-paired-character-machine-construction',schemaVersion=1,executableSha256=EXPECTED,collectorSha256=sha(Path(__file__)),
        scope='36 independently catalogued machines.35 PC original factories/default clones,34 complete lifetimes;Bloom factory and Goop teardown stopped. PS2 construction is static.21 inherited table slots captured;derived extensions not presumed absent.',
        sources=[ref(p/x) for x in ('catalog-family.json','ps2-constructor-map.json','ps2-vtable-candidates.json','ps2-factories/capture.json','ps2-constructors/capture.json')],classes=[])
    for row in family:
        name=row['className'];pcpath=p/f'pc-{name}-run1.json';pc=json.loads(pcpath.read_text());m=maps[name];v=vt[name];code=kind_code(name)
        item=dict(row)
        ops=[s for s in pc['stages'] if s['label'] in ('factory','rtti','clone','delete-clone','delete-original')]
        item['pc']=dict(status=pc['status'],source=ref(pcpath),factoryReturned=pc['factoryReached'],returnedClassOperations=len(ops))
        if pc['factoryReached']:
            assert len(ops)==(5 if pc['status']=='passed' else 3)
            b,offset=read_window('pc',pcraw,int(pc['initial']['vtable'],16),84,pe);slots=[f'{x:08X}' for x in struct.unpack('<21I',b)];assert slots[:7]==pc['initial']['slots']
            initial=bytes.fromhex(pc['initial']['bytes']);assert int.from_bytes(initial[0x128:0x12c],'little')==code,name
            item['pc'].update(allocationBytes=pc['initial']['allocationBytes'],kindField128=code,vtable=pc['initial']['vtable'],vtableFileOffset=offset,vtableBytes=b.hex(),slots=slots,
                constructedBytes=pc['initial']['bytes'],defaultCloneBytes=pc['clone']['bytes'],remainingAllocations=pc.get('remainingAllocations'))
        else:assert name=='wxBloomStateMachine'
        if pc['status']!='passed':item['pc'].update(error=pc['error'],pending=pc['pending'],lastIp=pc['lastIp'])
        table,toffset=read_window('ps2',psraw,v['vtable'],92,sections);words=struct.unpack('<23I',table);assert words[:2]==(0,0) and list(words[2:15])==v['slots']
        getter,goffset=read_window('ps2',psraw,v['getter'],12,sections);g=struct.unpack('<3I',getter)
        assert g[0]>>16==0x3c02 and g[1]==0x03e00008 and g[2]>>16==0x2442
        record=((g[0]&0xffff)<<16)+struct.unpack('<h',getter[8:10])[0];assert record==v['registration']==row['ps2Record']
        item['ps2']=dict(status='static-construction-reviewed',allocationBytes=m['allocationBytes'],kindField134=code,kindOwnStore=own_stores.get(name),
            kindSource='own literal' if name in own_codes else 'inherited '+row['base'],constructor=f'{m["constructor"]:08X}',
            vtable=f'{v["vtable"]:08X}',vtableFileOffset=toffset,vtableBytes=table.hex(),slots=[f'{x:08X}' for x in words[2:]],
            getter=f'{v["getter"]:08X}',getterFileOffset=goffset,getterBytes=getter.hex(),registration=f'{record:08X}',
            constructorCalls=[x for x in ctors[name]['reviewedBody'] if x['text'].startswith('jal ')],
            constructorStores=[x for x in ctors[name]['reviewedBody'] if x['text'].startswith(('sw ','sb ','swc1 ')) and '$sp' not in x['text']])
        result['classes'].append(item)
    result['counts']=dict(classes=36,pcFactoriesReturned=35,pcFullLifetimes=34,pcFactoryBlocked=1,pcTeardownBlocked=1,
        pcReturnedClassOperations=sum(x['pc']['returnedClassOperations'] for x in result['classes']),ps2IndependentIdentities=36,pairedKindDefaults=35)
    output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result['counts']))


if __name__=='__main__':main()
