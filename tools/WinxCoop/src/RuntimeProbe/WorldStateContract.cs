using System.Buffers.Binary;
using System.Runtime.InteropServices;

namespace WinxCoop.RuntimeProbe;

internal enum WorldStateRole : uint { Host = 1, Guest = 2 }
internal enum WorldStatePhase : uint { Waiting = 1, Connected = 2, Ready = 3, Closed = 4 }
internal enum WorldStateAvailability : uint { Waiting = 0, Ready = 1, Retired = 2 }
internal enum WorldStateTerminal : uint
{
    None = 0, Identity = 1, InvalidRecord = 2, Sequence = 3, Transport = 4, PeerClosed = 5,
    ActorClosed = 6, RunLimit = 7, Clock = 8, CounterExhausted = 9, ActorLeaseExpired = 10, NativeApply = 11
}
internal readonly record struct WorldStateIdentity(string MappingName, WorldStateRole Role, uint OwnerPid,
    ulong OwnerCreationFileTime, ulong Nonce, ulong SessionId, ulong ScenarioId, uint LevelEpoch, uint Generation);

[StructLayout(LayoutKind.Explicit, Size = 64, Pack = 8)]
internal struct WorldStateHeader
{
    [FieldOffset(0)] internal uint Magic;
    [FieldOffset(4)] internal uint Version;
    [FieldOffset(8)] internal uint Bytes;
    [FieldOffset(12)] internal uint MaximumSnapshotBytes;
    [FieldOffset(16)] internal ulong Nonce;
    [FieldOffset(24)] internal uint OwnerPid;
    [FieldOffset(28)] internal WorldStateRole Role;
    [FieldOffset(32)] internal ulong OwnerCreationFileTime;
    [FieldOffset(40)] internal ulong SessionId;
    [FieldOffset(48)] internal ulong ScenarioId;
    [FieldOffset(56)] internal uint LevelEpoch;
    [FieldOffset(60)] internal uint Generation;
}
[StructLayout(LayoutKind.Explicit, Size = 64, Pack = 8)]
internal struct WorldStateBridgeStatus
{
    [FieldOffset(0)] internal uint Publication;
    [FieldOffset(4)] internal WorldStatePhase Phase;
    [FieldOffset(8)] internal ulong ConnectionToken;
    [FieldOffset(16)] internal uint SyncEpoch;
    [FieldOffset(20)] internal WorldStateTerminal TerminalReason;
    [FieldOffset(24)] internal ulong SentRevision;
    [FieldOffset(32)] internal ulong AppliedRevision;
    [FieldOffset(40)] internal ulong BridgeTickMilliseconds;
    [FieldOffset(48)] internal ulong ReceivedRevision;
    [FieldOffset(56)] internal ulong Reserved;
}
[StructLayout(LayoutKind.Explicit, Size = 64, Pack = 8)]
internal struct WorldStateActorStatus
{
    [FieldOffset(0)] internal uint Publication;
    [FieldOffset(4)] internal WorldStateAvailability Availability;
    [FieldOffset(8)] internal uint ParticipantPid;
    [FieldOffset(12)] internal uint ReadinessRevision;
    [FieldOffset(16)] internal ulong ParticipantCreationFileTime;
    [FieldOffset(24)] internal ulong ObservedAtMilliseconds;
    [FieldOffset(32)] internal uint AppliedEpoch;
    [FieldOffset(36)] internal WorldStateTerminal TerminalReason;
    [FieldOffset(40)] internal ulong AppliedRevision;
    [FieldOffset(48)] internal ulong Reserved1;
    [FieldOffset(56)] internal ulong Reserved2;
}
[StructLayout(LayoutKind.Explicit, Size = 64, Pack = 8)]
internal struct WorldStateSnapshotPrefix
{
    [FieldOffset(0)] internal uint Publication;
    [FieldOffset(4)] internal uint Bytes;
    [FieldOffset(8)] internal ulong Revision;
    [FieldOffset(16)] internal ulong NativeTick;
    [FieldOffset(24)] internal uint Flags;
    [FieldOffset(28)] internal uint Reserved0;
    [FieldOffset(32)] internal ulong Reserved1;
    [FieldOffset(40)] internal ulong Reserved2;
    [FieldOffset(48)] internal ulong Reserved3;
    [FieldOffset(56)] internal ulong Reserved4;
}

