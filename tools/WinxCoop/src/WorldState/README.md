# WorldState1 bounded authority channel

This is WinxCoop-owned snapshot transport, separate from P/V3/IPC3 and Input5.
It uses the existing SessionCore/Packet/MemoryStream/WindowsTcpTransport. The
original256-byte packet capacity is unchanged. These sources do not simulate a
game, infer native pointer identities or perform native capture/application.

`WorldStateProtocol.h` defines snapshot schema2, independently of envelope/IPC
version1: header40, up to2 bodies and64 entities,100 bytes each, then one required
first SharedProfileV1 block411 and up to three optional blocks: EntryWitnessV1
64, EntrySeedV1 448 and StarPresentationV1. Schema1 snapshots and unknown/duplicate block types are
rejected. Each snapshot is an absolute replacement with nonzero increasing u64
revision and native capture tick. Publication may skip unpublished revisions;
already in-flight data never changes. Numeric pose limits are finite position
±1e7, scale nonzero with abs1e-6..1e4, rotation entries±2, velocity±1e6. Health is
0..maximum with positive maximum when HasHealth, or the pair-1/-1 otherwise.

The100-byte record order is stableId/typeId/stateId/animation/flags (five u32),
position3f/scale3f/rotation9f/velocity3f, hp/maxHP (two i32). Native scale is retained.
IDs are server-assigned or stable authored-path IDs, never native addresses.
Producer and native consumer must pin each ID/type to their bounded scene cohort;
this codec does not manufacture a generic native entity lifecycle. Animation is
0Idle/1Walk/2Jump/3Ordinary/4Dying/5Other; native stateId remains separately visible.
Flags are Present1/HasHealth2/Dead4/HasVelocity8/Visible16. Absent HasVelocity requires
zero velocity. Snapshot flags currently allow only NativePaused1.

The bounded Battle47 native consumer also uses these unchanged100-byte entity
records for authoritative Stinger visuals, type0x206A45A2. Their stable ID is
0x10000 plus the native activation generation, rather than a pool pointer or
reusable slot ID. State1 means an active native projectile; Animation5 has no
semantic clip. Flags are exactly Present|HasVelocity|Visible, HP/maxHP are-1/-1,
and PRS/velocity come from the original active Host projectile. This introduces
no additional snapshot block or schema version. A missing activation in the
next absolute snapshot retires that visual ID; a later activation may reuse its
physical presentation slot with a fresh ID.

The current Guest profile borrows at most six already inactive Stinger models
from two original three-slot Mosquito pools. Native active30 remains zero;
Activate, projectile Tick, collision and damage are not run on Guest. The native
consumer changes only model PRS and original recursive visibility, verifies
readback before ACK, and restores the saved PRS/visibility before the original
scene exit. It does not retain, release or delete the original pooled objects.
Projectile rotation is applied raw, without the passive-character half-turn.
Growing trail length and its local animation timeline are not replicated.

Passive Bloom and Mosquito models use the same default Y half-turn in their
receiver-owned visual wrappers, both initially and on later pose application.
The stock Guest Bloom rig, Stars and Stinger do not receive that extra basis.
Mosquito's typed native FSM state3 maps to Ordinary semantic3 and original
mcatst.san/set12 Actor playback. Clip starts follow semantic transitions; there
is no per-shot animation serial, and snapshot coalescing can omit an intermediate
transition. These are bounded native presentation policies, not codec-level
simulation or acceptance of every animation/projectile family. Native PRS,
visibility and following-world cache evidence do not alone close pixel-level
or normal-unload acceptance; full E5 qualification remains in progress.

SharedProfile411 is a semantic block, not a raw game-object memcpy. Byte groups:
gem mask48, gem counts6, spent u32, narrative13*u32, tutorial3*u32, progression u32,
indexed flags50, attack permission/context rawu8 each, power rank u32, FairyStars
u32, SideQuestManager2C..102 bool215, availability14C..155 bool10. Indexed flags
126..175 and manager bytes186..410 must be0/1; other native scalar ranges are not
guessed.

