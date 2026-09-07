#!/usr/bin/env python3
"""PC PRS sampling fixtures with original guest instructions and CRT acos seam.

Runtime descriptors are synthetic, NOT a claim that SAN wire stores computed
cubic coefficients. Loader construction and wire invariants are separate work.
"""
from __future__ import annotations
import math
from pathlib import Path
import struct
import sys
from pc_instruction_emulator import PcInstructions, run_bounded

checks=0
def check(value,description):
    global checks
    checks+=1
    if not value: raise AssertionError(description)

def bits(value): return struct.unpack('<I',struct.pack('<f',value))[0]
def near(a,b): return len(a)==len(b) and all(math.isclose(x,y,rel_tol=3e-5,abs_tol=3e-5) for x,y in zip(a,b))
def lerp(a,b,t): return tuple(x*(1-t)+y*t for x,y in zip(a,b))
def slerp(a,b,t):
    dot=sum(x*y for x,y in zip(a,b))
    if dot<0: b=tuple(-v for v in b);dot=-dot
    theta=math.acos(min(1,dot));sine=math.sin(theta)
    if sine<.001:return a
    return tuple(x*math.sin((1-t)*theta)/sine+y*math.sin(t*theta)/sine for x,y in zip(a,b))
def qmul(a,b):
    x,y,z,w=a;u,v,s,t=b
    return (w*u+x*t+y*s-z*v,w*v-x*s+y*t+z*u,w*s+x*v-y*u+z*t,w*t-x*u-y*v-z*s)
def euler(values):
    result=(0,0,0,1)
    for axis,angle in enumerate(values):
        q=[0,0,0,math.cos(angle*.5)];q[axis]=math.sin(angle*.5)
        result=qmul(q,result)
    return result
