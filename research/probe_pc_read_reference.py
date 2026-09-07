#!/usr/bin/env python3
"""Original PC reference reader; bounded guest, explicit stream/startup inputs."""
from pathlib import Path
import struct,sys
from pc_instruction_emulator import run_bounded
from pc_loader_fixtures import PCFileBytesFixture,empty_manager,empty_fat,animation_rtti

checks=0
CLASS=0x56ee563a
EMPTY_FIELDS=bytes.fromhex('60000000007f40000000006c0000000066000000006700000000680000000069000000006a000000006b0000000000')
EMPTY_OBJECT=struct.pack('<II',CLASS,0x4f4f4253)+EMPTY_FIELDS


def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)


class ReadReferenceFixture(PCFileBytesFixture):
    def __init__(self,payload,ids=(7,)):
        directory=struct.pack('<I',len(ids))+b''.join(struct.pack('<IHIII',i,0,CLASS,0,55) for i in ids)
        self.entry=0;self.publications=[];self.payload_start=len(directory)+16
        self.id_stream=0;self.id_data=b'';self.id_position=0
        self.native_names=0
        super().__init__(directory+payload);p=self.p;animation_rtti(self)
        self.fat=empty_fat(self);self.manager,_=empty_manager(self)
        p.put_uint(self.manager+0x28,self.fat);p.put_uint(self.manager+0x10,1)
        p.put_uint(self.manager+0x14,1);p.put_uint(0x75dde8,self.manager)
        self.serializer=self.call(0x43dab0)
        self.call(0x422d90,this=self.manager,args=(CLASS,self.serializer,0xff,3))
        check(self.call(0x466b90,this=self.fat,args=(self.stream,))&255==1,'actual native directory input')
        self.directory_end=self.position;self.entry=self.call(0x4664c0,this=self.fat,args=(7,))
        self.objects=[];self.io.clear()

    def read(self,p):
        if self.id_stream and p.reg('ECX')==self.id_stream:
            sp=p.reg('ESP');count=p.uint(sp+8);actual=min(count,len(self.id_data)-self.id_position)
            if actual:p.mu.mem_write(p.uint(sp+4),self.id_data[self.id_position:self.id_position+actual])
            self.id_position+=actual;p.fixture_return(8,eax=int(actual!=0));return
        if self.entry and self.position==self.payload_start:
            self.publications.append(p.uint(self.entry+0x20))
        super().read(p)

    def read_reference(self,expected=CLASS):
        return self.call(0x4678b0,this=self.serializer,args=(expected,self.id_stream or self.stream,self.stream))

    def cleanup(self):
        p=self.p
        self.call(0x466760,this=self.fat)
        for obj in dict.fromkeys(self.objects):self.call(p.uint(p.uint(obj)),this=obj,args=(1,))
        if self.native_names:
            from probe_pc_animation_manager import registry_entries
            check(registry_entries(p,self.native_names)=={},'reference tracks release original name registrations')
            self.call(0x4545d0,this=self.native_names,args=(1,))
        self.call(0x4228a0,this=self.manager)
        for address in (0x75db78,0x75526c,0x755264):
            obj=p.uint(address)
            if obj:self.call(p.uint(p.uint(obj)),this=obj,args=(1,))
        check(set(self.allocations)==set(self.freed),'all tracked allocations released without FAT owning objects')


