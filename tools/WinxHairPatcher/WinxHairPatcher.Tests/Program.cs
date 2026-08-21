using System.Buffers.Binary;
using WinxHairPatcher.Core;

if (args.Length != 1 || !File.Exists(args[0]))
{
    Console.Error.WriteLine("Usage: WinxHairPatcher.Tests <WinxClub.exe>");
    return 2;
}

int checks = 0;
void Check(bool condition, string message)
{
    checks++;
    if (!condition) throw new InvalidOperationException("FAIL: " + message);
}

byte[] Hex(string value) => Convert.FromHexString(value);
byte[] Build(byte[] template, int maskOffset, ushort mask)
{
    byte[] result = template.ToArray();
    BinaryPrimitives.WriteUInt16LittleEndian(result.AsSpan(maskOffset, 2), mask);
    return result;
}

var gameplaySites = new[]
{
    new
    {
        Original = Hex("8B83E806000083F805747D83F8017478"),
        Patched = Hex("8B83E8060000BA000000000FA3C27278"),
        MaskOffset = 7,
        ControlSkip = true
    },
    new
    {
        Original = Hex("BFE4596F00B90F00000033C0F3A65F5E74078BCBE868BFFFFF"),
        Patched = Hex("8B83E8060000BA000000000FA3C25F5E72078BCBE868BFFFFF"),
        MaskOffset = 7,
        ControlSkip = false
    },
    new
    {
        Original = Hex("8B86E806000083F8050F84EC09000083F8010F84E3090000"),
        Patched = Hex("8B86E8060000BA000000000FA3C20F82E709000090909090"),
        MaskOffset = 7,
        ControlSkip = true
    }
};
byte[] menuOriginal = Hex(
    "83FD058986CC020000740E8B8ED4020000518BC8E881A3E3FF" +
    "518BC451C700000000008BC46A1F68FB2700008BCEC70000000000E80175E2FF");
byte[] menuPatched = Hex(
    "8986CC02000066BA0000660FA3EA720DFFB6D40200008BC8E87DA3E3FF" +
    "6A006A008BC46A1F68FB2700008BCE9090909090909090E80175E2FF");
const int MenuMaskOffset = 8;

List<int> FindExact(byte[] data, byte[] pattern)
{
    var offsets = new List<int>();
    int searchStart = 0;
    while (searchStart <= data.Length - pattern.Length)
    {
        int relative = data.AsSpan(searchStart).IndexOf(pattern);
        if (relative < 0) break;
        int offset = searchStart + relative;
        offsets.Add(offset);
        searchStart = offset + 1;
    }
    return offsets;
}

List<(int Offset, ushort Mask)> FindPatched(byte[] data, byte[] pattern, int maskOffset)
{
    var matches = new List<(int, ushort)>();
    ReadOnlySpan<byte> prefix = pattern.AsSpan(0, maskOffset);
    int searchStart = 0;
    while (searchStart <= data.Length - pattern.Length)
    {
        int relative = data.AsSpan(searchStart).IndexOf(prefix);
        if (relative < 0) break;
        int offset = searchStart + relative;
        if (offset <= data.Length - pattern.Length)
        {
            ReadOnlySpan<byte> candidate = data.AsSpan(offset, pattern.Length);
            if (candidate[(maskOffset + 2)..].SequenceEqual(pattern.AsSpan(maskOffset + 2)))
            {
                ushort mask = BinaryPrimitives.ReadUInt16LittleEndian(candidate.Slice(maskOffset, 2));
                matches.Add((offset, mask));
            }
        }
        searchStart = offset + 1;
    }
    return matches;
}

(int Offset, bool IsOriginal, ushort Mask) Locate(
    byte[] data,
    byte[] originalSignature,
    byte[] patchedSignature,
    int maskOffset)
{
    List<int> originals = FindExact(data, originalSignature);
    List<(int Offset, ushort Mask)> patched = FindPatched(data, patchedSignature, maskOffset);
    if (originals.Count + patched.Count != 1)
        throw new InvalidOperationException("Fixture does not contain one unambiguous original/patched signature.");
    return originals.Count == 1
        ? (originals[0], true, (ushort)0)
        : (patched[0].Offset, false, patched[0].Mask);
}

