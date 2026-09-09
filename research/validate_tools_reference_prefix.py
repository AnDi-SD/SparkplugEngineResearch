#!/usr/bin/env python3
"""Fresh original reference paths and shared tools wire observation."""
from pathlib import Path
import ctypes as C
import hashlib,json,struct,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_read_reference import ReadReferenceFixture,EMPTY_OBJECT,early,success

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
class Prefix(C.Structure):_fields_=[(n,C.c_uint32) for n in ('id','inline_size','encoding','class_id')]
def main(output):
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    path=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll';dll=C.CDLL(str(path))
    dll.spv_reference_prefix.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,C.c_uint32,C.POINTER(Prefix)]
    dll.spv_last_error.restype=C.c_char_p
    def inspect(data,total=None,kind=1):
        result=Prefix();buf=C.create_string_buffer(data)
        ok=dll.spv_reference_prefix(buf,len(data),len(data) if total is None else total,kind,C.byref(result))
        return ok,{n:getattr(result,n) for n,_ in Prefix._fields_},None if ok else dll.spv_last_error().decode()
    records=[]
    for mode,data in [('null',bytes(4)),('existing',struct.pack('<II',7,0)),
        ('inline',struct.pack('<II',7,len(EMPTY_OBJECT))+EMPTY_OBJECT)]:
        f=ReadReferenceFixture(data);p=f.p;start=f.position
        if mode=='existing':
            obj=f.call(0x41a090);f.objects.append(obj);p.put_uint(f.entry+0x20,obj)
        result=f.read_reference();instructions=sum(p.visits.values())
        assert f.position-start==len(data) and not f.errors
        assert (result==0)==(mode=='null')
        if mode=='inline':f.objects.append(result);assert f.publications==[result]
        ok,observed,error=inspect(data);assert ok,error
        assert observed['id']==(0 if mode=='null' else 7)
        assert observed['inline_size']==(len(EMPTY_OBJECT) if mode=='inline' else 0)
        assert observed['encoding']=={'null':0,'existing':1,'inline':2}[mode]
        if mode=='inline':
            assert observed['class_id']==0x56ee563a
            ok,prefix,error=inspect(data[:8],len(data),0);assert ok and prefix['id']==7 and prefix['inline_size']==len(EMPTY_OBJECT)
        records.append({'mode':mode,'input_hex':data.hex(),'native_consumed':f.position-start,
            'native_instructions':instructions,'tools':observed});f.cleanup()
    # Existing original fixture observes failed size Read then deliberately
    # stops at467698; the original does not test this Read result. This is NOT
    # a successful native malformed-reference load or a native refusal claim.
    early('failed-size')
    success('split')
    invalid=[struct.pack('<I',7),bytes(8),struct.pack('<II',7,1)+b'X',
        struct.pack('<II',7,8)+struct.pack('<II',0x56ee563a,0),struct.pack('<II',7,9)+EMPTY_OBJECT[:8]]
    rejected=[]
    for data in invalid:
        ok,_,error=inspect(data);assert not ok;rejected.append({'input_hex':data.hex(),'error':error})
    # Failed output is not reused, and a following valid request remains usable.
    assert inspect(bytes(4))[0]
    report={'status':'passed','pc_exe_sha256':sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),
        'native_dll_sha256':sha(path),'cases':records,'rejected_host_cases':rejected,
        'original_extra':['split ID/payload streams complete','failed-size controlled stop467698 after failed Read, not native rejection'],
        'dependencies':sorted(({'path':str(Path(m.__file__).resolve().relative_to(ROOT)).replace('\\','/'),'sha256':sha(m.__file__)}
            for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda x:x['path']),
        'scope':'Common original prefix reads only; full inspector marker/extent guards are host policy. No resolution or runtime ownership claimed by prefix ABI.'}
    target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS shared reference prefix: 3 original paths, split stream, failed-size stop, 5 host rejections')
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
