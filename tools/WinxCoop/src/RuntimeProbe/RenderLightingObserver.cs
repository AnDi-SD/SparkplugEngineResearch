using System.ComponentModel;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own read-only diagnostic view of the pinned PC RenderNode/Light ABI.
/// Caller must keep all game threads stopped for the complete read. This
/// reader does not attach a light, rebuild a cache, or modify renderer state.
/// </summary>
internal sealed class RenderLightingObserver(nint processHandle)
{
    internal object Read(uint node)
    {
        var state = new ReadState(processHandle);
        try { return new { success = true, node = state.Node(node), readCalls = state.Calls, readBytes = state.BytesRead }; }
        catch (Exception exception) when (exception is InvalidDataException or Win32Exception or OverflowException)
        { return new { success = false, address = Hex(node), error = exception.Message, readCalls = state.Calls, readBytes = state.BytesRead }; }
    }

    private static string Hex(uint value) => $"0x{value:X8}";
    private static uint U(byte[] bytes, int at) => BitConverter.ToUInt32(bytes, at);
    private static float? F(byte[] bytes, int at)
    { float value = BitConverter.ToSingle(bytes, at); return float.IsFinite(value) ? value : null; }
    private static float?[] Floats(byte[] bytes, int at, int count) =>
        Enumerable.Range(0, count).Select(i => F(bytes, at + i * 4)).ToArray();
    private static bool IsLight(uint table) => table is VisualGraphReader.LightVtable or
        VisualGraphReader.LightDataVtable or VisualGraphReader.DXLightVtable;

    private sealed class ReadState(nint process)
    {
        internal int Calls { get; private set; }
        internal int BytesRead { get; private set; }
        private static void Require(bool value, string message)
        { if (!value) throw new InvalidDataException(message); }
        private byte[] Read(uint address, int count)
        {
            Require(process != 0 && process != -1 && address >= 0x10000 && count > 0 &&
                (ulong)address + (uint)count <= 0x100000000UL, "Invalid stopped lighting read range");
            Require(Calls < 1024 && count <= 128 * 1024 - BytesRead, "Lighting diagnostic read budget exceeded");
            ++Calls; BytesRead += count;
            return Win32Native.ReadMemory(process, address, count);
        }
        private uint Table(uint address) => U(Read(address, 4), 0);

        internal object Node(uint address)
        {
            Require(Table(address) == VisualGraphReader.RenderNodeVtable,
                $"Expected exact RenderNode at {Hex(address)}");
            // SparkplugAbi.h spRenderNodeLayout: prefix through four control
            // bytes, before self/scene links and cached device matrices.
            byte[] b = Read(address, 0x124);
            uint count = U(b, 0x114), ambient = U(b, 0x110), scene = U(b, 0x3C);
            Require(count <= 8, "RenderNode light-cache count exceeds its eight ordinary slots");
            uint[] rawSlots = Enumerable.Range(0, 8).Select(i => U(b, 0xF0 + i * 4)).ToArray();
            uint[] selected = rawSlots.Take((int)count).Concat(ambient == 0 ? [] : new[] { ambient }).ToArray();
            var lights = selected.Distinct().Select(Light).ToArray();
            uint begin = U(b, 0xBC), end = U(b, 0xC0), capacity = U(b, 0xC4);
            uint[] renderables;
            if (begin == 0)
            {
                Require(end == 0 && capacity == 0, "Inconsistent empty renderable vector"); renderables = [];
            }
            else
            {
                Require(begin >= 0x10000 && (begin & 3) == 0 && end >= begin && capacity >= end &&
                    (end - begin) % 4 == 0 && (capacity - begin) % 4 == 0 &&
                    (end - begin) / 4 <= 64 && capacity - begin <= 64 * 1024,
                    "Unbounded or inconsistent RenderNode renderable vector");
                int length = checked((int)(end - begin));
                byte[] entries = length == 0 ? [] : Read(begin, length);
                renderables = Enumerable.Range(0, length / 4).Select(i => U(entries, i * 4)).ToArray();
            }
            return new
            {
                address = Hex(address), vtable = Hex(U(b, 0)), rawPrefixHex = Convert.ToHexString(b),
                parent = Hex(U(b, 0x2C)), scene = Hex(scene), flags = U(b, 0xB0),
                enabled200 = (U(b, 0xB0) & 0x200) != 0, hierarchyActive100 = (U(b, 0xB0) & 0x100) != 0,
                worldSphere = Floats(b, 0xD8, 4),
                lightCache = new { rawOrdinarySlots = rawSlots.Select(Hex).ToArray(), ordinaryCount = count,
                    ambient = Hex(ambient), selectedLights = lights, unusedRawSlotsNotDereferenced = true },
                controls = new { raw120 = b[0x120], excludeShadowVolumeLights121 = b[0x121],
                    raw122 = b[0x122], updateLights123 = b[0x123] },
                renderables = renderables.Select(Renderable).ToArray(),
                sceneLightMembership = Scene(scene, selected)
            };
        }

