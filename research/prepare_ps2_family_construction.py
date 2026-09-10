#!/usr/bin/env python3
"""Bounded static factory/constructor candidates; never an execution proof."""
import hashlib,json,re,struct,sys
from pathlib import Path
from capture_native_ranges import ROOT,capture


def literal_vtable_writes(instructions):
    values={0:0};writes=[];delay=False
    for x in instructions:
        b=bytes.fromhex(x['bytes']);w=int.from_bytes(b,'little');op=w>>26;rs=(w>>21)&31;rt=(w>>16)&31;rd=(w>>11)&31;imm=struct.unpack('<h',b[:2])[0]
        if op==15:values[rt]=(w&65535)<<16
        elif op in (9,13):values[rt]=((values[rs]+imm)&0xffffffff if op==9 else values[rs]|(w&65535)) if values.get(rs) is not None else None
        elif op==43 and imm==0 and rs in (16,17,18,19) and values.get(rt) is not None:writes.append(dict(address=x['va'],table=values[rt],receiver=rs))
        elif op==0 and (w&63) in (0x21,0x25):values[rd]=(values[rs]+values[rt])&0xffffffff if values.get(rs) is not None and values.get(rt) is not None else None
        elif op in (32,33,35,36,37):values[rt]=None
        if delay:
            for reg in list(range(2,16))+[24,25]:values[reg]=None
            delay=False
        if op==3:delay=True
        if w==0x03e00008:break
    return writes


def main():
    family=sys.argv[1]
    if not re.fullmatch('[a-z0-9]+(?:-[a-z0-9]+)*',family):raise ValueError('Explicit cached family required')
    p=ROOT/f'local-data/results/native-cycle-20260910-1900/{family}';out=p/'ps2-construction-review.json'
    if out.exists() or (p/'ps2-construction').exists():raise ValueError('Fresh constructor capture required')
    candidates={r['className']:r for r in json.loads((p/'getter-candidates-run1.json').read_text())['classes']};mapping=[];ranges=[]
    for f in json.loads((p/'factories/capture.json').read_text())['ranges']:
        if f['platform']!='ps2':continue
        n=f['name'][4:];ins=f['instructions'];end=next((i+2 for i,x in enumerate(ins) if x['text']=='jr $ra'),len(ins));ins=ins[:end]
        calls=[int(x['text'][4:],16) for x in ins if x['text'].startswith('jal ')];sizes=[int(x['text'].split(', ')[-1],0) for x in ins if x['text'].startswith('addiu $a0, $zero')]
        row=dict(className=n,factory=f['address'],status='factory-prefix-needs-review')
        if len(calls)>=2 and calls[0] in (0x10d850,0x10d820) and sizes:
            inline=any(x['text'].endswith(', ($s0)') and x['text'].startswith('sw ') for x in ins);ctor=f['address'] if inline else calls[1]
            row.update(allocationBytes=sizes[0],allocator=calls[0],constructor=ctor,inline=inline,firstCallAfterAllocation=calls[1]);ranges.append(dict(name=n,platform='ps2',address=ctor,size=0x400))
        mapping.append(row)
    if ranges:capture({'ranges':ranges},p/'ps2-construction')
    windows={r['name']:r for r in json.loads((p/'ps2-construction/capture.json').read_text())['ranges']} if ranges else {}
    for row in mapping:
        n=row['className']
        if n not in windows:continue
        row['literalVtableWrites']=literal_vtable_writes(windows[n]['instructions']);g=candidates[n].get('ps2',{}).get('getters',[])
        if len(g)==1 and len(g[0]['references'])==1:
            table=g[0]['references'][0]['address']-24;matches=[r for r in row['literalVtableWrites'] if r['table']==table]
            row.update(expectedTableCandidate=table,matchingWrites=matches)
            if len(matches)==1:row['status']='literal-constructor-table-match'
        # Candidate status requires human interpretation of receiver/base paths.
        # No guessed table for adjustor aliases, missing getters or other layouts.
    report=dict(kind='ps2-family-construction-candidates',sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),scope='Simple literal propagation and bounded original windows;not general dataflow/CFG or full constructor execution. Exact receiver,aliases and parent identity require review.',classes=mapping)
    out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(classes=len(mapping),matched=sum(r['status']=='literal-constructor-table-match' for r in mapping),needsReview=[r['className'] for r in mapping if r['status']!='literal-constructor-table-match'])))


if __name__=='__main__':main()
