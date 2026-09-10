#!/usr/bin/env python3
"""Independent PS2 RTTI, secondary adjustments, null clones and manager bytes."""
import hashlib,json,struct,sys,time,traceback
from pathlib import Path
from ps2_scalar_prefix import Ps2ScalarPrefix
from capture_native_ranges import ROOT,EXPECTED


def main():
    output=Path(sys.argv[1]).resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    p=ROOT/'local-data/results/native-cycle-20260910-1900/projection-family';rows={r['className']:r for r in json.loads((p/'catalog-family.json').read_text())}
    cases=[]
    getters={'spBoxProjection':(0x135b50,None),'spPyramidProjection':(0x135b40,None),'spProjection':(0x1be200,None),'spProjectionFX':(0x1be8c0,0x1be7b0),'spPS2ProjectionFX':(0x20b760,0x20a160),'spProjectionManager':(0x1be8d0,None),'spPS2ProjectionManager':(0x20b770,None),'spShadowProjection':(0x1c05f0,None)}
    for n,(a,target) in getters.items():cases.append(dict(kind='getter',className=n,entry=a,aliasTarget=target,expected=rows[n]['ps2Record']))
    for n,a,target in (('spProjection',0x1be660,None),('spProjectionFX',0x1be8b0,0x1be890),('spProjectionManager',0x1becf0,None),('spShadowProjection',0x1c24a0,None)):
        cases.append(dict(kind='null-clone',className=n,entry=a,aliasTarget=target,expected=0))
    for n,a,target in (('spProjectionFX',0x1be8a0,0x1be7c0),('spPS2ProjectionFX',0x20b740,0x20aec0),('spPS2ProjectionFX',0x20b750,0x20b020)):
        cases.append(dict(kind='adjustor',className=n,entry=a,aliasTarget=target,expected=None))
    for kind,entry,values in (('enabled-get',0x1ad500,(0,255)),('enabled-set',0x1be8e0,(0,256,511)),('init',0x1bebe0,(0,255))):
        for value in values:cases.append(dict(kind=kind,entry=entry,value=value))
    report=dict(kind='ps2-projection-leaves-and-secondary-interface',status='running',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),cases=[],scope='Fresh original scalar leaves or adjustors to declared consumer. Full88-byte object guard. Manager byte methods shared by base/concrete table;no renderer phase execution. TextureProjection has no constant getter match and is not silently assigned another table.');started=time.perf_counter()
    def save():report['seconds']=time.perf_counter()-started;output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for c in cases:
            report['pending']=c;save();entry=c['entry'];target=c.get('aliasTarget');adjust=bool(target);ranges=[(entry,12 if c['kind']=='init' or c['kind']=='getter' and not target else 8)]
            if target and c['kind']!='adjustor':ranges.append((target,12 if c['kind']=='getter' else 8))
            q=Ps2ScalarPrefix(ranges);obj=0x21000000;q.map(obj,4096);q.write(obj,b'\xa5'*88);q.reg('A0',obj+(4 if adjust else 0));q.reg('A1',c.get('value',1));expected=bytearray(q.read(obj,88))
            if c['kind']=='enabled-get':q.write(obj+0x14,bytes([c['value']]));expected[0x14]=c['value']
            if c['kind']=='enabled-set':expected[0x14]=c['value']&255
            if c['kind']=='init':q.write(obj+0x14,bytes([c['value']]));expected[0x14]=1
            r=q.run(entry,[target if c['kind']=='adjustor' else q.RETURN]);assert q.read(obj,88)==bytes(expected),'whole object guard'
            assert q.reg('A0')==obj,'secondary adjustment or unchanged complete receiver'
            if c['kind'] in ('getter','null-clone'):assert q.reg('V0')==c['expected']
            elif c['kind']=='enabled-get':assert q.reg('V0')==c['value']
            elif c['kind']=='init':assert q.reg('V0')==1
            if c['kind']=='adjustor':assert q.reg('A1')==1
            report['cases'].append(dict(input=c,execution=r));report.pop('pending');save()
        report['status']='passed';save()
    except Exception as error:
        report.update(status='blocked',error=str(error),traceback=traceback.format_exc());save();raise
    print(json.dumps(dict(status='passed',cases=len(cases),seconds=report['seconds'])))


if __name__=='__main__':main()
