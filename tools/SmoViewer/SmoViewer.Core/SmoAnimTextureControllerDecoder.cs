using System.Diagnostics.CodeAnalysis;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

public sealed record SmoAnimTextureFrame(float Time, SmoNodeRelationship Texture);
public sealed record SmoAnimTextureControllerData(IReadOnlyList<SmoAnimTextureFrame> Frames, float Duration)
{
    internal AnimTextureHandle? NativeTrack { get; init; }
    public bool HasSerializedTrack { get; init; }

    /// <summary>Original end-time key selection; input is unwrapped track time.</summary>
    public bool TryGetFrameIndex(float time, out int index)
    {
        index = -1;
        return NativeTrack is not null && NativeMethods.spv_anim_texture_index(NativeTrack, time, out index) != 0;
    }
}

/// <summary>
/// Shared spAnimTexControllerSerializer and spTextureTrack, with unresolved
/// texture IDs bound to document metadata. No name-based sequence or timing rule.
/// </summary>
public static class SmoAnimTextureControllerDecoder
{
    public static unsafe bool TryDecode(SmoDocument document, SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoAnimTextureControllerData? value, out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        value = null;
        error = string.Empty;
        if (entry.TypeHash != SmoClassIds.AnimTextureController || !entry.IsWithinDataSection ||
            !entry.SignatureMatches || entry.PhysicalOffset < 0 || entry.SerializedSize < 9 ||
            entry.PhysicalEnd > document.Data.Length || entry.SerializedSize > int.MaxValue)
        {
            error = "Animated texture inspector requires a complete catalogued controller.";
            return false;
        }
        var payload = document.Data.Span.Slice(checked((int)entry.PhysicalOffset + 8), checked((int)entry.SerializedSize - 8));
        AnimTextureHandle? handle = null;
        try
        {
            fixed (byte* bytes = payload)
                handle = new AnimTextureHandle(NativeMethods.Check(NativeMethods.spv_anim_texture_read(bytes, (uint)payload.Length)));
            NativeMethods.Check(NativeMethods.spv_anim_texture_info(handle, out var info));
            var nativeFrames = new NativeMethods.AnimTextureFrame[checked((int)info.Frames)];
            fixed (NativeMethods.AnimTextureFrame* frames = nativeFrames)
                NativeMethods.Check(NativeMethods.spv_anim_texture_frames(handle, frames, info.Frames));
            var result = new List<SmoAnimTextureFrame>(nativeFrames.Length);
            foreach (var frame in nativeFrames)
            {
                var span = frame.Reference;
                if ((ulong)span.Offset + span.Size > (ulong)payload.Length ||
                    !SmoNodeDecoder.TryDecodeRelationship(document, payload.Slice((int)span.Offset, (int)span.Size), out var texture) ||
                    texture is null || (texture.ObjectId != 0 &&
                    (!SmoNodeDecoder.TryGetCataloguedObject(document, texture.ObjectId, out var target) ||
                     target.TypeHash != SmoClassIds.TextureData || !target.IsWithinDataSection || !target.SignatureMatches)))
                {
                    error = "Animated texture frame metadata does not bind to TextureData or an explicit NULL.";
                    return false;
                }
                result.Add(new SmoAnimTextureFrame(frame.Time, texture));
            }
            value = new SmoAnimTextureControllerData(result.AsReadOnly(), info.Duration)
                { NativeTrack = handle, HasSerializedTrack = info.HasTrack != 0 };
            handle = null; // ownership transferred to the immutable view
            return true;
        }
        catch (InvalidDataException exception)
        {
            error = exception.Message;
            return false;
        }
        finally { handle?.Dispose(); }
    }
}
