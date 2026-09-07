#!/usr/bin/env python3
"""Original complete RFX file/regex/ID/name/template creation, warm library."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_stl_fixtures import read_cstring
from pc_rfx_regex_fixtures import RFXRegexInputs
from pc_win32_file_fixtures import Win32FileInput

def document(identity=b'1234ABCD',name=b'Tiny'):
    return b'<RmDirectXEffect NAME="'+name+b'" TYPE="DirectX"><RmStringVariable NAME="ID" VALUE="'+identity+b'"/></RmDirectXEffect>'
CASES={
 'basic':document(),'signed':document(b'-1'),'hex-prefix-tail':document(b' \t+0x7fTAIL'),
 'lowercase':document().lower(),'newlines':document().replace(b'VALUE=',b'\nVALUE=').replace(b'TYPE=',b'TYPE\n='),
 'raw-entity':document(name=b'A&amp;B'),'empty-name':document(name=b''),
 'duplicates':document(b'1',b'First')+document(b'2',b'Second'),
 'nul-tail':document()+b'\0Ignored trailing data',
 'missing-id':b'<RmDirectXEffect NAME="Tiny" TYPE="DirectX"/>',
 'missing-name':b'<RmStringVariable NAME="ID" VALUE="1"/>',
 'invalid-id':document(b'xyz'),
 'spaced-close':document().replace(b'ABCD"/>',b'ABCD" />'),
 'reordered-name':document().replace(b'NAME="Tiny" TYPE="DirectX"',b'TYPE="DirectX" NAME="Tiny"'),
}

def main(mode,return_capture=False):
    data=CASES[mode]
    if not data.isascii():raise AssertionError('ASCII regex specimen')
    f=PCWriteBytesFixture();p=f.p;regex=RFXRegexInputs(f);api=Win32FileInput(p,data);checks=0
    def check(ok,label):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(label)
    name=p.allocate(len(api.name)+1);p.mu.mem_write(name,api.name+b'\0');loader=f.call(0x4d54b0)
    out=f.call(0x4d56d0,this=loader,args=(name,));maximum=sum(p.visits.values());visited=set(p.visits)
    check(p.visits.get(0x4d0a30) and p.visits.get(0x476550),'whole original file plus regex search')
    check(not api.opened,'original file destruction completed')
    diagnostic=next((read_cstring(p,text).decode('ascii') for branch,text in ((0x4d5a9a,0x6f3dc4),(0x4d5a13,0x6f3de8),(0x4d5857,0x6f3e1c)) if branch in visited),'')
    value=None
    if out:
        string=out+0x24;n=p.uint(string+20);chars=p.uint(string+4) if p.uint(string+24)>=16 else string+4
        doc=bytes(p.mu.mem_read(chars,n)) if n else b''
        value=[p.uint(out+0x10),read_cstring(p,p.uint(out+0x20)).decode('ascii'),doc.hex(),p.uint(out+0x1c),int.from_bytes(p.mu.mem_read(out+0x18,1),'little')]
        check(doc==data.split(b'\0',1)[0] and value[3:]==[2,0],'exact C-string document and uninitialized template type2')
        check(p.uint(loader+0x540)==out and not diagnostic,'loader borrows newly returned template')
    else:check(bool(diagnostic),'actual diagnostic branch identifies failure')
    active=int.from_bytes(p.mu.mem_read(0x73fe6b,1),'little');check(active==0,'completed metadata paths clear active flag')
    regex.close()
    for obj in (loader,out,p.uint(0x75db9c)):
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    for slot in (0x74e060,0x75526c,0x755264):
        obj=p.uint(slot)
        if obj:f.call(p.uint(p.uint(obj)),this=obj,args=(1,))
    leaks=[f.allocations[a] for a in sorted(set(f.allocations)-set(f.freed))]
    expected=[5000] if diagnostic in ('Effect file contains invalid class ID','Effect file does not contain a valid effect name') else []
    check(leaks==expected,'exact original input-buffer ownership on failure')
    capture=[mode,value,diagnostic,active,regex.scanCalls]
    if not return_capture:print('RFX_FILE_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original RFX file {mode}; fileInstructions={maximum}; initInstructions={regex.initializationInstructions}; heap={p.allocated}',flush=True)
    return capture if return_capture else 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