def success(mode='success'):
    split=mode=='split'
    declared={'declared-short':1,'declared-long':999}.get(mode,55)
    first=struct.pack('<II',7,declared)+EMPTY_OBJECT
    payload=first+struct.pack('<III',7,0,0)
    f=ReadReferenceFixture(payload);p=f.p
    if mode=='unresolved-file':p.put_uint(f.entry+8,42)
    if split:
        # First argument is expected type; second stream supplies ID only,
        # third supplies inline-size/object. Distinct cursors prove this ABI.
        f.id_stream=p.allocate(0x20);p.put_uint(f.id_stream,p.uint(f.stream))
        f.id_data=struct.pack('<I',7);f.position+=4
    result=f.read_reference(0xdeadbeef);visited=set(p.visits);f.objects.append(result)
    print('RESULT',hex(result),'instructions',sum(p.visits.values()),'position',f.position,'io',f.io,flush=True)
    check(result in f.allocations and p.uint(result)==0x6de6cc,'real factory produces Animation despite wrong expected type')
    check(p.uint(f.entry+0x20)==result and f.publications==[result],'entry published BEFORE first payload-field read')
    check(all(a in visited for a in (0x467670,0x4664c0,0x4224f0,0x4586b0,0x467550,0x43ecc0)),
          'actual lookup/cache/header/factory/payload chain')
    check(f.position==f.directory_end+63 and not any(io[0]=='seek' for io in f.io),'first reference reads inline, no FAT-offset seek')
    check(p.uint(result+8)&0xffff==0,'resolver returns borrowed pointer without retain')
    if mode in ('declared-short','declared-long'):
        check(declared!=55,'first materialization never bounds/skips to declared inline size')
    if mode=='unresolved-file':
        check(0x466490 not in visited and p.uint(f.entry+8)==42,'unresolved reference ignores fileID and reads inline')
    if split:
        check(f.id_position==4,'separate ID stream consumed exactly four bytes');f.id_stream=0
    check(f.read_reference()==result and 0x43ecc0 not in p.visits,'repeat returns same pointer without reading payload')
    check(f.read_reference()==0 and f.position==len(f.data),'null consumes only ID, not size')
    check(not f.errors,'normal references have no diagnostics');f.cleanup()


def materialized(mode):
    size=1000 if mode=='failed-skip' else 4
    f=ReadReferenceFixture(struct.pack('<II',7,size)+b'abcd'+struct.pack('<III',9,0,0),ids=(7,9));p=f.p
    obj=f.call(0x41a090);f.objects.append(obj)
    other=f.call(0x4664c0,this=f.fat,args=(9,))
    p.put_uint(f.entry+0x20,obj);p.put_uint(other+0x20,obj)
    p.put_uint(f.entry+8,42) # resolver never reads fileID, even nonzero
    count=p.uint(obj+8)
    check(f.read_reference()==obj,'already materialized reference returned')
    check(0x4224f0 not in p.visits and 0x4586b0 not in p.visits,'materialized bypasses serializer and cache')
    check(f.io[-1]==('seek',4,size,f.directory_end+8+size),'skip uses relative-current inline size, not FAT size/offset')
    if mode=='failed-skip':
        check(f.position==f.directory_end+8 and not f.errors,'failed skip still returns object silently')
    else:
        check(f.position==f.directory_end+12,'inline payload skipped')
        check(f.read_reference()==obj,'different FAT IDs may alias same object')
        check(f.read_reference()==0 and f.position==len(f.data),'null after alias')
    check(p.uint(obj+8)==count,'all alias/skip paths retain reference count');f.cleanup()


def early(mode):
    payload={'null':bytes(4),'failed-id':b'','missing-id':struct.pack('<II',99,0),
             'failed-size':struct.pack('<I',7)}[mode]
    f=ReadReferenceFixture(payload);p=f.p
    if mode=='missing-id':
        p.run(0x4678b0,this=f.serializer,args=(CLASS,f.stream,f.stream),stop_at=0x4676b9)
        check(p.reg('EIP')==0x4676b9 and f.position==len(f.data),'missing FAT ID reaches diagnostic before sprintf; no OS forwarding')
        check(p.uint(f.entry+0x20)==0,'missing ID does not materialize unrelated entry')
    elif mode=='failed-size':
        p.run(0x4678b0,this=f.serializer,args=(CLASS,f.stream,f.stream),stop_at=0x467698)
        check(p.reg('EAX')&255==0 and f.position==len(f.data),'inline-size read failed')
        check(p.reg('EIP')==0x467698,'native continues past failed size read, no conditional result test')
    else:
        check(f.read_reference()==0,'null or failed ID returns zero')
        check(f.position==len(f.data) and 0x467670 not in p.visits,'no resolver invocation for null/failed ID')
        check(bool(f.errors)==(mode=='failed-id'),'only failed ID emits Read diagnostic')
    f.cleanup()


