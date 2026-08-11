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

byte[] original = File.ReadAllBytes(args[0]);
WinxExePatchState originalState = WinxExeHairPatcher.Inspect(original);
Check(originalState.IsSupported, "original or compatible patched executable is recognized");
Check(WinxExeHairPatcher.Fashions.Any(item => item.Id == 0 && item.ModelName == "bloom_jeans.smo"),
    "Jeans fashion ID 0 is exposed");
Check(WinxExeHairPatcher.Fashions.Single(item => item.Id == 1).CanEnableExternalHair,
    "Bloom X external hair is enabled by default");

ushort requested = (ushort)(WinxExeHairPatcher.OriginalDisabledMask | (1 << 0) | (1 << 13) | (1 << 14));
byte[] patched = WinxExeHairPatcher.PatchBytes(original, requested);
WinxExePatchState patchedState = WinxExeHairPatcher.Inspect(patched);
Check(patchedState.IsSupported && !patchedState.IsOriginal, "patched executable is recognized");
Check(patchedState.DisabledMask == requested, "fashion mask round-trips");
Check(patched.Length == original.Length, "patch preserves executable size");
Check(!patched.SequenceEqual(original), "patch changes executable bytes");

ushort secondMask = (ushort)(requested | (1 << 2));
byte[] repatched = WinxExeHairPatcher.PatchBytes(patched, secondMask);
Check(WinxExeHairPatcher.Inspect(repatched).DisabledMask == secondMask, "compatible patch can be updated");

byte[] corrupted = original.ToArray();
corrupted[0x000E7D97] ^= 1;
Check(!WinxExeHairPatcher.Inspect(corrupted).IsSupported, "unknown code signature is rejected");

Console.WriteLine($"PASS: {checks} assertions; mask=0x{requested:X4}; bytes={original.Length}");
return 0;
