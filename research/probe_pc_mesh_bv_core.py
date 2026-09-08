#!/usr/bin/env python3
"""Bounded original-PC MeshBV ownership/geometry/transform probe for tools."""
from pathlib import Path
import hashlib,json,struct,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_node_serializer import field

def main(mode,output):
    face_modes=('faces','face-defaults','faces-unknown')
    if mode not in ('geometry','defaults',*face_modes):raise ValueError('Explicit micro case required')
    started=time.monotonic();f=PCWriteBytesFixture();p=f.p
    if mode in face_modes:
        from probe_pc_node_relationships import node_rtti
        node_rtti(f)
        # Replace the explicit one-entry startup fixture with the exact
        # wxFaceData record/factory. Original lookup and factory still execute.
        node=p.uint(p.uint(p.uint(0x755378)+0x18)+4)
        p.put_uint(node+12,0x313c4c17);p.put_uint(node+16,0x7669a8)
        for record,identity,parent in ((0x7669a8,0x313c4c17,0x7664e0),
                (0x7664e0,0x76181f6b,0x7610f8),(0x7610f8,0x6a1d0d2e,0x755310)):
            p.put_uint(record,identity);p.put_uint(record+0x48,parent)
        p.put_uint(0x7669a8+0x4c,0x5a4e30)
    obj=f.call(0x47aff0);serializer=f.call(0x4383b0)
    def capture():
        data=p.uint(obj+0x28)
        row=dict(sphere=p.floats(obj+0x18,4),data=data,
                 vtable=f'{p.uint(obj):08X}',vslots=[f'{p.uint(p.uint(obj)+i*4):08X}' for i in range(10)])
        if data:
            ib,vb=p.uint(data+0x10),p.uint(data+0x14)
            row.update(indexBuffer=ib,vertexBuffer=vb,faces=p.uint(data+0x18),
                       indexWords=[p.uint(ib+i) for i in (0x14,0x18,0x1c,0x20)],
                       vertexWords=[p.uint(vb+i) for i in (0x14,0x18,0x1c,0x20)])
            faces=p.uint(data+0x18)
            if faces:
                count=p.uint(faces+0x14);items=p.uint(faces+0x18)
                row['faceClass']=f'{p.uint(faces+0x1c):08X}'
                row['faceValues']=[list(struct.unpack('<BxHB',p.mu.mem_read(p.uint(items+i*4)+0x14,5))) for i in range(count)]
        return row
    report=dict(mode=mode,defaults=capture(),objectBytes=f.allocations[obj],serializerBytes=f.allocations[serializer])
    if mode!='defaults':
        geometry=struct.pack('<3I3H3I9f',2,1,0,0,1,2,0,3,0,0,0,0,2,0,0,0,4,0)
        f.data=field(0,geometry)
        if mode in face_modes:
            leaf=bytes.fromhex('0101070202cdab0301de00') if mode!='face-defaults' else b'\0'
            if mode=='faces-unknown':leaf=bytes.fromhex('ac0202aa550301110101090202cdab0301de00')
            f.data+=field(1,struct.pack('<II',0x313c4c17,1)+leaf)
        f.data+=b'\0';f.position=0;raw=f.data
        result=f.call(0x438490,this=serializer+0x10,args=(f.stream,obj))&255
        report.update(readResult=result,position=f.position,input=raw.hex(),loaded=capture(),readInstructions=sum(p.visits.values()))
        assert result==1 and f.position==len(raw) and not f.errors
        parameters=p.allocate(60);values=(7.,8.,9.,1.,0.,0.,0.,1.,0.,0.,0.,1.,2.,3.,4.)
        p.put_floats(parameters,values)
        # Native slot also receives CollisionInfo*, even though MeshBV is
        # exactly RET 16 and touches none of its four arguments.
        f.call(p.uint(p.uint(obj)+0x1c),this=obj,args=(parameters,parameters+12,parameters+48,0))
        report['transformBefore']=values;report['transformAfter']=p.floats(parameters,15)
        f.data=b'';f.position=0
        assert f.call(0x438680,this=serializer+0x10,args=(f.stream,obj))&255==1
        assert f.position==len(f.data) and not f.errors
        report['written']=f.data.hex()
        if mode=='faces-unknown':
            face=p.uint(p.uint(p.uint(p.uint(obj+0x28)+0x18)+0x18))
            f.call(0x52fd90,this=0x755588)
            manager=f.call(0x412540);p.put_uint(0x74e060,manager)
            clone=f.call(0x412be0,this=manager,args=(face,))
            report['faceCloneValues']=list(struct.unpack('<BxHB',p.mu.mem_read(clone+0x14,5)))
            assert report['faceCloneValues']==[9,43981,222]
            f.call(p.uint(p.uint(clone)),this=clone,args=(1,));f.call(0x6d7db0)
            f.call(p.uint(p.uint(manager)),this=manager,args=(1,))
    for value in (obj,serializer):f.call(p.uint(p.uint(value)),this=value,args=(1,))
    assert set(f.allocations)==set(f.freed),'all native owners released'
    report.update(kind='original-pc-mesh-bv-core',status='passed',profile='micro',
                  executableSha256=hashlib.sha256((ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()).hexdigest().upper(),
                  probeSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
                  releasedAllocations=len(f.freed),arenaReservedBytes=p.allocated,seconds=time.monotonic()-started,
                  scope='Original factory, serializer, native buffer readers, bounds/tree preparation and transform slot. No collision queries or game startup.')
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results');target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report),flush=True);return 0

if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(main(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
