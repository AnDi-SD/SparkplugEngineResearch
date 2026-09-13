"""Synthetic register markers shared by workbench regression tests."""

UPPER = tuple(0 if i == 0 else 0xACE0000000000000 + i * 0x101010101 for i in range(32))
