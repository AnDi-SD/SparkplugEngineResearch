using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;

namespace WinxGameControl;

/// <summary>Bounded read-only observations of the supported original x86 layout.</summary>
public static class WinxObservation
{
    public static object Read(TargetIdentity target)
    {
        var clock = Stopwatch.StartNew();
        var result = Data("qualified", false, "reason", "not read", "observedAtUtc", DateTimeOffset.UtcNow,
            "atomicSnapshot", false, "complete", false, "gameWrites", 0, "hooksInstalled", 0,
            "scope", "Cached input and native state; event delivery and gameplay acceptance are separate observations.");
        IntPtr process = IntPtr.Zero;
        Reader? reader = null;
        bool identityAdmitted = false;
        try
        {
            NativeDesktop.Validate(target, requireForeground: false);
            identityAdmitted = true;
            process = OpenProcess(0x0010 | 0x1000, false, target.Pid); // VM_READ | QUERY_LIMITED_INFORMATION
            if (process == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "OpenProcess read-only failed.");
            VerifyCreation(process, target.CreatedFileTimeUtc);
            reader = new Reader(process);
            var image = reader.Section(reader.Image);
            result["image"] = image;
            if (Admitted(image))
            {
                var input = reader.Section(reader.Input);
                result["input"] = input;
                result["mouse"] = reader.Section(reader.Mouse);
                result["keyboard"] = reader.Section(reader.Keyboard);
                result["pad"] = reader.Section(reader.Pad);
                result["gui"] = reader.Section(reader.Gui);
                result["timer"] = reader.Section(reader.Timer);
                result["flow"] = reader.Section(reader.Flow);
                result["player"] = reader.Section(reader.Player);
                var missing = result.Where(p => p.Value is Dictionary<string, object?> section &&
                    section.TryGetValue("qualified", out var q) && q is false).Select(p => p.Key).ToArray();
                result["unqualifiedSections"] = missing;
                result["complete"] = missing.Length == 0;
                reader.VerifyAnchors();
                result["qualified"] = Admitted(input);
                result["reason"] = Admitted(input) ? "typed image/input observed; inspect each optional section" : "input is not qualified";
            }
            else result["reason"] = "original x86 image layout is not qualified";
            VerifyCreation(process, target.CreatedFileTimeUtc);
        }
        catch (Exception error)
        {
            result["qualified"] = false;
            result["reason"] = error.Message;
        }
        finally
        {
            if (identityAdmitted)
            {
                try
                {
                    NativeDesktop.Validate(target, requireForeground: false);
                    if (process != IntPtr.Zero) VerifyCreation(process, target.CreatedFileTimeUtc);
                    result["identityStable"] = true;
                }
                catch (Exception error)
                {
                    result["qualified"] = false;
                    result["identityStable"] = false;
                    result["reason"] = "target identity changed or became unavailable: " + error.Message;
                }
            }
            else result["identityStable"] = false;
            if (process != IntPtr.Zero) CloseHandle(process);
        }
        result["readCalls"] = reader?.ReadCalls ?? 0;
        result["bytesRequested"] = reader?.BytesRequested ?? 0;
        result["maximumReadCalls"] = Reader.MaximumReadCalls;
        result["maximumBytes"] = Reader.MaximumBytes;
        result["elapsedMilliseconds"] = clock.ElapsedMilliseconds;
        return result;
    }

    private static Dictionary<string, object?> Data(params object?[] pairs)
    {
        var value = new Dictionary<string, object?>(StringComparer.Ordinal);
        for (int i = 0; i < pairs.Length; i += 2) value.Add((string)pairs[i]!, pairs[i + 1]);
        return value;
    }
    private static bool Admitted(Dictionary<string, object?> value) => value.TryGetValue("qualified", out var q) && q is true;
    private static string Hex(uint value) => "0x" + value.ToString("X8");
    private static uint U(byte[] bytes, int offset) => BitConverter.ToUInt32(bytes, offset);
    private static void Require(bool condition, string reason) { if (!condition) throw new ObservationFault(reason); }
    private sealed class ObservationFault(string message) : Exception(message) { }

