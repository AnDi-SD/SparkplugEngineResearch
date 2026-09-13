# PC shader parameter production boundary

This proves the32-byte field is the name storage, not just an interpretation
from the consumer. Tail bytes after NUL are copied unchanged. Existing input
type is ignored; case-sensitive unknown/empty/suffixed names become0 and
**are still appended**. Start register is unchanged, overlap/gaps accepted.
Total+34 is the **sum of counts**, not max(start+count); nativeuint32 overflow
wraps. Neither count0 nor unknown type is filtered from storage/accounting.

Source `AppendParameterForAnalysis` uses the shared existing name resolver,
preserves raw name/start/count, appends, then updates scalarWords[8]. It refuses
no-NUL-within32 as an explicit host boundary; native has no such checked-length
guard. std::vector storage is portable, not an original growth/allocator ABI.
Wrapping/counts tested only at storage boundary, never used for unsafe writes.

4CFFE0 has surviving path at6F3A18:
`Z:\Sparkplug\Code\SparkplugPC\spPCEffectTemplate.cpp`.
Its assembly branch copies44-byte template records at+78..7C and calls
4AF940 at4D0140. Its other branch calls611324, then enumerates a returned
constant-table interface (+14 description,+20 indexed lookup,+18 descriptor).
It copies each name, start/count to a44-byte record and calls4AF940 at4D04A0.
Name copy4D0460..4D0468 itself is unbounded; no safe32 limit proved upstream.
611241/611324 compiler/library implementation and provenance are not replaced
or declared complete. Output code buffer allocation/copy at4D04C0..4D051E
sets shader48 size and4C pointer; repeated/error ownership remains open.

PC DXShader60→65. The append producer is not the complete RFX/compiler
pipeline. See [scope correction](shader-template-frontier.md) for
registered dependencies missing from the previous SMO/SAN work envelope.
PS2 and all seven completion gates remain unchanged.
