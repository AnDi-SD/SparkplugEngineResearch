#!/usr/bin/env python3
"""Fixed PC-only byte ranges for native SAN/nested-block writer evidence."""
from pathlib import Path
from inspect_serializer_manager import read_pe,image_slice,sha256,PC_SHA256

BODIES=(
    ('BeginObject',0x472710,0xf,'EE2AC4939D40C582E3FE7330F51F028D0B26E82412B47412145D512C8E18635C'),
    ('WriteBegin original site including protected bridge',0x472d30,0xeb,'D27A8B2D3617D19DC655904EF252BD1A97B54A3BCCBC9B464806670C5B9303EB'),
    ('WriteBegin resolved bridge tail',0x44eb66,0xd,'818284A9866DBBA30C7BEC31E1BAF761DB5260F1ACCA311BEE0FA6D7ED610722'),
    ('WriteEnd',0x472e20,0x1ac,'2078056B914FAB0D796A11F0E187491FC298729636501369F14D07B48630DE6E'),
    ('WriteHeader PC zero-size early return',0x4f5ad0,0x22,'6B319EF7770AB8A8A1323BB6B1373F4ABEFD58D649439378A9886A9878902CA2'),
    ('Animation field writer',0x43dfe0,0xcd5,'323163548DD2BA23C00E0A378695C8F631D0AC83F2EAF4A30B0408F8DE5B1D9E'),
    ('Animation key writer',0x43ddc0,0x210,'7771BC50FA4E616483B860A9D3D4396071E71DB5791E78B4F4808BA97016FB81'),
)


def main():
    raw=(Path(__file__).resolve().parents[1]/'local-data/pc-pristine/WinxClub.exe').read_bytes()
    if sha256(raw)!=PC_SHA256:raise AssertionError('changed PC executable')
    base,sections=read_pe(raw)
    for name,address,size,digest in BODIES:
        if sha256(image_slice(raw,sections,address-base,size))!=digest:raise AssertionError(name)
    print(f'PASS {len(BODIES)+1}/{len(BODIES)+1}: fixed original PC serializer writer fingerprints')
    return 0


if __name__=='__main__':raise SystemExit(main())
