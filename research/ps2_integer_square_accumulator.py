"""Opt-in host CPU interpretation of three R5900 integer-square operations.

This is research infrastructure, not reconstructed game logic or a general
EE floating-point implementation. Normal/positive-zero FPRs must hold small
integers, FS=FT, and every nonnegative sum must fit exactly in24 binary bits.
No rounding/underflow/overflow or FCR-status semantics are claimed. FCR reads
and writes are rejected by this profile. Original guest code is not patched.

ISA references: Sony EE Core User's Manual,2002,sections8.4/8.7/8.8;
https://docs.alexrp.com/mips/ee.pdf
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/FPU.cpp
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/DebugTools/MipsAssemblerTables.cpp
"""
import struct


def transfer(word):
    op=word>>26
    return (op in (2,3,4,5,6,7,0x14,0x15,0x16,0x17)
        or op==1 and (word>>16)&31 in (0,1,2,3,0x10,0x11,0x12,0x13)
        or op==0 and word&63 in (8,9)
        or op in (0x10,0x11,0x12) and (word>>21)&31==8)


def accumulator_word(word):
    return word>>26==0x11 and (word>>21)&31==16 and 0x18<=word&63<=0x1f


def small_integer(word):
    exponent=(word>>23)&255
    if exponent==255 or exponent==0 and word!=0:
        raise ValueError('Integer-square profile rejects negative zero,denormal and nonfinite FPR inputs')
    value=struct.unpack('<f',struct.pack('<I',word))[0]
    if not value.is_integer() or abs(value)>2048:
        raise ValueError('Integer-square profile requires integer operands in[-2048,2048]')
    return int(value)


class IntegerSquareAccumulator:
    def __init__(self):
        self.accumulator=None
        self.events=[]

    def plan(self,p,address,word,stops):
        # Planning is read-only. All range,encoding,input,delay and count
        # checks finish before commit changes an FPR,the accumulator or PC.
        if word>>26==0x11 and (word>>21)&31 in (2,6):
            raise ValueError('Integer-square profile does not model FCR status;CFC1/CTC1 rejected')
        branch=None;native_address=address;addresses=[address];next_pc=address+4
        if transfer(word) and any(a<=address+4 and address+8<=a+n for a,n in p.ranges):
            delayed=p.uint(address+4)
            if accumulator_word(delayed):
                if word>>16!=0x1000:
                    raise ValueError('Accumulator in delay slot requires reviewed unconditional B;rejected before branch')
                if address+4 in stops:
                    raise ValueError('Stop inside interpreted B delay slot;stop before the branch')
                branch=dict(address=f'{address:08X}',word=f'{word:08X}')
                displacement=struct.unpack('<h',struct.pack('<H',word&65535))[0]*4
                next_pc=address+4+displacement
                if next_pc in (address,address+4):raise ValueError('Self/slot-target B outside reviewed profile')
                if next_pc not in stops and not any(a<=next_pc and next_pc+4<=a+n for a,n in p.ranges):
                    raise ValueError('Interpreted B target outside declared original ranges')
                word=delayed;native_address+=4;addresses.append(native_address)
        if not accumulator_word(word):return None
        if branch is None and p.trace and p.trace[-1]+4==address and transfer(p.uint(p.trace[-1])):
            raise ValueError('Accumulator reached in live branch delay slot;no PC-write interception there')
        fun=word&63;fs=(word>>11)&31;ft=(word>>16)&31;fd=(word>>6)&31
        if fun not in (0x1a,0x1c,0x1e) or fs!=ft or fun!=0x1c and fd!=0:
            raise ValueError('Integer-square profile accepts only legal MULA.S/MADDA.S/MADD.S with FS=FT')
        value=small_integer(p.reg('F'+str(fs))&0xffffffff);product=value*value
        if fun==0x1a:result=product
        else:
            if self.accumulator is None:raise ValueError('Accumulator input requires an earlier observed MULA.S')
            result=self.accumulator+product
        if result>0xffffff:raise ValueError('Integer-square sum exceeds exact24-bit bound')
        result_bits=struct.unpack('<I',struct.pack('<f',result))[0]
        return dict(addresses=addresses,operationAddress=f'{native_address:08X}',word=f'{word:08X}',
            operation={0x1a:'MULA.S',0x1c:'MADD.S',0x1e:'MADDA.S'}[fun],fs=fs,ft=ft,fd=fd,
            operand=value,product=product,accumulatorBefore=self.accumulator,result=result,resultBits=f'{result_bits:08X}',
            writesAccumulator=fun!=0x1c,branch=branch,nextPc=next_pc)

    def commit(self,p,plan):
        if plan['writesAccumulator']:self.accumulator=plan['result']
        else:p.reg('F'+str(plan['fd']),int(plan['resultBits'],16))
        p.reg('PC',plan['nextPc'])
        self.events.append(dict(**plan,accumulatorAfter=self.accumulator))
