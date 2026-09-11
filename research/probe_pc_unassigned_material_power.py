"""Original material installation copies the undefined Font material power word."""
from pathlib import Path
import hashlib,json,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def guest(folder):
 out=ROOT/folder;out.mkdir(parents=True,exist_ok=False);(out/'probe-source.py').write_bytes(Path(__file__).read_bytes())
 started=time.perf_counter();f=PCWriteBytesFixture();p=f.p;report=dict(status='running',checks=0,pc=sha(ROOT/'local-data/pc-pristine/WinxClub.exe'))
 def check(ok,label):
  report['checks']+=1
  if not ok:raise AssertionError(label)
 try:
  renderer=0x35000000;p.mu.mem_map(renderer,0x10000,p.uc.UC_PROT_READ|p.uc.UC_PROT_WRITE);p.put_uint(0x75db68,renderer)
  manager=f.call(0x4c35a0);p.put_uint(0x755270,manager);check(f.call(0x41e8b0,this=manager)&255,'actual Font default material initialization')
  mat=p.uint(manager+0x38);before=[p.uint(mat+0x78+i*4) for i in range(17)]
  check(before[-1]==0xcccccccc and p.uint(mat+0x38)==2,'unassigned power and unlit vertex-color mode')
  check(f.call(0x4be180,this=renderer,args=(mat,))&255,'actual material installer accepts unknown power')
  installed=[p.uint(renderer+0xe4a4+i*4) for i in range(17)]
  check(installed==before and p.uint(renderer+0xe47c)==mat,'all original words copied; material borrowed')
  report.update(before=before,installed=installed,instructions=sum(p.visits.values()),limits=p.last_execution_limits,arena=p.allocated)
  f.call(p.uint(p.uint(manager)),this=manager,args=(1,))
  for address in (0x75526c,0x755264):
   obj=p.uint(address)
   if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
  check(set(f.allocations)==set(f.freed),'normal ownership teardown');report['status']='passed-scoped-install'
 except Exception as error:report.update(status='failed',error=repr(error))
 report['seconds']=time.perf_counter()-started
 report['dependencies']=sorted((dict(path=Path(m.__file__).resolve().relative_to(ROOT).as_posix(),sha256=sha(m.__file__))
  for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda r:r['path'])
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report.get(k) for k in ('status','checks','seconds','error')}));return 0 if report['status'].startswith('passed') else 1
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(guest(*sys.argv[2:]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
