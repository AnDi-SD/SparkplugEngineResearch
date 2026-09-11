"""Read-only cycle ledger, sealed-reference graph and PS2 stop-site audit.

Historical source versions are checked at their declared archived paths. No
guest execution or asset corpus scan. Linked pre-cycle contracts are hashed,
but their older transitive graphs are outside this cycle audit.
"""
import argparse,collections,hashlib,json,sqlite3,sys,time
from pathlib import Path
from datetime import datetime,timezone
from capture_native_ranges import ROOT,PS2,EXPECTED,read_elf_sections,read_window

def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def walk_refs(value,location='$'):
    if isinstance(value,dict):
        for pk,hk in [('path','sha256'),('sourcePath','sourceSha256')]:
            if isinstance(value.get(pk),str) and isinstance(value.get(hk),str):yield value[pk],value[hk],location
        for k,v in value.items():yield from walk_refs(v,location+'/'+k)
    elif isinstance(value,list):
        for i,v in enumerate(value):yield from walk_refs(v,location+'/'+str(i))

def audit(baseline,output,extras):
    start=time.perf_counter();base=read(baseline);cycle=baseline.parent.relative_to(ROOT).as_posix();stamp=base['startedUtc'];hashes={};issues=[];refs=[];queue=collections.deque();parsed=set();bindings=[];stops=[];ambiguous=[];archive_bindings=collections.defaultdict(list)
    assert not output.exists() and output.is_relative_to(ROOT/'local-data/results')
    def digest(path):
        if path not in hashes:hashes[path]=hashlib.sha256(path.read_bytes()).hexdigest().upper() if path.is_file() else None
        return hashes[path]
    def add(path,expected,origin):
        absolute=(ROOT/path).resolve()
        if not absolute.is_relative_to(ROOT):raise ValueError('Reference outside repository: '+path)
        path=absolute.relative_to(ROOT).as_posix()
        actual=digest(absolute);refs.append(dict(path=path,expected=expected.upper(),origin=origin))
        if actual!=expected.upper():issues.append(dict(kind='source-hash',path=path,origin=origin,expected=expected,actual=actual))
        # New dated contracts plus local cycle evidence. Do not recursively
        # compare superseded source paths inside unrelated historical contracts.
        if absolute.suffix=='.json' and (path.startswith(cycle+'/') or path.startswith('research/') and '2026-09-11' in path):queue.append(path)
    db=sqlite3.connect((ROOT/'local-data/results/smo-corpus-v2.sqlite').as_uri()+'?mode=ro',uri=True)
    imports=list(db.execute('SELECT source_path,source_sha256,created_utc FROM native_platform_imports ORDER BY created_utc'))
    state={};initial=None;delta=collections.defaultdict(lambda:collections.defaultdict(float));active=[];updates=[]
    for path,h,created in imports:
        document=read(ROOT/path)
        if created>=stamp:
            if initial is None:initial=dict(state)
            add(path,h,'native_platform_imports');active.append(path)
        for a in document['assessments']:
            key=(a['platformKey'],a['className']);old=state.get(key,0);state[key]=a['coverageScore']
            if created>=stamp:
                delta[a['assessmentOrigin']][key[0]]+=a['coverageScore']-old
                updates.append(dict(platform=key[0],className=key[1],before=old,after=a['coverageScore'],origin=a['assessmentOrigin']))
    assert initial is not None
    actual={(k,n):s for k,n,s in db.execute('SELECT a.platform_key,t.class_name,a.score FROM native_platform_assessments a JOIN native_types t ON t.id=a.native_type_id')}
    if state!=actual:issues.append(dict(kind='manifest-replay',differingKeys=[list(k) for k in state.keys()|actual.keys() if state.get(k)!=actual.get(k)]))
    for path in extras:
        relative=path.relative_to(ROOT).as_posix();add(relative,digest(path),'explicit support root');queue.append(relative)
    raw=PS2.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()==EXPECTED['ps2'];sections=read_elf_sections(raw)
    def address(v):return v if isinstance(v,int) else int(v,16)
    def visit_stops(v,source,location='$',platform=None,status=None):
        if isinstance(v,dict):
            platform=v.get('platform',v.get('platformKey',platform));status=v.get('status',status)
            if isinstance(v.get('input'),dict):platform=v['input'].get('platform',platform)
            if 'stop' in v and 'entry' in v:
                try:entry=address(v['entry']);stop=address(v['stop'])
                except (TypeError,ValueError):entry=stop=0
                if platform is None and 0x100000<=entry<0x400000:platform='ps2'
                if status in ('passed',None) and 0x100000<=stop<0x500000:
                    if platform=='ps2':
                        word=int.from_bytes(read_window('ps2',raw,stop-4,4,sections)[0],'little');op=word>>26
                        transfer=op in (1,2,3,4,5,6,7,0x14,0x15,0x16,0x17) or op==0 and word&63 in (8,9) or op in (0x10,0x11,0x12) and (word>>21)&31==8
                        row=dict(source=source,location=location,entry=f'{entry:08X}',stop=f'{stop:08X}',precedingWord=f'{word:08X}',precedingTransfer=transfer);stops.append(row)
                        if transfer:issues.append(dict(kind='PS2-stop-after-transfer',**row))
                    elif platform is None:ambiguous.append(dict(source=source,location=location,entry=entry,stop=stop))
            for k,x in v.items():visit_stops(x,source,location+'/'+k,k if k in ('pc','ps2') else platform,status)
        elif isinstance(v,list):
            for i,x in enumerate(v):visit_stops(x,source,location+'/'+str(i),platform,status)
    while queue:
        path=queue.popleft()
        if path in parsed:continue
        parsed.add(path);document=read(ROOT/path)
        for p,h,loc in walk_refs(document):add(p,h,path+loc)
        if path.startswith(cycle+'/'):visit_stops(document,path)
        declared=document.get('sourceBindings',[]) if isinstance(document,dict) else []
        binding_items=declared.items() if isinstance(declared,dict) else enumerate(declared)
        for index,binding in binding_items:
            assert isinstance(binding,dict),'Unknown sourceBindings record shape'
            result=binding.get('result');probe=binding.get('probe',binding.get('runner'))
            if result is None and isinstance(declared,dict):
                candidates=[v for v in document.get('sources',[]) if isinstance(v,dict) and Path(v.get('path','')).name==index]
                assert len(candidates)==1,('Named run must resolve to one sealed source',path,index)
                result=candidates[0]
            if isinstance(result,dict) and 'path' in result:
                run_path=(ROOT/result['path']).resolve().relative_to(ROOT).as_posix()
                for role,source in binding.items():
                    if role!='result' and isinstance(source,dict) and 'path' in source and 'sha256' in source:
                        archive_bindings[run_path].append(dict(contract=path,index=index,role=role,**source))
            if isinstance(result,dict) and isinstance(probe,dict) and 'path' in result and 'path' in probe:
                run=read(ROOT/result['path']);recorded=run.get('sourceSha256',run.get('probeSha256'))
                row=dict(contract=path,index=index,result=result['path'],probe=probe['path'],recordedProbeHash=recorded,declaredProbeHash=probe.get('sha256'));bindings.append(row)
                if recorded and recorded.upper()!=probe['sha256'].upper():issues.append(dict(kind='result-probe-binding',**row))
    assert {(ROOT/p).resolve().relative_to(ROOT).as_posix() for p in active}<=parsed,'Every active manifest must be traversed, including native Windows paths'
    coverage={}
    for row in base['coverage']:
        if row['scope_key']!='all':continue
        k=row['platform_key'];den=row['denominator'];before=sum(v for (p,n),v in initial.items() if p==k)/den;after=sum(v for (p,n),v in state.items() if p==k)/den
        if abs(before-row['credited_percent'])>1e-9:issues.append(dict(kind='baseline-replay',platform=k,replayed=before,recorded=row['credited_percent']))
        coverage[k]=dict(initialPercent=before,currentPercent=after,deltaPercentagePoints=after-before,denominator=den,uniqueChangedClasses=len({u['className'] for u in updates if u['platform']==k and u['before']!=u['after']}),origins={o:d[k]/den for o,d in delta.items()})
    remaining=[];resolved=[]
    for issue in issues:
        matches=[]
        if issue['kind']=='source-hash':
            run_path=issue['origin'].split('$',1)[0]
            matches=[v for v in archive_bindings.get(run_path,[]) if v['sha256'].upper()==issue['expected'].upper() and digest((ROOT/v['path']).resolve())==issue['expected'].upper()]
        if matches:resolved.append(dict(originalReference=issue,explicitArchivedBindings=matches))
        else:remaining.append(issue)
    unique_issues=list({json.dumps(v,sort_keys=True):v for v in remaining}.values())
    report=dict(kind='native-cycle-sealed-reference-and-ledger-audit',createdUtc=datetime.now(timezone.utc).isoformat(),status='passed' if not unique_issues else 'review-required',baseline=baseline.relative_to(ROOT).as_posix(),sourceSha256=digest(Path(__file__).resolve()),activeManifests=len(active),assessmentUpdates=len(updates),allReplayManifests=len(imports),referenceCount=len(refs),uniqueHashedFiles=len(hashes),parsedEvidenceJson=len(parsed),checkedResultProbeBindings=len(bindings),checkedPS2Stops=len(stops),unclassifiedStops=ambiguous,resolvedHistoricalReferences=resolved,coverage=coverage,issues=unique_issues,manifests=active,explicitSupportRoots=[p.relative_to(ROOT).as_posix() for p in extras],references=refs,bindings=bindings,stops=stops,updates=updates,seconds=time.perf_counter()-start,scope='Exact declared sources and sourceBindings in current cycle graph;linked older contracts hash-only.Embedded historical live-source hashes resolve only to identical bytes explicitly bound to that same result by a sealed contract.No guest replay,asset corpus check,general ISA equivalence or whole-class completion claim.Successful recorded PS2 non-RETURN stops checked against original preceding instruction.')
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:report[k] for k in ('status','activeManifests','assessmentUpdates','referenceCount','uniqueHashedFiles','parsedEvidenceJson','checkedResultProbeBindings','checkedPS2Stops','coverage','seconds')}));print(json.dumps(unique_issues[:15]));print('unclassified stops',len(ambiguous))
    return int(bool(unique_issues))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('baseline',type=Path);parser.add_argument('output',type=Path);parser.add_argument('--support',type=Path,action='append',default=[]);args=parser.parse_args()
    raise SystemExit(audit(args.baseline.resolve(),args.output.resolve(),[p.resolve() for p in args.support]))
