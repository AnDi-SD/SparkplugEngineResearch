#!/usr/bin/env python3
"""PC outer materialization and DX preparation gate/body fingerprints."""
from pathlib import Path
from inspect_serializer_manager import read_pe,image_slice,sha256,PC_SHA256
BODIES=(
    (0x422940,0x205,'EA2A22E6B33926F40F3B9A498932A7915672F0F82ED201D386FA2BFDA5E59287'),
    (0x4aab80,0x3c,'9FD1CF12A59F7EB0AC5F91279596D117F5009DD0911B966CDCBF565C35CB81BE'),
    (0x4aa870,0x310,'CE51665D0C9199061C7D63F720F2910058C9188C2F0BF0CB9FA347D37C6A461F'),
)
def main():
    raw=(Path(__file__).resolve().parents[1]/'local-data/pc-pristine/WinxClub.exe').read_bytes()
    if sha256(raw)!=PC_SHA256:raise AssertionError('changed PC image')
    base,sections=read_pe(raw)
    for address,size,digest in BODIES:
        if sha256(image_slice(raw,sections,address-base,size))!=digest:raise AssertionError(hex(address))
    print('PASS 4/4: PC outer loader/DX hook fingerprints');return 0
if __name__=='__main__':raise SystemExit(main())
