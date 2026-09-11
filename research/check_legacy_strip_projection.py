"""Focused host mesh C ABI regression against retained original PC upload bytes."""
from pathlib import Path
import ctypes as C,hashlib,json,struct,subprocess,sys,time
from validate_tools_mesh_reader import Info
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def child(output):
 output=ROOT/output
 if output.exists():raise ValueError('Fresh report required')
 base=ROOT/'local-data/results/tools-core-cycle-20260911-1900/menu-mesh';source=ROOT/'local-data/pc-pristine/Media/Menus/menu.smo'
 inspection=base/'initial-inspection.json';original=base/'original-run1/report.json';dll=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll'
 rows=json.loads(inspection.read_text());old=json.loads(original.read_text());assert old['status']=='passed-scoped-reader-upload'
 raw=source.read_bytes();assert sha(source)==old['sourceSha256'];record=dict(status='running',sourceSha256=sha(source),
  nativeSha256=sha(dll),originalSha256=sha(original),inspectionSha256=sha(inspection),checks=0,meshes=[])
 started=time.perf_counter();u=C.c_uint32;p=C.c_void_p;lib=C.CDLL(str(dll))
 for name,args,result in [('spv_mesh_read',[p,u,u,u],p),('spv_mesh_destroy',[p],None),('spv_mesh_info',[p,p],C.c_int),
  ('spv_mesh_indices',[p,p,u],C.c_int),('spv_mesh_triangles',[p,p,u,p],C.c_int),('spv_last_error',[],C.c_char_p)]:
  fn=getattr(lib,name);fn.argtypes=args;fn.restype=result
 def check(ok,message):
  record['checks']+=1
  if not ok:raise AssertionError(message+': '+(lib.spv_last_error()or b'').decode())
 try:
  for row in rows:
   wire=raw[row['offset']:row['offset']+row['size']];payload=wire[8:];h=lib.spv_mesh_read(C.c_char_p(payload),len(payload),0,1)
   check(h,'original menu mesh accepted without source repair')
   try:
    info=Info();check(lib.spv_mesh_info(h,C.byref(info)),'mesh metadata');stored=(u*info.indices)()
    check(lib.spv_mesh_indices(h,stored,len(stored)),'raw indices');check(list(stored)==row['indices'],'raw source indices unchanged')
    count=u();check(lib.spv_mesh_triangles(h,None,0,C.byref(count)),'query triangle count');result=(u*count.value)()
    check(lib.spv_mesh_triangles(h,result,len(result),C.byref(count)),'copy triangles')
    expected=[]
    for i in range(len(stored)-2):
     a,b,c=stored[i:i+3]
     if len({a,b,c})==3:expected.extend((b,a,c) if i%2 else (a,b,c))
    check(list(result)==expected and all(v<info.vertices for v in result),'correct safe winding and degenerates')
    check(stored[-2:]==[0xcdcd,0xcdcd],'specific legacy tail preserved')
    canary=u(0xa5a5a5a5);guard=(u*max(1,len(result)))(*([0xa5a5a5a5]*max(1,len(result))));before=bytes(guard)
    check(not lib.spv_mesh_triangles(h,guard,max(0,len(result)-1),C.byref(canary)) and canary.value==0xa5a5a5a5 and bytes(guard)==before,'atomic short output guard')
    captured=next((item for item in old['cases'] if item['id']==row['id']),None)
    if captured:check(bytes(stored)==b''.join(struct.pack('<I',v) for v in struct.unpack('<'+str(info.indices)+'H',bytes.fromhex(captured['uploadedIndices']))),'matches actual original upload capture')
    record['meshes'].append(dict(id=row['id'],vertices=info.vertices,rawIndices=info.indices,triangles=len(result)//3))
   finally:lib.spv_mesh_destroy(h)
  # Corrupt a nondegenerate source triangle, leaving framing intact.
  wire=bytearray(raw[rows[0]['offset']:rows[0]['offset']+rows[0]['size']]);struct.pack_into('<H',wire,25+3*2,0xcdcd)
  h=lib.spv_mesh_read(C.c_char_p(bytes(wire[8:])),len(wire)-8,0,1)
  if h:lib.spv_mesh_destroy(h)
  check(not h,'a truly invalid emitted triangle still fails')
  check(sha(source)==record['sourceSha256'],'source file unchanged');record['status']='passed'
 except Exception as error:record.update(status='failed',error=repr(error))
 record['elapsedSeconds']=time.perf_counter()-started;record['scriptSha256']=sha(__file__)
 output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(record,indent=2)+'\n')
 print(json.dumps({k:record.get(k) for k in ('status','checks','elapsedSeconds','error')}));return 0 if record['status']=='passed' else 1
if __name__=='__main__':
 if sys.argv[1:2]==['--child']:raise SystemExit(child(*sys.argv[2:]))
 raise SystemExit(subprocess.run([sys.executable,str(Path(__file__)), '--child',*sys.argv[1:]],cwd=ROOT,timeout=30).returncode)