byte[] RestoreOriginal(byte[] source)
{
    byte[] result = source.ToArray();
    foreach (var site in gameplaySites)
    {
        var located = Locate(result, site.Original, site.Patched, site.MaskOffset);
        site.Original.CopyTo(result, located.Offset);
    }
    var menu = Locate(result, menuOriginal, menuPatched, MenuMaskOffset);
    menuOriginal.CopyTo(result, menu.Offset);
    return result;
}

byte[] BuildLegacyPatch(byte[] source, ushort disabledMask)
{
    byte[] result = RestoreOriginal(source);
    disabledMask |= WinxExeHairPatcher.OriginalDisabledMask;
    foreach (var site in gameplaySites)
    {
        int offset = Locate(result, site.Original, site.Patched, site.MaskOffset).Offset;
        ushort mask = site.ControlSkip
            ? (ushort)(disabledMask | WinxExeHairPatcher.OriginalControlSkipMask)
            : disabledMask;
        Build(site.Patched, site.MaskOffset, mask).CopyTo(result, offset);
    }
    return result;
}

byte[] fixture = File.ReadAllBytes(args[0]);
WinxExePatchState fixtureState = WinxExeHairPatcher.Inspect(fixture);
Check(fixtureState.IsSupported, "original or compatible patched executable is recognized");
Check(WinxExeHairPatcher.Fashions.Any(item => item.Id == 0 && item.ModelName == "bloom_jeans.smo"),
    "Jeans fashion ID 0 is exposed");
Check(WinxExeHairPatcher.Fashions.Single(item => item.Id == 1).CanEnableExternalHair,
    "Bloom X external hair is enabled by default");
Check(!WinxExeHairPatcher.Fashions.Single(item => item.Id == 5).CanEnableExternalHair,
    "Winter external hair remains unavailable");

byte[] original = RestoreOriginal(fixture);
WinxExePatchState originalState = WinxExeHairPatcher.Inspect(original);
Check(originalState.IsSupported && originalState.IsOriginal, "all four original sites are recognized");
Check(!originalState.HasCostumeMenuPatch, "original executable has no costume-menu patch");

ushort requested = (ushort)(WinxExeHairPatcher.OriginalDisabledMask | (1 << 0) | (1 << 13) | (1 << 15));
byte[] legacy = BuildLegacyPatch(original, requested);
WinxExePatchState legacyState = WinxExeHairPatcher.Inspect(legacy);
Check(legacyState.IsSupported && !legacyState.IsOriginal, "legacy gameplay-only patch is recognized");
Check(!legacyState.HasCostumeMenuPatch && legacyState.DisabledMask == requested,
    "legacy patch exposes its gameplay mask and pending menu upgrade");

byte[] patched = WinxExeHairPatcher.PatchBytes(legacy, requested);
WinxExePatchState patchedState = WinxExeHairPatcher.Inspect(patched);
Check(patchedState.IsSupported && !patchedState.IsOriginal, "full patched executable is recognized");
Check(patchedState.HasCostumeMenuPatch, "full patch includes the costume menu");
Check(patchedState.DisabledMask == requested, "gameplay and costume-menu mask round-trips");
Check(patched.Length == original.Length, "patch preserves executable size");
Check(!patched.SequenceEqual(legacy), "legacy patch is upgraded with costume-menu code");

var menuSite = Locate(patched, menuOriginal, menuPatched, MenuMaskOffset);
Check(!menuSite.IsOriginal && menuSite.Mask == requested, "costume-menu machine block stores the requested mask");
Check(patched.AsSpan(menuSite.Offset, menuPatched.Length)
        .SequenceEqual(Build(menuPatched, MenuMaskOffset, requested)),
    "costume-menu machine block matches the verified replacement");

var menuOriginalSite = Locate(original, menuOriginal, menuPatched, MenuMaskOffset);
var allowedRanges = gameplaySites
    .Select(site =>
    {
        var located = Locate(original, site.Original, site.Patched, site.MaskOffset);
        return (Start: located.Offset, End: located.Offset + site.Original.Length);
    })
    .Append((Start: menuOriginalSite.Offset, End: menuOriginalSite.Offset + menuOriginal.Length))
    .ToArray();
