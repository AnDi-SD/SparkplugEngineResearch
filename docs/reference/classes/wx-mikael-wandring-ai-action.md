# wxMikaelWandringAIAction

`wxMikaelWandringAIAction` has class ID `12487A8E`, PC allocation size `3E8`,
factory `5C95B0`, constructor `5C90D0`, table `70A608` and getter `5C8F00`
returning record `767AF0`. The factory passes its allocation unchanged to
the constructor, which calls `58FF00` on the same receiver before storing
its own table. This establishes the physical [`wxAIAction`](wx-ai-action.md)
prefix independently of registration. The [PC layout](../../../Winx/Analysis/PC/wxMikaelWandringAIActionAbi.h)
is separate from the portable C++ object.

The [source](../../../Winx/Code/wxMikaelWandringAIAction.h) admits a captured
PC Entry and its real self-slot48 body. Native command pointer `3A4` occupies
the parent prefix; it is not an appended own member. Its producer is unknown:
the captured constructor leaves this cell uninitialized. Own Node pointers
`3B0` and `3B4` start null and require explicit live borrowed bindings. The
unknown eight bytes `3A8..3AF` and 48 bytes `3B8..3E7` remain raw data.

PC primary slot28 selects Entry `5C8E10`, a complete 47-byte body ending in
RET4. It first calls the actual self-slot48 target `5C8F50`, then reloads
command3A4 and writes command byte1B=1. Only afterward does it capture owner20,
read owner1C0, copy that value to owner1BC and store literal1 to owner1C0.
The unused argument and raw return register do not define a portable Boolean
contract. Repeated calls perform the full sequence again.

Self-slot48 spans 200 bytes through `5C9017`, including both RET0 tails. It
copies target Node3B4 cached WORLD position, source Node3B0 WORLD position and
the source's raw WORLD orientation row2. It uses neither LOCAL orientation
nor the scaled WORLD matrix. The shared `597140` helper obtains current yaw
directly from this forward vector without normalizing it. The `5970B0` helper
computes target minus source, spills the X/Z differences to float, ignores Y,
normalizes via `41D2D0` and computes desired yaw. Both yaws are spilled to float.

The `5971A0` helper wraps argument copies and returns their folded absolute
angular distance without a float spill. For finite distance `<= 0.08f`, the
body writes motion word04=`3F800000` (1.0f); otherwise it writes `3D4CCCCD`
(0.05f). It publishes the saved original desired yaw to command word08 before
reloading command3A4 for the motion store. Wrapping argument copies cannot
change the published yaw: a desired `-PCpi` remains negative in word08.

These primitives live in the shared [planar math adapter](../../../Winx/Analysis/PC/wxPlanarAngleMath.h).
The existing normalized `PcPlanarFacingForAnalysis` retains its strict `<`
comparison; Mikael composes direct vector yaw and an inclusive comparison.
The finite source domain bounds X/Z inputs to absolute1024. Original engine
normalization, yaw, wrapping and distance instructions execute unchanged in
the bounded native comparison. Only external `MSVCR71!_CIacos` uses an explicit
host `acos` replacement under the declared default x87 profile. Portable
normalization uses the existing finite host adapter. This is conditional
numeric evidence, not original CRT, arbitrary extended precision or physical
CPU parity.

Source uses real shared [`spNode`](sp-node.md) WORLD caches and the shared
[captured command record](../../game/ai/ai-command-reset.md). Its narrow word08,
word04 and byte1B storage APIs are analytical field operations, not newly
attributed native methods. The chosen116-byte command record has table tag
`703090`; this does not establish Mikael's real command producer or a native
command class. The [host](../../../Winx/Analysis/Host/wxMikaelWandringAIActionHost.h)
only supplies live owner word cells and never returns a heading, facing or
movement result.

The default profile refuses. The admitted profile requires an empty inherited
action graph, finite prepared Node caches and live, stable, disjoint borrowed
Node, command and owner storage. Missing later storage causes an analytical
refusal after the completed store prefix; it does not model native faults or
rollback. Factory, Copy, Clone, Notify and other hooks refuse. Action teardown
preserves borrowed records. Full Init/search, construction and owning lifetime,
unknown encoded pointers, aliasing, reentry, concurrency, arbitrary hook
composition, PS2, runtime and gameplay remain open.

The [tests](../../../Winx/Tests/wxMikaelWandringAIActionTests.cpp) compare logical
borrowed identities, the56-byte raw own payload, parent duration/clearable
words, all116 command bytes, both owner cells and the120 represented transform
bytes of each real portable Node. The full native1000-byte action and180-byte
Node guards are independent evidence and do not describe portable object ABI.

PC primary slot4C, cell `70A654`, selects the complete172-byte body at
`5C9020` through `5C90CB`, including both RET0 tails. It uses source Node3B0
cached WORLD position and raw unscaled orientation exactly as the self48
calculation does. Its desired-position argument instead points directly to
own raw cell3B8: the position-yaw helper reads X at3B8 and Z at3C0 and ignores
Y. This establishes a stored-point consumer; the point's Init/assignment
producer remains unknown. Target Node3B4 and owner20 are not read, and command
byte1B is preserved. The only stores publish the original desired yaw to08,
then reload command3A4 and write the same inclusive `<= 0.08f` motion choice
to04. Raw EAX on RET0 contains desired-yaw float bits, without a Boolean claim.

`OwnSlot4CForAnalysis` has an independent per-call profile which defaults to
Unqualified. Admitting Entry/self48 does not admit4C, and admitting4C does not
admit Entry. This source operation reads the finite point from the existing
raw3B8 payload, uses the real Node caches and shared math/storage APIs, and
preserves all action/Node/unused target/owner data and the remaining command
bytes. It needs no additional host operation, shared math or record API.
The same finite/default-x87/host-acos qualification applies. Null or opposite
position target3B4 decoys and repeated calls are covered independently; arbitrary
Entry+4C composition, point producer and owning lifetime remain open. No
callable beyond4C is inferred from the following ASCII `gate` bytes.
