using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Runtime.InteropServices;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>A four-byte ID site actually consumed by the common game reader.</summary>
public sealed record SmoFileReferenceSite(int Offset, uint ObjectId, uint InlineSize = 0, uint ConsumerId = 0);
public sealed record SmoFileReferenceObject(int RelativeOffset, uint ObjectId, uint ClassId,
    uint SerializedSize, string RawNameSha256);
public sealed record SmoRemappedReferenceRange(byte[] Data, SmoFileReferenceRange Provenance);
/// <summary>Host worker transport of existing observations, never a game format or a new reader result.</summary>
public sealed record SmoFileReferenceRangeTransport(int Version, string ContentSha256, string CatalogSha256,
    uint[] CatalogIds, SmoFileReferenceSite[] Sites, SmoFileReferenceObject[] Objects);

/// <summary>One worker exchange; repeated ranges share one bounded catalog.</summary>
public sealed class SmoFileReferenceWorkerContext
{
    private readonly Dictionary<string, uint[]> catalogs = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<uint[], string> arrayHashes = new();
    private static string Hash(uint[] values) =>
        Convert.ToHexString(SHA256.HashData(MemoryMarshal.AsBytes(values.AsSpan())));
    internal (string Hash, uint[] Definition) Export(uint[] values)
    {
        if (!arrayHashes.TryGetValue(values, out string? hash))
            arrayHashes.Add(values, hash = Hash(values));
        if (catalogs.ContainsKey(hash)) return (hash, []);
        catalogs.Add(hash, values);
        return (hash, values.ToArray());
    }
    internal uint[] Restore(SmoFileReferenceRangeTransport transport)
    {
        if (transport.CatalogIds is null || transport.CatalogIds.Length > 8192 ||
            string.IsNullOrEmpty(transport.CatalogSha256))
            throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: invalid worker catalog definition.");
        if (transport.CatalogIds.Length != 0)
        {
            if (!Hash(transport.CatalogIds).Equals(transport.CatalogSha256, StringComparison.OrdinalIgnoreCase) ||
                transport.CatalogIds.Contains(0u) || transport.CatalogIds.Distinct().Count() != transport.CatalogIds.Length)
                throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: invalid worker resource catalog.");
            if (!catalogs.ContainsKey(transport.CatalogSha256))
                catalogs.Add(transport.CatalogSha256, transport.CatalogIds.ToArray());
        }
        return catalogs.TryGetValue(transport.CatalogSha256, out var ids) ? ids
            : throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: worker catalog reference has no prior definition.");
    }
}

/// <summary>
/// Host relocation metadata for exact bytes of an inspected range. It does not
/// discover references, serialize objects or infer meaning from field sizes.
/// </summary>
public sealed class SmoFileReferenceRange
{
    private readonly byte[] contentHash;
    private readonly uint[] catalogIds;
    private readonly int length;
    public IReadOnlyList<SmoFileReferenceSite> Sites { get; }
    public IReadOnlyList<SmoFileReferenceObject> Objects { get; }

    internal SmoFileReferenceRange(ReadOnlySpan<byte> data,
        IEnumerable<SmoFileReferenceSite> sites, IEnumerable<SmoFileReferenceObject> objects, uint[] catalogIds)
    {
        contentHash = SHA256.HashData(data);
        length = data.Length;
        Sites = Array.AsReadOnly(sites.ToArray());
        Objects = Array.AsReadOnly(objects.ToArray());
        this.catalogIds = catalogIds;
    }

    public SmoFileReferenceRangeTransport ExportWorkerTransport(SmoFileReferenceWorkerContext? context = null)
    {
        var catalog = (context ?? new()).Export(catalogIds);
        return new(1, Convert.ToHexString(contentHash), catalog.Hash, catalog.Definition, Sites.ToArray(), Objects.ToArray());
    }

