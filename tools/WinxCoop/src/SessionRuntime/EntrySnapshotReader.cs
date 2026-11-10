using System.Buffers.Binary;
using System.IO.MemoryMappedFiles;
using System.Reflection;
using System.Runtime.Loader;
using System.Runtime.Versioning;
using System.Security.Cryptography;

namespace WinxCoop.SessionRuntime;

internal sealed record EntrySnapshotIdentity(string MappingName, ulong Nonce, uint BridgePid,
    ulong BridgeCreation, ulong SessionId, ulong ScenarioId, uint LevelEpoch, uint Generation,
    uint ActorPid, ulong ActorCreation, ulong ConnectionToken, int ActorLeaseMilliseconds, uint Level, uint ExpectedHostEntryOrdinal);

internal sealed record EntrySnapshotCapture(byte[] Seed, byte[] Witness, ulong Revision,
    ulong NativeTick, uint SnapshotPublication, uint ActorPublication, uint BridgePublication, uint Level, uint LevelEpoch, uint Generation, uint HostEntryOrdinal)
{
    internal object Evidence => new
    {
        source = "actual-host-world-ipc", decoder = "pinned-actor-WorldStateContract.UnpackLocalSnapshot(byte[])",
        revisionDecimal = Revision.ToString(System.Globalization.CultureInfo.InvariantCulture),
        nativeTickDecimal = NativeTick.ToString(System.Globalization.CultureInfo.InvariantCulture),
        snapshotPublication = SnapshotPublication, actorPublication = ActorPublication, bridgePublication = BridgePublication,
        seedBytes = Seed.Length, witnessBytes = Witness.Length,
        seedSha256 = Convert.ToHexString(SHA256.HashData(Seed)), witnessSha256 = Convert.ToHexString(SHA256.HashData(Witness)),
        level = Level, levelEpoch = LevelEpoch, generation = Generation, hostEntryOrdinal = HostEntryOrdinal,
        guestConfigurationPublishedOnlyAfterHostReady = true, nativeGameReads = 0, nativeGameWrites = 0
    };
}

// Read-only IPC orchestration. Actor's exact managed decoder owns the local
// snapshot and entry-seed schemas; this helper does not copy either codec.
[SupportedOSPlatform("windows")]
internal sealed class EntrySnapshotReader : IDisposable
{
    private const BindingFlags StaticMethod = BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public;
    private const BindingFlags InstanceProperty = BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public;
    private readonly PinnedDecoderContext _context;
    private readonly MethodInfo _unpackSnapshot, _unpackSeed;
    private readonly PropertyInfo _revision, _nativeTick, _flags, _seed, _witness;
    private readonly PropertyInfo _seedLevel, _seedEpoch, _seedGeneration, _seedOrdinal;
    private bool _disposed;

    internal EntrySnapshotReader(string actorDirectory, string actorHash, string coreHash)
    {
        byte[] actor = ReadPinned(Path.Combine(actorDirectory, "WinxCoop.ActorProbe.dll"), actorHash);
        byte[] core = ReadPinned(Path.Combine(actorDirectory, "SmoNativeValidator.Core.dll"), coreHash);
        _context = new PinnedDecoderContext(core);
        try
        {
            using var stream = new MemoryStream(actor, writable: false);
            Assembly assembly = _context.LoadFromStream(stream);
            OwnedChild.Require(assembly.GetName().Name == "WinxCoop.ActorProbe", "Pinned Actor DLL assembly identity differs.");
            Type contract = assembly.GetType("WinxCoop.RuntimeProbe.WorldStateContract", throwOnError: true)!;
            Type snapshot = assembly.GetType("WinxCoop.RuntimeProbe.WorldStateSnapshot", throwOnError: true)!;
            Type entry = assembly.GetType("WinxCoop.RuntimeProbe.WorldEntrySeedState", throwOnError: true)!;
            _unpackSnapshot = Method(contract, "UnpackLocalSnapshot"); _unpackSeed = Method(entry, "Unpack");
            OwnedChild.Require(_unpackSnapshot.ReturnType == snapshot && _unpackSeed.ReturnType == entry,
                "Pinned Actor decoder return types differ.");
            _revision = Property(snapshot, "Revision", typeof(ulong)); _nativeTick = Property(snapshot, "NativeTick", typeof(ulong));
            _flags = Property(snapshot, "Flags", typeof(uint)); _seed = Property(snapshot, "EntrySeed", typeof(byte[]));
            _witness = Property(snapshot, "EntryWitness", typeof(byte[]));
            _seedLevel = Property(entry, "Level", typeof(uint)); _seedEpoch = Property(entry, "LevelEpoch", typeof(uint));
            _seedGeneration = Property(entry, "Generation", typeof(uint)); _seedOrdinal = Property(entry, "HostEntryOrdinal", typeof(uint));
        }
        catch { _context.Unload(); throw; }
    }