EntrySeed448 carries six u32 header fields (schema1, level, levelEpoch,
generation, Host entry ordinal, zero), Profile411, the original load-position
flag bool and saved position3f. It represents values before the original native
loader runs, rather than a post-init profile. Native consumers additionally pin
the admitted seed for the lifetime of that scene. EntryWitness64 contains eight
u32 fields (schema1, level, epoch, generation, Host entry ordinal, observed spawn
route, selected-cohort call count, accepted population count), then SHA256 of
ordered semantic placement paths. The native observer verifies actual accepted
placement slots before producing it. The witness contains no native addresses.
Transport validates the seed's epoch/generation against its fixed session;
native code must qualify the actual entry, spawn and cohort separately.

StarPresentation starts with four u32 fields: schema1, count0..16, recordBytes88,
zero. Each record is activationId u64, category u32, pool slot u32, raw native
state u32, flags u32, position3f, scale3f, rotation9f and reserved zero u32.
Category0..2 has slots0..49; category3 has slots0..99. Activation IDs and
category/slot pairs are unique. Present1 is required, Visible16 optional; scale
may be zero or signed during a native fade, with magnitude at most1e4. These
visual records neither award currency nor create a gameplay Star on Guest.
The bounded Battle47 consumer borrows inactive original pool Nodes, holds ACK
until native visibility calls/readback complete, and restores them before unload.

Message envelope72 uses class0x31534357/version1 and fixed identity
session/scenario/token/levelEpoch/generation/syncEpoch/revision. Snapshot chunks
are exactly184 bytes except the final remainder. Assembly is one contiguous
bounded revision, never interleaved, overwritten or visible to native code before
full validation. Scalar serialization uses the common MemoryStream; Packet framing
and socket retry ownership remain in the existing transport.

`WorldStateIpc.h` is the authoritative MMF ABI,16640 bytes: identity64, bridge64,
actor64, one seqlock slot16448. Snapshot payload begins at256. Host Actor is the
only Host snapshot writer; Guest Bridge is the only Guest snapshot writer.
Bridge and Actor status writers are separate. Native Guest applies a complete
initial snapshot while phase Connected2 with a nonzero epoch; it must not wait
for Ready3 first. Ready3 means its exact revision was ACKed after native apply.
Actor appliedEpoch/revision must only advance after that operation, never after
network receipt or merely reading the slot.

Actor availability Waiting0/Ready1/Retired2 is scene-world readiness, independent
of Input5 readiness, Diary pause or local focus. NativePaused is snapshot state;
it does not by itself rearm the world epoch. Actual scene readiness changes rearm
under Host-owned syncEpoch and require another full native apply. Fixed level
epoch/generation and connection identify one bounded instance; EOF is terminal,
and a new connection requires a new bridge/mapping. Applied revisions from an old
epoch cannot qualify the new one.

Actor observations use the local machine's monotonic GetTickCount64 clock and an
independent configurable lease (default2000ms,500..5000). Cached Ready expires even
when the writer is stuck odd or unreadable. Process PID/creation and liveness are
retained. Capture tick is an ordering label, not a cross-machine timestamp; there
is no invented500ms input TTL for absolute world snapshots. Native capture freshness
and actual frame readiness remain the Actor owner's responsibility.

Bridge CLI requires role, fresh mapping, local nonce, common session/scenario,
common nonzero `--connection-token`, level-epoch/generation, bind, ready-file/report.
Guest also needs `--server IPv4:port`. Optional run-ms is100..600000, actor-lease-ms
500..5000, and token is the common u16 SessionCore start token (default0xE501).
Connection-token is distinct from per-bridge MMF nonce and from that start token.
Outputs are fresh; running pump does not stream logs. Terminal report includes
native applied snapshot count separately from sent/received frames, lease and
handle/socket cleanup. A successful bridge exit is not gameplay acceptance.
