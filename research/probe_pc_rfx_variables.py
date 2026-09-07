#!/usr/bin/env python3
"""Original PC RFX variable producers, partial initialization and ownership."""
from pathlib import Path
import json,sys
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_stl_fixtures import install_char_traits,read_cstring
from pc_xml_event_fixtures import DecodedXmlAttributes
from probe_pc_rfx_events import start

CASES={
 'bool':[start('RmBooleanVariable',NAME='Flag',VALUE='TRUE',ARTIST_EDITABLE='TRUE',DISPLAY_NAME='Ignored'),start('RmBooleanVariable',NAME='',VALUE='true',ARTIST_EDITABLE='true')],
 'float-defaults':[start('RmFloatVariable',NAME='A',VALUE='1.25'),start('RmFloatVariable',NAME='B',VALUE='-2.5tail',CLAMP='FALSE')],
 'float-clamp':[start('RmFloatVariable',NAME='A',VALUE='3e-1',CLAMP='TRUE',MIN='-1',MAX='2'),start('RmFloatVariable',NAME='B',VALUE='0',CLAMP='TRUE',MIN='4')],
 'vector':[start('RmVectorVariable',NAME='Vector',VALUE_0='1',VALUE_1='-2',VALUE_2='.25',VALUE_3='4',ARTIST_EDITABLE='TRUE')],
 'color':[start('RmColorVariable',NAME='RGB',VALUE_0='1',VALUE_1='.5',VALUE_2='0'),start('RmColorVariable',NAME='Wrap',VALUE_0='-1',VALUE_1='.5',VALUE_2='2')],
 'texture':[start('Rm2DTextureVariable',NAME='Texture',FILE_NAME='a.dds')],
 'texture-long':[start('Rm2DTextureVariable',NAME='A longer variable name',FILE_NAME='textures/a-long-texture-name.dds')],
 'mixed':[start('RmBooleanVariable',NAME='Same',VALUE='FALSE'),start('RmFloatVariable',NAME='Same',VALUE='2',CLAMP='true'),start('RmVectorVariable',NAME='V',VALUE_0='0',VALUE_1='1',VALUE_2='2',VALUE_3='3'),start('Rm2DTextureVariable',NAME='T',FILE_NAME='x.dds')],
}

def string(p,address):return read_cstring(p,p.uint(address+4) if p.uint(address+0x18)>=16 else address+4).decode('latin1')

def variable_capture(p,address):
    kind=p.uint(address+0x3c);words=[];strings=[]
    if kind==1:words=[bytes(p.mu.mem_read(address+0x40,1))[0]]
    elif kind in (3,4,5):
        count=12 if kind==5 else 3
        words=[None if p.uint(address+0x40+4*i)==0xcccccccc else p.uint(address+0x40+4*i) for i in range(count)]
    elif kind==6:strings=[string(p,address+offset) for offset in (0x40,0x5c,0x78)]
    else:raise AssertionError('unknown variable kind')
    return [string(p,address),string(p,address+0x1c),bytes(p.mu.mem_read(address+0x38,1))[0],kind,words,strings]

def main(mode,return_capture=False):
    if mode not in CASES:raise ValueError('bounded RFX variable mode')
    f=PCWriteBytesFixture();p=f.p;install_char_traits(p);xml=DecodedXmlAttributes(p);maximum=0;checks=0
    def call(entry,this=0,args=()):
        nonlocal maximum
        result=f.call(entry,this=this,args=args);maximum=max(maximum,sum(p.visits.values()));return result
    empty=p.allocate(28);call(0x450d90,this=empty)
    obj=p.allocate(0x68);f.allocations[obj]=0x68;p.mu.mem_write(obj,b'\xcc'*0x68)
    call(0x4d0960,this=obj,args=(1,empty,2));loader=call(0x4d54b0)
    p.put_uint(loader+0x540,obj);p.put_uint(loader+0x544,xml.handle);captures=[];expected_leaks=set()
    for _,tag,attributes in CASES[mode]:
        xml.set(attributes);call(0x4d3a80,this=loader,args=(xml.string(tag),))
        begin,end=p.uint(obj+0x58),p.uint(obj+0x5c);count=(end-begin)//4 if begin else 0
        checks+=1
        if count!=len(captures)+1:raise AssertionError('actual callback appends exactly one variable, including duplicate names')
        variables=[p.uint(begin+i*4) for i in range(count)];captures.append([variable_capture(p,v) for v in variables])
        for v in variables:
            if p.uint(v+0x3c)==6:
                for offset in (0x40,0x5c,0x78):
                    if p.uint(v+offset+0x18)>=16:expected_leaks.add(p.uint(v+offset+4))
    call(0x59a660,this=empty)
    for owned in (loader,obj):call(p.uint(p.uint(owned)),this=owned,args=(1,))
    for slot in (0x74e060,0x75526c,0x755264):
        owned=p.uint(slot)
        if owned:call(p.uint(p.uint(owned)),this=owned,args=(1,))
    leaked=set(f.allocations)-set(f.freed);checks+=1
    if leaked!=expected_leaks:raise AssertionError(('unexpected original ownership delta',[(hex(a),f.allocations[a]) for a in leaked],list(map(hex,expected_leaks))))
    if bool(leaked)!=(mode=='texture-long'):raise AssertionError('bounded expected heap-string leak branch')
    capture=[mode,captures]
    if not return_capture:print('RFX_VARIABLES_CAPTURE',json.dumps(capture),flush=True)
    print(f'PASS {checks}/{checks}: original RFX variables {mode}; maxInstructions={maximum}; heap={p.allocated}; nativeTextureTailLeaks={len(leaked)}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