    internal EntrySnapshotCapture? TryRead(EntrySnapshotIdentity identity)
    {
        OwnedChild.Require(!_disposed && identity.Level != 0 && identity.ExpectedHostEntryOrdinal != 0 && identity.ActorLeaseMilliseconds is >= 500 and <= 5000, "Entry snapshot decoder/explicit identity is invalid.");
        using MemoryMappedFile file = MemoryMappedFile.OpenExisting(identity.MappingName, MemoryMappedFileRights.Read);
        using MemoryMappedViewAccessor view = file.CreateViewAccessor(0, 16640, MemoryMappedFileAccess.Read);
        byte[] header = Read(view, 0, 64); ValidateHeader(header, identity);
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            uint bridgePublication = view.ReadUInt32(64), actorPublication = view.ReadUInt32(128),
                snapshotPublication = view.ReadUInt32(192);
            if (((bridgePublication | actorPublication | snapshotPublication) & 1) != 0) continue;
            Thread.MemoryBarrier();
            byte[] bridge = Read(view, 64, 64), actor = Read(view, 128, 64), slot = Read(view, 192, 16448);
            Thread.MemoryBarrier();
            if (view.ReadUInt32(64) != bridgePublication || view.ReadUInt32(128) != actorPublication ||
                view.ReadUInt32(192) != snapshotPublication || U(bridge, 0) != bridgePublication ||
                U(actor, 0) != actorPublication || U(slot, 0) != snapshotPublication) continue;
            OwnedChild.Require(Read(view, 0, 64).AsSpan().SequenceEqual(header), "Host World IPC immutable header changed during bootstrap read.");
            ValidateHeader(header, identity);
            if (actorPublication == 0 || snapshotPublication == 0) return null;
            OwnedChild.Require(U(bridge, 20) == 0 && U(bridge, 4) is >= 1 and <= 3 && L(bridge, 56) == 0 &&
                (L(bridge, 8) == 0 || L(bridge, 8) == identity.ConnectionToken),
                "Host World bridge failed, retired, or changed its connection token before entry bootstrap.");
            OwnedChild.Require(U(actor, 4) is 0 or 1 && U(actor, 8) == identity.ActorPid && U(actor, 12) != 0 &&
                L(actor, 16) == identity.ActorCreation && U(actor, 36) == 0 && L(actor, 48) == 0 && L(actor, 56) == 0,
                "Host World actor status owner/readiness/terminal/reserved fields differ.");
            ulong now = checked((ulong)Environment.TickCount64), actorTick = L(actor, 24), bridgeTick = L(bridge, 40);
            OwnedChild.Require(actorTick != 0 && actorTick <= now && now - actorTick <= (ulong)identity.ActorLeaseMilliseconds &&
                bridgeTick != 0 && bridgeTick <= now && now - bridgeTick <= 500,
                "Host World actor/bridge heartbeat is stale before entry bootstrap.");
            if (U(actor, 4) != 1) return null; // WorldStateAvailability.Ready=1, Retired=2.
            uint bytes = U(slot, 4); ulong revision = L(slot, 8), tick = L(slot, 16); uint flags = U(slot, 24);
            OwnedChild.Require(bytes is >= 40 and <= 16384 && revision != 0 && tick != 0 && (flags & ~1u) == 0 &&
                slot.AsSpan(28, 36).IndexOfAnyExcept((byte)0) < 0 &&
                slot.AsSpan(checked(64 + (int)bytes)).IndexOfAnyExcept((byte)0) < 0,
                "Host World snapshot prefix/size/reserved/trailing bytes differ.");
            object decoded = Invoke(_unpackSnapshot, slot.AsSpan(64, checked((int)bytes)).ToArray());
            OwnedChild.Require((ulong)_revision.GetValue(decoded)! == revision && (ulong)_nativeTick.GetValue(decoded)! == tick &&
                (uint)_flags.GetValue(decoded)! == flags, "Decoded Host snapshot differs from its same-publication IPC prefix.");
            byte[]? seed = _seed.GetValue(decoded) as byte[], witness = _witness.GetValue(decoded) as byte[];
            if (seed is null || witness is null) return null;
            OwnedChild.Require(seed.Length == 448 && witness.Length == 64, "Host EntrySeed/EntryWitness lengths differ from448/64.");
            object decodedSeed = Invoke(_unpackSeed, seed);
            OwnedChild.Require((uint)_seedLevel.GetValue(decodedSeed)! == identity.Level && (uint)_seedEpoch.GetValue(decodedSeed)! == identity.LevelEpoch &&
                (uint)_seedGeneration.GetValue(decodedSeed)! == identity.Generation && (uint)_seedOrdinal.GetValue(decodedSeed)! == identity.ExpectedHostEntryOrdinal &&
                U(witness, 0) == 1 && U(witness, 4) == identity.Level && U(witness, 8) == identity.LevelEpoch &&
                U(witness, 12) == identity.Generation && U(witness, 16) == identity.ExpectedHostEntryOrdinal,
                "Host entry payload explicit level/epoch/generation/ordinal identity differs.");
            return new((byte[])seed.Clone(), (byte[])witness.Clone(), revision, tick,
                snapshotPublication, actorPublication, bridgePublication, identity.Level, identity.LevelEpoch, identity.Generation, identity.ExpectedHostEntryOrdinal);
        }
        return null; // Contention is not cached as a successful bootstrap.
    }

    private static void ValidateHeader(byte[] header, EntrySnapshotIdentity identity) => OwnedChild.Require(
        U(header, 0) == 0x31534357 && U(header, 4) == 1 && U(header, 8) == 16640 && U(header, 12) == 16384 &&
        L(header, 16) == identity.Nonce && U(header, 24) == identity.BridgePid && U(header, 28) == 1 &&
        L(header, 32) == identity.BridgeCreation && L(header, 40) == identity.SessionId && L(header, 48) == identity.ScenarioId &&
        U(header, 56) == identity.LevelEpoch && U(header, 60) == identity.Generation,
        "Host World IPC immutable identity differs from this exact owned bridge/epoch/generation.");

    private static byte[] Read(MemoryMappedViewAccessor view, long offset, int length)
    {
        byte[] result = new byte[length]; OwnedChild.Require(view.ReadArray(offset, result, 0, length) == length,
            "Short read-only Host World IPC bootstrap read."); return result;
    }
    private static MethodInfo Method(Type type, string name) => type.GetMethod(name, StaticMethod,
        binder: null, types: [typeof(byte[])], modifiers: null) ?? throw new InvalidDataException("Pinned Actor decoder lacks " + name + "(byte[]).");
    private static PropertyInfo Property(Type type, string name, Type expected)
    {
        PropertyInfo property = type.GetProperty(name, InstanceProperty) ?? throw new InvalidDataException("Pinned Actor decoder lacks property " + name + ".");
        OwnedChild.Require(property.PropertyType == expected && property.GetMethod is not null && property.GetIndexParameters().Length == 0,
            "Pinned Actor property type/getter differs: " + name); return property;
    }
    private static object Invoke(MethodInfo method, byte[] bytes)
    {
        try { return method.Invoke(null, [bytes]) ?? throw new InvalidDataException("Pinned Actor decoder returned null."); }
        catch (TargetInvocationException error) { throw new InvalidDataException("Pinned Actor decoder rejected actual Host snapshot/entry bytes.", error.InnerException ?? error); }
    }
    private static byte[] ReadPinned(string path, string hash)
    {
        OwnedChild.Require(hash.Length == 64 && hash.All(Uri.IsHexDigit) && File.Exists(path) &&
            (File.GetAttributes(path) & FileAttributes.ReparsePoint) == 0, "Pinned managed decoder DLL/hash is missing, invalid, or a reparse point.");
        // Load precisely the bytes whose hash was checked, not a later path read.
        using var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        OwnedChild.Require(file.Length is > 0 and <= 16 * 1024 * 1024, "Managed decoder DLL exceeds16MiB.");
        byte[] bytes = new byte[checked((int)file.Length)]; file.ReadExactly(bytes);
        OwnedChild.Require(Convert.ToHexString(SHA256.HashData(bytes)).Equals(hash, StringComparison.OrdinalIgnoreCase),
            "Managed decoder DLL pin differs: " + path); return bytes;
    }
    private static uint U(byte[] bytes, int offset) => BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(offset, 4));
    private static ulong L(byte[] bytes, int offset) => BinaryPrimitives.ReadUInt64LittleEndian(bytes.AsSpan(offset, 8));
    public void Dispose() { if (_disposed) return; _disposed = true; _context.Unload(); }

    private sealed class PinnedDecoderContext(byte[] core) : AssemblyLoadContext("WinxCoop.E6.EntrySnapshot." + Guid.NewGuid().ToString("N"), isCollectible: true)
    {
        protected override Assembly? Load(AssemblyName name)
        {
            if (name.Name != "SmoNativeValidator.Core") return null;
            using var stream = new MemoryStream(core, writable: false);
            Assembly result = LoadFromStream(stream);
            OwnedChild.Require(AssemblyName.ReferenceMatchesDefinition(name, result.GetName()), "Pinned Core DLL identity differs from Actor dependency.");
            return result;
        }
    }
}
