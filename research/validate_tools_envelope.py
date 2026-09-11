"""Directed, byte-exact tool migration validation against preserved pre-change outputs."""
from pathlib import Path
import hashlib, json, subprocess, sys, time

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT/'local-data/results/tools-core-cycle-20260911-1900/envelope'
BINARY = ROOT/'tools/SmoLVLcreator/SmoLVLcreator.CoreTests/bin/Release/net8.0/SmoLVLcreator.CoreTests.dll'
INPUTS = {'object':'Menus/object.smo','logo':'Menus/logo_screen.smo',
          'barrel':'SFX/barrel.smo','alfea02':'Levels/Alfea/Alfea02.smo'}

def main(mode):
    assert mode in ('pilot','batch')
    jobs = [(False,key) for key in INPUTS]+[(True,key) for key in ('object','barrel','alfea02')]
    jobs = jobs[:1] if mode == 'pilot' else jobs[1:]
    report = FOLDER/f'final-{mode}-validation.json'
    assert not report.exists()
    results=[]
    for leaf,key in jobs:
        suffix=('leaf-' if leaf else '')+key
        before=FOLDER/('before-'+suffix);after=FOLDER/('final-'+suffix)
        assert not after.exists()
        command=['dotnet',str(BINARY),'--leaf-envelope' if leaf else '--container-envelope',
                 str(ROOT/'local-data/pc-pristine/Media'/INPUTS[key]),str(after)]
        start=time.monotonic()
        child=subprocess.run(command,capture_output=True,text=True,timeout=30)
        row=dict(case=suffix,command=command,exitCode=child.returncode,
                 stdout=child.stdout,stderr=child.stderr,seconds=time.monotonic()-start)
        results.append(row)
        try:
            assert child.returncode==0
            a=json.loads((before/'capture.json').read_text());b=json.loads((after/'capture.json').read_text())
            assert a['sourceSha256']==b['sourceSha256'] and len(a['records'])==len(b['records'])
            for x,y in zip(a['records'],b['records']):
                assert (x['name'],x['bytes'],x['sha256'])==(y['name'],y['bytes'],y['sha256'])
                assert (before/(x['name']+'.smo')).read_bytes()==(after/(y['name']+'.smo')).read_bytes()
            for binary in b['binaries']:
                assert hashlib.sha256(Path(binary['path']).read_bytes()).hexdigest().upper()==binary['sha256']
            row.update(status='passed',operations=len(b['records']))
        except Exception as error:
            row.update(status='failed',error=repr(error))
        report.write_text(json.dumps(dict(mode=mode,results=results),indent=2)+'\n')
        if row['status']!='passed':
            print(json.dumps(row));return 1
    print(json.dumps(dict(status='passed',cases=len(results),operations=sum(x['operations'] for x in results))))
    return 0

if __name__=='__main__':raise SystemExit(main(sys.argv[1]))