def interval(time,count=3):
    if time<=0:return 0,0
    if time>=10*(count-1):return (count-2,1) if count>2 else (0,0)
    index=int(time//10)
    return index,(time-10*index)/10


def descriptor(p,rep,times,values):
    d=p.allocate(16);t=p.allocate(len(times)*4);v=p.allocate(len(values)*4)
    p.put_uint(d,len(times));p.put_uint(d+4,rep);p.put_uint(d+8,t);p.put_uint(d+12,v)
    p.put_floats(t,times);p.put_floats(v,values)
    return d

def install_acos_seams(p):
    # Only the CRT acos import is substituted, with both observed ABI entries.
    # The engine's Slerp/log/exp/control-point bodies remain original code.
    def stack_acos(machine):
        argument=machine.floats(machine.reg('ESP')+4,1)[0]
        machine.fixture_push_x87(math.acos(argument))
        machine.fixture_return()
    def register_acos(machine):
        top=(machine.reg('FPSW')>>11)&7
        register=getattr(machine.xr,'UC_X86_REG_FP'+str(top))
        mantissa,exponent=machine.mu.reg_read(register)
        argument=math.ldexp(mantissa/(1<<63),(exponent&0x7fff)-16383)*(-1 if exponent&0x8000 else 1)
        result=math.acos(argument)
        fraction,power=math.frexp(result)
        machine.mu.reg_write(register,(int(math.ldexp(fraction,64)),power-1+16383) if result else (0,0))
        machine.fixture_return()
    p.seams[0x467b50]=stack_acos
    p.seams[0x60dd50]=register_acos


def cubic_coefficients(values,dimensions):
    result=list(values);stride=dimensions*5;count=len(values)//stride
    for key in range(count-1):
        for axis in range(dimensions):
            a=key*stride+axis;b=(key+1)*stride+axis
            delta=values[b]-values[a]
            outgoing=values[a+2*dimensions];incoming=values[b+dimensions]
            result[a+3*dimensions]=3*delta-(2*outgoing+incoming)
            result[a+4*dimensions]=outgoing+incoming-2*delta
    return result

def setup_track(p,rep):
    track=p.allocate(0x44)
    times=(0,10,20)
    positions=[(1,2,3),(11,22,33),(21,42,63)]
    scales=[(2,3,4),(4,6,8),(6,9,12)]
    angles=[(.1,.2,.3),(.4,.5,.6),(.7,.8,.9)]
    rotations=[euler(a) for a in angles]
    controls=[euler((a[0]*.5,a[1]*1.5,a[2]*.75)) for a in angles]
    # Deliberately distinctive unused/tangent slot in cubic runtime records.
    coefficients=[(2,3,4),(3,5,7),(5,7,9)]
    for role,rows in ((0x18,positions),(0x24,rotations),(0x30,scales)):
        if rep>=3:
            rows=angles if role==0x24 else rows
            for axis in range(3):
                values=[]
                for key in range(3):
                    if rep==3: values.append(rows[key][axis])
                    else: values.extend((rows[key][axis],99+axis,*coefficients[axis]))
                p.put_uint(track+role+axis*4,descriptor(p,rep,times,values))
        else:
            values=[]
            for key in range(3):
                values.extend(rows[key])
                if rep==2:
                    if role==0x24: values.extend(controls[key])
                    else:
                        values.extend((97,98,99))
                        for component in range(3):
                            values.extend(coefficients[axis][component] for axis in range(3))
            p.put_uint(track+role,descriptor(p,rep,times,values))
    cache=p.allocate(36);outputs=p.allocate(52);flags=p.allocate(3)
    def sample(time):
        p.put_floats(outputs,(111,)*13)
        p.run(0x479290,track,[bits(time),cache,cache+12,cache+24,outputs,outputs+12,outputs+28,flags,flags+1,flags+2])
        return p.floats(outputs,3),p.floats(outputs+12,4),p.floats(outputs+28,3),bytes(p.mu.mem_read(flags,3))
    def expected(time):
        index,t=interval(time)
        def polynomial(row):
            return tuple(row[axis]+t*(coefficients[axis][0]+t*(coefficients[axis][1]+t*coefficients[axis][2])) for axis in range(3))
        position=polynomial(positions[index]) if rep in (2,4) else lerp(positions[index],positions[index+1],t)
        scale=polynomial(scales[index]) if rep in (2,4) else lerp(scales[index],scales[index+1],t)
        if rep==1:rotation=slerp(rotations[index],rotations[index+1],t)
        elif rep==2:
            rotation=slerp(slerp(rotations[index],rotations[index+1],t),slerp(controls[index],controls[index+1],t),2*t*(1-t))
        else:
            rotation=euler(polynomial(angles[index]) if rep==4 else lerp(angles[index],angles[index+1],t))
        return position,rotation,scale
    return track,cache,sample,expected


def guest():
    p=PcInstructions()
    # The import thunk 0x60DD50 has no OS loader here. Only CRT acos is a seam;
    # quaternion interpolation, normalization choices and all engine calls run.
    install_acos_seams(p)
    for rep in (1,2,3,4):
        p.reset_arena()
        track,cache,sample,expected=setup_track(p,rep)
        for time in (-5,0,2.5,5,10,17.5,20,25,12.5,2.5):
            actual=sample(time)
            reference=expected(time)
            for role,got,want in zip(('position','rotation','scale'),actual[:3],reference):
                check(near(got,want),f'rep {rep} {role} t={time}: {got} != {want}')
            check(actual[3]==b'\x01\x01\x01',f'rep {rep} valid channels')
        check(tuple(p.uint(cache+i*4) for i in range(9))==(0,)*9,'backward cache rewind')
    check(p.uint(0x13b1ad8)==0x425cb0,'protected quaternion-key interpolation bridge')

    for dimensions,entry in ((1,0x493160),(3,0x493290)):
        for count in (1,2,3,5,6,9):
            p.reset_arena()
            values=[(key+1)*(component+1)*.125 for key in range(count) for component in range(dimensions*5)]
            buffer=p.allocate(len(values)*4);p.put_floats(buffer,values)
            p.run(entry,0,[buffer,count],callee_pop=False)
            check(near(p.floats(buffer,len(values)),cubic_coefficients(values,dimensions)),
                  f'original cubic coefficient setup dimensions={dimensions}, count={count}')

    # Original payload reader -> pool-backed descriptor -> preparation -> sampler.
    # Explicit seams: byte source and 16-byte descriptor pool allocation only.
    for rep in (1,2,3,4):
        p.reset_arena()
        raw_track,_,_,_=setup_track(p,rep)
        wire=[]
        source=[]
        for role in range(3):
            channel=[];original=[]
            for axis in range(3 if rep>=3 else 1):
                d=p.uint(raw_track+0x18+role*12+axis*4)
                count=p.uint(d)
                stride=5 if rep==4 else 1 if rep==3 else (8 if role==1 else 15) if rep==2 else (4 if role==1 else 3)
                times=bytes(p.mu.mem_read(p.uint(d+8),count*4))
                values=bytes(p.mu.mem_read(p.uint(d+12),count*stride*4))
                channel.append(struct.pack('<II',rep,count)+times+values)
                original.append(struct.unpack('<'+'f'*(count*stride),values))
            wire.append(b''.join(channel));source.append(original)
        serializer=p.allocate(0x4c);animation=p.allocate(0x6c);track=p.allocate(0x44)
        p.put_uint(track+0x40,animation)
        for index in range(7): p.put_uint(animation+0x38+index*4,p.allocate(1024))
        stream=p.allocate(4);vtable=p.allocate(0x3c);p.put_uint(stream,vtable)
        p.put_uint(vtable+0x30,0x401000)
        payload=[b''];cursor=[0];allocations=[]
        def read_seam(machine):
            sp=machine.reg('ESP');out=machine.uint(sp+4);amount=machine.uint(sp+8)
            check(machine.reg('ECX')==stream,'payload read uses supplied stream')
            if cursor[0]+amount>len(payload[0]):
                machine.fixture_return(8,0);return
            machine.mu.mem_write(out,payload[0][cursor[0]:cursor[0]+amount])
            cursor[0]+=amount
            machine.fixture_return(8,1)
        def pool_seam(machine):
            check(machine.reg('ECX')==animation+0x58,'descriptors owned by animation pool')
            address=machine.allocate(16);allocations.append(address)
            machine.fixture_return(0,address)
        p.seams[0x401000]=read_seam;p.seams[0x417510]=pool_seam
        for role in range(3):
            payload[0]=wire[role];cursor[0]=0
            p.run(0x43db90,serializer,[stream,track,role,animation])
            check(p.reg('EAX')&0xff==1 and cursor[0]==len(payload[0]),f'complete payload reader rep={rep},role={role}')
            for axis in range(3 if rep>=3 else 1):
                d=p.uint(track+0x18+role*12+axis*4)
                check(p.uint(d)==3 and p.uint(d+4)==rep,'reader does not change key count/representation')
                check(p.floats(p.uint(d+8),3)==(0,10,20),'pool time slice retains exact keys')
                if rep==4 or rep==2 and role!=1:
                    expected=cubic_coefficients(source[role][axis],1 if rep==4 else 3)
                    check(near(p.floats(p.uint(d+12),len(expected)),expected),'reader prepares cubic coefficients in-place')
                elif rep!=2:
                    expected=source[role][axis]
                    check(near(p.floats(p.uint(d+12),len(expected)),expected),'linear values are copied unchanged')
        check(len(allocations)==(9 if rep>=3 else 3),'one/three descriptors per PRS role')
        check(p.uint(serializer+0x44)==(27 if rep>=3 else 9),'shared time cursor advances by actual count')
        check(bytes(p.mu.mem_read(track+0x3c,1))==b'\0','reader supplies borrowed-pool ownership flag')
        cache=p.allocate(36);outputs=p.allocate(40);flags=p.allocate(3)
        p.run(0x479290,track,[bits(5),cache,cache+12,cache+24,outputs,outputs+12,outputs+28,flags,flags+1,flags+2])
        check(bytes(p.mu.mem_read(flags,3))==b'\x01\x01\x01' and
              all(math.isfinite(v) for v in p.floats(outputs,10)),
              'reader output consumed by original full PRS sampler')
        p.seams.pop(0x401000);p.seams.pop(0x417510)
    print(f'PASS {checks}/{checks}: PRS representations 1/2/3/4, caches/endpoints, CRT acos seam')
    return 0

if __name__=='__main__':
    raise SystemExit(guest() if sys.argv[1:]==['--guest'] else run_bounded(Path(__file__)))