/// <summary>Copy-owned scalar state. C++ owns the WorldState network/Packet codec.</summary>
internal sealed class WorldStateSnapshot
{
    private readonly WorldPresentationActorState[] _bodies, _entities;
    private readonly byte[] _sharedProfile;
    private readonly byte[]? _entryWitness, _entrySeed;
    private readonly WorldStarState[]? _stars;
    internal WorldStateSnapshot(ulong revision, ulong nativeTick, uint flags,
        IReadOnlyList<WorldPresentationActorState> bodies, IReadOnlyList<WorldPresentationActorState> entities,
        byte[] sharedProfile, byte[]? entryWitness = null, byte[]? entrySeed = null,
        IReadOnlyList<WorldStarState>? stars = null)
    {
        ArgumentNullException.ThrowIfNull(bodies); ArgumentNullException.ThrowIfNull(entities);
        ArgumentNullException.ThrowIfNull(sharedProfile);
        Revision = revision; NativeTick = nativeTick; Flags = flags;
        _bodies = bodies.Select(WorldStateContract.CopyActor).ToArray();
        _entities = entities.Select(WorldStateContract.CopyActor).ToArray();
        _sharedProfile = (byte[])sharedProfile.Clone(); _entryWitness = entryWitness is null ? null : (byte[])entryWitness.Clone();
        _entrySeed = entrySeed is null ? null : (byte[])entrySeed.Clone();
        _stars = stars?.Select(value => value.DeepCopy()).ToArray();
        WorldStateContract.ValidateSnapshot(this);
    }
    internal ulong Revision { get; }
    internal ulong NativeTick { get; }
    internal uint Flags { get; }
    internal IReadOnlyList<WorldPresentationActorState> Bodies => Array.AsReadOnly(_bodies.Select(WorldStateContract.CopyActor).ToArray());
    internal IReadOnlyList<WorldPresentationActorState> Entities => Array.AsReadOnly(_entities.Select(WorldStateContract.CopyActor).ToArray());
    internal byte[] SharedProfile => (byte[])_sharedProfile.Clone();
    internal byte[]? EntryWitness => _entryWitness is null ? null : (byte[])_entryWitness.Clone();
    internal byte[]? EntrySeed => _entrySeed is null ? null : (byte[])_entrySeed.Clone();
    internal IReadOnlyList<WorldStarState>? Stars => _stars is null ? null :
        Array.AsReadOnly(_stars.Select(value => value.DeepCopy()).ToArray());
}

/// <summary>An exact Guest delivery reservation; reading it does not acknowledge native application.</summary>
internal sealed record WorldStateDelivery(uint Publication, uint SyncEpoch, ulong ConnectionToken, WorldStateSnapshot Snapshot);

/// <summary>
/// Fixed local IPC scalar packing only. There is no WorldState network envelope,
/// fragmentation, Packet codec, transport or game pointer serialization here.
/// </summary>
internal static class WorldStateContract
{
    internal const uint Magic = 0x31534357, Version = 1, NativePaused = 1;
    internal const uint SnapshotVersion = 2;
    internal const int SharedBytes = 16640, BridgeOffset = 64, ActorOffset = 128, SnapshotOffset = 192;
    internal const int PayloadOffset = 256, MaximumSnapshotBytes = 16384, SnapshotSlotBytes = 16448;
    internal const int SnapshotHeaderBytes = 40, ActorRecordBytes = 100, SharedProfileBytes = 411, EntryWitnessBytes = 64;
    internal const int MaximumBodies = 2, MaximumEntities = 64, MaximumBlocks = 4;
    internal const int EntrySeedBytes = WorldEntrySeedState.Bytes;

