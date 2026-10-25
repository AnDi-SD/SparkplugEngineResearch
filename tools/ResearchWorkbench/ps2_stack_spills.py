"""Opt-in research CPU support for aligned, non-delay EE stack SQ/LQ.

The upper 64 bits of each GPR are explicit caller state. Scalar instructions
preserve them; unsupported multimedia/quadword users are rejected. This is
not recovered game logic, a general EE CPU, or an implementation of unaligned
quadword access. Original instruction bytes are never replaced.

ISA: Sony EE Core User's Manual, sections 3.1.1 and LQ/SQ descriptions,
https://docs.alexrp.com/mips/ee.pdf ; independent implementation reference:
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/R5900OpcodeImpl.cpp
"""
from ps2_integer_square_accumulator import transfer

MASK64=(1<<64)-1


class StackSpills:
    def __init__(self,window,upper64,*,store_delay=False):
        if type(store_delay) is not bool:raise ValueError('Explicit Boolean stack store-delay option required')
        self.store_delay=store_delay
        if len(window)!=2:raise ValueError('Explicit stack address and size required')
        address,size=window
        if not (0<=address<0x100000000 and 0<size<=0x10000 and address+size<=0x100000000 and address%16==size%16==0):
            raise ValueError('Aligned bounded 32-bit stack window required')
        if len(upper64)!=32 or any(type(v) is not int or not 0<=v<=MASK64 for v in upper64) or upper64[0]!=0:
            raise ValueError('Explicit 32 upper64 GPR values, including hardwired zero, required')
        self.window=(address,size);self.initial_upper64=tuple(upper64);self.upper64=list(upper64);self.events=[]

    @staticmethod
    def is_quadword(word):return word>>26 in (0x1e,0x1f)

    def plan(self,p,address,word,stops):
        # Refuse a quadword delay slot before executing the branch or link.
        if transfer(word) and any(a<=address+4 and address+8<=a+n for a,n in p.ranges):
            if self.is_quadword(p.uint(address+4)):
                if self.store_delay and word>>26 in (3,4) and p.uint(address+4)>>26==0x1f:
                    return self.plan_store_delay(p,address,word,stops)
                raise ValueError('Stack SQ/LQ delay slot not qualified;rejected before branch')
        if not self.is_quadword(word):return None
        if p.trace and p.trace[-1]+4==address and transfer(p.uint(p.trace[-1])):
            raise ValueError('Stack SQ/LQ reached in live branch delay slot')
        base=(word>>21)&31;rt=(word>>16)&31
        if base!=29:raise ValueError('Only explicit SP-based SQ/LQ stack access qualified')
        offset=(word&0xffff)-(0x10000 if word&0x8000 else 0)
        effective=((p.reg('SP')&0xffffffff)+offset)&0xffffffff
        start,size=self.window
        if effective%16:raise ValueError('Unaligned SQ/LQ outside qualified domain;not a hardware alignment-fault claim')
        if not start<=effective or effective+16>start+size:raise ValueError('SQ/LQ outside declared stack window')
        if any(page not in p.pages for page in range(effective&~4095,(effective+16+4095)&~4095,4096)):
            raise ValueError('SQ/LQ stack bytes must already be mapped')
        before=p.read(effective,16);store=word>>26==0x1f
        low=0 if rt==0 else p.reg(str(rt))&MASK64
        high=self.upper64[rt]
        data=low.to_bytes(8,'little')+high.to_bytes(8,'little') if store else before
        return dict(addresses=[address],operationAddress=f'{address:08X}',word=f'{word:08X}',
            operation='SQ' if store else 'LQ',register=rt,baseRegister=base,offset=offset,
            effectiveAddress=f'{effective:08X}',memoryBefore=before.hex(),memoryAfter=(data if store else before).hex(),
            registerBefore=f'{high:016X}{low:016X}',registerAfter=(f'{high:016X}{low:016X}' if store or rt==0 else f'{int.from_bytes(data,"little"):032X}'),
            transferBytes=data.hex(),nextPc=address+4)

    def plan_store_delay(self,p,address,word,stops):
        if address+4 in stops:raise ValueError('Stop inside interpreted BEQ-or-JAL/SQ slot')
        if p.trace and p.trace[-1]+4==address and transfer(p.uint(p.trace[-1])):
            raise ValueError('Stack store-delay pair reached in live branch delay slot')
        op=word>>26
        if op==4:
            rs,rt=(word>>21)&31,(word>>16)&31
            taken=p.reg(str(rs))==p.reg(str(rt))
            displacement=(word&0xffff)-(0x10000 if word&0x8000 else 0)
            next_pc=address+4+displacement*4 if taken else address+8
            branch=dict(operation='BEQ/SQ',rs=rs,rt=rt,left=p.reg(str(rs)),right=p.reg(str(rt)),taken=taken)
        elif op==3:
            next_pc=((address+4)&0xf0000000)|((word&0x03ffffff)<<2)
            if address+8>=0x80000000:raise ValueError('Stack JAL/SQ link outside reviewed positive32 code domain')
            branch=dict(operation='JAL/SQ',linkRegister=31,linkBefore=p.reg('31'),linkAfter=address+8)
        else:raise ValueError('Only BEQ-or-JAL/SQ stack pairs qualified')
        if next_pc in (address,address+4):raise ValueError('Self/slot stack pair target rejected')
        if next_pc not in stops and not any(a<=next_pc and next_pc+4<=a+n for a,n in p.ranges):
            raise ValueError('Stack pair target outside declared original ranges')
        slot=self.plan(p,address+4,p.uint(address+4),stops)
        if op==3 and slot['register']==31:
            low=address+8;high=self.upper64[31]
            slot.update(registerBefore=f'{high:016X}{low:016X}',registerAfter=f'{high:016X}{low:016X}',
                transferBytes=(low.to_bytes(8,'little')+high.to_bytes(8,'little')).hex(),linkVisibleAtSlot=True)
            slot['memoryAfter']=slot['transferBytes']
        return dict(addresses=[address,address+4],operationAddress=f'{address:08X}',word=f'{word:08X}',
            slot=slot,nextPc=next_pc,**branch)

    def commit(self,p,plan):
        if plan['operation'] in ('BEQ/SQ','JAL/SQ'):
            if plan['operation']=='JAL/SQ':p.reg('31',plan['linkAfter'])
            slot=plan['slot'];p.write(int(slot['effectiveAddress'],16),bytes.fromhex(slot['transferBytes']))
            p.reg('PC',plan['nextPc']);self.events.append(plan);return
        data=bytes.fromhex(plan['transferBytes']);rt=plan['register']
        if plan['operation']=='SQ':p.write(int(plan['effectiveAddress'],16),data)
        elif rt!=0:
            p.reg(str(rt),int.from_bytes(data[:8],'little'));self.upper64[rt]=int.from_bytes(data[8:],'little')
        p.reg('PC',plan['nextPc']);self.events.append(plan)

    @staticmethod
    def guard_scalar(word,address):
        # Deliberately small allowlist: only operations that leave upper GPR
        # halves unchanged. Existing profile guards still apply afterwards.
        op=word>>26;fun=word&63
        if op==0:
            allowed=fun in (0,2,3,4,6,7,8,9,10,0x21,0x23,0x24,0x25,0x26,0x27,0x2a,0x2b,0x2d,0x2f,0x38,0x3a,0x3b,0x3c,0x3e,0x3f)
        elif op==1:allowed=(word>>16)&31 in (0,1,2,3,0x10,0x11,0x12,0x13)
        elif op==0x11:
            rs=(word>>21)&31
            allowed=rs in (0,4,8) or rs==16 and fun in (0,1,2,3,5,6,7,0x24,0x30,0x32,0x34,0x36) or rs==20 and fun==0x20
        else:allowed=op in (2,3,4,5,6,7,9,0xa,0xb,0xc,0xd,0xe,0xf,0x14,0x15,0x16,0x17,0x19,0x20,0x21,0x23,0x24,0x25,0x27,0x28,0x29,0x2b,0x31,0x37,0x39,0x3f)
        if not allowed:raise ValueError(f'Outside reviewed stack-spill scalar ISA {address:08X}: {word:08X}')
