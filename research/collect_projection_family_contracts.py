#!/usr/bin/env python3
"""Seal projection geometry and FX interfaces without inventing RTTI tables."""
import hashlib,json,struct
from pathlib import Path
from capture_native_ranges import ROOT,PC,PS2,EXPECTED,read_elf_sections,read_window,pefile


def main():
    p=ROOT/'local-data/results/native-cycle-20260910-1900/projection-family';out=ROOT/'research/projection-family-contracts-2026-09-10.json'
    if out.exists():raise ValueError('Sealed evidence is immutable')
    def sha(a):return hashlib.sha256(a.read_bytes()).hexdigest().upper()
    def ref(a):return dict(path=a.relative_to(ROOT).as_posix(),sha256=sha(a))
    raw={'pc':PC.read_bytes(),'ps2':PS2.read_bytes()};assert all(hashlib.sha256(raw[k]).hexdigest().upper()==EXPECTED[k] for k in raw)
    containers={'pc':pefile.PE(data=raw['pc'],fast_load=True),'ps2':read_elf_sections(raw['ps2'])}
    tables={'pc':{'spBoxProjection':0x6de7e8,'spPyramidProjection':0x6de868,'spProjection':0x6dca30,'spProjectionFX':0x6f3388,'spPCProjectionFX':0x6f2864,'spProjectionManager':0x6f34b0,'spPCProjectionManager':0x6f2a00,'spShadowProjection':0x7061e0},'ps2':{'spBoxProjection':0x48dae0,'spPyramidProjection':0x48da70,'spProjection':0x490730,'spProjectionFX':0x4907a0,'spPS2ProjectionFX':0x491d80,'spProjectionManager':0x4907e0,'spPS2ProjectionManager':0x491db0,'spShadowProjection':0x490830}}
    sources=['catalog-family.json','factories/capture.json','construction-windows/capture.json','interfaces/capture.json','clone-offset/capture.json','protocol-windows/capture.json','fx-init/capture.json','base-lifetimes/capture.json','shadow-construction/capture.json','manager-leaves/capture.json','getter-candidates-run1.json','fx-contracts-run3.json','ps2-leaves-run1.json']
    report=dict(kind='independent-projection-geometry-and-fx',schemaVersion=1,inputs=EXPECTED,collectorSha256=sha(Path(__file__)),classes=[],sources=[ref(p/n) for n in sources],scope='11 names,8 PC/9 PS2. Two old PC manager assessments unchanged. Three new PC lifetimes;PS2 four factory identities,eight RTTI interfaces and metadata-only TextureProjection. No complete render/geometry/FPU pipeline claim.')
    leaves=json.loads((p/'ps2-leaves-run1.json').read_text());assert leaves['status']=='passed' and len(leaves['cases'])==22
    getters={r['input']['className']:r for r in leaves['cases'] if r['input']['kind']=='getter'}
    for row in json.loads((p/'catalog-family.json').read_text()):
        n=row['className'];item=dict(row)
        for k in ('pc','ps2'):
            if not row[k+'Record']:continue
            if n not in tables[k]:item[k]=dict(status='registered-metadata-only',factory=0,noConstantGetterMatch=True);continue
            a=tables[k][n];count=16 if 'Manager' in n else 7;size=(count+(2 if k=='ps2' else 0))*4;t,off=read_window(k,raw[k],a,size,containers[k]);words=struct.unpack('<'+'I'*(size//4),t)
            if k=='ps2':assert words[:2]==(0,0);words=words[2:];assert getters[n]['input']['entry']==words[4] and getters[n]['input']['expected']==row['ps2Record']
            offset=4 if n in ('spProjectionFX','spPCProjectionFX','spPS2ProjectionFX') else 0
            item[k]=dict(objectInterfaceOffset=offset,objectVtable=f'{a:08X}',objectVtableBytes=t.hex(),objectVtableFileOffset=off,objectSlots=[f'{v:08X}' for v in words],tableScope=f'First{count} object-interface slots;FX primary render interface recorded separately')
            if offset:
                primary={'pc':{'spProjectionFX':0x6effe4,'spPCProjectionFX':0x6f2880},'ps2':{'spProjectionFX':0x490790,'spPS2ProjectionFX':0x491d70}}[k][n]
                b,boff=read_window(k,raw[k],primary,8 if k=='pc' else 16,containers[k]);item[k].update(primaryVtable=f'{primary:08X}',primaryBytes=b.hex(),primaryFileOffset=boff)
            if k=='pc' and n in ('spBoxProjection','spPyramidProjection','spPCProjectionFX'):
                path=p/f'pc-{n}-run{2 if n=="spPCProjectionFX" else 1}.json';r=json.loads(path.read_text());assert r['status']=='passed' and len(r['stages'])==5 and r['initial']['slots']==item[k]['objectSlots']
                item[k].update(lifetimeSource=ref(path),allocationBytes=r['initial']['allocationBytes'],constructedBytes=r['initial']['bytes'],cloneBytes=r['clone']['bytes'],remainingContextAllocations=[v for v in r['allocations'] if not v['freed']])
            elif k=='pc' and n in ('spProjectionManager','spPCProjectionManager'):item[k]['previousEvidence']='docs/research/native-pc-scene-special-managers.md'
            if k=='ps2' and n in ('spBoxProjection','spPyramidProjection','spPS2ProjectionFX','spPS2ProjectionManager'):
                item[k]['allocationBytes']={'spBoxProjection':176,'spPyramidProjection':200,'spPS2ProjectionFX':80,'spPS2ProjectionManager':68}[n]
        report['classes'].append(item)
    fx=json.loads((p/'fx-contracts-run3.json').read_text());assert fx['status']=='passed' and len(fx['pcOwnedCases'])==2 and len(fx['ps2SetupCases'])==4 and len(fx['initCases'])==6
    report['counts']=dict(classNames=11,unchangedPcAssessments=2,newPcLifetimes=3,newPcClassOperations=15,ps2FactoryIdentities=4,ps2ObjectInterfaces=8,ps2MetadataOnly=1,pcOwnedFxCases=2,ps2SetupCases=4,pairedInitCases=6,ps2LeafAndAdjustorCases=22)
    report['limitsAndCorrections']=['PCFX run1 assumed BaseObject at+0 and tried a string as getter. Native PC/PS2 construction/tables prove BaseObject+4;run2 uses original secondary RTTI/Clone/destructor and passes. Clone returns secondary pointer,allocator tracks whole object.','FX contracts runs1/2 overclaimed that every allocation must be freed:actual FX and both Projections are freed,but the same56-byte6DC3B8 context allocation remains as in previous cold lifetimes. Run3 compares that declared boundary,without attributing global lifetime or claiming a leak.','The scout window labelled pc-Shadow-ctor-window starts at getter and contains destructor. Actual PC ctor5A3D10 separately captured,calls Pyramid432F30. PS2 own ctor1C23F0 calls1C0550;both Shadow Clone slots are null.','PS2 TextureProjection ID58DA4026 identifies the previously unexplained same PC scene-dispatch literal. PC has no such registered class;this does not introduce a PC RTTI factory or guessed table.','PC layer state writes2/2/B/A;PS2 writes2/2/3/80. Native platform fields/results recorded separately;no automatic GPU equivalence or FPU rounding claim.']
    out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report['counts']))


if __name__=='__main__':main()