    internal static void ValidateLayout()
    {
        Require(BitConverter.IsLittleEndian && Marshal.SizeOf<WorldStateHeader>() == 64 &&
            Marshal.SizeOf<WorldStateBridgeStatus>() == 64 && Marshal.SizeOf<WorldStateActorStatus>() == 64 &&
            Marshal.SizeOf<WorldStateSnapshotPrefix>() == 64 && SnapshotOffset + SnapshotSlotBytes == SharedBytes &&
            SnapshotOffset + 64 == PayloadOffset, "WorldState1 local IPC sizes differ from WorldStateIpc.h.");
        Require(Marshal.OffsetOf<WorldStateHeader>(nameof(WorldStateHeader.Generation)).ToInt32() == 60 &&
            Marshal.OffsetOf<WorldStateBridgeStatus>(nameof(WorldStateBridgeStatus.ReceivedRevision)).ToInt32() == 48 &&
            Marshal.OffsetOf<WorldStateActorStatus>(nameof(WorldStateActorStatus.AppliedRevision)).ToInt32() == 40 &&
            Marshal.OffsetOf<WorldStateSnapshotPrefix>(nameof(WorldStateSnapshotPrefix.Revision)).ToInt32() == 8,
            "WorldState1 local IPC field offsets differ from WorldStateIpc.h.");
    }

    internal static void ValidateSnapshot(WorldStateSnapshot snapshot)
    {
        ArgumentNullException.ThrowIfNull(snapshot);
        var bodies = snapshot.Bodies; var entities = snapshot.Entities;
        byte[] profile = snapshot.SharedProfile; byte[]? witness = snapshot.EntryWitness, seed = snapshot.EntrySeed;
        var stars = snapshot.Stars;
        Require(snapshot.Revision != 0 && snapshot.NativeTick != 0 && (snapshot.Flags & ~NativePaused) == 0 &&
            bodies.Count is >= 1 and <= MaximumBodies && entities.Count <= MaximumEntities,
            "WorldState schema2 snapshot identity/flags/counts are invalid.");
        SharedProfileState.ValidatePayload(profile);
        Require(witness is null || witness.Length == EntryWitnessBytes, "WorldState1 EntryWitnessV1 is not exactly64 bytes.");
        if (seed is not null) _ = WorldEntrySeedState.Unpack(seed);
        if (stars is not null) WorldStarContract.Validate(stars);
        HashSet<uint> ids = [];
        foreach (var actor in bodies.Concat(entities))
        {
            actor.Validate();
            Require(ids.Add(actor.StableId) && actor.Position.All(v => Math.Abs(v) <= 10000000) &&
                actor.Rotation.All(v => Math.Abs(v) <= 2) && actor.Velocity.All(v => Math.Abs(v) <= 1000000),
                "WorldState1 actor has a duplicate identity or exceeds its native scalar envelope.");
        }
        Require(Size(bodies.Count, entities.Count, witness is not null, seed is not null, stars?.Count) <= MaximumSnapshotBytes,
            "WorldState1 snapshot exceeds its16KiB local slot.");
    }

