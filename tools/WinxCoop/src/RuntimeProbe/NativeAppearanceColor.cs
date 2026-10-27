using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal sealed record AppearancePreparation(byte[] RgbBytes, uint Level, uint ProfileOffset,
    float[] InputRgb, float[] ExpectedAmbient, uint[] OwnMaterials);

/// <summary>
/// Own stopped-process gates around the separately qualified stock AlphaManager
/// color call. This adapter only reads: no original color packing, setter,
/// recursive producer, remote writes or native calls are implemented here.
/// </summary>
internal sealed class NativeAppearanceColor
{
    private const uint SkinTable = 0x6E8C5C, MaterialTable = 0x6EF264, MaterialColorTable = 0x6EF238;
    private readonly DebuggerHost _host;
    private readonly nint _process;
    private readonly uint _pid;
    private readonly int _ownerThread;
    private readonly Dictionary<uint, byte[]> _localBefore = [];
    private uint[] _ownMaterials = [];
    private byte[] _expectedAmbient = [];
    private bool _prepared, _verified;
    private int _reads, _bytes;

    internal NativeAppearanceColor(DebuggerHost host)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _process = host.ProcessHandle; _pid = host.ProcessPid;
        _ownerThread = Environment.CurrentManagedThreadId;
        EnsureAtStop();
    }

    internal AppearancePreparation Prepare(uint profile, uint[] localRenderNodes,
        uint[] remoteRenderNodes, uint[] ownSkins)
    {
        EnsureAtStop();
        Require(!_prepared, "Appearance preparation is one-shot");
        ValidateList(localRenderNodes, "local RenderNode", 128);
        ValidateList(remoteRenderNodes, "remote RenderNode", 128);
        ValidateList(ownSkins, "owned Skin", 128);
        Require(U(0x765AD4) == profile && U(profile) == 0x6F5A44,
            "Appearance color requires the observed current PlayerProfile");
        uint flow = U(0x755294), level = U(checked(flow + 0x1B0));
        uint profileOffset = level is >= 1 and <= 49 ? checked(0x6F0u + level * 12) : 0x6FCu;
        // The stock profile wrapper chooses exactly this RGB row. Do not
        // duplicate AlphaManager's ARGB packing/normalization in the adapter.
        byte[] rgb = Read(checked(profile + profileOffset), 12);
        float[] inputRgb = Floats(rgb);
        var local = ReadSkins(localRenderNodes, false, null);
        var remote = ReadSkins(remoteRenderNodes, true, ownSkins.ToHashSet());
        Require(remote.Keys.ToHashSet().SetEquals(ownSkins),
            "Owned Skin inventory does not equal all remote RenderNode Skin references");
        var allLocalMaterials = local.Values.Select(s => s.Material).ToHashSet();
        foreach (uint skin in ownSkins)
        {
            Require(!local.ContainsKey(skin), "Owned Skin aliases a local Skin");
            SkinInfo own = remote[skin];
            SkinInfo[] matches = local.Values.Where(s => s.Mesh == own.Mesh).ToArray();
            Require(matches.Length == 1, "Owned Skin has no unique local counterpart with the same mesh identity");
            Require(!allLocalMaterials.Contains(own.Material), "Owned material aliases a local Skin material");
            byte[] ownMaterial = Material(own.Material), localMaterial = Material(matches[0].Material);
            byte[] ambient = localMaterial.AsSpan(0x88, 16).ToArray();
            float[] expected = Floats(ambient);
            Require(BitConverter.ToUInt32(ambient, 12) == 0x3F800000,
                "Qualified local material ambient must have alpha exactly one");
            if (_expectedAmbient.Length == 0) _expectedAmbient = ambient;
            else Require(_expectedAmbient.SequenceEqual(ambient),
                "Matched local Skin materials do not share one exact ambient color");
            _localBefore[matches[0].Material] = localMaterial;
            // Reading the full qualified own material proves both physical
            // vtables before the caller invokes the original subtree producer.
            _ = ownMaterial;
        }
        _ownMaterials = remote.Values.Select(s => s.Material).Distinct().ToArray();
        Require(_ownMaterials.Length > 0 && _expectedAmbient.Length == 16,
            "No qualified owned material or expected local ambient");
        _prepared = true;
        return new AppearancePreparation((byte[])rgb.Clone(), level, profileOffset,
            inputRgb, Floats(_expectedAmbient), (uint[])_ownMaterials.Clone());
    }

    internal object VerifyAfter()
    {
        EnsureAtStop(); Require(_prepared && !_verified, "Appearance verification requires one pending prepared operation");
        foreach (uint material in _ownMaterials)
        {
            byte[] after = Material(material);
            Require(after.AsSpan(0x88, 16).SequenceEqual(_expectedAmbient),
                $"Native appearance producer did not reproduce qualified local ambient for 0x{material:X8}");
        }
        foreach ((uint material, byte[] before) in _localBefore)
            Require(Material(material).SequenceEqual(before),
                $"Local matched material 0x{material:X8} changed during owned appearance update");
        _verified = true;
        return new { ownMaterialCount = _ownMaterials.Length, matchedLocalMaterialCount = _localBefore.Count,
            expectedAmbient = Floats(_expectedAmbient), ownAmbientMatchesLocal = true,
            matchedLocalMaterialPrefixesUnchanged = true, localComparisonBytesPerMaterial = 0xBC,
            readCalls = _reads, readBytes = _bytes, colorConversionPerformedByAdapter = false };
    }

    private sealed record SkinInfo(uint Material, uint Mesh);
    private Dictionary<uint, SkinInfo> ReadSkins(uint[] nodes, bool remote, IReadOnlySet<uint>? ownSkins)
    {
        var result = new Dictionary<uint, SkinInfo>();
        foreach (uint node in nodes)
        {
            Require(U(node) == VisualGraphReader.RenderNodeVtable, "Unexpected RenderNode identity in appearance inventory");
            byte[] prefix = Read(node, 0x124);
            if (remote)
            {
                Require(W(prefix, 0x3C) == 0, "Appearance producer must run before the remote graph is scene-attached");
                // Native 594820 also edits RenderNode's selected ambient Light.
                // A fresh zero ambient slot prevents changing any scene light.
                Require(W(prefix, 0x110) == 0, "Remote cached ambient must be null before the native color producer");
            }
            uint begin = W(prefix, 0xBC), end = W(prefix, 0xC0), capacity = W(prefix, 0xC4);
            if (begin == 0)
            { Require(end == 0 && capacity == 0, "Inconsistent empty appearance renderable vector"); continue; }
            Require(begin >= 0x10000 && (begin & 3) == 0 && end >= begin && capacity >= end &&
                (end - begin) % 4 == 0 && (capacity - begin) % 4 == 0 &&
                (end - begin) / 4 <= 64 && capacity - begin <= 64 * 1024,
                "Unbounded or inconsistent appearance renderable vector");
            int count = checked((int)((end - begin) / 4));
            byte[] pointers = count == 0 ? [] : Read(begin, checked(count * 4));
            for (int i = 0; i < count; ++i)
            {
                uint skin = W(pointers, i * 4), table = U(skin);
                if (!remote && table != SkinTable) continue; // No layout is invented for local helper Models.
                Require(table == SkinTable && (!remote || ownSkins!.Contains(skin)),
                    "Remote color traversal contains a renderable outside the owned exact Skin inventory");
                byte[] b = Read(skin, 0x60);
                uint material = W(b, 0x20), mesh = W(b, 0x58);
                Require(material >= 0x10000 && mesh >= 0x10000, "Skin lacks a qualified material or shared mesh");
                result[skin] = new SkinInfo(material, mesh);
                Require(result.Count <= 128, "Appearance Skin inventory exceeds its bound");
            }
        }
        return result;
    }

    private byte[] Material(uint pointer)
    {
        Require(U(pointer) == MaterialTable, "Appearance operation requires exact DXMaterial identity");
        byte[] b = Read(pointer, 0xBC);
        Require(W(b, 0x14) == MaterialColorTable, "DXMaterial secondary color interface identity mismatch");
        return b;
    }
    private byte[] Read(uint pointer, int count)
    {
        EnsureAtStop();
        Require(pointer >= 0x10000 && count > 0 && (ulong)pointer + (uint)count <= 0x100000000UL,
            "Invalid appearance diagnostic read range");
        Require(_reads < 2048 && count <= 256 * 1024 - _bytes, "Appearance diagnostic read budget exceeded");
        ++_reads; _bytes += count;
        return Win32Native.ReadMemory(_process, pointer, count);
    }
    private uint U(uint pointer) => W(Read(pointer, 4), 0);
    private static uint W(byte[] b, int at) => BitConverter.ToUInt32(b, at);
    private static float[] Floats(byte[] b)
    {
        var values = new float[b.Length / 4];
        for (int i = 0; i < values.Length; ++i)
        { values[i] = BitConverter.ToSingle(b, i * 4); Require(float.IsFinite(values[i]), "Appearance color is not finite"); }
        return values;
    }
    private static void ValidateList(uint[] values, string name, int maximum)
    {
        ArgumentNullException.ThrowIfNull(values);
        Require(values.Length > 0 && values.Length <= maximum && values.Distinct().Count() == values.Length &&
            values.All(p => p >= 0x10000 && (p & 3) == 0), $"Invalid bounded {name} inventory");
    }
    private void EnsureAtStop()
    {
        Require(_host.IsAtDebugStop && _process != 0 && _host.ProcessHandle == _process &&
            _host.ProcessPid == _pid && _pid != 0 && Environment.CurrentManagedThreadId == _ownerThread,
            "Appearance gate reads require the original owned debugger stop and thread");
    }
    private static void Require(bool condition, string message)
    { if (!condition) throw new InvalidDataException(message); }
}
