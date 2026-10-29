# spCinematic

PC `spCinematic` physically constructs the Named prefix and registers directly
under BaseObject. Its class ID is `0x772A0D41`; an engine RTTI query for Named is
false despite physical Name storage. The portable class preserves both facts.

The represented constructor creates a null Name, zero field14 and an empty
owned-record list. Native offset18 is unassigned and remains unknown; the
portable class omits it. The original PC allocation is36bytes with a12byte list
sentinel, independently of the host C++ size and storage. Empty destruction
releases the sentinel then the shared Name/Base prefix. The portable lifetime
uses the existing shared Named implementation and has no native sentinel ABI.

Only empty-record lifetime is supported. The original record type, producer,
nonempty cleanup, loading and playback remain unknown. Copy and Clone explicitly
refuse in the portable profile before effects; this analytical refusal is not
the original method's failure result. No implicit RTTI factory is registered.

The [captured CinematicManager profile](sp-cinematic-manager.md) borrows these
final objects from a separate owner context. Nonnull retained plain Names are
required there. The original headers, PS2 behavior, game integration and whole
class are not reconstructed.
