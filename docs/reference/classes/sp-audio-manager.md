# spAudioManager: PC bank removal

`spAudioManager` has class ID `0x65B74130` and registered parent
`spBaseObject`. Actual PC DX construction establishes the physical primary
Base → AudioManager → DXAudioManager chain. The portable
[source](../../../Sparkplug/Code/Sparkplug/spAudioManager.h) reconstructs the
bank-removal operation over an explicit captured registry. Original declaration
paths and the exact AudioManager constructor entry remain unknown.

PC primary slot `38` resolves to `48F9B0`. The registry stores begin/end/limit
at `84/88/8C`; the established default capacity is20 pointers. The method
searches active entries in order and compares the bank's raw identifier10.
It removes only the first equal identifier. Different banks may share an ID.
The actual complete DX allocation is344 bytes; this is not a recovered
standalone AudioManager base size or the portable C++ layout.

On a match the method calls the bank's deleting destructor, reloads the vector,
copies its last active pointer into the removed slot and reduces end by4.
Survivors therefore retain their state but their order can change. No match
is also successful. For key1, after that search/removal the method always
constructs a concrete DX bank, assigns identifier10=1 and appends it. This also
happens when no old key1 exists. The new bank retains its own constructor
word14=5 and separate empty300-slot storage. The two normal return sites write
AL1; the original C++ return declaration and the high bits of EAX are unknown.
The portable bool reports analytical completion and compares only that low-byte
normal result, registry state and ownership effects.

The source uses the real shared [DX bank](sp-dx-audio-bank.md) constructor and
destructor; the key1 producer does not call a fixture setter or a callback
that supplies a successful bank. A private friend store represents the actual
inline identifier assignment. Source input is an explicit
[captured PC registry](../../../Sparkplug/Analysis/Host/spAudioManagerHost.h):
the original profile requires exclusive synchronous ownership of0..8 concrete empty DX banks, inactive/null
backend, no allocator/destructor callback or reentry. Result count must also
remain at most8. The host vector reserves20 slots independently of the input
vector capacity. Null entries and unsupported input/profile bounds refuse
before ownership transfer or operation effects. A key1 miss at count8 refuses
before allocation because the ninth result is outside this bounded domain.
These are analytical guards, not native failure returns.

A separate `StableOwnedCapturedEntriesInactiveNullDevice` profile permits1..2
final DX banks with1..8 total accepted final DX Entries and result count<=2.
Some banks may be empty while the total is nonzero. Each populated bank comes
from the [typed captured Bank constructor](sp-dx-audio-bank.md), with the
established null-resource Entry profile. Nonnull plain shared Names retain
external owners throughout removal and destruction. This is supplied graph
input, not an original Add/Load producer or a source audio backend.

The complete input batch is checked before moving owners: bank identity must
be nonnull/unique and both count limits must hold. The same first-match,
delete-before-swap-last and direct key1 factory algorithm then runs. Actual
Entry virtual deletion proceeds forward inside the removed bank, followed by
its buffer and bank allocation. Unremoved banks and their Entries retain
identity and state; shared retained Names may span both banks. Key1 still
creates an empty default bank. A key1 miss at count2 refuses before effects.
If a successful operation leaves total0, queries and ordinary owner cleanup
remain valid but another Remove under this populated profile refuses before
effects. These source-domain refusals do not describe native empty-registry
failure. The original empty profile remains available independently.

Native Name reference bytes and end reloads are distinct from portable live
pointer/name identity and real host release witnesses. Source observation
does not read destroyed objects or require heap-address reuse behavior; a new
default bank may reuse a deleted bank's storage. Supporting source manager
cleanup and final external Name release do not qualify native global manager
or final Name-pool lifetime.

Native addresses, inactive spare cells, freed-object vptr writes and the
80-byte32-bit pointer buffer remain native-only observations. The source uses
host-width `unique_ptr` storage and compares active order/raw bank data and
actual buffer/object release events separately. It does not claim matching
heap addresses, storage bytes or allocation-reuse behavior. Native bank
destructor reentry and arbitrary derived bank implementations are excluded;
the admitted portable bank type is final.

If host allocation throws after a bank has already been deleted, the captured
source context remains poisoned and refuses retries. The caller must discard
it. This host safeguard does not qualify original allocation failures,
rollback or exception handling.

The captured owner and its C++ cleanup are our adapter. Whole original manager
construction, global singleton and teardown, Clone, other manager containers,
SDK/device/playback, growth paths, other populated resource/Entry shapes, PS2 and whole-class recovery
remain open. Portable default factory and Clone explicitly remain unavailable.
