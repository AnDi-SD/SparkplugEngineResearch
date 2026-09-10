#!/usr/bin/env python3
"""Independent PS2 constructor prefixes, controller leaves and intrusive lists."""
import hashlib,json,struct,time,traceback
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED
from ps2_scalar_prefix import Ps2ScalarPrefix


def main():
    p=ROOT/'local-data/results/native-cycle-20260910-1900/ps2-remainder';output=p/'controller-contracts-run1.json'
    if output.exists():raise ValueError('Fresh report required')
    t=time.perf_counter();r=dict(kind='independent-ps2-controller-contracts',status='running',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),cases=[],scope='Fresh bounded scalar guests, actual original bytes. Post-base constructor and post-save Copy prefixes are explicit; no SQ/LQ emulation,full allocation/lifetime or arbitrary R5900 floating-point equivalence.')
    def save():r['seconds']=time.perf_counter()-t;output.write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8')
    def base(ranges,data=b'\xa5'*0x400):
        q=Ps2ScalarPrefix(ranges);q.map(0x21000000,4096);q.write(0x21000000,data);q.map(0x22000000,4096);q.reg('SP',0x22000800);return q
    def done(kind,execution,**kw):r['cases'].append(dict(kind=kind,execution=execution,**kw));save()
    try:
        for n,entry,stop,reg,size,changes in [
            ('spActor',0x11883c,0x118ba0,'S1',84,{0:0x48cc00,0x1c:b'\1',0x24:b'\1',0x28:0}),
            ('spController',0x11a124,0x11a140,'S0',28,{0:0x48cc60,0x10:b'\1',0x14:0,0x18:0}),
            ('spSubController',0x11be34,0x11be44,'S0',16,{0:0x48cd30}),
            ('spTransformTrackEval',0x11df40,0x4076a8,'S2',120,{0:0x48ce80,0x10:0xffffffff,0x14:0,0x18:0,0x1c:0})]:
            q=base([(entry,0x40)]);o=0x21000000;q.reg(reg,o);expected=bytearray(b'\xa5'*0x400)
            for at,v in changes.items():expected[at:at+(len(v) if isinstance(v,bytes) else 4)]=v if isinstance(v,bytes) else struct.pack('<I',v)
            ex=q.run(entry,[stop]);assert q.read(o,0x400)==expected
            if n=='spTransformTrackEval':assert (q.reg('A0'),q.reg('A1'),q.reg('A2'))==(o+0x24,0,12)
            done('post-base-construction',ex,className=n,allocationBytesFromFactory=size,modifiedOffsets=list(changes))
        for n,address in [('spController',0x11a160),('spSubController',0x11be60),('spRenderController',0x11bda0)]:
            q=base([(address,8)]);q.reg('A0',0x21000000);q.reg('V0',0x12345678);ex=q.run(address,[q.RETURN]);assert q.reg('V0')==0 and q.read(0x21000000,0x400)==b'\xa5'*0x400;done('original-null-clone',ex,className=n)
        for address in (0x1190c0,0x116400):
            for word in (0,1,0x21000200,0xffffffff):
                data=bytearray(b'\xa5'*0x400);struct.pack_into('<I',data,0x14,word);q=base([(address,8)],data);q.reg('A0',0x21000000);ex=q.run(address,[q.RETURN]);assert q.reg('V0')&0xffffffff==word and q.read(0x21000000,0x400)==data;done('node-controller-word14-getter',ex,input=word)
        for first,second in ((0,0),(0x21000100,0),(0x21000100,0x21000200)):
            data=bytearray(b'\xa5'*0x400);struct.pack_into('<I',data,0x10,first);struct.pack_into('<I',data,0x110,second);q=base([(0x11b480,0x28)],data);q.reg('A0',0x21000000);ex=q.run(0x11b480,[q.RETURN]);assert q.reg('V0')==(second+9 if first and second else 0) and q.read(0x21000000,0x400)==data;done('node-controller-two-link-buffer-query',ex,first=first,second=second)
        for address,size in ((0x120fa0,12),(0x120fb0,8)):
            q=base([(address,size)]);q.reg('A0',0x21000000);q.reg('F0',0x3f800000);ex=q.run(address,[q.RETURN]);assert q.read(0x21000000,0x400)==b'\xa5'*0x400
            if address==0x120fa0:assert q.reg('F0')&0xffffffff==0
            done('track-base-zero-float-or-noop',ex)
        for old,dt in ((-2,.5),(0,0),(1,2),(2,-4)):
            data=bytearray(b'\xa5'*0x400);struct.pack_into('<f',data,0x20,old);expected=bytearray(data);struct.pack_into('<f',expected,0x20,old+dt);q=base([(0x11bcf0,16)],data);q.reg('A0',0x21000000);q.reg('F12',struct.unpack('<I',struct.pack('<f',dt))[0]);ex=q.run(0x11bcf0,[q.RETURN]);assert q.read(0x21000000,0x400)==expected;done('render-controller-finite-add',ex,old=old,delta=dt,result=old+dt)
        for value in (0,1,255):
            data=bytearray(b'\xa5'*0x400);data[0x10]=value;expected=bytearray(data);expected[0x110]=value;q=base([(0x11a030,0x58),(0x100320,0x48),(0x104f00,8)],data);q.map(0x49f000,4096);q.put_uint(0x49f810,0x21000300);q.reg('GP',0x4a4170);q.reg('A0',0x21000000);q.reg('A1',0x21000100);ex=q.run(0x11a030,[0x11a084]);assert q.reg('V0')==1 and q.read(0x21000000,0x400)==expected;done('controller-copy-byte10-only',ex,input=value)
        # Each next guest receives precisely the preceding guest's whole output.
        for order in ((1,0,2),(2,1,0),(0,1,2)):
            data=bytearray(b'\xa5'*0x400);o=0x21000000;objects=[o+0x100+i*0x40 for i in range(3)];struct.pack_into('<II',data,0x28,0,0)
            for obj in objects:struct.pack_into('<II',data,obj-o+0x14,0,0)
            alive=[]
            for operation,index in [('append',i) for i in range(3)]+[('remove',i) for i in order]:
                address=0x118d80 if operation=='append' else 0x118d10;obj=objects[index];expected=bytearray(data)
                if operation=='append':
                    if alive:
                        struct.pack_into('<I',expected,alive[-1]-o+0x14,obj);struct.pack_into('<I',expected,obj-o+0x18,alive[-1])
                    else:struct.pack_into('<I',expected,0x28,obj)
                    struct.pack_into('<I',expected,0x2c,obj);alive.append(obj)
                else:
                    i=alive.index(obj);previous=alive[i-1] if i else 0;following=alive[i+1] if i+1<len(alive) else 0
                    if i==len(alive)-1:struct.pack_into('<I',expected,0x2c,previous)
                    if i==0:struct.pack_into('<I',expected,0x28,following)
                    if following:struct.pack_into('<I',expected,following-o+0x18,previous)
                    if previous:struct.pack_into('<I',expected,previous-o+0x14,following)
                    alive.remove(obj)
                q=base([(address,0x34 if operation=='append' else 0x68)],data);q.reg('A0',o);q.reg('A1',obj);ex=q.run(address,[q.RETURN]);data=bytearray(q.read(o,0x400));assert data==expected
                done('manager-intrusive-list',ex,removeOrder=order,operation=operation,index=index,alive=alive.copy(),retainsRemovedObjectLinks=True)
        r['status']='passed';save()
    except Exception as error:
        r.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();raise
    print(json.dumps(dict(status='passed',cases=len(r['cases']),seconds=r['seconds'])))


if __name__=='__main__':main()
