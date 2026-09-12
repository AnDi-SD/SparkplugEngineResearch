# xxHash

`xxhash.h` is the unmodified xxHash 0.8.3 single header by Yann Collet,
under its embedded BSD 2-Clause license.

Source: https://github.com/Cyan4973/xxHash/blob/v0.8.3/xxhash.h

SHA256: `17973c0dc49d9854ca26caa191f0e12f7a424b68858d9a78de3860d959d85e4b`.

The adapter uses XXH3 to address the texture currently bound by the game
through the stock Remix API. Texture hashes do not determine surface roles.

# Remix light conversion

`remix_light_conversion.h` is an explicitly adapted CPU subset of NVIDIA's
`rtx_light_utils.cpp`, pinned to dxvk-remix
`b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4`. The source file's MIT notice is
retained. The reference checkout is unchanged.

Changes: remove renderer/vector dependencies, pass the intensity gain explicitly,
use the upstream default least-squares mode and unlimited maximum intensity,
retain original arithmetic with explicit casts for MSVC warnings. Thresholds
come from `rtx_lights.h`. This is Remix's physical conversion policy, not a
reconstruction of Winx lighting. Unsupported/non-finite game inputs are rejected
by the caller before API submission.
