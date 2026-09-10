#!/usr/bin/env python3
"""Original PS2 copy prefix proves that named lookup uses the destination Node."""
import hashlib,json,struct,sys,time
from pathlib import Path
from capture_native_ranges import ROOT,read_window,EXPECTED
from ps2_scalar_prefix import Ps2ScalarPrefix,pristine


def main(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    started=time.perf_counter();raw,sections=pristine();report=dict(kind='original-ps2-move-copy-destination-node-prefix',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
        scope='After inherited copy success and original branch-delay load. Fresh scalar prefixes stop at original Node lookup entry1A7160 or immediately before null destination Node dereference. No fake lookup result,full Copy,Clone or PS2 lifecycle claim.')
    for source_bound,destination_bound in ((True,True),(False,True),(True,False)):
        p=Ps2ScalarPrefix([(0x2b1e4c,0x254)]);p.map(0x21000000,0x6000);p.map(0x4902f0,0x40)
        source,destination,source_node,destination_node=0x21000000,0x21002000,0x21004000,0x21004100;size=0x1c30
        p.write(source,b'\xa5'*size);p.write(destination,b'\x5a'*size);p.put_uint(source+0x18,source_node if source_bound else 0);p.put_uint(destination+0x18,destination_node if destination_bound else 0)
        for node in (source_node,destination_node):p.put_uint(node,0x4902f0)
        p.write(0x4902f0,read_window('ps2',raw,0x4902f0,0x40,sections)[0]);assert p.uint(0x4902f0+0x30)==0x1a7160
        p.reg('S1',source);p.reg('S0',destination);p.reg('V0',0xffffffffa5a5a5a5)
        before_source=p.read(source,size);before_destination=p.read(destination,size)
        r=p.run(0x2b1e4c,[0x1a7160 if destination_bound else 0x2b2090])
        assert p.read(source,size)==before_source and p.uint(destination+0x18)==(destination_node if destination_bound else 0)
        assert p.reg('A0')==(destination_node if destination_bound else 0)
        assert p.reg('A1')==0x478c40 and p.reg('A2')==1
        if destination_bound:assert p.reg('A3')==0
        name=read_window('ps2',raw,0x478c40,32,sections)[0].split(b'\0')[0].decode('ascii');assert name=='camera_lookat_head'
        after=p.read(destination,size);r.update(sourceBound=source_bound,destinationBound=destination_bound,receiver=p.reg('A0'),name=name,recursive=p.reg('A2'),sourceUnchanged=True,destinationNodePreserved=True,
            destinationChangedOffsets=[i for i,(a,b) in enumerate(zip(before_destination,after)) if a!=b])
        report['cases'].append(r)
    report.update(status='passed',seconds=time.perf_counter()-started);output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(status='passed',cases=3,seconds=report['seconds'])))


if __name__=='__main__':main(*sys.argv[1:])
