"""Read-only source -> shared specialization -> SDK -> captured shader evidence.

Original resources and bytecode stay outside Git. Candidate keys are explicit
offline probe inputs, not runtime classification rules or observed game keys.
No lighting equation is implemented here. Windows SDK calls create no device.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import itertools
import json
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as ET
from pc_shader_sdk import D3dx

ROOT=Path(__file__).resolve().parents[1]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def hextext(value):
    return value.encode('latin1').hex() or '-'


def call_source(binary, mode, text):
    result=subprocess.run([str(binary),mode],input=text,capture_output=True,text=True,timeout=30,check=True)
    return json.loads(result.stdout)


def fields(record):
    return ' '.join(hextext(record.get(k,'')) for k in ('CODE','DECLARATION_BLOCK','ENTRY_POINT','TARGET'))


def compose(binary, record, insertion='', header=''):
    return call_source(binary,'--source-record',fields(record)+' '+hextext(insertion)+' '+hextext(header)+'\n').encode('latin1')


def candidates():
    result={}
    def add(weights,mode,lights=(),uv=0,specular=False,texcoords=1):
        # Explicit finite input cases for the documented two-word key ABI.
        # The shared C++ manager alone decodes and specializes these keys.
        key=(weights|(texcoords<<4)|(uv<<8)|(mode<<16)|(len(lights)<<20)|(int(specular)<<24),
             sum(t<<(2*i) for i,t in enumerate(lights)))
        result[key]=dict(weights=weights,colorMode=mode,lights=list(lights),uvMask=uv,
                         specular=specular,texcoords=texcoords)
    # Observed families: unlit color/material and additive lighting, up to
    # three selected directional/point lights; every 1..4 weight count.
    for weights,mode,uv in itertools.product(range(1,5),(1,2),(0,1)):
        add(weights,mode,uv=uv)
    for weights,count,uv in itertools.product(range(1,5),range(4),(0,1)):
        for lights in itertools.product((0,1),repeat=count):
            add(weights,4,lights,uv)
    # Independent source branches, including ones absent from this live run.
    for mode in (0,3,5,6,7):
        add(1,mode)
    for kind,specular in itertools.product(range(3),(False,True)):
        add(1,4,(kind,),specular=specular)
    for count in (0,2,8):
        for uv in (0,(1<<count)-1):
            add(1,2,uv=uv,texcoords=count)
    add(0,2) # Direct template probe; manager bypass is explicitly recorded.
    return result


def summarize_instructions(text):
    lines=text.splitlines()
    counts=Counter(line.split()[0] for line in lines if line)
    outputs=[]
    for line in lines:
        parts=line.replace(',',' ').split()
        if len(parts)>1 and parts[1].startswith(('oPos','oD','oT','oPts','oFog')):
            outputs.append(line)
    return dict(opcodes=dict(sorted(counts.items())),outputWrites=outputs)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game',type=Path,default=ROOT/'local-data/Winx Club')
    parser.add_argument('--run',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--source-probe',type=Path,default=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugShaderCompileTests.exe')
    parser.add_argument('--sdk',default='d3dx9_24.dll')
    args=parser.parse_args();started=time.perf_counter()
    output=args.output.resolve()
    if not output.is_relative_to(ROOT/'local-data'):
        raise ValueError('Extracted resources must remain in local-data')
    output.mkdir(parents=True,exist_ok=False)
    programs_dir=output/'programs';programs_dir.mkdir()
    sdk=D3dx(args.sdk);inputs=[];originals=[];programs=[];templates=[]
    for p in sorted((args.run/'shaders-client').glob('shader-*.bin')):
        data=p.read_bytes();assembly,normalized=sdk.disassemble(data)
        originals.append(dict(file=p.name,sha256=sha(data),normalizedSha256=sha(normalized.encode()),
            reflection=sdk.reflect(data),matches=[],instructions=summarize_instructions(normalized)))
    if not 1<=len(originals)<=4096:
        raise ValueError('No bounded captured shader set')
    def compile_record(label,source,assembly=False,entry='',target='',**extra):
        stem=f'{len(programs):04d}'
        record=dict(label=label,sourceSha256=sha(source),sourceFile=f'programs/{stem}.source',assembly=assembly,entry=entry,target=target,**extra)
        (programs_dir/f'{stem}.source').write_bytes(source)
        if assembly:
            hr,code,messages=sdk.assemble(source);reflection=sdk.reflect(code) if code else None
        else:
            hr,code,messages,reflection=sdk.compile(source,entry,target,flags=0)
        record.update(hresult=hr,messages=messages,reflection=reflection)
        if code:
            disassembly,normalized=sdk.disassemble(code)
            digest=sha(normalized.encode())
            (programs_dir/f'{stem}.bin').write_bytes(code)
            (programs_dir/f'{stem}.asm').write_text(disassembly,encoding='utf-8')
            record.update(bytecodeSha256=sha(code),normalizedSha256=digest,instructions=summarize_instructions(normalized))
            for original in originals:
                if original['normalizedSha256']==digest:
                    original['matches'].append(dict(program=len(programs),label=label,key=extra.get('key'),
                        rawBytecodeEqual=original['sha256']==sha(code),
                        reflectedConstantsEqual=(original['reflection'] or {}).get('constants')==(reflection or {}).get('constants')))
        programs.append(record)

    for path in sorted((args.game/'Shaders').iterdir()):
        if not path.is_file():
            continue
        data=path.read_bytes()
        if len(data)>65536:
            raise ValueError('Resource outside bounded shader corpus')
        inputs.append(dict(path=str(path.resolve()),sha256=sha(data),bytes=len(data)))
        if path.suffix.lower() in ('.vsh','.psh'):
            compile_record(path.name,data,assembly=True,
                declaredConstants=re.findall(r'//\s*([A-Za-z_][A-Za-z0-9_]*):\s*registers?\s+(c\d+(?:-c\d+)?)',data.decode('latin1')))
        elif path.suffix.lower()=='.rfx':
            for i,node in enumerate(e for e in ET.fromstring(data).iter() if 'Shader' in e.tag and 'CODE' in e.attrib):
                attributes=dict(node.attrib)
                label=f'{path.name}:{i}:{attributes.get("PIXEL_SHADER")}'
                templates.append(dict(file=path.name,index=i,tag=node.tag,
                    attributes={k:v for k,v in attributes.items() if k not in ('CODE','DECLARATION_BLOCK')},
                    codeSha256=sha(attributes['CODE'].encode('latin1')),
                    constants=[dict(e.attrib) for e in node.iter() if e.tag=='RmShaderConstant']))
                if path.name=='Fixed.rfx' and attributes.get('PIXEL_SHADER')=='FALSE':
                    keys=candidates()
                    text=fields(attributes)+' '+str(len(keys))+'\n'+'\n'.join(f'{a} {b}' for a,b in keys)+'\n'
                    requests=call_source(args.source_probe,'--source-batch',text)
                    if len(requests)!=len(keys):
                        raise ValueError('Incomplete shared specialization batch')
                    for request,(key,meaning) in zip(requests,keys.items()):
                        if request['key']!=list(key):
                            raise ValueError('Shared specialization key mismatch')
                        compile_record(label+f':{key[0]:08x}:{key[1]:08x}',request['source'].encode('latin1'),
                            entry=request['entry'],target=request['target'],key=list(key),candidate=meaning,
                            managerSelectsShader=request['managerSelectsShader'])
                elif path.name=='Bumpmap.rfx' and attributes.get('PIXEL_SHADER')=='FALSE':
                    for weights in range(5):
                        compile_record(label+f':weights{weights}',compose(args.source_probe,attributes,f'BlendWeightCount = {weights};\n'),
                            entry=attributes.get('ENTRY_POINT',''),target=attributes.get('TARGET',''),candidate=dict(weights=weights))
                else:
                    compile_record(label,compose(args.source_probe,attributes),assembly=node.tag=='RmShader',
                        entry=attributes.get('ENTRY_POINT',''),target=attributes.get('TARGET',''))
        else:
            raise ValueError(f'Unknown shader resource suffix {path.name}')
    names=sorted({row['name'] for p in programs if p.get('reflection') for row in p['reflection']['constants']}|
                 {name for p in programs for name,_ in p.get('declaredConstants',[])}|
                 {row['NAME'] for template in templates for row in template['constants'] if 'NAME' in row})
    parameter_types=call_source(args.source_probe,'--parameter-types',str(len(names))+'\n'+'\n'.join(map(hextext,names))+'\n')
    latest=None
    with (args.run/'shaders-client/audit.jsonl').open(encoding='utf-8') as f:
        for line in f:
            event=json.loads(line)
            if event['event']=='snapshot':latest=event
    if latest is None:
        raise ValueError('No completed audit snapshot')
    for original in originals:
        identity=int(re.search(r'(\d+)\.bin$',original['file'])[1])
        original['drawsThroughLastSnapshot']=sum(count for vs,ps,count in latest['totalDraws'] if identity in (vs,ps))
    summary=dict(resourceFiles=len(inputs),shaderRecords=len(templates)+sum(p['assembly'] and ':' not in p['label'] for p in programs),
        compiledCases=len(programs),compileFailures=sum(p['hresult']<0 or 'bytecodeSha256' not in p for p in programs),
        capturedShaders=len(originals),capturedMatched=sum(bool(p['matches']) for p in originals),
        capturedUnmatched=[p['file'] for p in originals if not p['matches']],
        elapsedSeconds=round(time.perf_counter()-started,3))
    report=dict(schema=1,scope='PC source and executable-instruction identity; no game rendering changes',
        compiler=dict(path=str(sdk.path),sha256=sha(sdk.path.read_bytes()),flags=0),
        sourceProbe=dict(path=str(args.source_probe.resolve()),sha256=sha(args.source_probe.read_bytes())),
        analyzerSha256=sha(Path(__file__).read_bytes()),sdkBoundarySha256=sha((ROOT/'research/pc_shader_sdk.py').read_bytes()),
        inputs=inputs,templates=templates,programs=programs,originals=originals,parameterTypes=parameter_types,summary=summary,
        limits=['Matching removes only disassembler comments, preserving every executable instruction and declaration.',
                'Candidate keys are not observed native keys; several keys can compile to the same program.',
                'Compiler creator metadata may differ; raw bytecode equality is reported separately.',
                'Synthetic branch coverage is separate from captured draw usage.',
                'No GPU image equivalence, full-game variant coverage or PS2 behavior is inferred.'])
    (output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(summary,ensure_ascii=False))
    return int(bool(summary['capturedUnmatched']) or summary['compileFailures']>0)


if __name__=='__main__':
    raise SystemExit(main())
