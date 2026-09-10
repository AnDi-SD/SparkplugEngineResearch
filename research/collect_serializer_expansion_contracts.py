#!/usr/bin/env python3
"""Seal only newly reviewed PC/PS2 serializer rows; preserve existing assessments."""
import hashlib,json,struct
from pathlib import Path
from capture_native_ranges import ROOT,PC,EXPECTED,read_window,pefile


def main():
    p=ROOT/'local-data/results/native-cycle-20260910-1900/serializer-expansion';out=ROOT/'research/serializer-expansion-contracts-2026-09-10.json'
    if out.exists():raise ValueError('Sealed evidence is immutable')
    def sha(a):return hashlib.sha256(a.read_bytes()).hexdigest().upper()
    def ref(a):return dict(path=a.relative_to(ROOT).as_posix(),sha256=sha(a))
    rows=json.loads((p/'catalog-family.json').read_text());ps2={r['className']:r for r in json.loads((p/'ps2-inline-identities.json').read_text())};parents={int(r['range'].split('-')[1],16):r for r in json.loads((p/'ps2-parent-vtable-identities.json').read_text())}
    raw=PC.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()==EXPECTED['pc'];pe=pefile.PE(data=raw,fast_load=True)
    report=dict(kind='independent-serializer-platform-expansion',schemaVersion=1,inputs=EXPECTED,collectorSha256=sha(Path(__file__)),classes=[],
        boundaries='34 class names selected because at least one platform was unassessed.20 new PC complete concrete lifetimes;29 PS2 independent factory/table identities. Existing13 PC assessments remain unchanged;one PS2-only class and5 PC-only classes. No complete new payload codec claim.',
        sources=[ref(p/x) for x in ('catalog-family.json','ps2-factories/capture.json','ps2-separate-constructor/capture.json','ps2-inline-identities.json','ps2-parent-constructors/capture.json','ps2-parent-vtable-identities.json','secondary-dispatch-run1.json','clone-pilot-windows/capture.json','secondary-pilot-windows/capture.json')])
    for row in rows:
        item=dict(row);name=row['className']
        if 'pc' in row['selectedPlatforms']:
            path=p/f'pc-{name}-run{2 if name=="spProjectionSerializer" else 1}.json';r=json.loads(path.read_text());assert r['status']=='passed' and r['factoryReached'];a=r['initial'];table,off=read_window('pc',raw,int(a['vtable'],16),28,pe);slots=[f'{w:08X}' for w in struct.unpack('<7I',table)];assert slots==a['slots']
            secondary=int.from_bytes(bytes.fromhex(a['bytes'])[16:20],'little');s,soff=read_window('pc',raw,secondary,12,pe);ops=[x for x in r['stages'] if x['label'] in ('factory','rtti','clone','delete-clone','delete-original')];assert len(ops)==5
            item['pc']=dict(status='original-lifetime-passed',source=ref(path),allocationBytes=a['allocationBytes'],vtable=a['vtable'],vtableBytes=table.hex(),vtableFileOffset=off,slots=slots,secondaryVtable=f'{secondary:08X}',secondaryBytes=s.hex(),secondaryFileOffset=soff,secondaryFirstThreeSlots=[f'{w:08X}' for w in struct.unpack('<3I',s)],constructedBytes=a['bytes'],cloneBytes=r['clone']['bytes'],remainingTrackedAllocations=len(r['remainingAllocations']))
            assert a['allocationBytes']==20 and slots[3]=='0040ECE0'
        if 'ps2' in row['selectedPlatforms']:
            v=ps2[name];parent=parents[v['parentConstructor']];assert parent['className']==row['base'];item['ps2']=dict(v,source=ref(p/'ps2-inline-identities.json'),parentClass=parent['className'],parentIdentitySource=ref(p/'ps2-parent-vtable-identities.json'))
            assert v['slots'][3]==0x100320
        report['classes'].append(item)
    d=json.loads((p/'secondary-dispatch-run1.json').read_text());assert d['status']=='passed' and len(d['routes'])==87 and len(d['cases'])==84
    report['counts']=dict(classNames=34,pcNewLifetimes=20,pcNewClassOperations=100,ps2NewIdentities=29,ps2InlineFactories=28,ps2SeparateConstructor=1,ps2SecondaryRoutes=87,ps2UniqueExecutedThunks=84,unchangedPreviouslyAssessedPc=13)
    report['limitsAndCorrections']=['Concrete serializer Clone must be inspected independently of null spSerializer base Clone. Projection pilot failed an overly broad harness expectation before Clone;standard distinct-object path then passed. No null-clone mode retained in shared runner.','PS2PartitionRenderableSerializer uniquely calls own20DD70 instead of inlining its own two table stores. First static inspector stopped,second separately validates that constructor.','PS2 secondary first three slots are original J/addiu-A0-16 adjustors;two following observed words are separately recorded,not silently treated as those adjustors.','PC CollisionInfoSerializer prior reader/index/writer/whole-load evidence is a reviewed-existing assessment;new lifecycle is not a claim that this entire codec was discovered today.']
    out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report['counts']))


if __name__=='__main__':main()
