"""One fresh original missing-mip case reused for end-to-end transport evidence."""
from pathlib import Path
import contextlib,hashlib,io,json,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_texture_missing_mips import main as original
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def guest(folder):
    out=(ROOT/folder).resolve();assert out.is_relative_to(ROOT/'local-data/results') and not out.exists()
    out.mkdir(parents=True);(out/'probe-source.py').write_bytes(Path(__file__).read_bytes())
    label='tools-mips-20260911-'+out.name
    old=ROOT/f'local-data/results/cycle-20260908-0700/cp108-texture-missing-{label}.json';assert not old.exists()
    log=io.StringIO()
    with contextlib.redirect_stdout(log):report=original(label,'corpus','16x16',return_capture=True)
    report['dependencies']=sorted((dict(path=Path(m.__file__).resolve().relative_to(ROOT).as_posix(),sha256=sha(m.__file__))
        for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda row:row['path'])
    report['sourceExecutableSha256']=sha(ROOT/'local-data/pc-pristine/WinxClub.exe')
    (out/'original-log.txt').write_text(log.getvalue());(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report.get(k) for k in ('status','readerSeconds','nativeInstructions','arenaReservedBytes','releasedAllocations')}))
    return 0 if report['status']=='completed' else 1
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(guest(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
