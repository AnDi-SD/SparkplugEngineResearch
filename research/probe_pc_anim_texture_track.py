#!/usr/bin/env python3
"""Original bounded PC AnimTex read/update/index/write with prebound CPU textures.

Textures are initialized by their actual tiny CPU cross reader. No GPU, timing
API, frame-evaluation seam or dangerous zero-duration update. Positive tracks
only for runtime; empty tracks are codec-only. Prebound read is not inline read.
"""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_loader_fixtures import empty_manager,empty_fat,animation_rtti
from probe_pc_node_serializer import field

def main(mode,return_capture=False):
    if mode=='three':raise ValueError('DISABLED: first FAT-read scout omitted RTTI and reached cold414C60 allocation guard. Never retry/resume that path.')
    if not mode.startswith('staged-'):raise ValueError('Only explicitly staged native FAT entries; no directory reader retry')
    mode=mode.removeprefix('staged-')
    if mode not in ('three','alias','null','empty','duplicate','negative','boundary'):
        raise ValueError('small positive-duration track or codec-only empty')
    times=[] if mode=='empty' else [1.,1.,3.] if mode=='duplicate' else [1.,2.,3.]
    ids=[] if mode=='empty' else [7,7,7] if mode=='alias' else [7,0,9] if mode=='null' else [7,8,9]
    used=sorted(set(ids)-{0})
    payload=field(0,struct.pack('<I',len(times))+struct.pack('<'+'f'*len(times),*times)+b''.join(struct.pack('<II',i,0) if i else bytes(4) for i in ids))+b'\0'
    f=PCWriteBytesFixture(payload);p=f.p;checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    # Alternative to the disabled directory load: use already verified native
    # save-entry construction466FA0 as explicit prebound reader input. No call
    # to466B90 or cold414C60 is made. A valid base RTTI chain is declared here.
    animation_rtti(f)
    def deny_cold(_):raise AssertionError('Known-capped cold RTTI entry is disabled before execution')
    p.seams[0x414c60]=deny_cold
    for record,identity,parent in ((0x75df10,0x2f281e13,0x7603a0),
                                  (0x7603a0,0x46f043fe,0x7555f8),(0x7555f8,0x44de07fd,0x755310),
                                  (0x75d1e8,0x16fb0e47,0x75deb0),(0x75deb0,0x14477ac7,0x75de50)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    animation_manager=f.call(0x454640);manager,_=empty_manager(f);fat=empty_fat(f)
    p.put_uint(manager+0x28,fat);p.put_uint(manager+0x10,2);p.put_uint(manager+0x14,1);p.put_uint(manager+0x18,2);p.put_uint(0x75dde8,manager)
    serializer=f.call(0x43c0c0);texture_serializer=f.call(0x42dc30)
    f.call(0x422d90,this=manager,args=(0x16fb0e47,serializer,0xff,3))
    f.call(0x422d90,this=manager,args=(0x78ea082b,texture_serializer,0xff,3))
    objects={}
    for identity in used:
        texture=f.call(0x41a2d0);objects[identity]=texture
        nested=field(5,struct.pack('<4I',2,1,0,4)+bytes([identity]*8))+b'\0'
        f.data=field(2,b'\0')+b'\0'+field(6,struct.pack('<I',1))+field(0,nested)+b'\0';f.position=0
        check(f.call(0x42f180,this=texture_serializer+0x10,args=(f.stream,texture))&255==1 and f.position==len(f.data),'actual CPU texture cross initialization')
        record=f.call(p.uint(p.uint(texture)+0x10),this=texture)
        p.put_uint(record,0x78ea082b);p.put_uint(record+0x48,0x75df10)
        p.put_uint(fat+0x10,identity)
        check(f.call(0x466fa0,this=fat,args=(0x78ea082b,texture))&255==1,'actual native entry construction supplies staged prebound FAT input')
    f.data=payload;f.position=0;controller=f.call(0x41a030)
    result=f.call(0x43c470,this=serializer+0x10,args=(f.stream,controller))&255
    print('ANIM_TRACK_READ',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'position',f.position,'errors',f.errors,flush=True)
    if mode=='empty':
        check(result==0 and len(f.errors)==2 and f.position==len(f.data)-1,'FileStream zero-byte times read fails; wrapper reports failure before terminator')
        f.errors.clear() # independent fresh writer observation below, no retry
    else:check(result==1 and f.position==len(f.data) and not f.errors,'complete original animated texture reader')
    check(p.uint(controller+0x44)==len(times) and (not times or list(p.floats(p.uint(controller+0x3c),len(times)))==times),'native times and count')
    slots=[p.uint(p.uint(controller+0x40)+4*i) for i in range(len(ids))]
    check(slots==[objects.get(i,0) for i in ids],'canonical prebound frame references')
    check(all((p.uint(obj+8)&65535)==ids.count(identity) for identity,obj in objects.items()),'one retained texture edge per frame slot')
    holder=f.call(0x467f30);f.call(0x476680,this=holder,args=(controller,));states=[]
    if times:
        increments=[-1.,.5,.5,1.,1.,1.,3.,6.5] if mode=='negative' else [0.,.999, .001, .999, .001,1.,.5,3.,6.] if mode=='boundary' else [0.,.5,.5,1.,1.,.5,3.,6.]
        for delta in increments:
            delta=struct.unpack('<f',struct.pack('<f',delta))[0]
            f.call(0x423190,this=controller,args=(struct.unpack('<I',struct.pack('<f',delta))[0],))
            f.call(0x42fc60,this=controller)
            selected=p.uint(holder+0x34);identity=next((i for i,o in objects.items() if o==selected),0)
            states.append([delta,*p.floats(controller+0x1c,2),p.floats(controller+0x48,1)[0],identity])
            check(all((p.uint(obj+8)&65535)==ids.count(i)+(selected==obj) for i,obj in objects.items()),'frame array and current fallback ownership exact after tick')
        print('ANIM_TRACK_STATES',states,flush=True)
    f.call(0x466760,this=fat);p.put_uint(manager+0x14,2)
    check(f.call(0x4672c0,this=serializer,args=(controller,))&255==1,'original controller recursive texture indexing')
    f.data=b'';f.position=0
    result=f.call(0x43c5d0,this=serializer+0x10,args=(f.stream,controller))&255
    print('ANIM_TRACK_WRITE',mode,result,'instructions',sum(p.visits.values()),'heap',p.allocated,'hex',f.data.hex(),'errors',f.errors,flush=True)
    check(result==1 and not f.errors,'original controller/common inline CPU texture writer')
    capture=[mode,payload.hex(),times,ids,states,f.data.hex()]
    f.call(0x466760,this=fat);f.call(p.uint(p.uint(holder)),this=holder,args=(1,))
    check(controller in f.freed and all(o in f.freed for o in objects.values()),'holder teardown deletes controller, frame arrays and textures')
    check(p.uint(animation_manager+0x24)==p.uint(animation_manager+0x28)==0,'controller unregisters actual animation manager')
    f.call(0x4545d0,this=animation_manager,args=(1,));f.call(0x4228a0,this=manager)
    for address in (0x75db78,0x75526c,0x755264):
        owner=p.uint(address)
        if owner:f.call(p.uint(p.uint(owner)),this=owner,args=(1,))
    if mode=='empty':
        leaked=[a for a in f.allocations if a not in f.freed]
        check(len(leaked)==1 and f.allocations[leaked[0]]==0,'failed zero-byte read leaks its native zero-size times allocation')
        p.run(0x412420,args=(leaked[0],),callee_pop=False) # explicit post-observation cleanup
    check(set(f.allocations)==set(f.freed),'all original allocations freed after explicitly labelled failure cleanup')
    print('ANIM_TRACK_CAPTURE',capture,flush=True);print(f'PASS {checks}/{checks}: original animated texture {mode}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
