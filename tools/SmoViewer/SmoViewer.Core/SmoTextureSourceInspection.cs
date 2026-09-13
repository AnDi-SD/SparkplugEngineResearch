using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>Host metadata copied from the actual PC source reader. The cache
/// avoids repeating initialization/mip generation when inspectors revisit an
/// immutable catalog entry. It retains no native owner or copied pixel data.</summary>
internal static class SmoTextureSourceInspection
{
    internal enum Scope : uint { SourceWrapper, Derived }
    internal enum Outcome : uint
    { Pending, Skipped, SourceNone, EmbeddedSource, ExternalSource, Platform, Cross, Native, Terminator }
    internal sealed record Snapshot(NativeMethods.TextureSourceInfo Info,
        NativeMethods.TextureSourceField[] Fields, NativeMethods.TextureSourceRepresentation[] Representations,
        NativeMethods.TextureMip[] Mips);
    private sealed record Result(Snapshot? Value, string Error);
    private static readonly ConditionalWeakTable<SmoDocument, Dictionary<int, Result>> Cache = new();

    internal static bool TryRead(SmoDocument document, SmoObjectEntry entry,
        [NotNullWhen(true)] out Snapshot? value, out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        value = null;
        if ((uint)entry.Index >= (uint)document.Objects.Count || !ReferenceEquals(document.Objects[entry.Index], entry) ||
            entry.TypeHash != SmoClassIds.TextureData || !entry.IsWithinDataSection || !entry.SignatureMatches ||
            entry.PhysicalOffset < 0 || entry.SerializedSize < 9 || entry.PhysicalEnd > document.Data.Length ||
            entry.SerializedSize > int.MaxValue)
        {
            error = "TEXTURE_INPUT_INVALID: Source inspection requires a complete catalogued TextureData.";
            return false;
        }
        if ((document.Header.PlatformMask & 2) == 0)
        {
            error = "TEXTURE_PC_SOURCE_UNAVAILABLE: This source inspector requires a PC-compatible container.";
            return false;
        }
        var entries = Cache.GetValue(document, static _ => new());
        lock (entries)
        {
            if (!entries.TryGetValue(entry.Index, out var result))
                entries.Add(entry.Index, result = Read(document, entry));
            value = result.Value; error = result.Error;
            return value is not null;
        }
    }

    private static unsafe Result Read(SmoDocument document, SmoObjectEntry entry)
    {
        var payload = document.Data.Span.Slice(checked((int)entry.PhysicalOffset + 8), checked((int)entry.SerializedSize - 8));
        try
        {
            TextureSourceHandle handle;
            fixed (byte* bytes = payload)
                handle = new(NativeMethods.Check(NativeMethods.spv_texture_source_read(bytes, (uint)payload.Length, document.Header.PlatformMask)));
            using (handle)
            {
                NativeMethods.Check(NativeMethods.spv_texture_source_info(handle, out var info));
                var fields = new NativeMethods.TextureSourceField[checked((int)info.Fields)];
                var representations = new NativeMethods.TextureSourceRepresentation[checked((int)info.Representations)];
                var mips = new NativeMethods.TextureMip[checked((int)info.Mips)];
                fixed (NativeMethods.TextureSourceField* output = fields)
                    NativeMethods.Check(NativeMethods.spv_texture_source_fields(handle, output, info.Fields));
                fixed (NativeMethods.TextureSourceRepresentation* output = representations)
                    NativeMethods.Check(NativeMethods.spv_texture_source_representations(handle, output, info.Representations));
                fixed (NativeMethods.TextureMip* output = mips)
                    NativeMethods.Check(NativeMethods.spv_texture_source_mips(handle, output, info.Mips));
                return new(new(info, fields, representations, mips), string.Empty);
            }
        }
        catch (InvalidDataException exception) { return new(null, "TEXTURE_PC_SOURCE_INVALID: " + exception.Message); }
    }
}
