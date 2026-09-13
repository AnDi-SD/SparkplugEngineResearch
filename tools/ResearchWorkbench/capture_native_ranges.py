#!/usr/bin/env python3
"""Capture explicit bounded PC/PS2 windows; linear disassembly is not a CFG proof."""
from pathlib import Path
import argparse, hashlib, json, struct, sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'local-data/research-cache/python'))
import capstone
import pefile
from inspect_executable_architecture import read_elf_sections

PC=ROOT/'local-data/pc-pristine/WinxClub.exe'
PS2=ROOT/'local-data/Winx Club the game PS2/SLES_532.19'
EXPECTED={'pc':'3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F',
          'ps2':'198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE'}

def digest(data):return hashlib.sha256(data).hexdigest().upper()

def read_window(platform,data,address,size,container):
    if not 0<size<=0x10000:raise ValueError('Each explicit window must contain1..65536 bytes')
    if platform=='pc':
        pe=container;base=int(pe.OPTIONAL_HEADER.ImageBase)
        if address<base:raise ValueError('Expected PC virtual address')
        rva=address-base
        section=pe.get_section_by_rva(rva)
        if section is None or rva+size>section.VirtualAddress+section.SizeOfRawData:
            raise ValueError('PC window must remain inside one file-backed section')
        offset=pe.get_offset_from_rva(rva)
    else:
        if address%4 or size%4:raise ValueError('PS2 words require four-byte alignment')
        section=next((s for s in container if s.section_type!=8 and s.address<=address and address+size<=s.address+s.size),None)
        if section is None:raise ValueError('PS2 window must remain inside one file-backed section')
        offset=section.offset+address-section.address
    raw=data[offset:offset+size]
    if len(raw)!=size:raise ValueError('Truncated file window')
    return raw,offset

def decode(platform,raw,address):
    if platform=='pc':
        md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
        md.skipdata=True
        return [dict(va=i.address,bytes=i.bytes.hex(),text=(i.mnemonic+' '+i.op_str).strip()) for i in md.disasm(raw,address)]
    md=capstone.Cs(capstone.CS_ARCH_MIPS,capstone.CS_MODE_MIPS64|capstone.CS_MODE_LITTLE_ENDIAN)
    result=[]
    for offset in range(0,len(raw),4):
        b=raw[offset:offset+4];word=int.from_bytes(b,'little');opcode=word>>26
        if opcode in (0x1e,0x1f):
            text=('lq' if opcode==0x1e else 'sq')+' r%d,%d(r%d)'%((word>>16)&31,struct.unpack('<h',b[:2])[0],(word>>21)&31)
        elif opcode in (0x12,0x1c):text=f'.word 0x{word:08X} # R5900 COP2/MMI; not decoded as generic MIPS/DSP'
        else:
            ins=list(md.disasm(b,address+offset))
            text=(ins[0].mnemonic+' '+ins[0].op_str).strip() if ins else f'.word 0x{word:08X} # undecoded'
        result.append(dict(va=address+offset,bytes=b.hex(),text=text))
    return result

def capture(spec,output):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'local-data/results'):raise ValueError('Raw captures belong inside local-data/results')
    ranges=spec['ranges']
    if not ranges or len(ranges)>128:raise ValueError('Explicit1..128 windows required')
    if sum(int(r['size'],0) if isinstance(r['size'],str) else r['size'] for r in ranges)>0x100000:
        raise ValueError('One capture is bounded to1MiB of windows')
    loaded={};reports=[];text=[]
    for item in ranges:
        platform=item['platform']
        if platform not in EXPECTED:raise ValueError('Explicit pc or ps2 required')
        if platform not in loaded:
            path=PC if platform=='pc' else PS2;data=path.read_bytes()
            if digest(data)!=EXPECTED[platform]:raise ValueError('Refusing non-pristine '+platform)
            loaded[platform]=(data,pefile.PE(data=data,fast_load=True) if platform=='pc' else read_elf_sections(data))
        address=int(item['address'],0) if isinstance(item['address'],str) else item['address']
        size=int(item['size'],0) if isinstance(item['size'],str) else item['size']
        raw,file_offset=read_window(platform,loaded[platform][0],address,size,loaded[platform][1])
        rows=decode(platform,raw,address)
        name=item['name']
        if not name or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_' for c in name):
            raise ValueError('Simple explicit range name required')
        reports.append(dict(name=name,platform=platform,address=address,size=size,fileOffset=file_offset,
                            sha256=digest(raw),bytes=raw.hex(),instructions=rows))
        text.append(f'{platform} {name} [{address:08X},{address+size:08X}) SHA256 {digest(raw)}')
        text.extend(f'{r["va"]:08X} {r["bytes"]:<30} {r["text"]}' for r in rows)
    result=dict(kind='bounded-native-linear-windows',scope='Explicit linear windows only; no reachability, whole-function or execution claim',
                inputs={p:EXPECTED[p] for p in loaded},spec=spec,scriptSha256=digest(Path(__file__).read_bytes()),ranges=reports)
    output.mkdir(parents=True,exist_ok=True)
    (output/'capture.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    (output/'capture.txt').write_text('\n'.join(text)+'\n',encoding='utf-8')
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('spec',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args();result=capture(json.loads(args.spec.read_text(encoding='utf-8-sig')),args.output)
    print(json.dumps(dict(windows=len(result['ranges']),bytes=sum(r['size'] for r in result['ranges']),
                         platforms=list(result['inputs']),output=str(args.output)),indent=2))

if __name__=='__main__':main()
