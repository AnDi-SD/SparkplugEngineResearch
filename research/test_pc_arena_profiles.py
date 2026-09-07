"""Explicit heap tiers preserve hard bounds; no PE execution in these tests."""
import unittest
from pc_instruction_emulator import PcInstructions,ARENA_SIZE,INTEGRATION_ARENA_SIZE,HEAP

class ArenaProfiles(unittest.TestCase):
 def bare(self,size):
  p=PcInstructions.__new__(PcInstructions);p.arena_size=size;p.allocated=0;return p
 def test_micro_exact_limit_and_overflow(self):
  p=self.bare(ARENA_SIZE);self.assertEqual(p.allocate(ARENA_SIZE),HEAP)
  with self.assertRaises(ValueError):p.allocate(1)
  self.assertEqual(p.allocated,ARENA_SIZE)
 def test_integration_above_micro_still_bounded(self):
  p=self.bare(INTEGRATION_ARENA_SIZE);p.allocate(ARENA_SIZE+16);p.allocate(ARENA_SIZE-16)
  with self.assertRaises(ValueError):p.allocate(16)
  self.assertEqual(p.allocated,INTEGRATION_ARENA_SIZE)
 def test_arbitrary_tiers_rejected_before_image_access(self):
  for size in (0,4096,65537,262144,0x1000000):
   with self.assertRaises(ValueError):PcInstructions(arena_size=size)
 def test_invalid_allocation_does_not_consume_budget(self):
  p=self.bare(INTEGRATION_ARENA_SIZE)
  for size in (0,-1,INTEGRATION_ARENA_SIZE+1):
   with self.assertRaises(ValueError):p.allocate(size)
  self.assertEqual(p.allocated,0)

if __name__=='__main__':unittest.main()
