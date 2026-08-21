using System.Buffers.Binary;

namespace WinxHairPatcher.Core;

public sealed record BloomFashion(int Id, string DisplayName, string ModelName, bool CanEnableExternalHair = true);
public sealed record WinxExePatchState(bool IsSupported, bool IsOriginal, ushort DisabledMask, string Description)
{
    public bool HasCostumeMenuPatch { get; init; }
}
public sealed record WinxExePatchResult(string ExePath, string BackupPath, ushort DisabledMask);

public static class WinxExeHairPatcher
{
    public const ushort OriginalDisabledMask = 1 << 5;
    public const ushort OriginalControlSkipMask = (1 << 1) | (1 << 5);

    public static IReadOnlyList<BloomFashion> Fashions { get; } =
    [
        new(0,  "Jeans",     "bloom_jeans.smo"),
        new(1,  "Bloom X",   "bloomx.smo"),
        new(2,  "Ball",      "bloom_ball.smo"),
        new(3,  "Silk",      "bloom_silk.smo"),
        new(4,  "Hawaii",    "bloom_hawaii.smo"),
        new(5,  "Winter",    "bloom_snow.smo", false),
        new(6,  "Crystal",   "bloom_crystal.smo"),
        new(7,  "Bike",      "bloom_bike.smo"),
        new(8,  "Goth",      "bloom_goth.smo"),
        new(9,  "Hip-hop",   "bloom_hiphop.smo"),
        new(10, "Punk",      "bloom_punk.smo"),
        new(11, "Business",  "bloom_business.smo"),
        new(12, "Hippie",    "bloom_hippie.smo"),
        new(13, "Jungle",    "bloom_jungle.smo"),
        new(14, "Princess",  "bloom_princess.smo"),
        new(15, "School",    "bloom_school.smo")
    ];

    private enum MaskKind
    {
        Disabled,
        ControlSkip
    }

    private sealed record Site(
        string Name, byte[] Original, Func<ushort, byte[]> Patched, int MaskOffset, MaskKind MaskKind);

    private sealed record LocatedSite(Site Site, int Offset, bool IsOriginal, ushort Mask);

    private static readonly Site[] GameplaySites =
    [
        new("attachment lookup",
            Hex("8B83E806000083F805747D83F8017478"),
            mask => Build("8B83E8060000BA000000000FA3C27278", 7, mask), 7, MaskKind.ControlSkip),
        new("hair setup gate",
            Hex("BFE4596F00B90F00000033C0F3A65F5E74078BCBE868BFFFFF"),
            mask => Build("8B83E8060000BA000000000FA3C25F5E72078BCBE868BFFFFF", 7, mask), 7, MaskKind.Disabled),
        new("runtime update",
            Hex("8B86E806000083F8050F84EC09000083F8010F84E3090000"),
            mask => Build("8B86E8060000BA000000000FA3C20F82E709000090909090", 7, mask), 7, MaskKind.ControlSkip)
    ];

    private static readonly Site[] CostumeMenuSites =
    [
        // The outfit menu has its own Bloom model and its own Hair_Master -> Head
        // attachment. The replacement keeps the original event call and stores the
        // same 16-bit disabled mask directly in `mov dx, imm16` for later updates.
        new("costume menu hair attachment",
            Hex("83FD058986CC020000740E8B8ED4020000518BC8E881A3E3FF518BC451C700000000008BC46A1F68FB2700008BCEC70000000000E80175E2FF"),
            mask => Build("8986CC02000066BA0000660FA3EA720DFFB6D40200008BC8E87DA3E3FF6A006A008BC46A1F68FB2700008BCE9090909090909090E80175E2FF", 8, mask),
            8,
            MaskKind.Disabled)
    ];

    public static WinxExePatchState Inspect(string path) => Inspect(File.ReadAllBytes(path));

    public static WinxExePatchState Inspect(ReadOnlySpan<byte> data)
    {
        if (!TryLocateSites(data, GameplaySites, out LocatedSite[] gameplay, out string gameplayError))
            return Unsupported(gameplayError);
        if (!TryLocateSites(data, CostumeMenuSites, out LocatedSite[] costumeMenu, out string costumeMenuError))
            return Unsupported(costumeMenuError);

        bool gameplayOriginal = gameplay.All(match => match.IsOriginal);
        bool costumeMenuOriginal = costumeMenu.All(match => match.IsOriginal);
        if (!gameplayOriginal && gameplay.Any(match => match.IsOriginal))
            return Unsupported("Найден частично применённый или несовместимый gameplay hair-патч.");
        if (!costumeMenuOriginal && costumeMenu.Any(match => match.IsOriginal))
            return Unsupported("Найден частично применённый или несовместимый hair-патч меню костюмов.");

        if (gameplayOriginal && costumeMenuOriginal)
            return new(true, true, OriginalDisabledMask,
                "Найдены все оригинальные gameplay- и costume-menu hair-сигнатуры WinxClub.exe.");

        if (gameplayOriginal)
            return Unsupported("Hair-патч найден только в меню костюмов; gameplay-блоки остались оригинальными.");

        if (!TryReadDisabledMask(gameplay, "gameplay", out ushort gameplayMask, out string gameplayMaskError))
            return Unsupported(gameplayMaskError);

        if (costumeMenuOriginal)
            return new(true, false, gameplayMask,
                "EXE содержит совместимый hair-патч 0.1.x только для игрового режима. Повторное применение добавит поддержку меню костюмов.");

        if (!TryReadDisabledMask(costumeMenu, "меню костюмов", out ushort costumeMenuMask, out string costumeMenuMaskError))
            return Unsupported(costumeMenuMaskError);
        if (gameplayMask != costumeMenuMask)
            return Unsupported("Gameplay и меню костюмов содержат разные маски отключения волос.");

        return new(true, false, gameplayMask,
            "EXE содержит совместимый hair-патч для игрового режима и меню костюмов.")
        {
            HasCostumeMenuPatch = true
        };
    }

