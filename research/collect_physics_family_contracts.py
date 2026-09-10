#!/usr/bin/env python3
"""Seal native physics identities, registry ownership and qualified BV copies."""
import hashlib,json,struct
from pathlib import Path
from capture_native_ranges import ROOT,PC,PS2,EXPECTED,read_window,read_elf_sections,pefile


def main():
    p=ROOT/'local-data/results/native-cycle-20260910-1900/physics-family';out=ROOT/'research/physics-family-contracts-2026-09-10.json'
    if out.exists():raise ValueError('Sealed evidence is immutable')
    def sha(a):return hashlib.sha256(a.read_bytes()).hexdigest().upper()
    def ref(a):return dict(path=a.relative_to(ROOT).as_posix(),sha256=sha(a))
    raw={'pc':PC.read_bytes(),'ps2':PS2.read_bytes()};assert all(hashlib.sha256(raw[k]).hexdigest().upper()==EXPECTED[k] for k in raw)
    containers={'pc':pefile.PE(data=raw['pc'],fast_load=True),'ps2':read_elf_sections(raw['ps2'])}
    tables={'pc':dict(spBoundingVolume=0x6ecacc,spBoxBV=0x6ec308,spCapsuleBV=0x6ec334,spCollisionManager=0x6e6fe4,spConstraint=0x6ec380,spConstraintSystem=0x6e7070,spContactConstraint=0x6e7090,spConvexBV=0x6eac88,spMeshBV=0x6eaab0,spOBBBV=0x6ec2d8,spPhysicsManager=0x6e70bc,spRigidBody=0x6ed450,spSphereBV=0x6e8f20),
            'ps2':dict(spBoundingVolume=0x48d1c0,spBoxBV=0x48d1f0,spCapsuleBV=0x48d220,spCollisionManager=0x48d170,spConvexBV=0x48d280,spMeshBV=0x48d2b0,spOBBBV=0x48d2e0,spSphereBV=0x48d310)}
    r=json.loads((p/'contracts-run2.json').read_text());assert r['status']=='passed' and [len(r[k]) for k in ('pcOwnershipCases','pcCloneCases','ps2Cases')]==[3,2,12]
    assert r['sourceSha256']==sha(ROOT/'research/probe_physics_family_contracts.py')
    getter_cases={x['className']:x for x in r['ps2Cases'] if x['kind']=='getter'}
    paths=['catalog-family.json','factories/capture.json','getter-candidates-run1.json','base-construction/capture.json','state-protocol/capture.json','ownership/capture.json','physical-parents/capture.json','contracts-run1.json','contracts-run2.json']
    report=dict(kind='native-physics-family-independent-platform-contracts',schemaVersion=1,inputs=EXPECTED,collectorSha256=sha(Path(__file__)),classes=[],sources=[ref(p/n) for n in paths],counts=dict(names=13,pcClasses=13,ps2Classes=8,newPcLifetimes=7,newPcClassOperations=35,pcOwnershipCases=3,pcGeometryCloneCases=2,ps2GetterCases=8,ps2NullCloneCases=1,ps2ConstructorPrefixCases=3,priorBvAssessmentsUnchanged=8))
    for row in json.loads((p/'catalog-family.json').read_text()):
        n=row['className'];item=dict(row)
        for k in ('pc','ps2'):
            if n not in tables[k]:assert row[k+'Record'] is None;continue
            a=tables[k][n];b,off=read_window(k,raw[k],a,28 if k=='pc' else 36,containers[k]);words=struct.unpack('<'+'I'*(len(b)//4),b)
            if k=='ps2':assert words[:2]==(0,0);words=words[2:];assert getter_cases[n]['entry']==words[4] and getter_cases[n]['record']==row['ps2Record']
            else:
                g,_=read_window(k,raw[k],words[4],6,containers[k]);assert g==b'\xb8'+struct.pack('<I',row['pcRecord'])+b'\xc3'
            item[k]=dict(objectVtable=f'{a:08X}',vtableBytes=b.hex(),vtableFileOffset=off,objectSlots=[f'{v:08X}' for v in words],scope='First seven object slots;no adjacent unproved words attributed as virtual methods')
            if n in ('spBoundingVolume','spConstraint'):assert row[k+'Factory']==0 and words[2]==(0x4a1bf0 if k=='pc' else 0x127f00)
            if k=='pc' and not row['priorAssessments'] and row['pcFactory']:
                run=2 if n in ('spRigidBody','spContactConstraint') else 1;path=p/f'pc-{n}-run{run}.json';life=json.loads(path.read_text());source=p/('probe-pc-lifecycle-memory-run2.py' if run==2 else 'probe-pc-lifecycle-initial.py')
                assert life['status']=='passed' and len(life['stages'])==5 and life['sourceSha256']==sha(source)
                assert life['initial']['slots']==item[k]['objectSlots']
                item[k].update(allocationBytes=life['initial']['allocationBytes'],lifetimeSource=ref(path),exactRunner=ref(source),initialBytes=life['initial']['bytes'],remainingContextAllocations=[v for v in life['allocations'] if not v['freed']])
            if k=='ps2' and n in ('spCapsuleBV','spConvexBV','spCollisionManager'):item[k]['allocationBytes']={'spCapsuleBV':120,'spConvexBV':48,'spCollisionManager':6032}[n]
        report['classes'].append(item)
    # Independently identify the actual PS2 parent constructor's table.
    cat=ROOT/'local-data/results/native-cycle-20260910-1900/catalog/ps2-architecture.json';named=next(x for x in json.loads(cat.read_text())['registered_types'] if x['class_name']=='spNamedObject')
    b,off=read_window('ps2',raw['ps2'],0x48c690,36,containers['ps2']);getter=struct.unpack('<9I',b)[6]
    g,goff=read_window('ps2',raw['ps2'],getter,12,containers['ps2']);lo=named['registration_object_va']&0xffff;hi=((named['registration_object_va']+0x8000)>>16)&0xffff
    assert g==struct.pack('<3I',0x3c020000|hi,0x03e00008,0x24420000|lo)
    report['ps2BoundingPhysicalParent']=dict(constructor='00127EC0',parentConstructor='00105F60',parent='spNamedObject',parentVtable='0048C690',vtableBytes=b.hex(),vtableFileOffset=off,getter=f'{getter:08X}',getterBytes=g.hex(),getterFileOffset=goff,record=named['registration_object_va'],catalog=ref(cat),scope='Registered parent remains spBaseObject;physical Named constructor is proved independently. PC physical Named path was already recorded in the older collision-core dossier.')
    report['ownership']=dict(managerBytes=132,globalAddress='0075DB80',bodyVectorBeginEndLimit=['2C','30','34'],constraintVectorBeginEndLimit=['3C','40','44'],observedByte21='RigidBody construction writes1;unchanged by deletion. Semantic name not inferred.',scope='Actual construction and deletion;stable order with middle/end removal;manager disposal frees buffers and clears global. Attached body/constraint graphs and solver remain open.')
    report['limitsAndCorrections']=['Contact/RigidBody initial delete reached genuine MSVCR71.memmove IAT6D9300. Existing shared bounded CRT memory fixture made fresh run2 pass;no engine algorithm replaced.','Initial combined prefix probe mapped GP inputs starting4A0000 and missed GP-496C=49F804. Run2 maps the declared6-page data range49F000..4A5000;all three ownership/two clone/twelve PS2 cases pass. Original failed report/source preserved.','The scout window called pc-constraint-constructor-entry at48B290 starts in preceding math code,not a proven constructor. It contributes no class method attribution. Other linear windows also contain adjacent/protected code;only exact entries,tables and guarded executions support claims.','Capsule/Convex PC populated geometry Clone retains own factory fields. PS2 common virtual Named copy and constructor defaults are independently established,but a complete populated PS2 Clone was not executed.','PC BoundingVolume previously researched physical Named/bounds behavior receives a first ledger row as reviewed-existing;this is not new discovery credit. Four other BV class scores on each platform remain unchanged.']
    out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report['counts']))


if __name__=='__main__':main()
