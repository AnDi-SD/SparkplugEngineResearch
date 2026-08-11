using System.Buffers.Binary;

namespace WinxHairPatcher.Core;

public sealed record BloomFashion(int Id, string DisplayName, string ModelName, bool CanEnableExternalHair = true);
public sealed record WinxExePatchState(bool IsSupported, bool IsOriginal, ushort DisabledMask, string Description);
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

    private sealed record Site(
        string Name, byte[] Original, Func<ushort, byte[]> Patched, int MaskOffset, bool IsSetupGate);

    private sealed record LocatedSite(Site Site, int Offset, bool IsOriginal, ushort Mask);

    private static readonly Site[] Sites =
    [
        new("attachment lookup",
            Hex("8B83E806000083F805747D83F8017478"),
            mask => Build("8B83E8060000BA000000000FA3C27278", 7, mask), 7, false),
        new("hair setup gate",
            Hex("BFE4596F00B90F00000033C0F3A65F5E74078BCBE868BFFFFF"),
            mask => Build("8B83E8060000BA000000000FA3C25F5E72078BCBE868BFFFFF", 7, mask), 7, true),
        new("runtime update",
            Hex("8B86E806000083F8050F84EC09000083F8010F84E3090000"),
            mask => Build("8B86E8060000BA000000000FA3C20F82E709000090909090", 7, mask), 7, false)
    ];

    public static WinxExePatchState Inspect(string path) => Inspect(File.ReadAllBytes(path));

    public static WinxExePatchState Inspect(ReadOnlySpan<byte> data)
    {
        var located = new List<LocatedSite>(Sites.Length);
        foreach (Site site in Sites)
        {
            IReadOnlyList<LocatedSite> matches = FindMatches(data, site);
            if (matches.Count != 1)
                return new(false, false, 0, matches.Count == 0
                    ? $"Не найден однозначный блок {site.Name}."
                    : $"Блок {site.Name} найден несколько раз; автоматический патч небезопасен.");
            located.Add(matches[0]);
        }

        if (located.All(match => match.IsOriginal))
            return new(true, true, OriginalDisabledMask,
                "Найдены все три оригинальные hair-сигнатуры WinxClub.exe.");
        if (located.Any(match => match.IsOriginal))
            return new(false, false, 0, "Найден частично применённый или несовместимый hair-патч.");

        ushort? setupMask = null;
        ushort? controlMask = null;
        foreach (LocatedSite match in located)
        {
            ushort? previous = match.Site.IsSetupGate ? setupMask : controlMask;
            if (previous.HasValue && previous.Value != match.Mask)
                return new(false, false, 0, "Сигнатуры не совпадают: другая версия EXE или посторонний патч.");
            if (match.Site.IsSetupGate) setupMask = match.Mask; else controlMask = match.Mask;
        }
        if (!setupMask.HasValue || !controlMask.HasValue ||
            controlMask.Value != (ushort)(setupMask.Value | OriginalControlSkipMask))
            return new(false, false, 0, "Hair-патч содержит несовместимые маски управления.");
        return new(true, false, setupMask.Value, "EXE уже содержит совместимый hair-патч.");
    }

    public static byte[] PatchBytes(ReadOnlySpan<byte> source, ushort disabledMask)
    {
        WinxExePatchState state = Inspect(source);
        if (!state.IsSupported) throw new InvalidDataException(state.Description);
        disabledMask |= OriginalDisabledMask;
        ushort controlSkipMask = (ushort)(disabledMask | OriginalControlSkipMask);
        byte[] result = source.ToArray();
        foreach (Site site in Sites)
        {
            LocatedSite match = FindMatches(source, site).Single();
            site.Patched(site.IsSetupGate ? disabledMask : controlSkipMask).CopyTo(result, match.Offset);
        }
        WinxExePatchState verified = Inspect(result);
        if (!verified.IsSupported || verified.DisabledMask != disabledMask)
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
