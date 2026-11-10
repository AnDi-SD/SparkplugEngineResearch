using System.Globalization;
using System.Text.Json.Nodes;

namespace WinxCoop.SessionHost;

// Opaque bytes produced by the original Host capture/writer. The existing
// original LoadSave remains the only save decoder/loader in the runtime path.
internal sealed class NativeCheckpointStore
{
    private readonly string _slot, _directory;
    private byte[]? _bytes;
    private object? _evidence;
    internal string? Sha256 { get; private set; }
    internal bool Available => _bytes is not null && Sha256 is not null;
    internal byte[]? Bytes => _bytes is null ? null : (byte[])_bytes.Clone();
    internal object? Evidence => _evidence;

    internal NativeCheckpointStore(GameProfile game, string caseDirectory)
    { _slot = Path.Combine(game.Media, "Saved", "Saved00.wxs"); _directory = Path.Combine(caseDirectory, "checkpoints"); }

    internal void Capture(RoleActor actor, ulong operation)
    {
        Require(actor.Role == "host" && operation != 0, "Only the current Host may preserve an original checkpoint.");
        JsonObject completed = actor.Last("progression-save-capture-writer-completed") ?? throw new InvalidDataException("Original checkpoint writer has not completed.");
        JsonObject captured = actor.Last("native-save-point-probe-capture-returned") ?? throw new InvalidDataException("Original current-state capture has not returned.");
        Require(NativeRetirementReceipts.B(completed, "originalWriterSuccessAlOneObserved") &&
            NativeRetirementReceipts.B(completed, "dispatcherPageReleasedAtLaterWorld") &&
            NativeRetirementReceipts.B(captured, "wholeOriginalCaptureCallbackReturned") &&
            NativeRetirementReceipts.B(captured, "selectedTupleReadbackVerified") && captured["actualSavedTuple"] is JsonObject,
            "Checkpoint lacks its actual capture, paired successful writer or later dispatcher-page release.");
        RuntimeBundle.Ordinary(Path.GetDirectoryName(_slot)!, true); RuntimeBundle.Ordinary(_slot, false);
        byte[] bytes;
        using (var input = new FileStream(_slot, FileMode.Open, FileAccess.Read, FileShare.Read))
        {
            Require(input.Length == 349, "This runtime admits only the qualified349-byte native checkpoint cohort.");
            bytes = new byte[349]; input.ReadExactly(bytes);
        }
        Directory.CreateDirectory(_directory); RuntimeBundle.Ordinary(_directory, true);
        string preserved = Path.Combine(_directory, "operation-" + operation.ToString("D8", CultureInfo.InvariantCulture) + "-Saved00.wxs");
        using (var output = new FileStream(preserved, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
        { output.Write(bytes, 0, bytes.Length); output.Flush(flushToDisk: true); }
        Require(File.ReadAllBytes(preserved).AsSpan().SequenceEqual(bytes) && File.ReadAllBytes(_slot).AsSpan().SequenceEqual(bytes),
            "Native checkpoint changed during exact preservation/readback.");
        _bytes = bytes; Sha256 = NativeRetirementReceipts.Hash(bytes);
        _evidence = new
        {
            operation, source = _slot, preserved, sha256 = Sha256, fileBytes = bytes.Length,
            originalNativeCapture = captured, originalNativeWriter = completed, rawFileReadbackExact = true,
            saveCodecDuplicated = false, wholeOriginalLoaderRequired = true, allSavedFieldsQualified = false
        };
    }

    internal void VerifySlot()
    {
        Require(Available, "No accepted native checkpoint belongs to this session.");
        RuntimeBundle.Ordinary(_slot, false);
        using var input = new FileStream(_slot, FileMode.Open, FileAccess.Read, FileShare.Read);
        Require(input.Length == 349, "Accepted native checkpoint changed size.");
        byte[] bytes = new byte[349]; input.ReadExactly(bytes);
        Require(bytes.AsSpan().SequenceEqual(_bytes) && NativeRetirementReceipts.Hash(bytes) == Sha256,
            "The prepared game's native slot no longer matches the accepted checkpoint; it will not be merged or overwritten.");
    }

    internal void VerifyNativeLoadRequest(RoleActor actor)
    {
        VerifySlot();
        JsonObject request = actor.Last("progression-native-saved-reload-requested") ?? throw new InvalidDataException("Original whole-checkpoint load was not requested.");
        Require(NativeRetirementReceipts.S(request, "fileSha256") == Sha256 &&
            NativeRetirementReceipts.B(request, "wholeOriginalGatewayRequested") && NativeRetirementReceipts.U(request, "nativeSaveSlot") == 0,
            "Original LoadSave request did not consume the exact accepted native checkpoint.");
    }

    private static void Require(bool value, string message) => SessionConfiguration.Require(value, message);
}
