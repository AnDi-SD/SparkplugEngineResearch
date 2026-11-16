using System.Security.Cryptography;
using System.Text;

namespace WinxCoop;

// A local admission record, compiled from this single source into Actor and UI.
// It is not a network codec and carries no native pointer or readiness claim.
internal sealed record CampaignSceneProfile
{
    internal const string Name = "campaign-prototype";
    internal const string BodyProfile = "campaign-network-input";
    internal const string OriginalImage = "3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F";
    internal const uint FirstLevel = 1;
    public uint Version { get; init; } = 1;
    public uint InitialLevel { get; init; } = FirstLevel;
    public uint Level { get; init; } = FirstLevel;
    public uint TransitionOrdinal { get; init; }
    public string DescriptorSha256 { get; init; } = InitialDescriptorSha256;

    internal static string InitialDescriptorSha256 => Convert.ToHexString(SHA256.HashData(
        Encoding.ASCII.GetBytes("WinxCoop.PC." + OriginalImage +
            ".CampaignPrototype.v1\n1\n0\nLevels/Gardenia/Gardenia01.smo\n" +
            "Levels/Gardenia/Gardenia01.spt\nLevels/Gardenia/Gardenia01.spl\n")));

    internal void Validate()
    {
        // First coherent draft: initial Gardenia1 only. The later destination
        // extension must derive its permit from the actual typed Host exit.
        if (Version != 1 || InitialLevel != FirstLevel || Level != FirstLevel ||
            TransitionOrdinal != 0 || DescriptorSha256 != InitialDescriptorSha256)
            throw new InvalidDataException("Campaign entry lacks its exact initial Gardenia1 descriptor.");
    }

    internal bool Admits(uint level)
    {
        Validate();
        return level == Level;
    }
}