        private object Light(uint address)
        {
            uint table = Table(address);
            if (!IsLight(table)) return new { address = Hex(address), vtable = Hex(table), knownLight = false };
            // Exact inherited Light extent F0; no DX payload F0..157 is read.
            byte[] b = Read(address, 0xF0);
            return new
            {
                address = Hex(address), vtable = Hex(table), knownLight = true,
                rawPrefixHex = Convert.ToHexString(b), parent = Hex(U(b, 0x2C)), scene = Hex(U(b, 0x3C)),
                flags = U(b, 0xB0), hierarchyActive100 = (U(b, 0xB0) & 0x100) != 0,
                type = U(b, 0xC0), color = Floats(b, 0xC4, 4), intensity = F(b, 0xD8), range = F(b, 0xE0),
                projectShadowVolumeEC = b[0xEC], enabledED = b[0xED], worldPosition = Floats(b, 0x74, 3)
            };
        }

        private object Renderable(uint address)
        {
            uint table = Table(address);
            int size = table switch { 0x6DC9B0 => 0x58, 0x6EAA58 or VisualGraphReader.SkinVtable => 0x60, _ => 0 };
            if (size == 0) return new { address = Hex(address), vtable = Hex(table), knownRenderablePrefix = false };
            byte[] b = Read(address, size);
            return new
            {
                address = Hex(address), vtable = Hex(table), knownRenderablePrefix = true,
                rawPrefixHex = Convert.ToHexString(b), runtimeMode14 = U(b, 0x14),
                materialPointer = Hex(U(b, 0x20)), fogPointer = Hex(U(b, 0x24)),
                meshPointer = size >= 0x60 ? Hex(U(b, 0x58)) : null,
                material = U(b, 0x20) == 0 ? null : Material(U(b, 0x20))
            };
        }

        private object Material(uint address)
        {
            uint table = Table(address);
            // Exact material common prefix (78) and DXMaterial extent (BC)
            // are documented by spMaterialObservedLayout in SparkplugAbi.h.
            int size = table switch { 0x6DC984 or 0x6DE9FC => 0x78, 0x6EF264 => 0xBC, _ => 0 };
            if (size == 0) return new { address = Hex(address), vtable = Hex(table), knownMaterial = false };
            byte[] b = Read(address, size);
            return new
            {
                address = Hex(address), vtable = Hex(table), knownMaterial = true,
                rawPrefixHex = Convert.ToHexString(b), renderStates18 = Enumerable.Range(0, 11).Select(i => U(b, 0x18 + i * 4)).ToArray(),
                passCount = U(b, 0x48), rawPassPointers = Enumerable.Range(0, 8).Select(i => Hex(U(b, 0x4C + i * 4))).ToArray(),
                renderOverride6C = b[0x6C], useVertexAlpha6D = b[0x6D],
                diffuse = size == 0xBC ? Floats(b, 0x78, 4) : null,
                ambient = size == 0xBC ? Floats(b, 0x88, 4) : null,
                specular = size == 0xBC ? Floats(b, 0x98, 4) : null,
                emissive = size == 0xBC ? Floats(b, 0xA8, 4) : null
            };
        }

        private object? Scene(uint address, uint[] selected)
        {
            try { return SceneCore(address, selected); }
            catch (Exception exception) when (exception is InvalidDataException or Win32Exception or OverflowException)
            { return new { address = Hex(address), membershipReadSucceeded = false, error = exception.Message }; }
        }

        private object? SceneCore(uint address, uint[] selected)
        {
            if (address == 0) return null;
            uint table = Table(address);
            if (table != 0x6E7358) return new { address = Hex(address), vtable = Hex(table), knownScene = false };
            byte[] scene = Read(address, 0x38); uint manager = U(scene, 0x34);
            if (manager == 0) return new { address = Hex(address), manager = Hex(manager), missingManager = true };
            // The pointer is typed by the qualified Scene+34 owned field;
            // no manager identity is inferred from vtable neighbourhood.
            byte[] b = Read(manager, 0x24);
            uint first = U(b, 0x10), last = U(b, 0x14), count = U(b, 0x18);
            Require(U(b, 0x20) == address, "LightManager owner does not match the typed Scene");
            Require(count <= 256, "Scene light membership exceeds the bounded diagnostic limit");
            var members = new List<uint>(); uint current = first, previous = 0;
            while (current != 0 && members.Count < count)
            {
                Require(!members.Contains(current) && IsLight(Table(current)), "Unknown or cyclic Scene light entry");
                byte[] links = Read(checked(current + 0xB8), 8);
                Require(U(links, 0) == previous, "Scene light-list backlink mismatch");
                members.Add(current); previous = current; current = U(links, 4);
            }
            Require(current == 0 && members.Count == count && previous == last,
                "Scene light-list count or tail mismatch");
            return new
            {
                address = Hex(address), knownScene = true, manager = Hex(manager), managerVtable = Hex(U(b, 0)),
                lightCount = count, memberAddresses = members.Select(Hex).ToArray(),
                selected = selected.Distinct().Select(p => new { address = Hex(p), inSceneLightList = members.Contains(p) }).ToArray()
            };
        }
    }
}
