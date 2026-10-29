# spDXAudioManager: captured PC bank registry

`spDXAudioManager` has class ID `0x116F7E16` and registered/physical primary
parent [spAudioManager](sp-audio-manager.md). Its original PC factory is
`4C4200`; allocation extent is `158` hex. Actual construction writes primary
tables Base `6DAEB8`, AudioManager `6EC620`, then DX `6F26D8` at the same
receiver. The common Base constructor receives that complete pointer. Getter
`4C3CE0` returns record `764890`; the AudioManager table's getter `48F3B0`
returns record `762790`.

The additional table word at offset10 is `6F26D0` after DX construction. Its
C++ interface declaration is unknown; it is recorded separately in the
[native ABI view](../../../Sparkplug/Analysis/PC/spAudioManagerBankAbi.h).
It does not establish portable multiple inheritance. A previously observed
helper address `48F3D0` receives several internal subobject pointers and is
not attributed as the AudioManager constructor.

The original default manager has an inactive byte14, null deviceF0 and a
20-slot bank vector containing one own default DX bank ID1. Those actual
factory/lifetime observations establish the receiver and native setup used
for the bounded bank-removal contract. They do not reconstruct a portable
complete constructor, device backend or global manager lifetime.

The portable [class](../../../Sparkplug/Code/SparkplugDX/spDXAudioManager.h)
requires an explicit captured registry profile and delegates its single
recovered operation to the common AudioManager source. It deliberately has
no portable RTTI factory without that input. Source Clone throws an explicit
analytical refusal; this is not an assertion that native Clone `4C4280` fails.
No caller pointer is cast into a foreign native object.

The original shared RemoveBank profile covers stable owned empty concrete DX
banks. A separate captured profile admits1..2 concrete banks with1..8 total
accepted final DX Entries, live retained external Names and result<=2. Both
use the same first-match deletion/swap-last and real key1 empty-bank
construction. The populated input is validated before ownership moves; it
reuses the accepted Bank/Entry implementation and changes no native layout,
factory, resource field or device binding. SDK calls,
other containers and voices, device activation/playback, allocation failure
parity, registry growth/reentry, original full construction/teardown, PS2 and
whole-class/game readiness remain unqualified.
