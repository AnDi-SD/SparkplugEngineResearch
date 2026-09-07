#!/usr/bin/env python3
"""Source-written fields feed a fresh whole original PC mesh reader/COM copy."""
from pathlib import Path
import sys,json,struct,subprocess
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_mesh_writer import main as original
from probe_pc_dx_mesh_payload import MeshFixture
from pc_loader_fixtures import empty_manager
from probe_pc_dx_buffers import check
import probe_pc_dx_buffers as counters

def main(mode,policy):
 native=original(mode,policy,True)
 binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugMeshReaderTests.exe'
 result=subprocess.run([str(binary),'--write-roundtrip',mode,policy],input='\n'.join(native[2:4])+'\n',capture_output=True,text=True,timeout=10)
 if result.returncode:raise AssertionError(result.stderr)
 source=json.loads(result.stdout)
 check(source[:5]==native,'exact original/source writer bytes before original acceptance')
 # A fresh emulator is an explicit file-byte boundary, not one live engine graph.
 data=struct.pack('<II',0x33c34cf0,0x4f4f4253)+bytes.fromhex(source[4])
 f=MeshFixture(data);p=f.p;manager,_=empty_manager(f)
 p.put_uint(manager+0x10,2);p.put_uint(0x75dde8,manager)
 serializer=f.call(0x4297c0);mesh=f.call(0x42afd0,this=serializer,args=(f.stream,))
 check(f.call(0x429bc0,this=serializer+0x10,args=(f.stream,mesh))&255==1,'original whole PC reader accepts source writer output')
 instructions=sum(p.visits.values())
 check(f.position==len(data) and not f.errors,'original full cursor with no engine errors')
 check(all(p.visits.get(a) for a in (0x429a40,0x45fb80,0x460300,0x4aa000,0x4ae0e0)),'actual CPU readers, DX copy and declaration lookup')
 check(len(f.declaration_arrays)==1,'only native field decoded despite cross/native coexistence')
 vb=f.buffers[p.uint(p.uint(mesh+0x58)+0x10)];ib=f.buffers[p.uint(p.uint(mesh+0x54)+0x10)]
 observed=[[p.uint(mesh+off) for off in (0x70,0x74,0x64,0x60,0x80)],bytes(p.mu.mem_read(vb['data'],vb['size'])).hex(),bytes(p.mu.mem_read(ib['data'],ib['size'])).hex()]
 check(observed==source[5],'exact original/source decoded metadata and all vertex/index bytes')
 f.call(0x4aa350,this=mesh,args=(1,));f.clear_declarations()
 f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,));f.call(0x4228a0,this=manager)
 for address in (0x75db78,0x75526c,0x755264):
  obj=p.uint(address)
  if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
 check(all(b['refs']==0 for b in f.buffers.values()) and set(f.allocations)==set(f.freed),'all native reader/COM owners released')
 print(f'PASS exact writer/original reader/source backend {mode}/{policy}; nativeAssertions={7+counters.checks}; read={instructions}; arena={p.allocated}',flush=True)
 return 0
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
