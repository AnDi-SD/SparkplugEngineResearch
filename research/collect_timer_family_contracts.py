#!/usr/bin/env python3
"""Seal separately verified timer identities and native integer contracts."""
import hashlib,json,struct
from pathlib import Path
from capture_native_ranges import ROOT,PC,PS2,EXPECTED,read_elf_sections,read_window,pefile


def main():
    p=ROOT/'local-data/results/native-cycle-20260910-1900/timer-family';out=ROOT/'research/timer-family-contracts-2026-09-10.json'
    if out.exists():raise ValueError('Sealed evidence is immutable')
    def sha(a):return hashlib.sha256(a.read_bytes()).hexdigest().upper()
    def ref(a):return dict(path=a.relative_to(ROOT).as_posix(),sha256=sha(a))
    raw={'pc':PC.read_bytes(),'ps2':PS2.read_bytes()};assert all(hashlib.sha256(raw[k]).hexdigest().upper()==EXPECTED[k] for k in raw)
    containers={'pc':pefile.PE(data=raw['pc'],fast_load=True),'ps2':read_elf_sections(raw['ps2'])}
    report=dict(kind='independent-four-timer-platform-contracts',schemaVersion=1,inputs=EXPECTED,collectorSha256=sha(Path(__file__)),classes=[],sources=[ref(p/n) for n in ('catalog-family.json','factories/capture.json','body-windows/capture.json','common-run1.json','task-update-run1.json')],
        scope='Three new PC complete lifetimes;spTaskTimer PC83 unchanged. Four independent PS2 identities.24 paired timer cases;13 PS2 Task Update prefixes,10 with new PC leaf comparisons. No full PS2 lifetime or bit-exact R5900 floating arithmetic claim.')
    tables={'pc':{'spTimer':0x7291b8,'spMasterTimer':0x6e693c,'spTaskTimer':0x6e6968,'wxGameTimer':0x702f94},'ps2':{'spTimer':0x48cb60,'spMasterTimer':0x48cae0,'spTaskTimer':0x48cb20,'wxGameTimer':0x4931e0}}
    for row in json.loads((p/'catalog-family.json').read_text()):
        name=row['className'];item=dict(row)
        for k in tables:
            n=11 if name in ('spTaskTimer','wxGameTimer') else 7;address=tables[k][name]
            t,off=read_window(k,raw[k],address,(n+(2 if k=='ps2' else 0))*4,containers[k]);words=struct.unpack('<'+'I'*(len(t)//4),t)
            if k=='ps2':assert words[:2]==(0,0);words=words[2:]
            item[k]=dict(vtable=f'{address:08X}',vtableBytes=t.hex(),vtableFileOffset=off,slots=[f'{v:08X}' for v in words],allocationBytes={'spTimer':36,'spMasterTimer':40,'spTaskTimer':60,'wxGameTimer':136}[name])
            if k=='pc' and name!='spTaskTimer':
                path=p/f'pc-{name}-run1.json';r=json.loads(path.read_text());assert r['status']=='passed' and len(r['stages'])==5 and r['initial']['slots']==item[k]['slots'][:7]
                assert r['initial']['allocationBytes']==item[k]['allocationBytes'];item[k].update(lifetimeSource=ref(path),constructedBytes=r['initial']['bytes'],cloneBytes=r['clone']['bytes'])
            elif k=='pc':item[k]['previousEvidence']='docs/research/native-class-sp-task-timer.md'
            else:
                g,goff=read_window(k,raw[k],words[4],12,containers[k]);a,b,c=struct.unpack('<3I',g)
                assert a>>16==0x3c02 and b==0x03e00008 and c>>16==0x2442
                registration=((a&0xffff)<<16)+struct.unpack('<h',g[8:10])[0];assert registration==row['ps2Record']
                item[k].update(getterBytes=g.hex(),getterFileOffset=goff,verifiedRegistration=f'{registration:08X}',construction='own1E7900 calls102BF0' if name=='spTimer' else 'factory inlines own state after1E7900' if name=='spMasterTimer' else 'factory inlines own state after102BF0 and link helpers' if name=='spTaskTimer' else 'factory inlines own state after115530 and embedded10DE00')
        report['classes'].append(item)
    pe=containers['pc'];pe.parse_data_directories(directories=[1]);imports=[(d.dll.decode(),i.name.decode()) for d in pe.DIRECTORY_ENTRY_IMPORT for i in d.imports if i.address==0x6d9454];assert imports==[('WINMM.dll','timeGetTime')]
    report['clockBoundary']=dict(pcIat='006D9454',importIdentity=imports[0],ps2OriginalClockEntry='001E7980',ps2ClockBodyNotEmulated=True)
    a=json.loads((p/'common-run1.json').read_text());b=json.loads((p/'task-update-run1.json').read_text());assert a['status']==b['status']=='passed' and len(a['cases'])==24 and len(b['cases'])==13
    report['counts']=dict(newPcLifetimes=3,newPcClassOperations=15,ps2IndependentIdentities=4,pairedCommonCases=24,ps2TaskUpdateCases=13,pairedTaskLeafCases=10,ps2OnlyChildCases=3)
    report['platformDifference']='PC Task delta uses unsigned integer multiplied by float32 constant3A83126F then float store. PS2 original code converts unsigned difference to float and divides by1000. Integer prefixes verified;R5900 arithmetic rounding not inferred from R4000.'
    out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report['counts']))


if __name__=='__main__':main()
