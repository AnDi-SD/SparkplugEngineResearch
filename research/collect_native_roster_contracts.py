#!/usr/bin/env python3
"""Seal a cached roster, keeping metadata, candidate tables and lifetimes distinct."""
import hashlib,json,re,struct,sys
from pathlib import Path
from capture_native_ranges import ROOT,PC,PS2,EXPECTED,read_window,read_elf_sections,pefile
from ps2_scalar_prefix import Ps2ScalarPrefix


def main():
    family=sys.argv[1]
    if not re.fullmatch('[a-z0-9]+(?:-[a-z0-9]+)*',family):raise ValueError('Explicit cached family required')
    p=ROOT/f'local-data/results/native-cycle-20260910-1900/{family}';out=ROOT/f'research/{family}-contracts-2026-09-10.json'
    if out.exists():raise ValueError('Sealed evidence is immutable')
    def sha(a):return hashlib.sha256(a.read_bytes()).hexdigest().upper()
    def ref(a):return dict(path=a.relative_to(ROOT).as_posix(),sha256=sha(a))
    raw={'pc':PC.read_bytes(),'ps2':PS2.read_bytes()};assert all(hashlib.sha256(raw[k]).hexdigest().upper()==EXPECTED[k] for k in raw)
    containers={'pc':pefile.PE(data=raw['pc'],fast_load=True),'ps2':read_elf_sections(raw['ps2'])}
    candidates={r['className']:r for r in json.loads((p/'getter-candidates-run1.json').read_text())['classes']}
    review=json.loads((p/'ps2-construction-review.json').read_text());constructors={r['className']:r for r in review['classes']}
    versions=list(p.glob('probe-pc-lifecycle-*.py'));by_sha={sha(a):a for a in versions};latest={};all_runs=[]
    for a in sorted(p.glob('pc-sp*-run*.json')):
        r=json.loads(a.read_text());assert r['sourceSha256'] in by_sha,a
        latest[r['className']]=(a,r);all_runs.append(dict(report=ref(a),exactRunner=ref(by_sha[r['sourceSha256']]),status=r['status']))
    captures=sorted(p.glob('*/capture.json'));sources=[p/'catalog-family.json',p/'getter-candidates-run1.json',p/'ps2-construction-review.json']+captures
    report=dict(kind='native-roster-with-qualified-platform-evidence',family=family,schemaVersion=1,inputs=EXPECTED,collectorSha256=sha(Path(__file__)),classes=[],sources=[ref(a) for a in sources],allLifecycleRuns=all_runs,
        scope='Original getters and independently matched constructor tables. A unique reference is only a candidate table unless construction proves it. Full PC lifetimes and blocked stages recorded separately. No corresponding platform is invented;missing constant getter pattern is not proof that a method does not exist.')
    operations=0;ps2_getters=0
    for row in json.loads((p/'catalog-family.json').read_text()):
        n=row['className'];item=dict(row)
        for k in ('pc','ps2'):
            if row[k+'Record'] is None:continue
            g=candidates[n][k]['getters'];d=item[k]=dict(status='registered-metadata-only',constantGetterCandidates=g)
            if len(g)==1:
                address=g[0]['address'];b,off=read_window(k,raw[k],address,6 if k=='pc' else 12,containers[k]);d.update(getter=f'{address:08X}',getterBytes=b.hex(),getterFileOffset=off,status='original-getter-verified')
                if k=='pc':assert b==b'\xb8'+struct.pack('<I',row[k+'Record'])+b'\xc3'
                else:
                    q=Ps2ScalarPrefix([(address,12)]);execution=q.run(address,[q.RETURN]);assert q.reg('V0')==row[k+'Record'];d['getterExecution']=execution;ps2_getters+=1
                if len(g[0]['references'])==1:
                    table=g[0]['references'][0]['address']-(16 if k=='pc' else 24);t,toff=read_window(k,raw[k],table,28 if k=='pc' else 36,containers[k]);w=list(struct.unpack('<'+'I'*(len(t)//4),t));abi=(k=='pc' or w[:2]==[0,0]);slots=w if k=='pc' else w[2:]
                    d['tableCandidate']=dict(address=f'{table:08X}',bytes=t.hex(),fileOffset=toff,hasExpectedAbiPrefix=abi,slots=[f'{v:08X}' for v in slots],constructionProved=False)
                    if n in constructors and k=='ps2' and constructors[n]['status']=='literal-constructor-table-match':
                        c=constructors[n];assert abi and c['expectedTableCandidate']==table and slots[4]==address
                        d['tableCandidate']['constructionProved']=True;d.update(status='independent-static-construction-and-getter',construction=c)
            if k=='pc' and n in latest:
                path,r=latest[n];class_ops=[s for s in r['stages'] if s['label'] in ('factory','rtti','clone','delete-clone','delete-original')];operations+=len(class_ops)
                d.update(status=r['status'],lifecycleSource=ref(path),exactRunner=ref(by_sha[r['sourceSha256']]),factoryReached=r['factoryReached'],returnedClassOperations=len(class_ops),context=r.get('declaredContext'),platformInput=r.get('platformInput'),crtFixture=r['crtFixture'])
                if r['factoryReached']:
                    d.update(allocationBytes=r['initial']['allocationBytes'],initialBytes=r['initial']['bytes'],objectVtable=r['initial']['vtable'],objectSlots=r['initial']['slots'])
                    if 'tableCandidate' in d and d['tableCandidate']['address']==r['initial']['vtable']:
                        assert d['tableCandidate']['slots']==r['initial']['slots'];d['tableCandidate']['constructionProved']=True
                else:
                    table=d.get('tableCandidate',{}).get('address');d['partialObjectAllocations']=[x for x in r['allocations'] if x['firstWord']==table]
                if r['status']=='passed':assert len(class_ops)==5;d['remainingContextAllocations']=[v for v in r['allocations'] if not v['freed']]
                else:d.update(blockedStage=r.get('pending'),lastIp=r['lastIp'],error=r['error'])
        report['classes'].append(item)
    report['counts']=dict(classNames=len(report['classes']),pcRegistered=sum('pc' in r for r in report['classes']),ps2Registered=sum('ps2' in r for r in report['classes']),pcFactoryAttempts=len(latest),pcFullLifetimes=sum(r['status']=='passed' for _,r in latest.values()),pcBlocked=sum(r['status']!='passed' for _,r in latest.values()),pcClassOperations=operations,ps2FactoryTableMatches=sum(r['status']=='literal-constructor-table-match' for r in constructors.values()),ps2ExecutedGetters=ps2_getters)
    out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report['counts']))


if __name__=='__main__':main()
