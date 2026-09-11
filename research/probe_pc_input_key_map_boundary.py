"""Original PC key map, stopping before reads beyond the reviewed144 rows."""
import hashlib,json,sys,time
from pathlib import Path
from capture_native_ranges import ROOT,EXPECTED
from pc_block_emulator import PcBlocks
from pc_instruction_emulator import run_bounded


class TableBoundary(RuntimeError):pass


def guest(output):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Fresh local result required')
    start=time.perf_counter();report=dict(kind='original-key-map-reviewed-table-boundary',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),cases=[])
    for key in [0,142,176,143,0xffffffff]:
        p=PcBlocks();reads=[];boundary=[]
        def observe(u,access,address,size,value,user):
            if address>=0x6f43c8:
                boundary.append(dict(address=f'{address:08X}',size=size,nextWord=f'{p.uint(address):08X}'))
                raise TableBoundary('Outside reviewed key/scan rows;no replacement result')
            reads.append(address)
        hook=p.mu.hook_add(p.uc.UC_HOOK_MEM_READ,observe,begin=0x6f3f48,end=0x6f43cf)
        row=dict(key=key)
        try:
            p.run(0x4d6470,args=(key,));value=p.reg('EAX');assert value=={0:1,142:142,176:176}[key]
            row.update(status='passed',completion='original return',value=value)
        except TableBoundary:
            assert key in (143,0xffffffff) and boundary==[dict(address='006F43C8',size=4,nextWord='3A83126F')]
            row.update(status='passed',completion='explicit pre-read table boundary;not original return',boundary=boundary)
        p.mu.hook_del(hook);row.update(readCount=len(reads),lastReviewedRead=f'{reads[-1]:08X}' if reads else None);report['cases'].append(row)
    report.update(status='passed',seconds=time.perf_counter()-start);output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(status=report['status'],cases=len(report['cases']),seconds=report['seconds'])))
    return 0


if __name__=='__main__':
    args=sys.argv[1:];raise SystemExit(guest(*args[1:]) if args and args[0]=='--guest' else run_bounded(Path(__file__),args))