bool onlyKnownRangesChanged = true;
for (int index = 0; index < original.Length; index++)
{
    if (original[index] != patched[index] && !allowedRanges.Any(range => index >= range.Start && index < range.End))
    {
        onlyKnownRangesChanged = false;
        break;
    }
}
Check(onlyKnownRangesChanged, "patch changes bytes only inside the four verified signature ranges");

byte[] identical = WinxExeHairPatcher.PatchBytes(patched, requested);
Check(identical.SequenceEqual(patched), "reapplying the same mask is byte-for-byte idempotent");

ushort secondMask = (ushort)(requested | (1 << 2));
byte[] repatched = WinxExeHairPatcher.PatchBytes(patched, secondMask);
WinxExePatchState repatchedState = WinxExeHairPatcher.Inspect(repatched);
Check(repatchedState.DisabledMask == secondMask && repatchedState.HasCostumeMenuPatch,
    "compatible full patch updates both contexts");
Check(Locate(repatched, menuOriginal, menuPatched, MenuMaskOffset).Mask == secondMask,
    "costume-menu mask is updated on repatch");

byte[] forcedWinter = WinxExeHairPatcher.PatchBytes(original, 0);
WinxExePatchState forcedWinterState = WinxExeHairPatcher.Inspect(forcedWinter);
Check((forcedWinterState.DisabledMask & WinxExeHairPatcher.OriginalDisabledMask) != 0,
    "Winter remains disabled when the caller requests an empty mask");
Check((Locate(forcedWinter, menuOriginal, menuPatched, MenuMaskOffset).Mask & (1 << 5)) != 0,
    "Winter bit is also forced in the costume-menu block");

byte[] shifted = new byte[original.Length + 257];
original.CopyTo(shifted, 257);
Check(WinxExeHairPatcher.Inspect(shifted).IsSupported,
    "signature scan does not depend on fixed file offsets");
WinxExePatchState shiftedState = WinxExeHairPatcher.Inspect(
    WinxExeHairPatcher.PatchBytes(shifted, requested));
Check(shiftedState.DisabledMask == requested && shiftedState.HasCostumeMenuPatch,
    "all four shifted signatures are patched at their discovered offsets");

byte[] mismatched = patched.ToArray();
var mismatchedMenu = Locate(mismatched, menuOriginal, menuPatched, MenuMaskOffset);
BinaryPrimitives.WriteUInt16LittleEndian(
    mismatched.AsSpan(mismatchedMenu.Offset + MenuMaskOffset, 2),
    (ushort)(requested ^ (1 << 2)));
Check(!WinxExeHairPatcher.Inspect(mismatched).IsSupported,
    "different gameplay and costume-menu masks are rejected");

byte[] menuOnly = original.ToArray();
int originalMenuOffset = Locate(menuOnly, menuOriginal, menuPatched, MenuMaskOffset).Offset;
Build(menuPatched, MenuMaskOffset, requested).CopyTo(menuOnly, originalMenuOffset);
Check(!WinxExeHairPatcher.Inspect(menuOnly).IsSupported,
    "menu-only partial patch is rejected");

byte[] corruptedMenu = original.ToArray();
int corruptedMenuOffset = Locate(corruptedMenu, menuOriginal, menuPatched, MenuMaskOffset).Offset;
corruptedMenu[corruptedMenuOffset] ^= 1;
Check(!WinxExeHairPatcher.Inspect(corruptedMenu).IsSupported,
    "unknown costume-menu code signature is rejected");

byte[] duplicatedMenu = new byte[original.Length + menuOriginal.Length];
original.CopyTo(duplicatedMenu, 0);
menuOriginal.CopyTo(duplicatedMenu, original.Length);
Check(!WinxExeHairPatcher.Inspect(duplicatedMenu).IsSupported,
    "ambiguous duplicate costume-menu signature is rejected");

byte[] corruptedGameplay = original.ToArray();
int attachmentOffset = Locate(
    corruptedGameplay,
    gameplaySites[0].Original,
    gameplaySites[0].Patched,
    gameplaySites[0].MaskOffset).Offset;
corruptedGameplay[attachmentOffset] ^= 1;
Check(!WinxExeHairPatcher.Inspect(corruptedGameplay).IsSupported,
    "unknown gameplay code signature is rejected");

Console.WriteLine($"PASS: {checks} assertions; mask=0x{requested:X4}; bytes={original.Length}");
return 0;
