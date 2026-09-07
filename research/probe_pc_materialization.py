#!/usr/bin/env python3
"""Original PC outer FAT materialization422940, independent directed inputs."""
from pathlib import Path
import sys
import probe_pc_read_reference as refs
from probe_pc_read_reference import ReadReferenceFixture,EMPTY_OBJECT,check
from pc_instruction_emulator import run_bounded


def main(mode):
    allowed={'empty','one','two','prebound','external-root','bad-offset','short-size','failed-payload'}
    if mode not in allowed:raise ValueError('bounded materialization mode required')
    ids=() if mode=='empty' else (7,9) if mode in {'two','prebound','external-root'} else (7,)
    payload=EMPTY_OBJECT*len(ids)
    if mode=='failed-payload':payload=EMPTY_OBJECT[:9]
    f=ReadReferenceFixture(payload,ids=ids);p=f.p
    p.put_uint(f.stream+0x14,f.directory_end);f.payload_start=f.directory_end+8
    entries=[f.call(0x4664c0,this=f.fat,args=(identity,)) for identity in ids]
    for i,entry in enumerate(entries):p.put_uint(entry+0x14,i*55)
    if mode=='prebound':
        obj=f.call(0x41a090);f.objects.append(obj);p.put_uint(entries[0]+0x20,obj)
    if mode=='external-root':p.put_uint(entries[0]+8,42)
    if mode=='bad-offset':p.put_uint(entries[0]+0x14,999)
    if mode=='short-size':p.put_uint(entries[0]+0x18,1)
    result=f.call(0x422940,this=f.manager,args=(f.stream,));visited=set(p.visits)
    objects=[p.uint(e+0x20) for e in entries]
    f.objects.extend(obj for obj in objects if obj)
    print('MATERIALIZATION',mode,'result',hex(result),'objects',[hex(o) for o in objects],
          'position',f.position,'instructions',sum(p.visits.values()),'errors',f.errors,flush=True)
    if mode=='empty':
        check(result==0 and not f.io and not f.errors,'empty FAT returns null without I/O/error')
    elif mode=='bad-offset':
        check(result==0 and objects==[0] and bool(f.errors),'failed FAT seek returns null and diagnostic before factory')
        check(f.position==f.directory_end and 0x467550 not in visited,'failed seek leaves stream and allocation unchanged')
    elif mode=='failed-payload':
        check(result==0 and objects[0] in f.allocations and bool(f.errors),'failed secondary reader returns null but retains object')
        check(f.publications==objects and f.position==len(f.data),'partial object published before failed duration read')
    else:
        check(f.position==len(f.data) and not f.errors,'successful data consumed')
        if mode=='prebound':
            check(result==objects[1] and objects[0]!=objects[1],'already materialized first entry is excluded from return-root selection')
            check([io for io in f.io if io[0]=='seek']==[('seek',1,55,f.directory_end+55)],'prebound first payload never touched')
        elif mode=='external-root':
            check(result==0 and objects[0]==0 and objects[1] in f.allocations,'unresolved fileID first entry consumes root flag: later object does not become root')
            check(0x466490 in visited,'actual file-map lookup called, result not published')
        else:
            check(result==objects[0] and len(set(objects))==len(ids),'first newly processed entry is return root')
            check(all(obj in f.allocations for obj in objects),'all runtime objects created by actual factory')
        check(p.uint(f.fat+0x50)==len(ids),'outer helper does not clear FAT on return')
        if mode=='short-size':check(p.uint(entries[0]+0x18)==1,'FAT size1 ignored by native field reader')
    f.cleanup()
    print(f'PASS {refs.checks}/{refs.checks}: original PC outer materialization {mode}')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
