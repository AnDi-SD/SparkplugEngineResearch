#!/usr/bin/env python3
"""Seal reviewed paired family construction evidence without rerunning guests."""
from pathlib import Path
import hashlib,json,struct
from capture_native_ranges import ROOT,PS2,EXPECTED,read_elf_sections,read_window


def main():
    base=ROOT/'local-data/results/native-cycle-20260910-1900/game-flow-family'
    output=ROOT/'research/game-flow-family-contracts-2026-09-10.json'
    if output.exists():raise ValueError('Do not overwrite sealed contracts')
    def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest().upper()
    def ref(path):return dict(path=path.relative_to(ROOT).as_posix(),sha256=sha(path))
    family=json.loads((base/'catalog-family.json').read_text())
    vt={x['className']:x for x in json.loads((base/'ps2-vtable-candidates.json').read_text())}
    factories={x['name']:x for x in json.loads((base/'ps2-factories/capture.json').read_text())['ranges']}
    raw=PS2.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()==EXPECTED['ps2']
    sections=read_elf_sections(raw)
    result=dict(kind='reviewed-paired-game-flow-family-construction',schemaVersion=1,
        executableSha256=EXPECTED,collectorSha256=sha(Path(__file__)),
        scope='PC original bounded lifecycle; PS2 static original construction and independently resolved vtable/getter. No active gameplay or whole-class closure.',
        sources=[ref(base/x) for x in ('catalog-family.json','ps2-vtable-candidates.json','ps2-factories/capture.json','ps2-factories-extended/capture.json','ps2-constructors/capture.json')],classes=[])
    for row in family:
        name=row['className'];item=dict(row)
        pc_path=base/f'pc-{name}-run1.json'
        if pc_path.exists():
            pc=json.loads(pc_path.read_text());item['pc']=dict(status=pc['status'],source=ref(pc_path),factoryReturned=pc['factoryReached'],returnedOperations=len(pc['stages']))
            if pc['status']=='passed':
                assert pc['factoryReached'] and len(pc['stages'])==5 and pc['initial']['baseSlots'][3]=='0040ECE0'
                item['pc'].update(allocationBytes=pc['initial']['allocationBytes'],vtable=pc['initial']['vtable'],slots=pc['initial']['baseSlots'],
                    constructedBytes=pc['initial']['bytes'],cloneBytes=pc['clone']['bytes'],remainingAllocationCount=len(pc['remainingAllocations']))
            else:item['pc'].update(error=pc['error'],lastIp=pc['lastIp'],pending=pc.get('pending'))
        else:
            assert not row['pcFactory'];item['pc']=dict(status='abstract-no-factory',factoryReturned=False,returnedOperations=0)
        v=vt[name];table,offset=read_window('ps2',raw,v['vtable'],60,sections)
        words=struct.unpack('<15I',table);assert words[:2]==(0,0) and list(words[2:])==v['slots'] and words[5]==0x100320
        getter,getter_offset=read_window('ps2',raw,v['getter'],12,sections)
        g=struct.unpack('<3I',getter)
        assert g[0]>>16==0x3c02 and g[1]==0x03e00008 and g[2]>>16==0x2442
        registration=((g[0]&0xffff)<<16)+struct.unpack('<h',getter[8:10])[0]
        assert registration==v['registration']
        item['ps2']=dict(status='static-construction-reviewed',vtable=f'{v["vtable"]:08X}',vtableFileOffset=offset,vtableBytes=table.hex(),
            slots=[f'{a:08X}' for a in v['slots']],getter=f'{v["getter"]:08X}',getterFileOffset=getter_offset,getterBytes=getter.hex(),registration=f'{registration:08X}',
            storeAddress=f'{v["storeAddress"]:08X}',captureFolder=v['captureFolder'],captureRange=v['range'])
        if row['ps2Factory']:
            cap=factories[name];ins=cap['instructions'];allocation=next(i for i,x in enumerate(ins) if x['text']=='jal 0x10d850')
            values=[x for x in ins[:allocation+2] if x['text'].startswith('addiu $a0, $zero,')]
            assert len(values)==1 and allocation<10
            item['ps2'].update(allocationBytes=int(values[0]['text'].split(',')[-1].strip(),0),allocationLiteralAddress=f'{values[0]["va"]:08X}')
        result['classes'].append(item)
    assert len(result['classes'])==34
    result['counts']=dict(classes=34,pcPassed=sum(c['pc']['status']=='passed' for c in result['classes']),pcBlocked=sum(c['pc']['status']=='blocked' for c in result['classes']),
        pcReturnedOperations=sum(c['pc']['returnedOperations'] for c in result['classes']),ps2StaticVtables=len(vt),ps2FactoryAllocations=len(factories))
    output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result['counts']))


if __name__=='__main__':main()