def cache_hit():
    f=ReadReferenceFixture(struct.pack('<II',7,4)+b'abcd');p=f.p
    # Change explicit, already loaded entry to the independently established
    # MeshData RTTI chain. This is a directed cache fixture, not a shipped SMO.
    rtti=p.uint(0x755378);node=p.uint(p.uint(rtti+0x18)+4)
    p.put_uint(node+12,0x33c34cf0);p.put_uint(node+16,0x75d428)
    for record,identity,parent in ((0x75d428,0x33c34cf0,0x75e090),
            (0x75e090,0x3f077b6c,0x7603a0),(0x7603a0,0x46f043fe,0x7555f8),
            (0x7555f8,0x44de07fd,0x755310)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    p.put_uint(f.entry+0x10,0x33c34cf0)
    mesh=f.call(0x41a270);f.objects.append(mesh)
    # Constructor leaves both owning pointers indeterminate (already proved
    # in probe_pc_model_runtime). Explicit empty-resource input, NOT native
    # initialization evidence, keeps cache-only teardown out of poisoned data.
    p.put_uint(mesh+0x50,0);p.put_uint(mesh+0x54,0)
    text=p.allocate(6);p.mu.mem_write(text,b'cache\0')
    f.call(0x4130f0,this=mesh,args=(text,)) # existing explicit name-storage seam
    entry_name=p.allocate(6);p.mu.mem_write(entry_name,b'cache\0')
    f.allocations[entry_name]=6;p.put_uint(f.entry+0xc,entry_name)
    resource_manager=f.call(0x458d00)
    result=f.call(0x458cb0,this=resource_manager,args=(mesh,))
    print('REGISTER',hex(result),'manager words',f.words(resource_manager,0x20,0x30),'instructions',sum(p.visits.values()),flush=True)
    check(p.uint(resource_manager+0x28)-p.uint(resource_manager+0x24)==8,'actual cache registration adds one8-byte entry')
    check(f.read_reference()==mesh,'actual nonempty cache reused MeshData')
    check(0x4586b0 in p.visits and 0x467550 not in p.visits,'cache-hit bypasses header/factory even with missing serializer')
    check(p.uint(f.entry+0x20)==mesh and f.position==len(f.data),'cache result published and inline bytes skipped')
    check(not f.errors,'cache-hit no diagnostic');f.cleanup()


def payload_failure():
    f=ReadReferenceFixture(struct.pack('<II',7,9)+struct.pack('<II',CLASS,0x4f4f4253)+b'\x60');p=f.p
    p.run(0x4678b0,this=f.serializer,args=(CLASS,f.stream,f.stream),stop_at=0x4677da)
    print('PAYLOAD_FAILURE_STOP',p.reason,'EIP',hex(p.reg('EIP')),'entry',hex(p.uint(f.entry+0x20)),
          'position',f.position,'errors',f.errors,flush=True)
    check(p.reg('EIP')==0x4677da,'actual secondary reader fails; stop before external diagnostic formatting')
    obj=p.uint(f.entry+0x20);f.objects.append(obj)
    check(obj in f.allocations and f.publications==[obj],'failed payload retains already published live object')
    check(0x43ecc0 in p.visits and f.position==len(f.data),'partial reader state/position retained')
    f.cleanup()


def asset_reference():
    global checks
    import probe_pc_san_loader as loader
    from inspect_pc_san_keys import DEFAULT,inspect,u32
    from pc_stl_fixtures import install_char_traits
    from probe_pc_animation_manager import registry_entries
    raw=(DEFAULT/'bbush.san').read_bytes();summary,tracks=inspect(DEFAULT/'bbush.san')
    obj=raw[u32(raw,20):]
    f=ReadReferenceFixture(struct.pack('<II',7,len(obj))+obj);p=f.p
    install_char_traits(p);del p.seams[0x454370];del p.seams[0x453b10]
    names=f.call(0x454640)
    f.native_names=names
    animation=f.read_reference();instructions=sum(p.visits.values());f.objects.append(animation)
    check(animation in f.allocations and p.uint(f.entry+0x20)==animation,'bbush reference creates/publishes actual Animation')
    check(f.publications==[animation] and f.position==len(f.data),'bbush publication precedes complete real payload read')
    check(bool(registry_entries(p,names)) and not f.errors,'bbush uses actual name registry, not binding seam')
    before=loader.checks;captured=loader.capture(f,animation,summary,tracks,raw);checks+=loader.checks-before
    f.cleanup()
    print('ASSET_REFERENCE bbush.san instructions',instructions,flush=True)
    return captured


def main(mode):
    if mode in ('success','split','declared-short','declared-long','unresolved-file'):success(mode)
    elif mode in ('materialized','failed-skip'):materialized(mode)
    elif mode in ('null','failed-id','missing-id','failed-size'):early(mode)
    elif mode=='cache':cache_hit()
    elif mode=='failed-payload':payload_failure()
    elif mode=='bbush':asset_reference()
    else:raise ValueError('explicit bounded mode required')
    print(f'PASS {checks}/{checks}: original PC read-reference {mode}')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
