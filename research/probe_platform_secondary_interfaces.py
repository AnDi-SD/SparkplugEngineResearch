import json,struct,sys,hashlib,time
from pathlib import Path
sys.path.insert(0,'research')
from capture_native_ranges import *
from ps2_scalar_prefix import Ps2ScalarPrefix
p=ROOT/'local-data/results/native-cycle-20260910-1900/engine-platform-remainder';raw=PS2.read_bytes();elf=read_elf_sections(raw);rows=json.loads((p/'catalog-family.json').read_text());cases=[];t=time.perf_counter()
for n,v,entry,stop,alias,getter in [('spPS2BallisticPFX',0x491d30,0x20a0c8,0x20a0e4,0x20a150,0x209dd0),('spPS2VideoStream',0x491e30,0x20d8e8,0x20d8d4,0x20d940,0x20d620)]:
 q=Ps2ScalarPrefix([(entry,0x20)]);o=0x21000000;q.map(o,4096);before=b'\xcc'*36;q.write(o,before);q.reg('S0',o);execution=q.run(entry,[stop]);expected=bytearray(before);primary=0x491d20 if 'Ballistic' in n else 0x491e00;struct.pack_into('<II',expected,0,primary,v)
 if 'Ballistic' in n:struct.pack_into('<I',expected,0x20,0)
 assert q.read(o,36)==expected
 table,_=read_window('ps2',raw,v,36,elf);w=list(struct.unpack('<9I',table));assert w[:2]==[0,0] and w[6]==alias
 g=Ps2ScalarPrefix([(alias,8),(getter,12)]);g.reg('A0',o+4);run=g.run(alias,[g.RETURN]);assert g.reg('A0')==o and g.reg('V0')==next(r['ps2Record'] for r in rows if r['className']==n)
 cases.append(dict(className=n,objectBytes=36,interfaceOffset=4,primaryTable=f'{primary:08X}',baseObjectTable=f'{v:08X}',slots=[f'{x:08X}' for x in w[2:]],constructionPrefix=execution,prefixOutput=bytes(expected).hex(),getterAdjustor=run))
out=dict(kind='independently-verified-ps2-secondary-base-interfaces',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),status='passed',cases=cases,seconds=time.perf_counter()-t,scope='Original post-base constructor prefixes and adjusted getters, fresh guests and whole-object guards. No complete PS2 construction,allocation,Clone,particle pipeline or video execution.')
(p/'secondary-interfaces-run1.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out))
