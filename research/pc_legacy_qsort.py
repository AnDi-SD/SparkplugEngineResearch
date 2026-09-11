"""Map only exact guest qsort/shortsort bytes of a named local CRT version.

This is an explicit import binding, never a host LoadLibrary or game launch.
The DLL is not proved to be the original distribution's historical version.
"""
from pathlib import Path
import hashlib

ROOT=Path(__file__).resolve().parents[1]
DLL=ROOT/'local-data/results/tools-core-cycle-20260910-0730/occlusion-optimizer/msvcr71-7.10.7031.4.dll'
DLL_SHA='DCA0E5FAF6C94B6ADFF4D90D40795D5A91BA3A3059EA408E992A0F039A494D46'
QSORT=0x7c382650
SHORTSORT=0x7c3825e0

def install_qsort(p,*,maximum_count,widths,comparators):
    import pefile
    assert 0<=maximum_count<=65535 and set(widths)<=set((2,24))
    assert set(comparators)<=set((0x4607f0,0x454800))
    raw=DLL.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()==DLL_SHA
    pe=pefile.PE(data=raw,fast_load=True)
    assert pe.OPTIONAL_HEADER.ImageBase==0x7c360000 and pe.FILE_HEADER.Machine==0x14c
    page=0x7c382000;data=pe.get_data(page-0x7c360000,4096);assert len(data)==4096
    p.mu.mem_map(page,4096,p.uc.UC_PROT_READ|p.uc.UC_PROT_WRITE)
    p.mu.mem_write(page,data);p.mu.mem_protect(page,4096,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC)
    allowed={}
    for begin,end in ((SHORTSORT,0x7c38264d),(QSORT,0x7c3828d3)):
        code=data[begin-page:end-page];instructions=list(p.decoder.disasm(code,begin))
        assert sum(ins.size for ins in instructions)==len(code)
        for ins in instructions:
            assert ins.mnemonic not in {'int','int3','syscall','sysenter','in','out','hlt'}
            expected=bytes(ins.bytes);allowed[ins.address]=expected
            def verify(p,at=ins.address,expected=expected):
                assert bytes(p.mu.mem_read(at,len(expected)))==expected
            p.seams[ins.address]=verify
    calls=[]
    def entry(p):
        assert bytes(p.mu.mem_read(QSORT,len(allowed[QSORT])))==allowed[QSORT]
        base,count,width,compare=(p.uint(p.reg('ESP')+4*i) for i in (1,2,3,4))
        assert count<=maximum_count and width in widths and compare in comparators
        if count:p.mu.mem_read(base,count*width)
        calls.append(dict(base=base,count=count,width=width,compare=compare))
    p.seams[QSORT]=entry;p.put_uint(0x6d9360,QSORT)
    return allowed,calls
