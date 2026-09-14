"""Prepare bounded Fixed Color4 program contracts from the shared source helper.

The source digest identifies the proven shader implementation, not any model.
Original/compiled shader bytes remain in a fresh ignored runtime directory.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import xml.etree.ElementTree as ET
from pc_shader_sdk import D3dx

ROOT=Path(__file__).resolve().parents[2]
SOURCE_SHA256='ac6785428ba851dea88e3a6cddc03fdf620fa06d82063db83176598140803db1'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-helper',required=True,type=Path)
    parser.add_argument('--fixed',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    target=args.output.resolve()
    if not target.is_relative_to(ROOT/'local-data') or target.exists():
        raise ValueError('Use a fresh output directory inside local-data')
    source=args.fixed.read_bytes()
    if len(source)>65536 or hashlib.sha256(source).hexdigest()!=SOURCE_SHA256:
        raise ValueError('Fixed shader implementation requires separate qualification')
    records=[e.attrib for e in ET.fromstring(source).iter() if 'CODE' in e.attrib and e.attrib.get('PIXEL_SHADER')=='FALSE']
    if len(records)!=1 or records[0].get('TARGET')!='vs_1_1' or records[0].get('ENTRY_POINT')!='Main':
        raise ValueError('Unexpected Fixed vertex program')
    helper=args.source_helper.resolve();manifest=helper.with_name('build.json')
    build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    if hashlib.sha256(helper.read_bytes()).hexdigest().upper()!=build['exeSha256']:
        raise ValueError('Source helper differs from its build manifest')
    for item in build['sources']:
        expected=item['sha256'].lower()
        if hashlib.sha256((ROOT/item['path']).read_bytes()).hexdigest()!=expected:
            raise ValueError('Rebuild the shared source helper for current source')
    target.mkdir(parents=True);record=records[0]
    code=target/'code.source';declaration=target/'declaration.source'
    code.write_bytes(record['CODE'].encode('latin1'));declaration.write_bytes(record.get('DECLARATION_BLOCK','').encode('latin1'))
    sdk=D3dx('d3dx9_24.dll');entries=[];payload=bytearray(struct.pack('<III',0x31465357,1,16)+bytes.fromhex(SOURCE_SHA256))
    for lights in range(4):
        for bones in range(1,5):
            key=(bones|0x10|0x40000|(lights<<20),0)
            process=subprocess.run([str(helper),str(key[0]),str(key[1]),str(code),str(declaration)],capture_output=True,timeout=15,check=True)
            generated=process.stdout
            hr,bytecode,messages,reflection=sdk.compile(generated,record['ENTRY_POINT'],record['TARGET'],flags=0)
            if hr<0 or not bytecode or len(bytecode)>16384 or not reflection:
                raise RuntimeError(f'Fixed compilation failed for {key}: {hr} {messages}')
            stem=f'b{bones}-directional{lights}'
            (target/f'{stem}.source').write_bytes(generated);(target/f'{stem}.bin').write_bytes(bytecode)
            payload+=struct.pack('<III',*key,len(bytecode))+bytecode
            entries.append({'key':key,'file':stem+'.bin','sourceSha256':hashlib.sha256(generated).hexdigest(),'bytecodeSha256':hashlib.sha256(bytecode).hexdigest(),'reflection':reflection})
    catalog=target/'contracts.wsf';catalog.write_bytes(payload)
    result={'schema':1,'status':'PREPARED','entries':entries,'sourceSha256':SOURCE_SHA256,'sourceHelper':str(helper),'sourceHelperSha256':hashlib.sha256(helper.read_bytes()).hexdigest(),'sdk':str(sdk.path),'sdkSha256':hashlib.sha256(sdk.path.read_bytes()).hexdigest(),'compilerFlags':0,'catalogSha256':hashlib.sha256(payload).hexdigest(),'scope':'B1-B4, Color4, UV0, 0-3 directional lights, no specular; runtime must match instructions and complete reflected register metadata'}
    (target/'manifest.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'status':'PREPARED','programs':len(entries),'catalog':str(catalog),'bytes':len(payload)}))


if __name__=='__main__':
    main()
