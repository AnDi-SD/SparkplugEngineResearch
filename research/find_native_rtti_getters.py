#!/usr/bin/env python3
"""Find exact constant RTTI getters and candidate table references, not ownership."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_native_ranges import ROOT,PC,PS2,EXPECTED,pefile,read_elf_sections


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('catalog',type=Path);ap.add_argument('output',type=Path);args=ap.parse_args()
    output=args.output.resolve();catalog=args.catalog.resolve()
    if not output.is_relative_to(ROOT/'local-data/results') or output.exists():raise ValueError('Fresh local output required')
    rows=json.loads(catalog.read_text());assert 0<len(rows)<=64
    raw={'pc':PC.read_bytes(),'ps2':PS2.read_bytes()};assert all(hashlib.sha256(raw[k]).hexdigest().upper()==EXPECTED[k] for k in raw)
    pe=pefile.PE(data=raw['pc'],fast_load=True);elf=read_elf_sections(raw['ps2'])
    def address(k,offset):
        if k=='pc':
            for s in pe.sections:
                if s.PointerToRawData<=offset<s.PointerToRawData+s.SizeOfRawData:return int(pe.OPTIONAL_HEADER.ImageBase)+s.VirtualAddress+offset-s.PointerToRawData
        else:
            for s in elf:
                if s.section_type!=8 and s.offset<=offset<s.offset+s.size and s.address:return s.address+offset-s.offset
        return None
    def find(k,pattern,alignment=1):
        at=0;result=[]
        while True:
            at=raw[k].find(pattern,at)
            if at<0:break
            a=address(k,at)
            if a is not None and a%alignment==0:result.append(dict(address=a,fileOffset=at))
            at+=1
        if len(result)>64:raise ValueError('More than64 candidate references; narrow the search')
        return result
    result=dict(kind='exact-native-rtti-getter-candidates',inputs=EXPECTED,sourceSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),catalogSha256=hashlib.sha256(catalog.read_bytes()).hexdigest().upper(),scope='Exact getter bytes and data word references only. A word reference is NOT a proven complete vtable or physical base. Call/constructor/adjustor review is still required.',classes=[])
    for row in rows:
        item=dict(className=row['className'])
        for k in raw:
            record=row[k+'Record']
            if not record:continue
            if k=='pc':pattern=b'\xb8'+struct.pack('<I',record)+b'\xc3'
            else:pattern=struct.pack('<3I',0x3c020000|((record+0x8000)>>16),0x03e00008,0x24420000|(record&0xffff))
            getters=find(k,pattern,4 if k=='ps2' else 1)
            for g in getters:
                g['references']=find(k,struct.pack('<I',g['address']),4)
                if k=='ps2':
                    aliases=find(k,struct.pack('<I',0x08000000|(g['address']>>2)),4)
                    g['adjustorAliases']=[]
                    for alias in aliases:
                        word=struct.unpack_from('<I',raw[k],alias['fileOffset']+4)[0]
                        if word>>16==0x2484:
                            alias['thisAdjustment']=struct.unpack('<h',struct.pack('<H',word&0xffff))[0];alias['references']=find(k,struct.pack('<I',alias['address']),4);g['adjustorAliases'].append(alias)
            item[k]=dict(registration=record,getters=getters)
        result['classes'].append(item)
    output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    for row in result['classes']:
        print(row['className'],{k:[dict(getter=f'{g["address"]:08X}',refs=[f'{r["address"]:08X}' for r in g['references']],adjustors=[f'{a["address"]:08X}' for a in g.get('adjustorAliases',[])]) for g in row[k]['getters']] for k in raw if k in row})


if __name__=='__main__':main()
