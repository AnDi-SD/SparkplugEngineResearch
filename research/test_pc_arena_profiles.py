"""Explicit heap tiers preserve hard bounds; no PE execution in these tests."""
import unittest
from pc_instruction_emulator import PcInstructions,ARENA_SIZE,INTEGRATION_ARENA_SIZE,CHARACTER_ARENA_SIZE,HEAP,RETURN,execution_limits

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
  for size in (0,4096,65537,262145,0x1000000):
   with self.assertRaises(ValueError):PcInstructions(arena_size=size)
 def test_invalid_allocation_does_not_consume_budget(self):
  p=self.bare(INTEGRATION_ARENA_SIZE)
  for size in (0,-1,INTEGRATION_ARENA_SIZE+1):
   with self.assertRaises(ValueError):p.allocate(size)
  self.assertEqual(p.allocated,0)
 def test_character_allocation_bound_and_mapping_separation(self):
  p=self.bare(CHARACTER_ARENA_SIZE);p.allocate(CHARACTER_ARENA_SIZE-16);p.allocate(16)
  with self.assertRaises(ValueError):p.allocate(1)
  self.assertEqual(p.allocated,CHARACTER_ARENA_SIZE)
  self.assertLess(HEAP+CHARACTER_ARENA_SIZE,RETURN)
 def test_explicit_character_limits_preserve_earlier_profiles(self):
  self.assertEqual(execution_limits('micro'),(100000,2000000))
  self.assertEqual(execution_limits('file'),(1000000,8000000))
  self.assertEqual(execution_limits('character'),(4000000,16000000))
  with self.assertRaises(ValueError):execution_limits('unlimited')
 def test_explicit_protected_constructor_profile(self):
  self.assertEqual(execution_limits('protected-constructor'),(6000000,24000000))
  self.assertEqual(execution_limits('micro'),(100000,2000000))
  self.assertEqual(execution_limits('file'),(1000000,8000000))
  self.assertEqual(execution_limits('character'),(4000000,16000000))

if __name__=='__main__':unittest.main()
