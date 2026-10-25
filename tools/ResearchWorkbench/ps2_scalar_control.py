"""Explicit opt-in scalar transfer/slot model for bounded original EE probes.

All integer transfers in this profile are interpreted before the generic MIPS
backend executes them. Thus host-returned transfers never mix with hidden
pending backend branches. Unknown transfers/slots are refusals,not callbacks.
Original code bytes remain unchanged; no CPU internal flags are read or patched.

Primary ISA: https://docs.alexrp.com/mips/ee_insns.pdf (instruction manualv6,
word operand restrictions included). Independent facts:
PCSX2265d123dc33ee0ddf9c31e5bdf6c03f9d64e9d34,
pcsx2/Interpreter.cpp and R5900OpcodeImpl.cpp. Own implementation,not copied
upstream source; no EE exception,timing,MMI or privileged execution claim.
"""
from ps2_integer_square_accumulator import transfer

MASK64=(1<<64)-1
def signed(value,bits):
    value&=(1<<bits)-1
    return value-(1<<bits) if value>>(bits-1) else value
def sext32(value):return signed(value,32)&MASK64
def require_word(*values):
    # EE instruction manualv6: non-sign-extended word operands are undefined.
    # Do not promote the pinned interpreter's permissive implementation into
    # an EE guarantee for those inputs.
    if any(value!=sext32(value) for value in values):
        raise ValueError('Undefined non-sign-extended32 operand in control slot')

class ProjectedRegisters:
    def __init__(self,p,writes):self.p=p;self.writes=writes
    def __getattr__(self,name):return getattr(self.p,name)
    def reg(self,name,value=None):
        if value is not None:raise ValueError('Control planning may not mutate registers')
        # Delegated stack slots use SP while scalar slots use numeric names.
        # JALR may name either as its link destination, including RD==RS.
        aliases={'ZERO':0,'AT':1,'V0':2,'V1':3,'A0':4,'A1':5,'A2':6,'A3':7,
            'T0':8,'T1':9,'T2':10,'T3':11,'T4':12,'T5':13,'T6':14,'T7':15,
            'S0':16,'S1':17,'S2':18,'S3':19,'S4':20,'S5':21,'S6':22,'S7':23,
            'T8':24,'T9':25,'K0':26,'K1':27,'GP':28,'SP':29,'FP':30,'S8':30,'RA':31}
        index=int(name) if name.isdigit() else aliases.get(name)
        return self.writes[index] if index in self.writes else self.p.reg(name)

