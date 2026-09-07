#!/usr/bin/env python3
"""PC payload/declaration exact-image anchors, including explicitly protected entries."""
from pathlib import Path
from inspect_serializer_manager import read_pe,image_slice,sha256,PC_SHA256
BODIES=(
 (0x429a40,0x179,'9442FD578116DC4760145BE5E8F9206E889240405A718F21267DA663A5EF7A93'),
 (0x4ae0e0,0x58,'7DC3E3162ADB970931DC7A19D07E9F861D4893093B7652A99F0BA33017C64FE2'),
 (0x4b23c0,0x6,'C7C4C51798C280FD6A13C4998D778067C30DA05E3A478A0EEF8175DACA310762'),
 (0x4c9a00,0x6,'DEB1CEF45DB795E7EDA3A899EA67ED9FA6FD8D0B4F938B47ECC17A360B383DBB'),
 (0x4c9c20,0x6,'6143DE7A699829E2F50DD3500AE368C1ECDE18CF16A5E27345F5C357B4F3069F'),
 (0x4c9d20,0x53,'20C4A320BE756984DD6B2DD7F4E803F1FFD6F0BE3B0AB3AD98BEC55D1518C233'),
 (0x4c9d00,0x1d,'3903B6CE68849387C79B46B3273ED227C77209461E0E52274747EE1438BE6BAE'),
 (0x4c99a0,0x5e,'8C33945BEC875743C72A33DBFD3B923F605605F351A7FFC325700AEAC6D87B48'),
 (0x6d5a30,0x26,'397296A9BCD6B2421727A32A467FD98AC31566C783E6F3B599454D168840EEAE'),
)
def main():
 raw=(Path(__file__).resolve().parents[1]/'local-data/pc-pristine/WinxClub.exe').read_bytes()
 if sha256(raw)!=PC_SHA256:raise AssertionError('changed PC image')
 base,sections=read_pe(raw)
 for address,size,digest in BODIES:
  if sha256(image_slice(raw,sections,address-base,size))!=digest:raise AssertionError(hex(address))
 if image_slice(raw,sections,0x6f2e7c-base,22).split(b'\0')[0]!=b'spPCVertexDeclaration':raise AssertionError('original class name')
 print('PASS 11/11: PC DX payload/declaration image anchors');return 0
if __name__=='__main__':raise SystemExit(main())
