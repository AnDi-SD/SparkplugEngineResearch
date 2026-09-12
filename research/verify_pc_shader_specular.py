"""Known vertex inputs through original-source bytecode and system D3D9.

This is a numeric fixture, not another implementation of game lighting.
Packed ARGB output is compared with its explicit quantization tolerance.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import time

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--contract',type=Path,required=True)
    parser.add_argument('--oracle',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();started=time.perf_counter();checks=0
    output=args.output.resolve()
    if not output.is_relative_to(ROOT/'local-data'):
        raise ValueError('Fixture evidence must remain in local-data')
    output.mkdir(parents=True,exist_ok=False)
    contract=json.loads(args.contract.read_text(encoding='utf-8'))
    observations=[]
    for kind,label in ((1,'point'),(2,'spot')):
        program=next(p for p in contract['programs'] if p.get('candidate',{}).get('specular') and p['candidate']['lights']==[kind])
        path=output/label;path.mkdir()
        shader=(args.contract.parent/program['sourceFile'].replace('.source','.bin')).read_bytes()
        if hashlib.sha256(shader).hexdigest()!=program['bytecodeSha256']:
            raise ValueError('Shader bytecode differs from source compiler evidence')
        (path/'shader.bin').write_bytes(shader)
        constants=[0.0]*384
        def put(name,values):
            row=next(c for c in program['reflection']['constants'] if c['name']==name)
            if len(values)>row['count']*4:
                raise ValueError('Fixture writes beyond reflected constant range')
            at=row['index']*4;constants[at:at+len(values)]=values
        identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
        put('BlendMatrices',[1,0,0,0,0,1,0,0,0,0,1,0]*16)
        put('view_matrix',identity);put('VPTransform',identity)
        put('AmbientCol',[0,0,0,0]);put('MatDiffuse',[0,0,0,.75]);put('MatSpecularPwr',[4])
        put('LightMatDiff',[0,0,0,0]);put('LightMatSpec',[1,1,1,1]);put('LightPos',[1,0,1,1])
        if kind==2:
            put('LightDir',[-1,0,0,0]);put('LightInner',[1]);put('LightOuter',[.5])
        (path/'constants.bin').write_bytes(struct.pack('<384f',*constants))
        vertices=[]
        for z in (1,.1,10):
            vertices.extend([0,0,z,1]+[1,0,0,0]+[0,0,0,0]+[1,0,0,0]+[0,0,0,0]+[0,0,0])
        (path/'vertices.bin').write_bytes(struct.pack('<'+str(len(vertices))+'f',*vertices))
        completed=subprocess.run([str(args.oracle.resolve()),str(path/'shader.bin'),str(path/'constants.bin'),
            str(path/'vertices.bin'),str(path/'output.bin')],capture_output=True,text=True,timeout=20)
        (path/'process.json').write_text(json.dumps(dict(returncode=completed.returncode,stdout=completed.stdout,stderr=completed.stderr),indent=2),encoding='utf-8')
        if completed.returncode:
            raise RuntimeError('D3D9 oracle failed; see process.json')
        raw=(path/'output.bin').read_bytes()
        if len(raw)!=60:
            raise ValueError('Unexpected ProcessVertices output size')
        rows=[]
        for x,y,z,w,color in struct.iter_unpack('<4fI',raw):
            rows.append(dict(position=[x,y,z,w],rgba=[(color>>16)&255,(color>>8)&255,color&255,color>>24]))
        # Exact source expression for this one deliberately simple fixture:
        # normalized homogeneous view = (0,0,-1/sqrt(2)), light=(1,0,0),
        # half.x=sqrt(2/3), so the fourth-power specular term is 4/9.
        expected=[4/9,4/9,4/9,.75]
        errors=[abs(a/255-e) for a,e in zip(rows[0]['rgba'],expected)]
        checks+=4
        if max(errors)>1/255:
            raise AssertionError(f'{label}: native result contradicts float4 normalization fixture')
        observations.append(dict(label=label,sourceProgram=program['label'],bytecodeSha256=program['bytecodeSha256'],
            expectedCenter=expected,centerErrors=errors,rgbaTolerance=1/255,rows=rows))
    report=dict(status='passed',checks=checks,verticesExecuted=6,
        scope='Two center results checked against source expression; near/far outputs retained as observations.',
        backend='System D3D9 software vertex processing, legacy packed ARGB output',
        oracleSha256=hashlib.sha256(args.oracle.read_bytes()).hexdigest(),
        contractSha256=hashlib.sha256(args.contract.read_bytes()).hexdigest(),
        driverSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        seconds=round(time.perf_counter()-started,3),observations=observations)
    (output/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:report[k] for k in ('status','checks','verticesExecuted','seconds')}))
    return 0


if __name__=='__main__':
    raise SystemExit(main())