    /// <summary>
    /// Accepts observations produced by our isolated worker. Hash/extent checks
    /// bind them to its exact attachment bytes; this does not independently
    /// re-execute the reader and is not an import path for arbitrary evidence.
    /// </summary>
    public static SmoFileReferenceRange RestoreWorkerTransport(ReadOnlySpan<byte> data,
        SmoFileReferenceRangeTransport transport, SmoFileReferenceWorkerContext? context = null)
    {
        ArgumentNullException.ThrowIfNull(transport);
        if (transport.Version != 1 || transport.Sites is null || transport.Objects is null ||
            transport.Sites.Length > 262144 || transport.Objects.Length > 8192 ||
            !Convert.ToHexString(SHA256.HashData(data)).Equals(transport.ContentSha256, StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: invalid worker snapshot or attachment hash.");
        uint[] catalog = (context ?? new()).Restore(transport);
        var ids = catalog.ToHashSet();
        int previous = -1;
        foreach (var site in transport.Sites)
        {
            if (site is null || (previous >= 0 && site.Offset < previous + sizeof(uint)) || site.Offset < 0 || site.Offset > data.Length - sizeof(uint) ||
                (site.ObjectId != 0 && !ids.Contains(site.ObjectId)) || (site.ConsumerId != 0 && !ids.Contains(site.ConsumerId)) ||
                BinaryPrimitives.ReadUInt32LittleEndian(data[site.Offset..]) != site.ObjectId)
                throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: invalid worker reference position.");
            if ((site.ObjectId == 0 && site.InlineSize != 0) || (site.ObjectId != 0 &&
                ((ulong)site.Offset + 8 + site.InlineSize > (ulong)data.Length ||
                BinaryPrimitives.ReadUInt32LittleEndian(data[(site.Offset + 4)..]) != site.InlineSize)))
                throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: worker reference extent differs from its bytes.");
            previous = site.Offset;
        }
        var seen = new HashSet<uint>();
        foreach (var entry in transport.Objects)
            if (entry is null || !ids.Contains(entry.ObjectId) || !seen.Add(entry.ObjectId) ||
                entry.RelativeOffset < 0 || entry.SerializedSize < 8 ||
                (ulong)entry.RelativeOffset + entry.SerializedSize > (ulong)data.Length ||
                entry.RawNameSha256 is null || entry.RawNameSha256.Length != 64)
                throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: invalid worker object extent.");
        return new(data, transport.Sites, transport.Objects, catalog);
    }

    public SmoRemappedReferenceRange Remap(ReadOnlySpan<byte> data,
        IReadOnlyDictionary<uint, uint> idMap)
        => Remap(data, new SmoFileReferenceRemapContext(idMap));

    public SmoRemappedReferenceRange Remap(ReadOnlySpan<byte> data, SmoFileReferenceRemapContext context)
    {
        ArgumentNullException.ThrowIfNull(context);
        uint[] remappedCatalog = context.RemapCatalog(catalogIds);
        byte[] result = RewriteObservedIds(data, context.MapId);
        var mappedSites = Sites.Select(site => site with
            {ObjectId=context.MapId(site.ObjectId),ConsumerId=context.MapId(site.ConsumerId)});
        return new(result, new SmoFileReferenceRange(result, mappedSites,
            Objects.Select(entry => entry with { ObjectId = context.MapId(entry.ObjectId) }), remappedCatalog));
    }

    /// <summary>Copy an observed range into another file's explicit catalog.
    /// The host chooses cross-file identities; only actual reader sites change.
    /// Unlike same-file Remap, unused source catalog IDs do not reserve target IDs.
    /// External references may converge; copied object identities must stay unique.</summary>
    public byte[] Relocate(ReadOnlySpan<byte> data,
        IReadOnlyDictionary<uint, uint> idMap, IReadOnlySet<uint> destinationIds)
    {
        ArgumentNullException.ThrowIfNull(idMap);
        ArgumentNullException.ThrowIfNull(destinationIds);
        if (destinationIds.Contains(0) || idMap.Any(pair =>
            pair.Key == 0 || pair.Value == 0 || !destinationIds.Contains(pair.Value)))
            throw new ArgumentException("Relocation requires nonzero mappings into the explicit destination catalog.");
        uint Map(uint id) => idMap.TryGetValue(id, out uint value) ? value : id;
        if (Sites.Any(site => site.ObjectId != 0 && !destinationIds.Contains(Map(site.ObjectId))) ||
            Objects.Any(entry => !destinationIds.Contains(Map(entry.ObjectId))) ||
            Objects.Select(entry => Map(entry.ObjectId)).Distinct().Count() != Objects.Count)
            throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: relocation has missing destinations or colliding copied objects.");
        return RewriteObservedIds(data, Map);
    }

    private byte[] RewriteObservedIds(ReadOnlySpan<byte> data, Func<uint, uint> map)
    {
        if (data.Length != length || !SHA256.HashData(data).AsSpan().SequenceEqual(contentHash))
            throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: range bytes changed after observation.");
        byte[] result = data.ToArray();
        int previous = -1;
        foreach (var site in Sites)
        {
            if ((previous >= 0 && site.Offset < previous + sizeof(uint)) || site.Offset < 0 || site.Offset > result.Length - sizeof(uint) ||
                BinaryPrimitives.ReadUInt32LittleEndian(data[site.Offset..]) != site.ObjectId)
                throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: observed ID does not match its source bytes.");
            previous = site.Offset;
            uint mapped = map(site.ObjectId);
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(site.Offset), mapped);
        }
        return result;
    }
}

/// <summary>
/// Immutable observations from one actual ResourceGraph load. A successful
/// load can skip cached inline bytes, so coverage is checked for every copied
/// FAT extent before any authoring range is accepted.
/// </summary>
public sealed class SmoFileReferenceTrace
{
    private readonly SmoDocument document;
    private readonly NativeMethods.ReferenceRead[] references;
    private readonly HashSet<(uint Id, uint Class, uint Offset, uint Size)> covered;
    private readonly uint[] catalogIds;
    private readonly byte[] sourceHash;
    public int ReferenceCount => references.Length;
    public int CoveredExtentCount => covered.Count;
    public uint DataPhysicalOrigin { get; }