    internal static byte[] PackLocalSnapshot(WorldStateSnapshot snapshot)
    {
        ValidateSnapshot(snapshot);
        var bodies = snapshot.Bodies; var entities = snapshot.Entities;
        byte[] profile = snapshot.SharedProfile; byte[]? witness = snapshot.EntryWitness, seed = snapshot.EntrySeed;
        var stars = snapshot.Stars;
        byte[]? starBytes = stars is null ? null : WorldStarContract.Pack(stars);
        byte[] result = new byte[Size(bodies.Count, entities.Count, witness is not null, seed is not null, stars?.Count)]; int at = 0;
        Put32(result, ref at, SnapshotVersion); Put32(result, ref at, snapshot.Flags);
        Put64(result, ref at, snapshot.Revision); Put64(result, ref at, snapshot.NativeTick);
        Put32(result, ref at, (uint)bodies.Count); Put32(result, ref at, (uint)entities.Count);
        Put32(result, ref at, 1u + (witness is null ? 0u : 1u) + (seed is null ? 0u : 1u) +
            (starBytes is null ? 0u : 1u)); Put32(result, ref at, 0);
        foreach (var actor in bodies.Concat(entities))
        {
            Put32(result, ref at, actor.StableId); Put32(result, ref at, actor.TypeId); Put32(result, ref at, actor.StateId);
            Put32(result, ref at, actor.Animation); Put32(result, ref at, actor.Flags);
            foreach (float value in actor.Position.Concat(actor.Scale).Concat(actor.Rotation).Concat(actor.Velocity))
                Put32(result, ref at, BitConverter.SingleToUInt32Bits(value));
            Put32(result, ref at, unchecked((uint)actor.Health)); Put32(result, ref at, unchecked((uint)actor.MaximumHealth));
        }
        Put32(result, ref at, 1); Put32(result, ref at, SharedProfileBytes);
        profile.CopyTo(result, at); at += profile.Length;
        if (witness is not null)
        {
            Put32(result, ref at, 2); Put32(result, ref at, EntryWitnessBytes);
            witness.CopyTo(result, at); at += witness.Length;
        }
        if (seed is not null)
        {
            Put32(result, ref at, 3); Put32(result, ref at, EntrySeedBytes);
            seed.CopyTo(result, at); at += seed.Length;
        }
        if (starBytes is not null)
        {
            Put32(result, ref at, 4); Put32(result, ref at, (uint)starBytes.Length);
            starBytes.CopyTo(result, at); at += starBytes.Length;
        }
        Require(at == result.Length, "WorldState1 local snapshot pack length differs."); return result;
    }

    internal static WorldStateSnapshot UnpackLocalSnapshot(byte[] payload)
    { ArgumentNullException.ThrowIfNull(payload); return UnpackLocalSnapshot(payload.AsSpan()); }

    internal static WorldStateSnapshot UnpackLocalSnapshot(ReadOnlySpan<byte> payload)
    {
        Require(payload.Length is >= SnapshotHeaderBytes and <= MaximumSnapshotBytes, "WorldState1 local snapshot length is invalid.");
        int at = 0; uint version = Get32(payload, ref at), flags = Get32(payload, ref at);
        ulong revision = Get64(payload, ref at), nativeTick = Get64(payload, ref at);
        uint bodies = Get32(payload, ref at), entities = Get32(payload, ref at), blocks = Get32(payload, ref at), reserved = Get32(payload, ref at);
        Require(version == SnapshotVersion && reserved == 0 && bodies is >= 1 and <= MaximumBodies && entities <= MaximumEntities &&
            blocks is >= 1 and <= MaximumBlocks, "WorldState local snapshot header differs from schema2.");
        Require(SnapshotHeaderBytes + (bodies + entities) * ActorRecordBytes + blocks * 8 <= payload.Length,
            "WorldState schema2 actor records/block headers exceed the local payload.");
        List<WorldPresentationActorState> bodyValues = [], entityValues = [];
        for (uint index = 0; index < bodies + entities; ++index)
        {
            uint id = Get32(payload, ref at), type = Get32(payload, ref at), state = Get32(payload, ref at);
            uint animation = Get32(payload, ref at), actorFlags = Get32(payload, ref at);
            float[] position = GetFloats(payload, ref at, 3), scale = GetFloats(payload, ref at, 3);
            float[] rotation = GetFloats(payload, ref at, 9), velocity = GetFloats(payload, ref at, 3);
            int hp = unchecked((int)Get32(payload, ref at)), maximum = unchecked((int)Get32(payload, ref at));
            var actor = new WorldPresentationActorState(id, type, state, animation, actorFlags, position, scale, rotation, velocity, hp, maximum);
            (index < bodies ? bodyValues : entityValues).Add(actor);
        }
        byte[]? profile = null, witness = null, seed = null;
        IReadOnlyList<WorldStarState>? stars = null; uint seen = 0;
        for (uint index = 0; index < blocks; ++index)
        {
            uint type = Get32(payload, ref at), bytes = Get32(payload, ref at);
            Require(type is >= 1 and <= MaximumBlocks && (index != 0 || type == 1) &&
                (seen & (1u << (int)type)) == 0 && bytes <= payload.Length - at,
                "WorldState schema2 block is unknown, duplicate, out of bounds, or not required-first Profile.");
            seen |= 1u << (int)type;
            Require(type switch
            {
                1 => bytes == SharedProfileBytes,
                2 => bytes == EntryWitnessBytes,
                3 => bytes == EntrySeedBytes,
                4 => bytes >= WorldStarContract.HeaderBytes &&
                     bytes <= WorldStarContract.HeaderBytes + WorldStarContract.MaximumStars * WorldStarContract.RecordBytes &&
                     (bytes - WorldStarContract.HeaderBytes) % WorldStarContract.RecordBytes == 0,
                _ => false
            }, "WorldState schema2 block length differs from its exact scalar contract.");
            byte[] values = payload.Slice(at, (int)bytes).ToArray(); at += (int)bytes;
            switch (type)
            {
                case 1: profile = values; break;
                case 2: witness = values; break;
                case 3: seed = values; break;
                case 4: stars = WorldStarContract.Unpack(values); break;
            }
        }
        Require(at == payload.Length && profile is not null, "WorldState schema2 local snapshot has trailing data or no Profile.");
        return new(revision, nativeTick, flags, bodyValues, entityValues, profile!, witness, seed, stars);
    }

