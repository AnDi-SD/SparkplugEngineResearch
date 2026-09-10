#!/usr/bin/env python3
"""Original adjacent state control writes and animation-event string handlers.

Full PC calls; PS2 scalar leaves or post-SQ event bodies with original strcmp.
Explicit string alignments1/8 qualify byte/64-bit paths, not the16-byte MMI path.
"""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from unittest.mock import patch
from capture_native_ranges import ROOT,EXPECTED
from pc_instruction_emulator import run_bounded
from pc_block_emulator import PcBlocks
from ps2_scalar_prefix import Ps2ScalarPrefix
import probe_pc_animation_lifecycle as lifetime

CATALOG=ROOT/'research/character-state-construction-contracts-2026-09-10.json'
PREFIX={0x2e5010:(0x2e501c,0x2e5068),0x2e5480:(0x2e548c,0x2e54bc),
        0x2f00d0:(0x2f00e0,0x2f0134),0x2f8d40:(0x2f8d50,0x2f8da4)}


def execute(row,platform,c):
    ispc=platform=='pc';slot=c['slot'];entry=int(row[platform]['slots'][slot],16)
    pc_entry=int(row['pc']['slots'][slot],16);size=row[platform]['allocationBytes'];fill=c.get('fill',0xa5)
    if ispc:
        with patch.object(lifetime,'PcInstructions',PcBlocks):f=lifetime.LifetimeFixture()
        p=f.p;storage=p.allocate(0x3000);read=lambda a,n:bytes(p.mu.mem_read(a,n));write=lambda a,b:p.mu.mem_write(a,bytes(b));put=p.put_uint
    else:
        ranges=[(entry,0x200)]
        if slot==15:ranges += [(0x40a8a8,0x144),(0x49d1b8,8),(0x463b50,32),(0x464030,32),(0x4642f0,32)]
        p=Ps2ScalarPrefix(ranges);storage=0x21000000;p.map(storage,0x3000);p.map(0x22000000,0x1000)
        p.reg('SP',0x22000800);p.reg('GP',0x4a4170);read,write,put=p.read,p.write,p.put_uint
    definitions={'state':(0,size),'character':(0x400,0x300),'machine':(0x800,0x200),'move':(0xc00,0x300),
        'argument':(0x1000,0x40),'event':(0x1100,0x40),'string':(0x1200,0x80)}
    addresses={k:storage+o for k,(o,n) in definitions.items()}
    for k,(o,n) in definitions.items():write(storage+o,bytes([fill])*n)
    obj,char,machine,move,arg,event,string=addresses.values();delta=0 if ispc else 12
    put(obj,int(row[platform]['vtable'],16));put(obj+0x14,char)
    if size>0x3c:write(obj+0x3c,bytes([c.get('flag',0)]))
    put(char+0x124+delta,machine);put(char+0x12c+delta,move);put(machine+0x12c+delta,move)
    put(arg,c.get('packed',0xffffffff));put(arg+0x1c,event);alignment=c.get('alignment',1);put(event+0x10,string+alignment)
    raw=bytes.fromhex(c.get('textHex',''))+b'\0';assert len(raw)<64;write(string+alignment,raw)
    before={k:read(a,definitions[k][1]) for k,a in addresses.items()};expected={k:bytearray(b) for k,b in before.items()}
    if slot==12:
        if pc_entry==0x523690 or pc_entry==0x520e50 and not c.get('flag',0):struct.pack_into('<I',expected['move'],4,0)
        elif pc_entry==0x521b60:expected['move'][0x60]=0;struct.pack_into('<I',expected['move'],4,0)
        elif pc_entry==0x523790:struct.pack_into('<I',expected['argument'],0,(c.get('packed',0xffffffff)&0xfffffff2)|2)
        elif pc_entry!=0x520e50:raise ValueError(hex(pc_entry))
    else:
        text=raw.split(b'\0')[0]
        if pc_entry==0x517c40 and text==b'event_takeoff':expected['character'][0x26c+delta]=0
        elif pc_entry==0x51ad60 and text==b'air':struct.pack_into('<III',expected['move'],0x1c8+delta,0,0x43bb8000,0);expected['move'][0x1d4+delta]=1
        elif pc_entry in (0x51a680,0x51a0c0):
            if text in (b'event_air_begin',b'event_air_end'):expected['state'][0x3d if pc_entry==0x51a680 else 0x3c]=int(text==b'event_air_begin')
        elif pc_entry not in (0x517c40,0x51ad60):raise ValueError(hex(pc_entry))
    if ispc:
        f.call(entry,obj,(arg,));result=dict(entry=f'{entry:08X}',completion='original return',blocks=sum(p.visits.values()))
    else:
        p.reg('A0',obj);p.reg('A1',arg);start,stop=PREFIX.get(entry,(entry,p.RETURN));result=p.run(start,[stop],timeout_us=500000)
        result['originalEntry']=f'{entry:08X}'
        if slot==15:
            compare_paths=[]
            for a in p.trace:
                if a==0x40a8a8:compare_paths.append('entry')
                elif a==0x40a8f4:raise AssertionError('Unqualified MMI path')
                elif a==0x40a96c:compare_paths.append('original64bit')
                elif a==0x40a9d8:compare_paths.append('originalByte')
            result['stringComparePaths']=compare_paths
    after={k:read(a,definitions[k][1]) for k,a in addresses.items()}
    for k,b in after.items():assert b==expected[k],(k,[(hex(i),a,z) for i,(a,z) in enumerate(zip(b,expected[k])) if a!=z][:16])
    result['buffers']={k:dict(size=len(before[k]),beforeSha256=hashlib.sha256(before[k]).hexdigest().upper(),afterSha256=hashlib.sha256(b).hexdigest().upper(),changes=[dict(offset=i,before=a,after=z) for i,(a,z) in enumerate(zip(before[k],b)) if a!=z]) for k,b in after.items()}
    return result


def guest(output,selection):
    output=Path(output).resolve();selection=Path(selection).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local result required')
    cases=json.loads(selection.read_text())['cases'];rows={r['className']:r for r in json.loads(CATALOG.read_text())['classes']}
    if not 0<len(cases)<=150:raise ValueError('Bounded adjacent batch1..150 cases')
    started=time.perf_counter();report=dict(kind='original-character-state-control-and-event-handlers',status='running',inputs=EXPECTED,
        sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),selectionSha256=hashlib.sha256(selection.read_bytes()).hexdigest().upper(),
        catalogSha256=hashlib.sha256(CATALOG.read_bytes()).hexdigest().upper(),scope='Seven guarded borrowed inputs;original strcmp called with explicit alignment1/8. Post-SQ event components on PS2;no MMI or complete gameplay startup claim.',cases=[])
    output.parent.mkdir(parents=True,exist_ok=True)
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for c in cases:
        report['pending']=c;save()
        try:result=dict(input=c,status='passed',**execute(rows[c['className']],c['platform'],c))
        except Exception as error:result=dict(input=c,status='blocked',error=str(error),traceback=traceback.format_exc())
        report['cases'].append(result);report.pop('pending');save()
    failed=sum(c['status']!='passed' for c in report['cases']);report['status']='passed' if not failed else 'partial';save()
    print(json.dumps(dict(status=report['status'],cases=len(cases),failed=failed,seconds=report['seconds'])))
    return int(failed!=0)


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
