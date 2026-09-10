#!/usr/bin/env python3
"""Opt-in basic-block tracing of the same bounded original x86 guest.

Each translated block is decoded before execution. A block containing a
denied instruction stops at its entry, potentially earlier than instruction
tracing. Host/guest image writes invalidate the validation cache. Explicit
fixture and requested-boundary hooks remain at exact instruction addresses.

``visits`` and ``tail`` describe BLOCK ENTRIES in this class, not instruction
coverage/counts. Unicorn's native instruction/time caps are unchanged.
"""
from pc_instruction_emulator import PcInstructions,BASE,denied_instruction
from bisect import bisect_right
from types import MappingProxyType


class PcBlocks(PcInstructions):
    visit_unit='basic-block-entry'

    def __init__(self,*args,**kwargs):
        if kwargs.get('code_cache_mode','page')!='page':raise ValueError('Block validation requires page invalidation')
        self.block_cache={};self.block_cache_pages={};self.seam_hooks={};self.hook_boundaries=[]
        super().__init__(*args,**kwargs)
        self.mu.hook_del(self.instruction_hook)
        self.block_hook=self.mu.hook_add(self.uc.UC_HOOK_BLOCK,self._block)

    def _invalidate_code(self,address,size,*,host=False):
        super()._invalidate_code(address,size,host=host)
        if size<=0 or address>=BASE+self.size or address+size<=BASE:return
        first,last=max(address,BASE)>>12,min(address+size-1,BASE+self.size-1)>>12
        for page in range(first,last+1):
            entries=self.block_cache_pages.pop(page,())
            if entries and host:self.mu.ctl_remove_cache(page<<12,(page+1)<<12)
            for entry in entries:self.block_cache.pop(entry,None)

    def _block(self,mu,address,length,_):
        self.visits[address]+=1;self.tail.append(address)
        if address==self.stop_at:self._stop('requested boundary');return
        if address in self.seams:return
        if not BASE<=address<BASE+self.size:self._stop('external execution denied');return
        if not 0<length<=0x10000 or address+length>BASE+self.size:
            self._stop('invalid basic block extent');return
        # A requested boundary or fixture replaces execution at that address.
        # Bytes past it are outside this prefix, even inside the same TB.
        if self.hook_boundaries and address<self.hook_boundaries[-1] and address+length>self.hook_boundaries[0]:
            index=bisect_right(self.hook_boundaries,address)
            if index<len(self.hook_boundaries):length=min(length,self.hook_boundaries[index]-address)
        key=(address,length)
        if key not in self.block_cache:
            raw=bytes(mu.mem_read(address,length));next_address=address;denied=None
            for ip,n,mnemonic,operands in self.decoder.disasm_lite(raw,address):
                if ip!=next_address:denied=(next_address,'invalid');break
                next_address+=n
                if denied_instruction(mnemonic):denied=(ip,mnemonic);break
            if denied is None and next_address!=address+length:denied=(next_address,'invalid')
            self.block_cache[key]=denied
            for page in range(address>>12,((address+length-1)>>12)+1):self.block_cache_pages.setdefault(page,set()).add(key)
        denied=self.block_cache[key]
        if denied is not None:
            self._stop(f'OS/privileged block rejected before execution: {denied[1]} at {denied[0]:#x}')

    def _seam(self,mu,address,length,_):
        # Hooks are synchronized before each run; changing seam membership
        # within a run is deliberately outside this optional tracer contract.
        callback=self.seams.get(address)
        if callback is None:self._stop('fixture membership changed during block run');return
        callback(self)

    def _boundary(self,mu,address,length,_):self._stop('requested boundary')

    def run(self,entry,this=0,args=(),*,stop_at=None,callee_pop=True):
        for address in set(self.seam_hooks)-set(self.seams):self.mu.hook_del(self.seam_hooks.pop(address))
        for address in set(self.seams)-set(self.seam_hooks):
            self.seam_hooks[address]=self.mu.hook_add(self.uc.UC_HOOK_CODE,self._seam,begin=address,end=address)
        boundary=None
        if stop_at is not None:boundary=self.mu.hook_add(self.uc.UC_HOOK_CODE,self._boundary,begin=stop_at,end=stop_at)
        self.hook_boundaries=sorted(set(self.seams)|({stop_at} if stop_at is not None else set()))
        writable_seams=self.seams;self.seams=MappingProxyType(dict(writable_seams))
        try:return super().run(entry,this,args,stop_at=stop_at,callee_pop=callee_pop)
        except AssertionError as error:
            raise AssertionError(str(error).replace('instructions=','basicBlocks=')) from error
        finally:
            self.seams=writable_seams
            if boundary is not None:self.mu.hook_del(boundary)
