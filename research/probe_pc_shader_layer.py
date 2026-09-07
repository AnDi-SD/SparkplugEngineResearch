#!/usr/bin/env python3
"""Bounded original PC DXShaderLayer scout; no GPU or protected-core startup."""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_function_eval import cleanup

def main(mode,return_capture=False):
    if mode not in ('factory','reject','clone-empty','clone-one','clone-multi','clone-texture','clear','stubs'):raise ValueError('bounded shader layer contract')
    f=PCWriteBytesFixture();p=f.p;checks=0;maximum=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    def call(entry,this=0,args=()):
        nonlocal maximum
        result=f.call(entry,this=this,args=args);maximum=max(maximum,sum(p.visits.values()));return result
    obj=f.call(0x4abdc0)
    size=f.allocations[obj];table=p.uint(obj)
    maximum=sum(p.visits.values())
    check(size==0x28 and table==0x6f0b04,'actual factory28/vtable')
    check([p.uint(obj+i) for i in (0x10,0x14,0x18,0x1c,0x20,0x24)]==[0,0xcccccccc,0xcccccccc,0,0,0],'NULL nested texture; untouched shader14/allocator18; empty parameter vector')
    for record,identity,parent in ((0x763270,0x71643e66,0x75df70),(0x75df70,0x7f577c6d,0x755310),(0x755310,0x415352a1,0)):
        p.put_uint(record,identity);p.put_uint(record+0x48,parent)
    check(call(0x408370,this=obj,args=(0x7f577c6d,))&255==1,'actual RTTI direct inheritance')
    check(call(0x408370,this=obj,args=(0x234c576b,))&255==0,'ShaderLayer is NOT StdLayer')
    states=[];owned=[obj];clone=0
    def snapshot(target):
        begin=p.uint(target+0x1c);end=p.uint(target+0x20);values=[]
        count=(end-begin)//16 if begin else 0
        if not 0<=count<=3:raise AssertionError('bounded parameter pair count')
        for i in range(count):
            values.append([])
            for offset in (0,8):
                n=p.uint(begin+16*i+offset);data=p.uint(begin+16*i+offset+4)
                if not 0<=n<=3:raise AssertionError('bounded float4 count')
                values[-1].append([p.uint(data+j*4) for j in range(n*4)])
        return [p.uint(target+0x14),bool(p.uint(target+0x10)),values]
    if mode=='reject':
        serializer=call(0x4b0dd0);owned.insert(0,serializer)
        for entry,args in ((0x4b12a0,(f.stream,obj)),(0x4b0f10,(f.stream,obj)),(0x4b0eb0,(obj,))):
            before=f.position;result=call(entry,this=serializer+0x10,args=args)&255
            check(result==0 and f.position==before and not f.errors,'original DX layer helper rejects exact shader type before payload/index work')
            states.append(result)
    else:
        p.put_uint(obj+0x14,0x12345678) # declared borrowed shader token, never dereferenced
        if mode=='clone-texture':p.put_uint(obj+0x10,call(0x467f30))
        count=3 if mode=='clone-multi' else 1 if mode in ('clone-one','clear') else 0
        for i in range(count):
            descriptor=p.allocate(16)
            for offset,n in ((0,i+1),(8,3-i)):
                p.run(0x412400,args=(16*n,),callee_pop=False);data=p.reg('EAX')
                p.put_uint(descriptor+offset,n);p.put_uint(descriptor+offset+4,data)
                for j in range(n*4):p.put_uint(data+4*j,0x3f000000+i*64+offset+j)
            call(0x4b1230,this=obj+0x18,args=(descriptor,))
        states.append(snapshot(obj))
        if mode.startswith('clone-'):
            call(0x52fd90,this=0x755588);clone=call(0x4ac060,this=obj);owned.insert(0,clone)
            check(snapshot(clone)==snapshot(obj),'actual virtual clone preserves shader token and deeply copied parameter values')
            if count:
                a=p.uint(obj+0x1c);b=p.uint(clone+0x1c)
                check(a!=b and all(p.uint(a+i*16+off)!=p.uint(b+i*16+off) for i in range(count) for off in (4,12)),'clone arrays and vector own distinct allocations')
                p.put_uint(p.uint(a+4),0x40400000);check(snapshot(clone)!=snapshot(obj),'source parameter mutation does not affect clone')
            if mode=='clone-texture':check(p.uint(obj+0x10)!=p.uint(clone+0x10),'actual inherited nested texture clone')
            states.append(snapshot(clone));call(0x6d7db0)
        if mode=='clear':
            before=p.uint(obj+0x1c);call(0x4b2f60,this=obj)
            check([p.uint(obj+i) for i in (0x1c,0x20,0x24)]==[0,0,0] and before in f.freed,'clear releases array payloads AND vector storage, resets three pointers')
            states.append(snapshot(obj))
        if mode=='stubs':
            check(call(0x4d74a0,this=obj,args=(0x123,0x456))&255==1 and call(0x4f3df0,this=obj)&255==1,'actual no-op virtual success slots8/9')
            check(call(0x5a7db0,this=obj,args=(0xdeadbeef,))&255==1,'reader scalar setter is a no-op success stub')
    cleanup(f,owned);check(set(f.allocations)==set(f.freed),'all native shader/vector/array/nested allocations freed')
    capture=[mode,states]
    print('SHADER_LAYER_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original shader-layer {mode}; maxInstructions={maximum}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
