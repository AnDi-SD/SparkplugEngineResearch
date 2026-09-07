#!/usr/bin/env python3
"""Read-only original PC spatial RTTI, vtables and render-support anchors."""
from pathlib import Path
import struct
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local-data/research-cache/python'))
from inspect_executable_architecture import scan_pe
from inspect_serializer_manager import read_pe,image_slice,sha256,PC_SHA256


def main():
    path = ROOT/'local-data/pc-pristine/WinxClub.exe'
    raw = path.read_bytes()
    base,sections = read_pe(raw)
    count = 0

    def check(value,label):
        nonlocal count
        count += 1
        if not value: raise AssertionError(label)

    def data(address,size): return image_slice(raw,sections,address-base,size)
    check(sha256(raw)==PC_SHA256,'pristine PC fingerprint')
    records={r['class_name']:r for r in scan_pe(path,0x12ff0)['registered_types']}
    for name,identity,parent,factory in (
            ('spPartitionNode',0x67672341,0x415352a1,0x426910),
            ('spPartitionSystem',0x912cc341,0x695c0f65,0x48e7c0),
            ('spZone',0x61254ab3,0x695c0f65,0x480fd0),
            ('spStaticRenderObject',0x56d67170,0x44de07fd,0x41a7c0),
            ('spPartitionRenderable',0x94bbca2a,0x415352a1,0x4cd950),
            ('spPCPartitionRenderable',0x9cbb56a2,0x94bbca2a,0x4cd950),
            ('spVisibilityManager',0x3d7f4387,0x415352a1,0x46d1c0),
            ('spDXShadowVolumeManager',0x04680bc1,0x63fea321,0x4a9030)):
        row=records[name]
        check((row['class_hash'],row['base_class_hash'],row['constructor_arg5_va']) ==
              (identity,parent,factory),'original identity/factory '+name)
    for table,expected in (
            (0x6dcb08,(0x426890,0x5b7a00,0x426970,0x40ece0,0x4264c0,0x408350,0x408370,
                       0x426670,0x425a80,0x425ae0,0x426690,0x425b60,0x425bc0,
                       0x4266b0,0x425c40,0x425ca0,0x425680,0x518dc0,0x518dc0,
                       0x425690,0x425690,0x4d6550,0x426740,0x5a7dc0,0x4258a0,
                       0x4256b0,0x4d6550,0x4256f0,0x425770,0x425730,0x425770,
                       0x425770,0x4269c0)),
            (0x6ebacc,(0x480fb0,0x420b40,0x481030,0x421f80,0x480f50,0x408350,0x408370,
                       0x420e30,0x420e60,0x4212f0,0x420610,0x421330,0x421420,0x421640)),
            (0x6ec528,(0x48e870,0x420b40,0x48e820,0x424980,0x48e690,0x408350,0x408370,
                       0x424760,0x4249f0,0x424af0,0x420610,0x421330,0x48e710,0x424e70)),
            (0x6ec510,(0x424b60,0x4248d0,0x424c30,0x48e7b0,0x424790,0x4247b0)),
            (0x6e65e8,(0x44fb80,0x5b7a00,0x41b150,0x413120,0x44fa50,0x408350,0x408370)),
            (0x6e6604,(0x44fc00,0x44fba0,0x44fb20,0x44fa80,0x44fa60,0x4f3df0)),
            (0x6f4540,(0x4d73d0,0x5b7a00,0x4cda10,0x40ece0,0x4d7390,0x408350,0x408370)),
            (0x6f4528,(0x4d72c0,0x4d7260,0x5a7db0,0x4d73c0,0x4d7180,0x4f3df0))):
        check(struct.unpack('<'+'I'*len(expected),data(table,4*len(expected)))==expected,
              f'complete bounded vtable{table:08X}')
    for address,target in ((0x426678,0x465470),(0x426698,0x425660),
                            (0x4266b8,0x481080),(0x425bac,0x424d60),
                            (0x425aCC,0x464f70),(0x425c8c,0x46dc80),
                            (0x6d38c7,0x461c10),(0x44fc4a,0x456310),
                            (0x4d730a,0x456310),(0x425a3c,0x4259e0)):
        code=data(address,5)
        check(code[0]==0xe8 and address+5+struct.unpack('<i',code[1:])[0]==target,
              f'original CALL{address:08X}->{target:08X}')
    check(data(0x44fc5a,7)==bytes.fromhex('FF52048B5C2410') and
          data(0x4d731a,7)==bytes.fromhex('FF52048B5C2410'),
          'both support draws proceed after matrix call without AL gate')
    check(data(0x44fc8b,4)==bytes.fromhex('84C07403') and
          data(0x4d734b,4)==bytes.fromhex('84C07403'),
          'both support draws stop immediately on model AL false')
    check(data(0x4259ee,6)==bytes.fromhex('899E80000000') and
          data(0x4259f6,6)==bytes.fromhex('899888000000'),
          'Scene propagation root80 -> actual payload88')
    print(f'PASS {count}/{count}: original PC partition/static runtime anchors')
    return 0


if __name__=='__main__':raise SystemExit(main())
