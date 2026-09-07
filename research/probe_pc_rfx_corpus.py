#!/usr/bin/env python3
"""Hash-verified original RFX documents through all original engine callbacks.

External XML decoding is an explicit input boundary. This is not execution of
the full original file/regex/XML-library wrapper. No resource bytes are edited.
"""
from pathlib import Path
import hashlib,json,sys,xml.etree.ElementTree as ET
from pc_instruction_emulator import ROOT,run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from pc_stl_fixtures import install_char_traits
from pc_rfx_corpus_fixtures import CorpusXmlAttributes
from probe_pc_rfx_events import pass_capture
from probe_pc_rfx_variables import variable_capture

FILES={
 'fixed':('Fixed.rfx','ac6785428ba851dea88e3a6cddc03fdf620fa06d82063db83176598140803db1'),
 'bumpmap':('Bumpmap.rfx','1deacdcd1c7692edcd5451ccd50b2b771af047ea80bb3b4fddf42750a510e245'),
}

def events_for(mode):
    name,expected=FILES[mode];data=(ROOT/'local-data/pc-pristine/Shaders'/name).read_bytes()
    if hashlib.sha256(data).hexdigest()!=expected:raise AssertionError('refusing changed original RFX')
    events=[]
    def visit(node):
        if '}' in node.tag:raise AssertionError('namespaced input outside this fixture')
        events.append([True,node.tag,node.attrib])
        for child in node:visit(child)
        events.append([False,node.tag,{}])
    visit(ET.fromstring(data));return events

def main(mode,return_capture=False):
    events=events_for(mode);f=PCWriteBytesFixture();p=f.p;install_char_traits(p)
    xml=CorpusXmlAttributes(f,[v for _,_,a in events for v in a.values() if len(v)>4095]);maximum=0;checks=0
    def call(entry,this=0,args=(),cdecl=False):
        nonlocal maximum
        p.run(entry,this=this,args=args,callee_pop=not cdecl)
        if p.uint(f.teb)!=0xffffffff:raise AssertionError('original call restores SEH')
        maximum=max(maximum,sum(p.visits.values()));return p.reg('EAX')
    empty=p.allocate(28);call(0x450d90,this=empty)
    obj=p.allocate(0x68);f.allocations[obj]=0x68;p.mu.mem_write(obj,b'\xcc'*0x68)
    call(0x4d0960,this=obj,args=(1,empty,2));p.put_uint(obj+0x40,0)
    loader=call(0x4d54b0);p.put_uint(loader+0x540,obj);p.put_uint(loader+0x544,xml.handle)
    call(0x4cffc0,this=loader+0x548)
    for is_start,tag,attributes in events:
        xml.set(attributes);name=xml.string(tag)
        if is_start:call(0x4d4750,args=(loader,0,0,name,0),cdecl=True)
        else:call(0x4d6050,args=(loader,0,0,name),cdecl=True)
        checks+=1
    begin,end=p.uint(obj+0x58),p.uint(obj+0x5c);count=(end-begin)//4 if begin else 0
    if count>32:raise AssertionError('bounded corpus variable count')
    variables=[p.uint(begin+i*4) for i in range(count)];expected_leaks=set()
    for variable in variables:
        if p.uint(variable+0x3c)==6:
            for offset in (0x40,0x5c,0x78):
                if p.uint(variable+offset+0x18)>=16:expected_leaks.add(p.uint(variable+offset+4))
    begin,end=p.uint(obj+0x48),p.uint(obj+0x4c);pass_count=(end-begin)//0x1dc if begin else 0
    if pass_count!=1:raise AssertionError('original corpus contains one pass')
    capture=[mode,p.uint(obj+0x40),[variable_capture(p,v) for v in variables],[pass_capture(p,begin+i*0x1dc) for i in range(pass_count)]]
    call(0x59a660,this=empty)
    for owned in (loader,obj):call(p.uint(p.uint(owned)),this=owned,args=(1,))
    for slot in (0x74e060,0x75526c,0x755264):
        owned=p.uint(slot)
        if owned:call(p.uint(p.uint(owned)),this=owned,args=(1,))
    leaked=set(f.allocations)-set(f.freed);checks+=1
    if leaked!=expected_leaks:raise AssertionError('unexpected original corpus ownership delta')
    if not return_capture:
        destination=ROOT/'local-data/results'/f'pc-rfx-{mode}-engine-capture.json'
        destination.write_text(json.dumps(capture,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print('CORPUS_CAPTURE',destination.relative_to(ROOT),flush=True)
    print(f'PASS {checks}/{checks}: original {mode} RFX engine callbacks={len(events)}; variables={count}; passes={pass_count}; maxInstructions={maximum}; heap={p.allocated}; nativeTextureTailLeaks={len(leaked)}',flush=True)
    return capture if return_capture else 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))
