"""Allocation seam limits remain distinct from total guest arena bounds."""
import unittest
from unittest.mock import patch
from pc_instruction_emulator import PcInstructions, HEAP
from probe_pc_animation_lifecycle import LifetimeFixture


class AllocationProfiles(unittest.TestCase):
    def test_invalid_profile_rejected_before_guest_construction(self):
        class Invalid(LifetimeFixture):
            guest_max_allocation_size = 131072
        with patch('probe_pc_animation_lifecycle.PcInstructions') as create:
            with self.assertRaisesRegex(ValueError, 'allocation bound'): Invalid()
            create.assert_not_called()

    def test_request_and_total_limits_are_independent(self):
        # Exercise the real bump allocator and seam against a data-only memory
        # recorder. No PE, dependency import or original instructions required.
        class Guest:
            arena_size = 131072
            allocated = 0
            request = 57344
            allocate = PcInstructions.allocate
            def reg(self, name): return 0
            def uint(self, address): return self.request
            def mem_write(self, address, data): self.last_write = (address, len(data))
            def fixture_return(self, **kwargs): self.last_return = kwargs
        p = Guest(); p.mu = p
        f = LifetimeFixture.__new__(LifetimeFixture)
        f.allocations = {}; f.requests = []; f.max_allocation_size = 32768
        with self.assertRaisesRegex(AssertionError, 'bounded fixture'): f.allocate(p)
        self.assertEqual(p.allocated, 0)
        f.max_allocation_size = 65536
        f.allocate(p); self.assertEqual(p.last_write, (HEAP, 57344))
        self.assertEqual(f.requests, [(HEAP, 57344)])
        f.allocate(p); self.assertEqual(p.allocated, 114688)
        with self.assertRaisesRegex(ValueError, 'arena exhausted'): f.allocate(p)
        self.assertEqual(p.allocated, 114688)
        p.request = 65537
        with self.assertRaisesRegex(AssertionError, 'bounded fixture'): f.allocate(p)
        self.assertEqual(p.allocated, 114688)


if __name__ == '__main__': unittest.main()
