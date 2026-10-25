using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>
/// Platform boundary shared by production model writers. The current writer
/// contracts were proven against PC/common FFPS profiles only; PS2 remains a
/// read-only research/import source until its serializer has an independent
/// native write matrix.
/// </summary>
internal static class SmoProductionPlatformGuard
{
    private const uint CommonPlatform = 0x1;
    private const uint PcPlatform = 0x2;
    private const uint SupportedMask = CommonPlatform | PcPlatform;

    public static void EnsurePcWritable(
        SmoDocument document,
        string description = "Target SMO")
    {
        ArgumentNullException.ThrowIfNull(document);
        string? issue = GetPcWriteIssue(document.Header.PlatformMask, description);
        if (issue is not null)
            throw new NotSupportedException(issue);
    }

    public static string? GetPcWriteIssue(
        uint platformMask,
        string description = "Target SMO")
    {
        bool selectsPcRuntime = (platformMask & SupportedMask) != 0;
        bool containsUnprovenPlatform = (platformMask & ~SupportedMask) != 0;
        if (selectsPcRuntime && !containsUnprovenPlatform)
            return null;

        return $"{description} uses FFPS platform mask 0x{platformMask:X}. " +
               "Production model writing is enabled only for the proven PC/common " +
               "profiles 0x1, 0x2 and 0x3. PS2 and unknown profiles are read-only.";
    }
}