    private sealed class Reader(IntPtr process)
    {
        internal const int MaximumReadCalls = 192, MaximumBytes = 32768;
        internal int ReadCalls { get; private set; }
        internal int BytesRequested { get; private set; }
        private readonly Dictionary<uint, uint> anchors = new();
        private uint manager, command;

        private byte[] Bytes(uint address, int count)
        {
            Require(address >= 0x10000 && count > 0 && count <= 4096 && (ulong)address + (uint)count <= 0x100000000UL,
                "read address or span is outside the bounded x86 domain");
            Require(ReadCalls < MaximumReadCalls && count <= MaximumBytes - BytesRequested, "observation read budget exhausted");
            ++ReadCalls; BytesRequested += count;
            var bytes = new byte[count];
            if (!ReadProcessMemory(process, new IntPtr((long)address), bytes, (UIntPtr)(uint)count, out var got) || got.ToUInt64() != (uint)count)
                throw new ObservationFault("ReadProcessMemory failed at " + Hex(address) + ", bytes=" + count + ", win32=" + Marshal.GetLastWin32Error());
            return bytes;
        }
        private uint Word(uint address) => U(Bytes(address, 4), 0);
        private uint Anchor(uint address)
        {
            uint value = Word(address);
            Require(!anchors.TryGetValue(address, out uint previous) || value == previous, "native anchor changed while reading " + Hex(address));
            anchors[address] = value;
            return value;
        }
        private void Remember(uint address, uint value)
        {
            Require(!anchors.TryGetValue(address, out uint previous) || value == previous, "native anchor changed while reading " + Hex(address));
            anchors[address] = value;
        }
        private byte[] Object(uint address, uint table, int count, string label)
        {
            var bytes = Bytes(address, count);
            Require(U(bytes, 0) == table, label + " physical table differs at " + Hex(address));
            Remember(address, table);
            return bytes;
        }
        internal Dictionary<string, object?> Section(Func<Dictionary<string, object?>> read)
        {
            try { var value = read(); value["qualified"] = true; return value; }
            catch (ObservationFault error) { return Data("qualified", false, "reason", error.Message); }
        }
        internal void VerifyAnchors()
        {
            foreach (var item in anchors)
                Require(Word(item.Key) == item.Value, "native graph changed while reading " + Hex(item.Key));
        }
        internal Dictionary<string, object?> Image()
        {
            var dos = Bytes(0x400000, 64);
            Require(BitConverter.ToUInt16(dos, 0) == 0x5A4D, "no DOS header at required image base 00400000");
            uint offset = U(dos, 0x3C);
            Require(offset >= 64 && offset <= 0x100000, "PE header offset exceeds the observation bound");
            var pe = Bytes(checked(0x400000 + offset), 248);
            Require(U(pe, 0) == 0x4550 && BitConverter.ToUInt16(pe, 4) == 0x14C &&
                BitConverter.ToUInt16(pe, 24) == 0x10B && U(pe, 24 + 28) == 0x400000,
                "loaded module is not the required x86 PE32 image at base 00400000");
            uint imageBytes = U(pe, 24 + 56);
            Require(imageBytes >= 0x369000 && imageBytes <= 64 * 1024 * 1024 &&
                BitConverter.ToUInt16(pe, 6) is > 0 and <= 96, "PE image or section count is outside the supported layout");
            return Data("base", Hex(0x400000), "machine", "I386", "format", "PE32", "imageBytes", imageBytes,
                "wholeExecutableHashGate", false, "qualification", "PE fields plus physical object tables; not whole-game compatibility");
        }
        internal Dictionary<string, object?> Input()
        {
            uint input = Anchor(0x7552A0);
            var bytes = Object(input, 0x6F5508, 0x24, "UserInput");
            command = U(bytes, 0x18); uint device = U(bytes, 0x1C);
            Remember(input + 0x18, command); Remember(input + 0x1C, device);
            var d = Object(device, 0x7116CC, 0x44, "input device");
            var c = Object(command, 0x703090, 0x74, "input command");
            var flags = new Dictionary<string, object?>();
            for (int i = 0x18; i <= 0x31; ++i) flags.Add(i.ToString("X2"), (int)c[i]);
            return Data("address", Hex(input), "enabled20Raw", (int)bytes[0x20], "device", Hex(device), "context14Raw", U(d, 0x14),
                "mouse34", Hex(U(d, 0x34)), "keyboard38", Hex(U(d, 0x38)), "pad3C", Hex(U(d, 0x3C)),
                "command", Hex(command), "axes", Data("04", Float(c, 4), "08", Float(c, 8), "0C", Float(c, 0xC),
                    "10", Float(c, 0x10), "14", Float(c, 0x14)), "flags18Through31Raw", flags,
                "knownFlags", Data("jump1A", (int)c[0x1A], "crouch1C", (int)c[0x1C], "escape2B", (int)c[0x2B]),
                "interpretation", "Cached stock command; this does not prove a consumer used it.");
        }
        private byte[] Manager()
        {
            uint actual = Anchor(0x755288);
            Require(manager == 0 || manager == actual, "input-manager identity changed");
            manager = actual;
            return Object(manager, 0x6F2688, 0x50, "DX input manager");
        }
        internal Dictionary<string, object?> Mouse()
        {
            var m = Manager(); uint mouse = U(m, 0x30); Remember(manager + 0x30, mouse);
            var head = Object(mouse, 0x6F3138, 0x98, "DX mouse");
            Require(U(head, 0x14) == 0x6F311C, "DX mouse secondary interface differs");
            var packets = Bytes(checked(mouse + 0x5098), 48);
            return Data("address", Hex(mouse), "acquired45Raw", (int)head[0x45], "cursorVisible94Raw", (int)head[0x94],
                "exclusive95Raw", (int)head[0x95], "window88", Hex(U(head, 0x88)),
                "relativeXYZ", new[] { BitConverter.ToInt32(packets, 0), BitConverter.ToInt32(packets, 4), BitConverter.ToInt32(packets, 8) },
                "currentButtons100Through107", Values(packets, 12, 8), "changedButtons100Through107", Values(packets, 32, 8),
                "ownCursor", Data("x", BitConverter.ToInt32(packets, 40), "y", BitConverter.ToInt32(packets, 44)),
                "interpretation", "Own cursor integrates DirectInput relative motion; it is not the OS screen pointer.");
        }
        internal Dictionary<string, object?> Keyboard()
        {
            // Init4C3BB0 has secondary this=manager+18. It stores keyboard at
            // secondary+14 (full+2C), mouse at secondary+18 (full+30).
            var m = Manager(); uint keyboard = U(m, 0x2C); Remember(manager + 0x2C, keyboard);
            var head = Object(keyboard, 0x6F31D4, 0x48, "DX keyboard");
            Require(U(head, 0x14) == 0x6F31B8, "DX keyboard secondary interface differs");
            // ProcessEvents4CC980 writes scan-indexed current858/changed958.
            var cache = Bytes(checked(keyboard + 0x858), 512);
            return Data("address", Hex(keyboard), "acquired45Raw", (int)head[0x45],
                "heldScanIndices", Enumerable.Range(0, 256).Where(i => cache[i] != 0).ToArray(),
                "changedScanIndices", Enumerable.Range(0, 256).Where(i => cache[256 + i] != 0).ToArray(),
                "current858Hex", Convert.ToHexString(cache.AsSpan(0, 256)), "changed958Hex", Convert.ToHexString(cache.AsSpan(256, 256)),
                "interpretation", "Native scan-indexed cache, not GetAsyncKeyState and not a logical-key map.");
        }
        internal Dictionary<string, object?> Pad()
        {
            // The selected primary pointer is device+3C. NativeMenuInputObservation
            // independently qualified its embedded +14 receiver against these
            // exact tables and the original 4CB240/4CB570/4CB770 slots.
            uint input = Anchor(0x7552A0); var user = Object(input, 0x6F5508, 0x24, "UserInput for pad");
            uint device = U(user, 0x1C); Remember(input + 0x1C, device);
            var mapper = Object(device, 0x7116CC, 0x40, "input device for pad");
            uint pad = U(mapper, 0x3C); Remember(device + 0x3C, pad);
            Require(pad != 0, "UserInput has no selected native DirectInput pad; native pad neutral is unknown");
            var m = Manager(); uint count = U(m, 0x3C);
            Require(count <= 4, "native enumerated controller count exceeds four");
            var slots = new uint[4];
            for (int i = 0; i < slots.Length; ++i)
            { slots[i] = U(m, 0x40 + i * 4); Remember(manager + 0x40u + (uint)i * 4, slots[i]); }
            Remember(manager + 0x3C, count);
            int index = Array.IndexOf(slots, pad);
            Require(index >= 0, "selected native pad is absent from the input manager's four owned slots");
            var p = Object(pad, 0x6F30B4, 0xD0, "DX gamepad");
            Require(U(p, 0x14) == 0x6F3098, "DX gamepad secondary interface differs");
            var methods = Bytes(0x6F3098, 12);
            Require(U(methods, 0) == 0x4CB240 && U(methods, 4) == 0x4CB570 && U(methods, 8) == 0x4CB770,
                "DX gamepad producer/held/rising-query table slots differ");

            // 4CB240 uses SECONDARY this. On successful GetDeviceState it first
            // copies the old cache, then stores four current axes and16 buttons.
            // Buttons12..15 are subsequently replaced by synthesized POV states.
            // 4CB290 early return leaves all these cache values unchanged.
            int[] current = { I(p, 0x14 + 0x7C), I(p, 0x14 + 0x80), I(p, 0x14 + 0x8C), I(p, 0x14 + 0x90) };
            int[] previous = { I(p, 0x14 + 0x84), I(p, 0x14 + 0x88), I(p, 0x14 + 0x94), I(p, 0x14 + 0x98) };
            var buttons = Values(p, 0x14 + 0x9C, 16);
            var previousButtons = Values(p, 0x14 + 0xAC, 16);
            bool currentNeutral = current.All(v => v == 0) && buttons.All(v => v == 0);
            bool bothNeutral = currentNeutral && previous.All(v => v == 0) && previousButtons.All(v => v == 0);
            uint com = U(p, 0x14 + 0x78); Remember(pad + 0x14 + 0x78, com);
            var comEvidence = com == 0 ? Data("qualified", false, "reason", "native pad has no DirectInput COM device") : Section(() => PadCom(com));
            bool enumerated = (uint)index < count;
            return Data("address", Hex(pad), "secondary", Hex(checked(pad + 0x14)), "selectedManagerSlot", index,
                "enumeratedCount", count, "managerSlots", slots.Select(Hex).ToArray(), "selectedIsEnumerated", enumerated,
                "enabled30Raw", (int)p[0x14 + 0x30], "directInputCom", comEvidence,
                "currentAxesBySecondaryOffset", Data("7C", current[0], "80", current[1], "8C", current[2], "90", current[3]),
                "previousAxesBySecondaryOffset", Data("84", previous[0], "88", previous[1], "94", previous[2], "98", previous[3]),
                "currentButtons9CThroughABRaw", buttons, "previousButtonsACThroughBBRaw", previousButtons,
                "currentHeldPhysical127Through142", Enumerable.Range(0, 16).Where(i => buttons[i] != 0).Select(i => 127 + i).ToArray(),
                "risingPhysical127Through142", Enumerable.Range(0, 16).Where(i => buttons[i] != 0 && previousButtons[i] == 0).Select(i => 127 + i).ToArray(),
                "button8PauseRaw", buttons[8], "button8PausePreviousRaw", previousButtons[8],
                "currentCacheNeutral", currentNeutral, "currentAndPreviousCacheNeutral", bothNeutral,
                "enabledEnumeratedCacheNeutral", bothNeutral && enumerated && p[0x44] != 0 && Admitted(comEvidence),
                "freshPollQualified", false, "directInputInstanceGuid", null,
                "guidQualification", "Instance GUID storage is not established; a readable COM pointer does not identify the owned virtual controller.",
                "interpretation", "Native cache only. Producer early return can preserve stale values; command neutral does not prove cache neutral, and cache neutral does not prove a fresh successful poll.");
        }
        private Dictionary<string, object?> PadCom(uint com)
        {
            uint table = Anchor(com); var v = Bytes(table, 0x68);
            Require(U(v, 0x1C) >= 0x10000 && U(v, 0x24) >= 0x10000 && U(v, 0x64) >= 0x10000,
                "DirectInput COM method slots are not readable bounded pointers");
            return Data("address", Hex(com), "table", Hex(table), "acquire1C", Hex(U(v, 0x1C)),
                "getDeviceState24", Hex(U(v, 0x24)), "poll64", Hex(U(v, 0x64)), "methodsCalledByObserver", 0);
        }
        internal Dictionary<string, object?> Gui()
        {
            uint gui = Anchor(0x75DB8C); var g = Object(gui, 0x6E6D08, 0x50, "GUI manager");
            uint graphics = Anchor(0x75DB68); var size = Bytes(checked(graphics + 0x1C), 8);
            return Data("address", Hex(gui), "mouseEnabled45Raw", (int)g[0x45], "mouse20", Hex(U(g, 0x20)),
                "cursorNode1C", Hex(U(g, 0x1C)), "normalizedCursor", Data("x38", Float(g, 0x38), "y3C", Float(g, 0x3C)),
                "hover48", Hex(U(g, 0x48)), "keyboardFocus4C", Hex(U(g, 0x4C)),
                "graphicsWidth", U(size, 0), "graphicsHeight", U(size, 4),
                "interpretation", "Mouse GUI update is gated by45; hover/cursor movement alone does not prove a click action.");
        }
        internal Dictionary<string, object?> Timer()
        {
            uint timer = Anchor(0x755298); var t = Object(timer, 0x702F94, 0x44, "game timer");
            return Data("address", Hex(timer), "paused40Raw", (int)t[0x40], "active18Raw", (int)t[0x18],
                "current1CRaw", U(t, 0x1C), "delta28", Float(t, 0x28));
        }
        internal Dictionary<string, object?> Flow()
        {
            uint flow = Anchor(0x755294); var f = Object(flow, 0x702D2C, 0x1BC, "game flow");
            uint depth = U(f, 0x1AC); Require(depth <= 20, "flow depth exceeds20");
            uint slot = checked(0x15C + depth * 4), active = U(f, (int)slot);
            Remember(flow + 0x1AC, depth); Remember(flow + slot, active);
            Remember(flow + 0x1B0, U(f, 0x1B0)); Remember(flow + 0x1B8, U(f, 0x1B8));
            var a = Bytes(active, 0x14);
            return Data("address", Hex(flow), "depth", depth, "current1B0", U(f, 0x1B0), "pending1B8", U(f, 0x1B8),
                "top", Hex(active), "topTable", Hex(U(a, 0)), "topLevel10", U(a, 0x10));
        }
        internal Dictionary<string, object?> Player()
        {
            uint profile = Anchor(0x765AD4); var p = Object(profile, 0x6F5A44, 0x2B8, "player profile");
            uint record = U(p, 0x2B4); Remember(profile + 0x2B4, record);
            var r = Object(record, 0x710F80, 0x28, "player instance");
            Require(U(r, 0x20) == 11, "player instance does not have the qualified eleven-component graph");
            uint root = U(r, 0x24), head = U(r, 0x1C); Remember(record + 0x24, root); Remember(record + 0x1C, head);
            var pose = Object(root, 0x6DC4F4, 0xB4, "player root");
            var sentinel = Bytes(head, 8); uint link = U(sentinel, 0), previous = head, bloom = 0;
            var seen = new HashSet<uint>();
            for (int i = 0; i < 11; ++i)
            {
                Require(link != head && seen.Add(link), "player component chain repeats or ends early");
                var item = Bytes(link, 12); Require(U(item, 4) == previous, "player component chain is not reciprocal");
                uint component = U(item, 8);
                if (Word(component) == 0x6FA288)
                { Require(bloom == 0 && Word(component + 0x24) == record, "Bloom FSM owner or uniqueness differs"); bloom = component; }
                previous = link; link = U(item, 0);
            }
            Require(link == head && U(sentinel, 4) == previous && bloom != 0, "player component chain tail or Bloom FSM differs");
            var b = Object(bloom, 0x6FA288, 0x148, "Bloom FSM");
            return Data("profile", Hex(profile), "record", Hex(record), "root", Hex(root), "worldPosition", Floats(pose, 0x74, 3),
                "localPosition", Floats(pose, 0x20, 3), "worldOrientation", Floats(pose, 0x8C, 9), "bloom", Hex(bloom),
                "fsm", Data("owner124", Hex(U(b, 0x124)), "kind128", U(b, 0x128), "command12C", Hex(U(b, 0x12C)),
                    "current130", U(b, 0x130), "previous134", U(b, 0x134), "gate138", U(b, 0x138),
                    "currentData140Raw", U(b, 0x140)),
                "commandMatchesObservedInput", command != 0 ? (bool?)(U(b, 0x12C) == command) : null,
                "interpretation", "Position and native FSM fields; state IDs are not renamed or equated with accepted network motion.");
        }
        private static object Float(byte[] bytes, int offset)
        {
            float value = BitConverter.ToSingle(bytes, offset);
            return Data("value", float.IsFinite(value) ? (float?)value : null, "finite", float.IsFinite(value), "raw", Hex(U(bytes, offset)));
        }
        private static float[] Floats(byte[] bytes, int offset, int count)
        {
            var values = new float[count];
            for (int i = 0; i < count; ++i)
            { values[i] = BitConverter.ToSingle(bytes, offset + i * 4); Require(float.IsFinite(values[i]), "pose contains a non-finite scalar"); }
            return values;
        }
        private static int[] Values(byte[] bytes, int offset, int count) => Enumerable.Range(offset, count).Select(i => (int)bytes[i]).ToArray();
        private static int I(byte[] bytes, int offset) => BitConverter.ToInt32(bytes, offset);
    }

    private static void VerifyCreation(IntPtr process, long expected)
    {
        if (!GetProcessTimes(process, out var created, out _, out _, out _))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetProcessTimes read-only identity check failed.");
        Require(created.Value == expected, "process creation time differs from the selected target");
    }
    [StructLayout(LayoutKind.Sequential)]
    private struct NativeFileTime { public uint Low, High; public readonly long Value => ((long)High << 32) | Low; }
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr OpenProcess(uint access, [MarshalAs(UnmanagedType.Bool)] bool inherit, int pid);
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool ReadProcessMemory(IntPtr process, IntPtr address, [Out] byte[] buffer, UIntPtr count, out UIntPtr read);
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetProcessTimes(IntPtr process, out NativeFileTime created, out NativeFileTime exited, out NativeFileTime kernel, out NativeFileTime user);
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CloseHandle(IntPtr handle);
}