class ScalarControlPairs:
    def __init__(self,windows):
        if not isinstance(windows,(list,tuple)) or not 1<=len(windows)<=16:raise ValueError('Explicit control-slot memory windows required')
        self.windows=[];self.events=[]
        for a,n,mode in windows:
            if (type(a) is not int or type(n) is not int or a<0 or not 0<n<=65536 or a+n>0x100000000
                    or mode not in ('r','rw') or any(a<b+m and b<a+n for b,m,_ in self.windows)):
                raise ValueError('Disjoint bounded control-slot windows required')
            self.windows.append((a,n,mode))
        if sum(n for _,n,_ in self.windows)>0x20000:raise ValueError('Control-slot windows exceed128KiB')

    def memory(self,p,address,size,store):
        own=any(a<=address and address+size<=a+n and (not store or mode=='rw') for a,n,mode in self.windows)
        heap=p.call_extension is not None and p.call_extension.permits(address,size,store)
        if address%size or not(own or heap):raise ValueError('Control slot memory outside aligned declared permissions/liveheap')
        if any(page not in p.pages for page in range(address&~4095,(address+size+4095)&~4095,4096)):
            raise ValueError('Control slot memory must already be mapped')

    def scalar_slot(self,p,address,word):
        op,rs,rt,rd,sa,fun=word>>26,(word>>21)&31,(word>>16)&31,(word>>11)&31,(word>>6)&31,word&63
        a,b=p.reg(str(rs))&MASK64,p.reg(str(rt))&MASK64
        row=dict(operationAddress=f'{address:08X}',word=f'{word:08X}',operation='scalar',writes={})
        if op==0 and fun==0x3f:
            # EE instruction manualv6p58: signed low64 shift by32+sa.
            # Unlike word shifts, arbitrary low64 operands are defined here.
            if rs:raise ValueError('Reserved DSRA32 encoding in control slot')
            row['writes']={rd:(signed(b,64)>>(32+sa))&MASK64}
        elif op==0 and fun in (0,2,3):
            if rs:raise ValueError('Reserved scalar shift encoding in control slot')
            require_word(b)
            value=(b&0xffffffff)<<sa if fun==0 else (b&0xffffffff)>>sa if fun==2 else signed(b,32)>>sa
            row['writes']={rd:sext32(value)}
        elif op==0 and fun in (0x21,0x23,0x25,0x2b,0x2d):
            if sa:raise ValueError('Reserved scalar ALU encoding in control slot')
            if fun in (0x21,0x23):require_word(a,b)
            value={0x21:lambda:sext32(a+b),0x23:lambda:sext32(a-b),0x25:lambda:a|b,
                0x2b:lambda:int(a<b),0x2d:lambda:(a+b)&MASK64}[fun]()
            row['writes']={rd:value}
        elif op==0x0b:
            # EE instruction manualv6p108: unsigned low64 comparison against
            # the SIGN-extended16 immediate; upper64 stays in explicit shadow.
            row['writes']={rt:int(a<(signed(word,16)&MASK64))}
        elif op in (9,0x19,0xc,0xd):
            imm=signed(word,16)
            if op==9:require_word(a)
            value=sext32(a+imm) if op==9 else (a+imm)&MASK64 if op==0x19 else a&(word&65535) if op==0xc else a|(word&65535)
            row['writes']={rt:value}
        elif op in (0x23,0x24,0x25,0x37,0x28,0x29,0x2b,0x3f):
            size={0x23:4,0x24:1,0x25:2,0x37:8,0x28:1,0x29:2,0x2b:4,0x3f:8}[op]
            at=((a&0xffffffff)+signed(word,16))&0xffffffff;store=op in (0x28,0x29,0x2b,0x3f)
            self.memory(p,at,size,store);before=p.read(at,size)
            row.update(operation='store' if store else 'load',effectiveAddress=f'{at:08X}',size=size,memoryBefore=before.hex())
            if store:row['data']=(b&((1<<(size*8))-1)).to_bytes(size,'little').hex()
            else:
                value=int.from_bytes(before,'little');row['writes']={rt:sext32(value) if op==0x23 else value}
        else:raise ValueError(f'Unqualified scalar instruction in control slot {word:08X}')
        row['writes'].pop(0,None)
        return row

    def plan(self,p,address,word,stops):
        if not transfer(word):return None
        if not any(a<=address and address+8<=a+n for a,n in p.ranges):raise ValueError('Whole original transfer/slot words required')
        op,rs,rt,rd,sa,fun=word>>26,(word>>21)&31,(word>>16)&31,(word>>11)&31,(word>>6)&31,word&63
        a,b=p.reg(str(rs))&MASK64,p.reg(str(rt))&MASK64
        likely=op in (0x14,0x15);taken=True;writes={};name=''
        if op in (2,3):
            target=((address+4)&0xf0000000)|((word&0x03ffffff)<<2);name='JAL' if op==3 else 'J'
            if op==3:writes[31]=sext32(address+8)
        elif op==0 and fun in (8,9):
            if rt or sa or fun==8 and rd:raise ValueError('Reserved JR/JALR encoding')
            target=a&0xffffffff;name='JALR' if fun==9 else 'JR'
            if fun==9 and rd:writes[rd]=sext32(address+8)
        elif op in (4,5,0x14,0x15):
            taken=a==b if op in (4,0x14) else a!=b;target=address+4+signed(word,16)*4
            name={4:'BEQ',5:'BNE',0x14:'BEQL',0x15:'BNEL'}[op]
        elif op==1 and rt in (0,1):
            taken=signed(a,64)<0 if rt==0 else signed(a,64)>=0;target=address+4+signed(word,16)*4
            name='BLTZ' if rt==0 else 'BGEZ'
        else:raise ValueError('Unqualified control transfer;generic backend execution refused')
        next_pc=target if taken else address+8
        external=p.call_extension is not None and p.call_extension.is_entry(next_pc)
        if next_pc%4 or not 0<=next_pc<0x80000000 or next_pc==address+4:
            raise ValueError('Control target outside aligned positive32 non-slot domain')
        if next_pc not in stops and not any(a<=next_pc and next_pc+4<=a+n for a,n in p.ranges) and not external:
            raise ValueError('Control target outside declared original ranges/stops/primitives')
        if address+4 in stops and not(likely and not taken):raise ValueError('Stop inside executed control slot')
        slot_word=p.uint(address+4);slot=None;delegate=None;addresses=[address]
        if not(likely and not taken):
            if transfer(slot_word):raise ValueError('Control transfer in delay slot refused')
            view=ProjectedRegisters(p,writes)
            if slot_word>>26==0x1f:
                if p.stack_extension is None:raise ValueError('SQ slot requires explicit stack state')
                delegate='stack';slot=p.stack_extension.plan(view,address+4,slot_word,stops)
            elif slot_word>>26 in (0x31,0x39) or slot_word>>26==0 and slot_word&63==0x0a:
                if p.raw_transfer_extension is None:raise ValueError('Raw FPR/MOVZ slot requires explicit shadow state')
                delegate='raw';slot=p.raw_transfer_extension.plan(view,address+4,slot_word,stops)
                if p.call_extension is not None:p.call_extension.validate_transfer(slot)
            else:slot=self.scalar_slot(view,address+4,slot_word)
            addresses.append(address+4)
        return dict(operation=name,operationAddress=f'{address:08X}',word=f'{word:08X}',addresses=addresses,
            rs=rs,rt=rt,left=a,right=b,taken=taken,likely=likely,target=target,nextPc=next_pc,linkWrites=writes,
            slotWord=f'{slot_word:08X}',slot=slot,slotDelegate=delegate,annulled=likely and not taken)

    def commit(self,p,row):
        for register,value in row['linkWrites'].items():p.reg(str(register),value)
        slot=row['slot']
        if slot is not None:
            if row['slotDelegate']=='stack':p.stack_extension.commit(p,slot)
            elif row['slotDelegate']=='raw':p.raw_transfer_extension.commit(p,slot)
            else:
                for register,value in slot['writes'].items():p.reg(str(register),value)
                if slot['operation']=='store':p.write(int(slot['effectiveAddress'],16),bytes.fromhex(slot['data']))
        p.reg('PC',row['nextPc']);self.events.append(row)