    private SmoFileReferenceTrace(SmoDocument document, uint origin,
        NativeMethods.ReferenceRead[] references, NativeMethods.PayloadRead[] payloads)
    {
        this.document = document;
        this.references = references;
        DataPhysicalOrigin = origin;
        catalogIds = document.Objects.Select(entry => entry.Id).ToArray();
        sourceHash = SHA256.HashData(document.Data.Span);
        // Native enum: Outer=0, Inline=1, PreparedMesh=4. Existing/cache skips
        // (2/3) never establish reader coverage, even when the skip succeeded.
        covered = payloads.Where(value => value.Complete != 0 && value.Kind is 0 or 1 or 4)
            .Select(value => (value.ObjectId, value.WireClassId, value.PhysicalOffset, value.Size)).ToHashSet();
    }

    internal static unsafe SmoFileReferenceTrace Read(SmoDocument document, GraphHandle graph)
    {
        NativeMethods.Check(NativeMethods.spv_graph_reference_trace_info(graph,
            out uint referenceCount, out uint payloadCount, out uint origin));
        if ((ulong)referenceCount + payloadCount > 262144 || origin != document.Header.DataStart)
            throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: invalid snapshot bounds or file origin.");
        var references = new NativeMethods.ReferenceRead[checked((int)referenceCount)];
        var payloads = new NativeMethods.PayloadRead[checked((int)payloadCount)];
        fixed (NativeMethods.ReferenceRead* output = references)
            NativeMethods.Check(NativeMethods.spv_graph_reference_reads(graph, output, referenceCount));
        fixed (NativeMethods.PayloadRead* output = payloads)
            NativeMethods.Check(NativeMethods.spv_graph_payload_reads(graph, output, payloadCount));
        if (references.Any(value => value.Success != 1 || value.Resolution is < 1 or > 4) ||
            payloads.Any(value => value.Kind > 4))
            throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: unfinished or unknown reader observation.");
        return new(document, origin, references, payloads);
    }