    public static byte[] PatchBytes(ReadOnlySpan<byte> source, ushort disabledMask)
    {
        WinxExePatchState state = Inspect(source);
        if (!state.IsSupported) throw new InvalidDataException(state.Description);
        disabledMask |= OriginalDisabledMask;
        ushort controlSkipMask = (ushort)(disabledMask | OriginalControlSkipMask);
        byte[] result = source.ToArray();
        foreach (Site site in GameplaySites.Concat(CostumeMenuSites))
        {
            LocatedSite match = FindMatches(source, site).Single();
            ushort siteMask = site.MaskKind == MaskKind.Disabled ? disabledMask : controlSkipMask;
            site.Patched(siteMask).CopyTo(result, match.Offset);
        }
        WinxExePatchState verified = Inspect(result);
        if (!verified.IsSupported || verified.IsOriginal || !verified.HasCostumeMenuPatch ||
            verified.DisabledMask != disabledMask)
            throw new InvalidDataException("Проверка записанного патча завершилась неудачно.");
        return result;
    }

    public static WinxExePatchResult PatchFile(string path, ushort disabledMask)
    {
        string exePath = Path.GetFullPath(path);
        byte[] source = File.ReadAllBytes(exePath);
        byte[] patched = PatchBytes(source, disabledMask);
        string stamp = DateTime.Now.ToString("yyyyMMdd-HHmmss");
        string backup = exePath + $".hairpatch-backup-{stamp}";
        string temporary = exePath + $".hairpatch-{Guid.NewGuid():N}.tmp";
        File.WriteAllBytes(temporary, patched);
        try { File.Replace(temporary, exePath, backup, true); }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
        return new(exePath, backup, (ushort)(disabledMask | OriginalDisabledMask));
    }

    private static byte[] Build(string hex, int maskOffset, ushort mask)
    {
        byte[] bytes = Hex(hex);
        BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(maskOffset, 2), mask);
        return bytes;
    }

    private static byte[] Hex(string value) => Convert.FromHexString(value);

    private static WinxExePatchState Unsupported(string description) => new(false, false, 0, description);

    private static bool TryLocateSites(
        ReadOnlySpan<byte> data,
        IReadOnlyList<Site> sites,
        out LocatedSite[] located,
        out string error)
    {
        var matchesBySite = new List<LocatedSite>(sites.Count);
        foreach (Site site in sites)
        {
            IReadOnlyList<LocatedSite> matches = FindMatches(data, site);
            if (matches.Count != 1)
            {
                located = [];
                error = matches.Count == 0
                    ? $"Не найден однозначный блок {site.Name}."
                    : $"Блок {site.Name} найден несколько раз; автоматический патч небезопасен.";
                return false;
            }
            matchesBySite.Add(matches[0]);
        }

        located = matchesBySite.ToArray();
        error = string.Empty;
        return true;
    }

    private static bool TryReadDisabledMask(
        IReadOnlyList<LocatedSite> located,
        string familyName,
        out ushort disabledMask,
        out string error)
    {
        ushort? disabled = null;
        ushort? controlSkip = null;
        foreach (LocatedSite match in located)
        {
            ref ushort? current = ref match.Site.MaskKind == MaskKind.Disabled
                ? ref disabled
                : ref controlSkip;
            if (current.HasValue && current.Value != match.Mask)
            {
                disabledMask = 0;
                error = $"Сигнатуры {familyName} содержат разные маски: другая версия EXE или посторонний патч.";
                return false;
            }
            current = match.Mask;
        }

        if (!disabled.HasValue ||
            (disabled.Value & OriginalDisabledMask) == 0 ||
            (controlSkip.HasValue && controlSkip.Value != (ushort)(disabled.Value | OriginalControlSkipMask)))
        {
            disabledMask = 0;
            error = $"Hair-патч {familyName} содержит несовместимые маски управления.";
            return false;
        }

        disabledMask = disabled.Value;
        error = string.Empty;
        return true;
    }

    private static IReadOnlyList<LocatedSite> FindMatches(ReadOnlySpan<byte> data, Site site)
    {
        var matches = new List<LocatedSite>();
        int searchStart = 0;
        while (searchStart <= data.Length - site.Original.Length)
        {
            int relative = data[searchStart..].IndexOf(site.Original);
            if (relative < 0) break;
            int offset = searchStart + relative;
            matches.Add(new(site, offset, true, 0));
            searchStart = offset + 1;
        }

        byte[] patchedZero = site.Patched(0);
        ReadOnlySpan<byte> patchedPrefix = patchedZero.AsSpan(0, site.MaskOffset);
        searchStart = 0;
        while (searchStart <= data.Length - patchedZero.Length)
        {
            int relative = data[searchStart..].IndexOf(patchedPrefix);
            if (relative < 0) break;
            int offset = searchStart + relative;
            if (offset <= data.Length - patchedZero.Length)
            {
                ReadOnlySpan<byte> candidate = data.Slice(offset, patchedZero.Length);
                if (candidate[(site.MaskOffset + 2)..].SequenceEqual(patchedZero.AsSpan(site.MaskOffset + 2)))
                {
                    ushort mask = BinaryPrimitives.ReadUInt16LittleEndian(candidate.Slice(site.MaskOffset, 2));
                    matches.Add(new(site, offset, false, mask));
                }
            }
            searchStart = offset + 1;
        }
        return matches;
    }
}
