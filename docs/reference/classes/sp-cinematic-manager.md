# spCinematicManager

PC `spCinematicManager` has class ID `0x48E66610` and a physical primary Base
prefix. Its separate secondary interface is not identified. The portable class
is an explicit captured-registry profile, with no original default constructor,
factory, global instance, embedded Timer or matrix behavior.

The selected operation scans the borrowed list in order for exact bytewise Name
equality. A match invokes the Cinematic deleting destructor before unlinking the
node and decrementing the count. It removes only the first matching Name and
preserves survivor order. Native AL is1 for a hit and0 for a miss; other EAX bits
are not a Boolean result. Whole manager destruction frees list nodes and its
sentinel without deleting surviving Cinematic payloads.

The portable captured profile admits0..4 unique final empty
[Cinematics](sp-cinematic.md). A separate analytical owner context owns them;
the manager holds only a weak context binding and borrowed ordered slots. The
whole input is validated before ownership moves, and only one active manager
may bind a context. All payload Names must be nonnull retained plain ASCII,
with external Name owners alive. The host imposes a63byte admission bound;
this is not a native string limit. Query admission failures throw before effects
and remain distinct from an admitted native miss.

On a match, the source resets the selected real C++ owner before updating its
registry metadata. Manager destruction clears only borrowed metadata and its
binding. Context disposal is separate caller cleanup of survivors; the context
outlives the manager in the qualified profile. Expired-context rejection is a
host safeguard. Native list/sentinel frees and source object/control-block frees
are separate ownership evidence, without allocation-size or ABI equivalence.

Copy retains only the existing literal Base operation selected by the original
primary table. It does not copy the captured registry. Clone explicitly refuses
because original construction is unqualified. Registry insertion, Name loading,
callbacks/reentry, nonempty Cinematics, native allocation failure, original
Timer/global lifetime, other methods, PS2 and runtime remain open.