    public SmoFileReferenceRange CaptureRange(SmoDocument source, int physicalOffset, int length)
    {
        ValidateSource(source);
        return CaptureRangeCore(source, physicalOffset, length);
    }

    /// <summary>
    /// Captures ranges from the same immutable document, in the requested order.
    /// The source hash is checked once for this synchronous batch, while every
    /// range retains its own coverage checks and content hash. The source's owned
    /// buffer must not be mutated during this operation, as for CaptureRange.
    /// </summary>
    public IReadOnlyList<SmoFileReferenceRange> CaptureRanges(SmoDocument source,
        IReadOnlyList<(int PhysicalOffset, int Length)> ranges)
    {
        ArgumentNullException.ThrowIfNull(ranges);
        ValidateSource(source);
        var result = new SmoFileReferenceRange[ranges.Count];
        for (int index = 0; index < result.Length; index++)
        {
            var range = ranges[index];
            result[index] = CaptureRangeCore(source, range.PhysicalOffset, range.Length);
        }
        return Array.AsReadOnly(result);
    }

    private void ValidateSource(SmoDocument source)
    {
        ArgumentNullException.ThrowIfNull(source);
        if (!ReferenceEquals(source, document))
            throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: observations belong to another document.");
        if (!SHA256.HashData(source.Data.Span).AsSpan().SequenceEqual(sourceHash))
            throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: source bytes changed after reader observation.");
    }

    private SmoFileReferenceRange CaptureRangeCore(SmoDocument source, int physicalOffset, int length)
    {
        if (physicalOffset < 0 || length <= 0 || physicalOffset > source.Data.Length - length)
            throw new ArgumentOutOfRangeException(nameof(physicalOffset), "Range lies outside the document.");
        long end = (long)physicalOffset + length;
        var entries = source.Objects.Where(entry => entry.PhysicalOffset >= physicalOffset &&
            entry.PhysicalOffset < end).ToArray();
        if (entries.Length == 0)
            throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: range contains no complete catalogued objects.");
        foreach (var entry in entries)
            if (entry.PhysicalEnd > end || !covered.Contains((entry.Id, entry.TypeHash,
                checked((uint)entry.PhysicalOffset), entry.SerializedSize)))
                throw new InvalidDataException($"SPARKPLUG_REFERENCE_TRACE: copied payload {entry.Id} was not completely read at its FAT extent.");

        var sites = new Dictionary<int, SmoFileReferenceSite>();
        foreach (var reference in references)
        {
            long offset = reference.IdPhysicalOffset;
            if (offset < physicalOffset && offset + sizeof(uint) > physicalOffset)
                throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: range cuts the start of a reference ID.");
            if (offset < physicalOffset || offset >= end) continue;
            if (offset > end - sizeof(uint))
                throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: range cuts a reference ID.");
            int relative = checked((int)(offset - physicalOffset));
            if (BinaryPrimitives.ReadUInt32LittleEndian(source.Data.Span[checked((int)offset)..]) != reference.Id)
                throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: reader observation disagrees with immutable file bytes.");
            if (reference.Id != 0 && (reference.SizePhysicalOffset != reference.IdPhysicalOffset + 4 ||
                offset + 8 + reference.InlineSize > end))
                throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: range cuts a reference payload or has a split prefix.");
            var site = new SmoFileReferenceSite(relative, reference.Id, reference.InlineSize, reference.ConsumerId);
            if (sites.TryGetValue(relative, out var previous) && previous != site)
                throw new InvalidDataException("SPARKPLUG_REFERENCE_TRACE: conflicting observations at one byte position.");
            sites[relative] = site;
        }
        return new(source.Data.Span.Slice(physicalOffset, length), sites.OrderBy(pair => pair.Key).Select(pair => pair.Value),
            entries.Select(entry => new SmoFileReferenceObject(checked((int)entry.PhysicalOffset - physicalOffset),
                entry.Id, entry.TypeHash, entry.SerializedSize, Convert.ToHexString(SHA256.HashData(entry.RawName.Span)))), catalogIds);
    }
}
