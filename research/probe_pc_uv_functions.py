#!/usr/bin/env python3
"""Original bounded PC TransFunctionEval/UV; no transform-algorithm seams."""
from pathlib import Path
import json,struct,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_function_eval import configure_floor,bits,cleanup
from probe_pc_node_serializer import field

OFFSETS=(0x10,0x48,0x80,0xb8,0xf0,0x128,0x178)

def main(mode,return_capture=False):
    kind,case=mode.split('-',1)
    if kind not in ('prs','matrix','uv','codec'):raise ValueError('explicit bounded dependency')
    if case not in ('identity','translate','scale','rotation','pivot','compound','nonunit','zeroaxis','animated','random','default'):raise ValueError('explicit small transform case')
    f=PCWriteBytesFixture();p=f.p;floor=configure_floor(f);manager=f.call(0x454640);uv=f.call(0x41a210);trans=uv+0x4c;holder=f.call(0x467f30);checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    check(p.uint(trans)==0x6eaaec and all(p.uint(trans+offset)==0x6ea9ac for offset in OFFSETS),'actual TransFunctionEval and seven embedded FunctionEval vtables')
    check(p.floats(trans+0x160,6)==(0.,0.,0.,0.,0.,1.),'pivot and axis defaults')
    values=[0.,0.,0.,1.,1.,1.,0.];pivot=[0.,0.,0.];axis=[0.,0.,1.]
    if case=='translate':values[:3]=[2.,3.,4.]
    if case=='scale':values[3:6]=[2.,3.,4.]
    if case in ('rotation','pivot','nonunit','zeroaxis'):values[6]=.25 # native turns, not degrees
    if case in ('pivot','compound'):pivot=[.5,1.,2.]
    if case=='compound':values=[2.,3.,4.,2.,3.,4.,.125]
    if case=='nonunit':axis=[1.,2.,3.]
    if case=='zeroaxis':axis=[0.,0.,0.]
    for offset,value in zip(OFFSETS,values):p.put_floats(trans+offset+0x24,(value,))
    if case in ('animated','random'):
        for i,offset in enumerate(OFFSETS):
            p.put_uint(trans+offset+0x34,6 if case=='random' else 8)
            p.put_floats(trans+offset+0x28,((i+1)*.5,))
    p.put_floats(trans+0x160,pivot);p.put_floats(trans+0x16c,axis)
    saved=(2.,.5,9.,.25,3.,8.,.5,.75,7.) if case=='compound' else (1.,0.,0.,0.,1.,0.,0.,0.,1.)
    matrix=p.allocate(64);p.put_floats(matrix,saved);f.call(0x467cb0,this=holder,args=(matrix,));f.call(0x467d90,this=holder,args=(uv,))
    states=[];payload=b'';written=b'';max_instructions=0
    if kind=='codec':
        # Seven scalar sections + two vectors; declared tiny valid wire input.
        functions=b''
        for i,value in enumerate(values):
            function=field(4,struct.pack('<f',value)) if value else b''
            if case in ('animated','random'):
                function=field(0,struct.pack('<I',6 if case=='random' else 8))+function+field(5,struct.pack('<f',(i+1)*.5))
            functions+=function+b'\0'
        nested=field(0,functions+struct.pack('<6f',*pivot,*axis))+b'\0'
        payload=field(0,nested)+b'\0';f.data=payload;f.position=0
        serializer=f.call(0x440b00)
        result=f.call(0x440be0,this=serializer+0x10,args=(f.stream,uv))&255
        print('UV_ORIGINAL_READ',result,'instructions',sum(p.visits.values()),'position',f.position,'errors',f.errors,flush=True)
        check(result==1 and f.position==len(payload) and not f.errors,'original nested UV/Trans/Function read')
        f.data=b'';f.position=0
        result=f.call(0x440d70,this=serializer+0x10,args=(f.stream,uv))&255
        print('UV_ORIGINAL_WRITE',result,'instructions',sum(p.visits.values()),'errors',f.errors,flush=True)
        check(result==1 and not f.errors,'original nested UV/Trans/Function write');written=f.data
        f.call(p.uint(p.uint(serializer)),this=serializer,args=(1,))
    else:
        output=p.allocate(64);flags=p.allocate(16)
        for delta in (0.,.25,.75,-.5):
            if kind=='prs':
                f.call(0x47cd10,this=trans,args=(bits(delta),output,output+12,output+28,flags,flags+1,flags+2))
                state=list(p.floats(output,10))+list(p.mu.mem_read(flags,3));check(state[-3:]==[1,1,1],'all three PRS channels valid')
            elif kind=='matrix':
                f.call(0x47cee0,this=trans,args=(bits(delta),output));state=list(p.floats(output,16))
            else:
                f.call(0x423190,this=uv,args=(bits(delta),));f.call(0x434820,this=uv);state=list(p.floats(holder+0x3c,9))
                check(p.uint(holder+0x60)&255==1,'UV update marks static matrix present')
            max_instructions=max(max_instructions,sum(p.visits.values()));states.append([delta,state,[p.floats(trans+offset+0x10,1)[0] for offset in OFFSETS]])
    f.call(p.uint(p.uint(holder)),this=holder,args=(1,))
    check(uv in f.freed and p.uint(manager+0x24)==p.uint(manager+0x28)==0,'UV holder destruction unregisters controller')
    f.call(0x4545d0,this=manager,args=(1,));cleanup(f,())
    check(set(f.allocations)==set(f.freed),'all tracked allocations released')
    capture=[mode,payload.hex(),states,written.hex()]
    print('UV_FUNCTION_CAPTURE',json.dumps(capture,allow_nan=False),flush=True)
    print(f'PASS {checks}/{checks}: original {mode}; maxInstructions={max_instructions}; heap={p.allocated}; floorCalls={len(floor)}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
