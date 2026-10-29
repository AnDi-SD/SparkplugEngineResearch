# spDXAudioBank

`spDXAudioBank` has class ID `0x32A624E0` and physical/registered parent
[spAudioBank](sp-audio-bank.md). Its PC factory `4C7450` creates40 bytes and
executes the Base → AudioBank → DXAudioBank chain; final table is `6F2B00`.
Getter `4C7430` returns registration record `764BF0`. Original declaration
paths are unknown. The portable
[source](../../../Sparkplug/Code/SparkplugDX/spDXAudioBank.h) reuses the common
bank implementation and adds concrete RTTI, the empty factory/Clone and a separate captured ownership constructor.

The concrete constructor adds no established own words after the common
bank initialization. Primary Copy slot0C resolves to literal Base `40ECE0`:
distinct and self Copy preserve existing bank words and ownership storage.
Clone `4C74C0` constructs a new concrete bank, registers the source/destination
pair in the common CloneManager and invokes that inherited Copy. The new
bank therefore keeps constructor defaults0/5 and a separate empty reserved
buffer even when the source words differ. It does not copy a bank's own data.

Destructor body `4C7440` installs the DX table then delegates to common bank
teardown. Deleting wrapper `4C7510` tests only the low flag bit: the reserved
buffer is released in both modes, while the object allocation is released
only for the deleting mode. The portable default destructor and explicit
caller storage ownership represent those effects; native post-destruction
vtable/padding bytes are not a portable object-lifetime contract.

The existing default constructor/RTTI factory remains empty. A separate
`CapturedOwnedEntriesProfileForAnalysis::StableUniqueCapturedEntries` constructor
accepts an rvalue vector of unique final DXEntry owners, validates the bounded
profile before moving any pointers, and reuses the existing300-slot storage.
This is an analytical captured graph producer, not a recovered original
Add/loader. It refuses nonempty replacement, more than8, null/duplicate pointers
or incompatible Entry profiles. Nonnull Entry Names retain live external owners.
Every admitted Entry is deleted in forward logical order, followed by storage
and optional Bank allocation release. Flag0 retains only the Bank storage for
an explicit caller free, never a second destructor. Portable code does not read
dead objects or copy native stale vector cells.

Populated Clone remains an explicit analytical refusal until independently
qualified; empty Clone is unchanged. The existing manager's empty-bank profile
does not acquire populated RemoveBank support. Original Add/Load, arbitrary
entry types/reentry/aliases/growth, name-pool final-owner cleanup, native global
CloneManager lifetime, SDK/hardware/playback, allocation failure, PS2 and full
class recovery remain unqualified.
