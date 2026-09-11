"""Original SOUND_SetGroupMasterVolume rejection prefix, before diagnostic I/O."""
import hashlib,json,sys,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED,read_window
from pc_instruction_emulator import run_bounded
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine
from probe_ps2_stack_spills import UPPER
FOLDER=ROOT/'local-data/results/native-cycle-20260911-0730/audio-invalid-handle'
def sha(b):return hashlib.sha256(b).hexdigest().upper()

def guest(output,selection):
    out=Path(output).resolve();assert out.is_relative_to(FOLDER) and not out.exists();assert selection in ('pilot','batch')
    inputs=[(h,f) for h in (0,0x50000,0xffffffff) for f in (0x5a,0xa5)];inputs=[x for i,x in enumerate(inputs) if (i==0)==(selection=='pilot')]
    r=dict(kind='original-ps2-audio-group-diagnostic',inputs=EXPECTED,sourceSha256=sha(Path(__file__).read_bytes()),cases=[]);start=time.perf_counter()
    for handle,fill in inputs:
        c=dict(platform='ps2',input=dict(groupCode=handle,fill=fill,left=0 if fill==0x5a else 4096,right=2048));r['cases'].append(c)
        try:
            p=Ps2ScalarPrefix([(0x1df400,0x44),(0x1df5ec,0x1c)],stack_window=(0x22000000,4096),upper64=UPPER);p.map(0x22000000,4096);p.write(0x22000000,bytes([fill])*4096);p.reg('SP',0x22000800)
            p.map(0x21000000,4096);p.write(0x21000000,bytes([fill])*4096);before=p.read(0x21000000,4096)
            raw,sections=pristine();p.map(0x45a730,32);p.write(0x45a730,read_window('ps2',raw,0x45a730,32,sections)[0]);p.reg('A0',handle);p.reg('A1',c['input']['left']);p.reg('A2',2048)
            code=[p.read(a,n) for a,n in p.ranges];v=p.run(0x1df400,[0x414d88],count=1000,timeout_us=500000)
            assert p.reg('A0')==0x45a730 and p.reg('S2')&0xffffffff==handle>>16 and p.reg('S6')&0xffffffff==handle
            assert p.reg('S1')==c['input']['left'] and p.reg('S0')==2048 and p.reg('SP')==0x22000780 and p.reg('RA')==0x1df5f8
            assert p.read(0x45a730,len(b'SOUND_SetGroupMasterVolume\n'))==b'SOUND_SetGroupMasterVolume\n';assert code==[p.read(a,n) for a,n in p.ranges] and before==p.read(0x21000000,4096)
            c.update(status='passed',**v,diagnosticPointer='0045A730',diagnostic='SOUND_SetGroupMasterVolume',guardedBytes=4096,beforeSha256=sha(before),afterSha256=sha(before),scope='GroupCode0 or unsigned high16>=5 reaches original error branch and first diagnostic call;stops before diagnostic I/O.No return code or sound application claim.')
        except Exception as e:c.update(status='blocked',error=str(e),traceback=traceback.format_exc())
        r['seconds']=time.perf_counter()-start;out.write_text(json.dumps(r,indent=2)+'\n')
        if c['status']!='passed':break
    r['status']='passed' if len(r['cases'])==len(inputs) and all(x['status']=='passed' for x in r['cases']) else 'blocked';out.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(dict(status=r['status'],cases=len(r['cases']),seconds=r['seconds'],errors=[x['error'] for x in r['cases'] if 'error' in x])));return int(r['status']!='passed')

if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