    internal static WorldPresentationActorState CopyActor(WorldPresentationActorState value)
    {
        ArgumentNullException.ThrowIfNull(value);
        return value with { Position = (float[])value.Position.Clone(), Scale = (float[])value.Scale.Clone(),
            Rotation = (float[])value.Rotation.Clone(), Velocity = (float[])value.Velocity.Clone() };
    }
    private static int Size(int bodies, int entities, bool witness, bool seed, int? stars) => checked(SnapshotHeaderBytes +
        (bodies + entities) * ActorRecordBytes + 8 + SharedProfileBytes + (witness ? 8 + EntryWitnessBytes : 0) +
        (seed ? 8 + EntrySeedBytes : 0) +
        (stars.HasValue ? 8 + WorldStarContract.HeaderBytes + stars.Value * WorldStarContract.RecordBytes : 0));
    private static void Put32(byte[] bytes, ref int at, uint value)
    { BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(at, 4), value); at += 4; }
    private static void Put64(byte[] bytes, ref int at, ulong value)
    { BinaryPrimitives.WriteUInt64LittleEndian(bytes.AsSpan(at, 8), value); at += 8; }
    private static uint Get32(ReadOnlySpan<byte> bytes, ref int at)
    { Require(at >= 0 && at <= bytes.Length - 4, "WorldState schema2 scalar is truncated.");
      uint value = BinaryPrimitives.ReadUInt32LittleEndian(bytes.Slice(at, 4)); at += 4; return value; }
    private static ulong Get64(ReadOnlySpan<byte> bytes, ref int at)
    { Require(at >= 0 && at <= bytes.Length - 8, "WorldState schema2 scalar is truncated.");
      ulong value = BinaryPrimitives.ReadUInt64LittleEndian(bytes.Slice(at, 8)); at += 8; return value; }
    private static float[] GetFloats(ReadOnlySpan<byte> bytes, ref int at, int count)
    { float[] result = new float[count]; for (int i = 0; i < count; ++i) result[i] = BitConverter.UInt32BitsToSingle(Get32(bytes, ref at)); return result; }
    private static void Require(bool condition, string message)
    { if (!condition) throw new InvalidDataException(message); }
}
