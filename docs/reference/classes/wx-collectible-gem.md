# wxCollectibleGem

`wxCollectibleGem` has class ID `27F540A1`. Its PC receiver has extent
`0x154`, primary table `6FCF20` and a physical `wxEntity` prefix.
Constructor `544B30` calls `4DA7C0` on the same receiver, then installs
that table. Getter `544BA0` returns registration `752C28`.

## PC predicate

Primary slot `2C` selects `544C90`. Byte `149` rejects immediately.
The remaining path calls inherited slot `28`, rereads the owner record's
flag word, optionally compares cached Node positions with squared radius,
and applies the `1A` fallback mask. The strict height gate and extended
arithmetic order are described in the [proximity contract](../../game/entities/collectible-proximity.md).

The [source facade](../../../Winx/Code/wxCollectibleGem.h) borrows an existing
`wxEntity`, the 48 bytes at PC `124..153`, the owner record flag word and two
`spNode` objects. It calls the shared `wxEntity::UpdateFlagsForAnalysis`
implementation. Its flag-word reference must name the storage written by
that Entity host's `WriteComputedFlagsForAnalysis` callback. Node world
positions come from `spNode::GetWorldPositionForAnalysis`.

The supplied common profile has byte `28` set, an empty predicate cache,
threshold `3C=7F7FFFFF` and stable borrowed records. The point argument
is unused under this threshold sentinel. Used coordinates and radius
must be finite with absolute values at most 16384. The geometry kernel
reuses the shared PC nearest/64 arithmetic and comparison operations.

Own bytes and represented Node state stay unchanged. The common call
updates the owner record flags. Early disable preserves that word and
skips the common call and Node accesses. The [driver](../../../Winx/Tests/wxCollectibleGemTests.cpp)
has a normal unit-check entry and a raw-state replay entry.

Unsupported-input exceptions belong to the analytical facade contract.
Warm/sentinel rejection precedes the common update. Used-geometry rejection
can follow that update and retain its flag word. Native construction,
ownership and lifetime, Clone/Copy, cold or nonempty caches, arbitrary
threshold paths, changed bindings, pickup/award delivery, PS2 source,
gameplay and whole-class completeness remain open.
