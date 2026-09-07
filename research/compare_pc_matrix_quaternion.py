#!/usr/bin/env python3
"""Whole PC464CB0 vs source: finite trace/maximum-diagonal branches and stores."""
from pathlib import Path
import sys,struct,json,subprocess
from pc_instruction_emulator import ROOT,PcInstructions,run_bounded
MATRICES={
 'identity':(1,0,0,0,1,0,0,0,1),
 'positive':(.17,.23,-.51,.13,.37,.19,-.29,.31,.61),
 'x180':(1,0,0,0,-1,0,0,0,-1),
 'y180':(-1,0,0,0,1,0,0,0,-1),
 'z180':(-1,0,0,0,-1,0,0,0,1),
 'tie-x':(-.25,.17,.23,-.31,-.25,.43,-.53,.67,-.25),
 'tie-y':(-.5,.17,.23,-.31,.25,.43,-.53,.67,.25),
 'maximum-z':(-.9,.17,.23,-.31,-.8,.43,-.53,.67,.7),
 'small-trace':(.125,.17,.23,-.31,-.25,.43,-.53,.67,.1250000149011612),
 'corpus-light':(1,0,0,0,5.960464477539063e-8,.9999999403953552,0,-.9999999403953552,5.960464477539063e-8),
}
def main(case):
 if case not in MATRICES:raise ValueError('fixed finite matrix case')
 p=PcInstructions();data=struct.pack('<9f',*MATRICES[case]);matrix=p.allocate(36);out=p.allocate(16);p.mu.mem_write(matrix,data)
 p.run(0x464cb0,this=out,args=(matrix,));original=[p.uint(out+4*i) for i in range(4)]
 result=subprocess.run([str(ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugLightSerializationTests.exe'),'--matrix',data.hex()],capture_output=True,text=True,timeout=10,check=True)
 source=json.loads(result.stdout)
 if source!=original:raise AssertionError(f'{case}: source={source};original={original}')
 if p.seams or not p.visits.get(0x464cb0):raise AssertionError('whole native arithmetic without seams')
 print('MATRIX_QUATERNION_CAPTURE',json.dumps([case,data.hex(),original]),flush=True)
 print(f'PASS 2/2: exact matrix quaternion {case}; instructions={sum(p.visits.values())}; arena={p.allocated}',flush=True);return 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
