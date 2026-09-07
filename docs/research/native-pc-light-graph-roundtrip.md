# PC real light graph: index, save references and fresh native load

CP94,2026-09-07.9 exact captures/135 native assertions. Each unchanged pinned
SMO light from CP89 is read through4400B0/440640, then owns an actual Node child
at local(1,2,3) through421A60. Whole4672C0 invokes inherited4639D0/467300 and
indexes root then child. Whole467350 writes the root and nested child through
440110/463F10 and467260, followed by repeated root and null references.

The writer preserves the actual runtime class6B3E7BAA DXLight in the saved
header and FAT entry. It does not substitute original input class5E6402DF
LightData. Registry entries explicitly map runtime DXLight to the concrete
LightData serializer and Node to NodeSerializer. Their startup installation
is a fixture; no claim is made that all stock platform/export registrations
are identical. The eight-byte header plus serializer grammar is executable.

After original FAT entry cleanup, a declared two-entry directory is built
from the saved entries' actual ID/class/offset/size, then parsed by actual
466B90. The saved references are consumed unchanged through whole4678B0,
including actual specialized DXLight header creation, nested Node loading,
owning attachment and world propagation. The repeated root resolves to the
same newly loaded object; null consumes exactly four bytes. Both original
and fresh graphs are destroyed by native destructors, plus all helper owners.

Index14169 instructions;write11841..14560;fresh whole read69097..71333;
arena6864 bytes;written reference stream77..115 bytes including repeat/null.
Original EXE and100k/2s/64KiB/32KiB/30s limits unchanged. Captures compare
exact output, original FAT metadata, root scalar fields excluding opaqueDC,
root120 Node bytes, child120 Node bytes before save and after fresh load.
The uninitialized/nonserialized opaqueDC is deliberately outside comparison.

Reconstructed generic index/reference writer, concrete Light/Node writers,
FAT reader, dispatch and common reference reader all agree without production
changes. Test code supplies the same declared directory; it is not evidence
for native FAT directory output or full FFPS save orchestration. Round-trip
state is compared against the original's result, not assumed equal to input;
writer quantization/default omission and recalculated worlds remain observable.

Final report20260907T145957138341Z-pc-light-graph-roundtrip.json9/9.
Source LightSerializationTests201/201. The first local capture accidentally
treated the Node list sentinel as its first link; this observation bug was
fixed before exact comparison, with no engine patch or capped replay.
Cumulative CP51..94:345 exact/10718 native assertions, plus38 native-only CP88.
