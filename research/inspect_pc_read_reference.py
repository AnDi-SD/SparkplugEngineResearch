#!/usr/bin/env python3
"""Fixed PC read-reference/resolve/FAT-lookup fingerprints."""
from pathlib import Path
from inspect_serializer_manager import read_pe,image_slice,sha256,PC_SHA256

BODIES=(
    (0x4678b0,0x79,'87CC2947B52A8C736822DF88473E9086DAC164C42658B800DE5F5A5F0B671246'),
    (0x467670,0x23f,'19E5B1C2310D277D4BAEA574A3AE1658EC237E1FA191B6499DEFF77CDFE977CF'),
    (0x4664c0,0x2f,'9A831AA3C056FADFAC8CFE75347FB7505216E0FBF47DB14DA67C16820DDF1E6B'),
)
def main():
    raw=(Path(__file__).resolve().parents[1]/'local-data/pc-pristine/WinxClub.exe').read_bytes()
    if sha256(raw)!=PC_SHA256:raise AssertionError('changed PC image')
    base,sections=read_pe(raw)
    for address,size,digest in BODIES:
        if sha256(image_slice(raw,sections,address-base,size))!=digest:raise AssertionError(hex(address))
    print('PASS 4/4: PC read-reference fingerprints');return 0
if __name__=='__main__':raise SystemExit(main())
