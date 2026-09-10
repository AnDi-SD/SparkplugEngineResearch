#!/usr/bin/env python3
"""Original bounded pointer-array removal helper, complete PC and PS2 bodies."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from pc_instruction_emulator import ROOT,run_bounded
from pc_block_emulator import PcBlocks
from probe_pc_task_timer import TimerFixture
from ps2_scalar_prefix import Ps2ScalarPrefix
from capture_native_ranges import EXPECTED
import probe_pc_animation_lifecycle as lifetime


def guest(output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    started=time.perf_counter();target=0x11111111;other=0x22222222;third=0x33333333
    cases=[dict(label='empty',capacity=10,live=[]),dict(label='not-found',capacity=10,live=[other,third]),
        dict(label='single-last',capacity=1,live=[target]),dict(label='shift-eight',capacity=9,live=[target,other,third]),
        dict(label='shift-nine',capacity=10,live=[target,other,third]),dict(label='middle',capacity=10,live=[other,target,third]),
        dict(label='last-live-long-capacity',capacity=80,live=[other,third,target]),dict(label='capacity-last',capacity=130,live=[other]*129+[target]),
        dict(label='consecutive-duplicates',capacity=10,live=[target,target,other]),dict(label='separated-duplicates',capacity=10,live=[target,other,target,third]),
        dict(label='all-three-duplicates',capacity=10,live=[target,target,target]),dict(label='button-capacity',capacity=105,live=[target,other]),
        dict(label='middle-large-capacity',capacity=130,live=[other]*63+[target]+[third]*3)]
    report=dict(kind='paired-original-entity-manager-array-removal',status='running',cases=[],inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),scope='Complete original helper bodies PC573CE0 and PS2289180. Literal pointer-value arrays/counts;no pointed object destruction or full manager cleanup claim.',
        limits='Fresh guests,PC block100k/2s,PS2 ordinary2000/100ms,30s outer;arrays guarded before/after and inactive tail carries explicit nonzero values.')
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for case in cases:
            report['pending']=case;save();capacity=case['capacity'];live=case['live'];values=live+[0x40000000+i for i in range(capacity-len(live))]
            expected=values.copy();count=len(live);i=0;removed=False
            while i<count:
                if expected[i]==target:
                    expected[i:capacity-1]=expected[i+1:capacity];expected[-1]=0;count-=1;removed=True
                i+=1
            pair={}
            for platform in ('pc','ps2'):
                raw=b'BEFORE!!'+struct.pack('<'+'I'*capacity,*values)+b'!!AFTER!'
                desired=b'BEFORE!!'+struct.pack('<'+'I'*capacity,*expected)+b'!!AFTER!'
                if platform=='pc':
                    with patch.object(lifetime,'PcInstructions',PcBlocks):f=TimerFixture()
                    p=f.p;storage=p.allocate(len(raw));counter=p.allocate(12);p.mu.mem_write(storage,raw);p.mu.mem_write(counter,b'AAAA'+struct.pack('<I',len(live))+b'BBBB')
                    value=f.call(0x573ce0,0,(target,counter+4,capacity,storage+8))&255
                    assert value==removed and bytes(p.mu.mem_read(storage,len(raw)))==desired and bytes(p.mu.mem_read(counter,12))==b'AAAA'+struct.pack('<I',count)+b'BBBB'
                    pair[platform]=dict(returned=value,count=p.uint(counter+4),array=expected,blocks=sum(p.visits.values()),completion='original return')
                else:
                    p=Ps2ScalarPrefix([(0x289180,0x138)]);p.map(0x21000000,4096);storage,counter=0x21000000,0x21000f00;p.write(storage,raw);p.write(counter,b'AAAA'+struct.pack('<I',len(live))+b'BBBB')
                    for name,value in (('A0',0),('A1',target),('A2',counter+4),('A3',capacity),('T0',storage+8)):p.reg(name,value)
                    r=p.run(0x289180,[p.RETURN]);value=p.reg('V0');assert value==removed and p.read(storage,len(raw))==desired and p.read(counter,12)==b'AAAA'+struct.pack('<I',count)+b'BBBB'
                    r.update(returned=value,count=p.uint(counter+4),array=expected);pair[platform]=r
            assert all(pair['pc'][k]==pair['ps2'][k] for k in ('returned','count','array'))
            report['cases'].append(dict(input=case,**pair));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();print(json.dumps(dict(status='blocked',error=str(error))));return 1
    print(json.dumps(dict(status='passed',pairedCases=len(cases),seconds=report['seconds'])));return 0


if __name__=='__main__':
    args=sys.argv[1:]
    raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
