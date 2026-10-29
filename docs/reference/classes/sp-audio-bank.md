# spAudioBank

`spAudioBank` has class ID `0x2C1225F5` and registered parent `spBaseObject`.
The PC concrete DX factory establishes the physical Base → AudioBank →
DXAudioBank constructor chain. The base registration has no factory. Original
header and translation-unit paths are unknown; the portable
[source](../../../Sparkplug/Code/Sparkplug/spAudioBank.h) covers its empty PC
storage, the two established own words and a separate captured owned-entry profile.

PC constructor `456CC0` calls common Base construction, installs table
`6EED30`, writes identifier10 =0 and raw word14 =5, initializes the pointer
vector and reserves300 entries through `49E8B0`. Field18 itself is unassigned.
The actual32-bit concrete object is40 bytes; vector begin/end/limit occupy
`1C/20/24`. Initially begin equals end and limit is1200 bytes beyond begin.
The identifier is read by the manager's bank-removal search. Its key1 path
also writes1 into a newly constructed concrete bank; the shared
[AudioManager operation](sp-audio-manager.md) represents this inline producer
through narrow private friend access. The meaning of
word14 remains unknown; its name is analytical.

Destructor `49E730` installs the base bank table and walks active pointers
forward from begin1C to end20. It invokes each nonnull entry's virtual deleting
destructor with flag1 and reloads end after every callee. It then frees storage,
clears1C/20/24 and performs Base teardown; it does not swap-last in this loop.
The portable slice admits empty storage and a separate stable supplied graph
of at most8 unique final [DXAudioBankEntry](sp-dx-audio-bank-entry.md) owners.
These entries keep the captured nonzero-unassignedE0/null-resource profile;
any nonnull Names have live external owners until Bank teardown ends.
Source explicitly resets owners forward before ordinary vector storage release.
The final Entry type and retained Names exclude arbitrary destructor callbacks
and end mutation. This is our graph-input adapter, not native Add/Load.
Host pointer width and container bytes are separate from the
[native PC layout](../../../Sparkplug/Analysis/PC/spAudioBankAbi.h).

The concrete bank inherits literal Base Copy, which leaves the bank's own
identifier, raw word and storage unchanged. The explicit fixture setter
supplies captured existing data; it does not claim an original setter.

Other entry types, aliases/null active slots, growth, destruction reentry,
audio SDK handles, original Add/Load, populated manager Remove, allocator
failure/rollback, PS2 and whole-class completeness remain open. The captured
ownership constructor validates before moving caller owners; that refusal
policy is analytical. Raw native dead-object/vector guards are distinct from
portable live represented state and actual host release order. This lifetime
contract does not establish playback or hardware behavior.
