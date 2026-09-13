# PC Fog codec, Model relationship and lifetime

## Native stale pointer and blank clone

Clearing the sole owned Fog deletes it, while native FAT entry.object still
contains its freed address. This is observed via allocation/lifetime records
and the FAT entry. **No dereference/reload of the stale address executed.**
Host context deliberately pins canonical shared owners until teardown; it does
not treat the freed pointer as a valid owning reference.

## Source, tests and remaining boundary

Concrete `spFogSerializer` read/write/index uses common core and analytical
SectionCursor. Exact20byte envelope validation rejects truncation **before
mutation**, deliberately stricter than native per-word updates. Raw scalar
bits/enum are not clamped. Unknowns skip, not yet lossless.

Remaining: reentry/allocation/error/cache-reuse/cleanup variants, original
paths/API names/native NameManager lifetime, unknown lossless and complete scene/backend.
Nonempty Material/mesh, Texture and Skin are next neighboring dependencies.
