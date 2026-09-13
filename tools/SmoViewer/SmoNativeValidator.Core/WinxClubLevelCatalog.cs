namespace SmoNativeValidator.Core;

public enum WinxClubLevelAvailability
{
    Available,
    Unavailable,
    Incomplete
}

public sealed record WinxClubLevelInfo
{
    public required int Id { get; init; }
    public required string InternalName { get; init; }
    public required string DisplayName { get; init; }
    public required WinxClubLevelAvailability Availability { get; init; }
    public string? Warning { get; init; }
    public bool CanStart => Availability == WinxClubLevelAvailability.Available;
}

/// <summary>
/// Native <c>startLevel</c> values recovered from the game's own level table.
/// Slots 38-40 have empty level descriptors, and Gardenia04 (50) has no SMO.
/// </summary>
public static class WinxClubLevelCatalog
{
    private static readonly IReadOnlyList<WinxClubLevelInfo> KnownLevels = BuildLevels();
    private static readonly IReadOnlyDictionary<int, WinxClubLevelInfo> LevelsById =
        KnownLevels.ToDictionary(level => level.Id);
    private static readonly IReadOnlyDictionary<string, int> StartLevelsByLogicalSmo =
        BuildLogicalSmoMap();

    public static IReadOnlyList<WinxClubLevelInfo> Levels => KnownLevels;

    public static bool TryGet(int id, out WinxClubLevelInfo? level)
    {
        if (LevelsById.TryGetValue(id, out WinxClubLevelInfo? found))
        {
            level = found;
            return true;
        }
        level = null;
        return false;
    }

    public static WinxClubLevelInfo Get(int id) =>
        TryGet(id, out WinxClubLevelInfo? level)
            ? level!
            : throw new ArgumentOutOfRangeException(
                nameof(id), id, "startLevel must be between 1 and 50.");

    public static bool CanStart(int id) =>
        TryGet(id, out WinxClubLevelInfo? level) && level!.CanStart;

    public static string? GetWarning(int id) =>
        TryGet(id, out WinxClubLevelInfo? level)
            ? level!.Warning
            : "Unknown startLevel. Valid native IDs are 1 through 50.";

    public static bool TryResolveStartLevelForLogicalSmo(
        string logicalGameAssetPath,
        out int startLevel)
    {
        startLevel = 0;
        if (string.IsNullOrWhiteSpace(logicalGameAssetPath))
            return false;
        string normalized = LogicalAssetMatcher.Normalize(logicalGameAssetPath)
            .TrimStart('\\');
        if (normalized.StartsWith("Media\\", StringComparison.OrdinalIgnoreCase))
            normalized = normalized["Media\\".Length..];
        return StartLevelsByLogicalSmo.TryGetValue(normalized, out startLevel);
    }

    private static IReadOnlyList<WinxClubLevelInfo> BuildLevels()
    {
        string[] names =
        [
            "Gardenia01", "Gardenia02", "Gardenia03",
            "Domino01", "Domino02", "Domino03", "Domino04", "Domino05",
            "BMS01", "BMS02", "BMS03", "BMS04", "BMS05",
            "Cloud01_01", "Cloud01_02", "Cloud01_03",
            "Cloud02_01", "Cloud02_02", "Cloud02_03", "Cloud02_04", "Cloud02_05", "Cloud02_06",
            "RedF_01", "RedF_02", "RedF_03", "RedF_04",
            "Alfea01_01", "Alfea01_02", "Alfea01_03",
            "AlfeaNight_01", "AlfeaNight_02", "AlfeaNight_03",
            "AlfeaBroken_01", "AlfeaBroken_02", "AlfeaBroken_03",
            "Sky01", "Sky02"
        ];

        List<WinxClubLevelInfo> result = new(50);
        for (int index = 0; index < names.Length; index++)
            result.Add(Available(index + 1, names[index]));

        for (int id = 38; id <= 40; id++)
        {
            result.Add(new WinxClubLevelInfo
            {
                Id = id,
                InternalName = string.Empty,
                DisplayName = $"{id} — unavailable slot",
                Availability = WinxClubLevelAvailability.Unavailable,
                Warning = "The native level descriptor has an empty SPL path; the game cannot start this slot."
            });
        }

        string[] challengeNames =
        [
            "star_01", "star_02", "star_03",
            "race_01", "race_02", "race_03",
            "battle_01", "battle_02", "battle_03"
        ];
        for (int index = 0; index < challengeNames.Length; index++)
            result.Add(Available(index + 41, challengeNames[index]));

        result.Add(new WinxClubLevelInfo
        {
            Id = 50,
            InternalName = "Gardenia04",
            DisplayName = "50 — Gardenia04",
            Availability = WinxClubLevelAvailability.Incomplete,
            Warning = "The PC corpus has Gardenia04 SPL/SPT data but no level SMO; " +
                      "the PS2 corpus has no Gardenia04 triplet. Native startup cannot complete."
        });
        return result.AsReadOnly();
    }

    private static IReadOnlyDictionary<string, int> BuildLogicalSmoMap()
    {
        var result = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        AddNumbered(result, @"Levels\Gardenia\Gardenia{0:D2}.smo", 1, 3, 1);
        AddNumbered(result, @"Levels\Domino\Domino{0:D2}.smo", 1, 5, 4);
        AddNumbered(result, @"Levels\Swamp\BMS_{0:D2}.smo", 1, 5, 9);
        AddNumbered(result, @"Levels\Cloud01\cloud01_{0:D2}.smo", 1, 3, 14);
        AddNumbered(result, @"Levels\Cloud02\cloud02_{0:D2}.smo", 1, 6, 17);
        AddNumbered(result, @"Levels\RedF\RedF{0:D2}.smo", 1, 4, 23);
        AddNumbered(result, @"Levels\Alfea\Alfea{0:D2}.smo", 1, 3, 27);
        AddNumbered(result, @"Levels\Alfea\Alfea_night_{0:D2}.smo", 1, 3, 30);
        AddNumbered(result, @"Levels\Alfea\Alfea_broken_{0:D2}.smo", 1, 3, 33);
        AddNumbered(result, @"Levels\Sky\Sky_{0:D2}.smo", 1, 2, 36);
        AddNumbered(result, @"Levels\Challenges\star_{0:D2}.smo", 1, 3, 41);
        AddNumbered(result, @"Levels\Challenges\race_{0:D2}.smo", 1, 3, 44);
        AddNumbered(result, @"Levels\Challenges\battle_{0:D2}.smo", 1, 3, 47);
        return result;
    }

    private static void AddNumbered(
        Dictionary<string, int> result,
        string pathFormat,
        int firstNumber,
        int count,
        int firstLevel)
    {
        for (int offset = 0; offset < count; offset++)
        {
            result.Add(
                string.Format(
                    System.Globalization.CultureInfo.InvariantCulture,
                    pathFormat,
                    firstNumber + offset),
                firstLevel + offset);
        }
    }

    private static WinxClubLevelInfo Available(int id, string internalName) => new()
    {
        Id = id,
        InternalName = internalName,
        DisplayName = $"{id} — {internalName}",
        Availability = WinxClubLevelAvailability.Available
    };
}
