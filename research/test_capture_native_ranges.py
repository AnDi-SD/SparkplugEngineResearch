"""Regression checks for byte preservation and PS2/PC capture boundaries."""
import struct
import unittest
from types import SimpleNamespace as S
from capture_native_ranges import decode, read_window


class CaptureTests(unittest.TestCase):
    def test_r5900_words_preserve_bytes_and_avoid_generic_dsp(self):
        words=[(0x1e<<26)|(29<<21)|(16<<16)|0xfff0,
               (0x1f<<26)|(29<<21)|(17<<16)|0x20,
               (0x12<<26)|0x12345,(0x1c<<26)|0x54321,0x03e00008,0]
        raw=struct.pack('<6I',*words)
        rows=decode('ps2',raw,0x1000)
        self.assertEqual(b''.join(bytes.fromhex(r['bytes']) for r in rows),raw)
        self.assertEqual(rows[0]['text'],'lq r16,-16(r29)')
        self.assertEqual(rows[1]['text'],'sq r17,32(r29)')
        for row in rows[2:4]:self.assertIn('not decoded as generic MIPS/DSP',row['text'])
        self.assertEqual([r['va'] for r in rows],list(range(0x1000,0x1018,4)))
        self.assertEqual(rows[4]['text'],'jr $ra')

    def test_ps2_file_backing_alignment_and_edges(self):
        sections=[S(section_type=1,address=0x1000,size=16,offset=8),
                  S(section_type=8,address=0x2000,size=16,offset=24)]
        data=bytes(range(32))
        self.assertEqual(read_window('ps2',data,0x100c,4,sections),(data[20:24],20))
        for address,size in [(0x1001,4),(0x1000,3),(0x100c,8),(0x2000,4),(0x1000,0)]:
            with self.assertRaises(ValueError):read_window('ps2',data,address,size,sections)
        with self.assertRaises(ValueError):read_window('ps2',data[:22],0x100c,4,sections)

    def test_pc_no_virtual_tail_or_cross_section(self):
        section=S(VirtualAddress=0x1000,SizeOfRawData=16)
        pe=S(OPTIONAL_HEADER=S(ImageBase=0x400000),
             get_section_by_rva=lambda r:section if 0x1000<=r<0x1020 else None,
             get_offset_from_rva=lambda r:8+r-0x1000)
        data=bytes(range(32))
        self.assertEqual(read_window('pc',data,0x40100c,4,pe),(data[20:24],20))
        for address,size in [(0x1000,4),(0x401010,1),(0x40100c,5),(0x402000,4)]:
            with self.assertRaises(ValueError):read_window('pc',data,address,size,pe)


if __name__=='__main__':unittest.main()
