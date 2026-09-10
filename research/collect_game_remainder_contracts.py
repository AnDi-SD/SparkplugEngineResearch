#!/usr/bin/env python3
"""Seal the remaining registered game roster with platform-specific limits."""
import hashlib,json,struct
from pathlib import Path
from capture_native_ranges import ROOT,PC,PS2,EXPECTED,read_window,read_elf_sections,pefile


def main():
    p=ROOT/'local-data/results/native-cycle-20260910-1900/game-remainder';out=ROOT/'research/game-remainder-contracts-2026-09-10.json'
    if out.exists():raise ValueError('Sealed evidence is immutable')
    def sha(a):return hashlib.sha256(a.read_bytes()).hexdigest().upper()
    def ref(a):return dict(path=a.relative_to(ROOT).as_posix(),sha256=sha(a))
    raw={'pc':PC.read_bytes(),'ps2':PS2.read_bytes()};assert all(hashlib.sha256(raw[k]).hexdigest().upper()==EXPECTED[k] for k in raw)
    containers={'pc':pefile.PE(data=raw['pc'],fast_load=True),'ps2':read_elf_sections(raw['ps2'])}
    candidates={r['className']:r for r in json.loads((p/'getter-candidates-run1.json').read_text())['classes']}
    maps={r['className']:r for r in json.loads((p/'ps2-constructor-map.json').read_text())}
    proofs={r['className']:r for r in json.loads((p/'ps2-construction-candidates-run1.json').read_text())}
    identities=json.loads((p/'ps2-identities-run1.json').read_text());assert identities['status']=='passed' and len(identities['getters'])==46 and len(identities['nullClones'])==5
    nulls={r['className']:r['entry'] for r in identities['nullClones']}
    copies=json.loads((p/'copy-run3.json').read_text());assert copies['status']=='passed' and len(copies['cases'])==13 and len(copies['diagnostics'])==1
    versions=[p/'probe-pc-lifecycle-initial.py',p/'probe-pc-lifecycle-arena-run2.py'];by_sha={sha(a):a for a in versions}
    latest={};all_runs=[]
    for a in sorted(p.glob('pc-wx*-run*.json')):
        r=json.loads(a.read_text());assert r['sourceSha256'] in by_sha
        latest[r['className']]=(a,r);all_runs.append(dict(report=ref(a),exactRunner=ref(by_sha[r['sourceSha256']]),status=r['status']))
    sources=['catalog-family.json','factories/capture.json','getter-candidates-run1.json','batch-run1.json','initial-dependencies/capture.json','ps2-construction/capture.json','ps2-constructor-map.json','ps2-construction-candidates-run1.json','copy-and-state/capture.json','copy-prefix-dependencies/capture.json','base-copy-full/capture.json','copy-run1.json','copy-run2.json','copy-run3.json','physical-base-constructors/capture.json','ancestry-followup/capture.json','pc-particle-parent-scout/capture.json','pc-particle-parent-constructor/capture.json','ps2-null-clones/capture.json','ps2-identities-run1.json']
    report=dict(kind='remaining-game-classes-native-contracts',schemaVersion=1,inputs=EXPECTED,collectorSha256=sha(Path(__file__)),classes=[],sources=[ref(p/a) for a in sources],allLifecycleRuns=all_runs)
    class_operations=0
    for row in json.loads((p/'catalog-family.json').read_text()):
        n=row['className'];item=dict(row)
        for k in ('pc','ps2'):
            if row[k+'Record'] is None:continue
            g=candidates[n][k]['getters'];assert len(g)==1 and len(g[0]['references'])==1
            a=g[0]['references'][0]['address']-(16 if k=='pc' else 24);b,off=read_window(k,raw[k],a,28 if k=='pc' else 36,containers[k]);words=struct.unpack('<'+'I'*(len(b)//4),b)
            if k=='ps2':assert words[:2]==(0,0);words=words[2:]
            assert words[4]==g[0]['address']
            getter,goff=read_window(k,raw[k],words[4],6 if k=='pc' else 12,containers[k])
            if k=='pc':assert getter==b'\xb8'+struct.pack('<I',row['pcRecord'])+b'\xc3'
            else:
                lo=row['ps2Record']&65535;hi=((row['ps2Record']+0x8000)>>16)&65535
                assert getter==struct.pack('<3I',0x3c020000|hi,0x03e00008,0x24420000|lo)
            item[k]=dict(objectVtable=f'{a:08X}',objectVtableBytes=b.hex(),objectVtableFileOffset=off,objectSlots=[f'{v:08X}' for v in words],getterBytes=getter.hex(),getterFileOffset=goff,scope='First seven object-interface slots only;no adjacent virtuals inferred')
            if k=='pc':
                if n in latest:
                    path,r=latest[n];class_operations+=len(r['stages']);item[k].update(lifecycleSource=ref(path),exactRunner=ref(by_sha[r['sourceSha256']]),status=r['status'],factoryReached=r['factoryReached'],returnedClassOperations=len(r['stages']))
                    if r['factoryReached']:
                        assert r['initial']['vtable']==f'{a:08X}' and r['initial']['slots']==item[k]['objectSlots']
                        item[k].update(allocationBytes=r['initial']['allocationBytes'],constructedBytes=r['initial']['bytes'])
                    else:
                        matches=[x for x in r['allocations'] if x['firstWord']==f'{a:08X}'];item[k]['partialConstructedAllocations']=matches
                    if r['status']=='passed':assert len(r['stages'])==5;item[k]['remainingTrackedAllocations']=[v for v in r['allocations'] if not v['freed']]
                    else:item[k].update(blockedStage=r['pending'],lastIp=r['lastIp'],error=r['error'])
                elif row['pcFactory']==0:
                    assert words[2]==0x4a1bf0
                    item[k].update(status='no-factory-getter-and-null-clone',tableQualification='Constructed MoviePlayer base table additionally proved. Other no-factory tables are static unique getter-reference/object-slot candidates,not instantiated objects.')
                else:
                    assert n=='wxFaceData';item[k]['status']='previous-PC80-retained'
            elif n in maps:
                m=maps[n];pr=proofs[n];assert pr['expectedTable']==a and len(pr['matchedWrites'])==1
                item[k].update(status='independent-static-construction-and-executed-getter',construction=m,ownVtableWrite=pr['matchedWrites'][0],firstConstructorCall=pr['firstCall'])
            else:
                assert row['ps2Factory']==0 and words[2]==nulls[n]
                item[k].update(status='no-factory-original-getter-and-null-clone',baseConstructor={'wxInputDeviceInterface':'00372F30','wxMoviePlayer':'003823E0','wxPerceptionTrigger':'0036E7E0','wxProcessBuffer':'0036FC00'}.get(n),scope='Four base constructors captured separately;DialogueBox previous PS215 retained')
        report['classes'].append(item)
    report['counts']=dict(names=50,pcRegisteredClasses=44,ps2RegisteredClasses=46,newPcFactoryAttempts=38,newPcFullLifetimes=sum(r['status']=='passed' for _,r in latest.values()),pcBlockedLifetimes=sum(r['status']!='passed' for _,r in latest.values()),pcReturnedClassOperations=class_operations,ps2FactoryIdentities=41,ps2ExecutedGetters=46,ps2NullClones=5,pairedCopyAndStateCases=13,qualifiedNanDiagnostic=1,unchangedAssessments=3)
    assert report['counts']['newPcFullLifetimes']==28 and class_operations==152
    report['ancestry']=[dict(className='wxMoviePlayer',registeredParent='wxEntity',physicalParent='spBaseObject',pcConstructor='005FA2D0',pcParentCall='0040E910',ps2Constructor='003823E0',ps2ParentCall='00102BF0'),dict(className='wxParticleManager',registeredParent='spBaseObject',physicalParent='spParticleSystemManager',pcParentConstructor='004A2E40',pcParentTable='006E70F8',pcParentGetter='0045A430',ps2ParentConstructor='001BC410',ps2ParentTable='004906D0',ps2ParentGetter='001BC150')]
    report['limitsAndCorrections']=['AlphaManager exhausted64KiB only during Clone;explicit128KiB fresh guest passes. Total native requests89720 bytes/1012 allocations includes internal objects/context,not880-byte class size. No allocation cap or game counts relaxed.','FairyStar micro/file stopped in protected delete traversal;fresh protected-block reached actual null dependency40FB60. Deletion remains blocked;no forced success.','HUD depends on actual global75DB68 state;input-family failures reach a game tree reset4BE693,not an OS import. Scout name pc-input-os-dependency is not an OS attribution.','Asset-name consumer592C71 reads actual AssetManager root pointer+14 which is null before initialization. PlayerProfile/TargetManager/StableLamp remain stopped there. No fabricated root name or asset success result.','LocalizedStrings factory initializes storage pointers to null;original destructor dereferences pointer+18 expecting four entries and pointer+28 expecting two. Cold uninitialized destructor is not bypassed.','Copy run1 used nonexistent probe convenience put;run2 fixes it. Run2 assumed signaling-NaN quieting but Unicorn preserves Y/Z raw bits;run3 separates that diagnostic. Game x87 control/exception mode and hardware NaN result remain open,not a golden.','PC and PS2 Perception Copy share primitive tail but have independently different container-copy implementations. PS2 only post-container tail executed;no populated-tree equivalence claim.','Selected cached roster includes three prior scores:PC FaceData80 and PS2 DialogueBox/OptionMenu15. These remain unchanged. No class is marked fully closed from identity or lifetime alone.']
    out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report['counts']))


if __name__=='__main__':main()
