using System.Reflection;
using System.Runtime.InteropServices;

namespace WinxCoop.CompatibilityChecks;

internal static class AbiChecks
{
    private const BindingFlags PrivateStatic = BindingFlags.NonPublic | BindingFlags.Static;
    private static Type Gate(Assembly actor) => ChecksProgram.Type(actor, "WinxCoop.RuntimeProbe.ClientNativeGate");
    private static Type Nested(Type owner, string name) => owner.GetNestedType(name, BindingFlags.NonPublic) ??
        throw new InvalidDataException("Missing ABI type: " + name);
    private static void Layout(Type type, int size, params (string Name, int Offset)[] fields)
    {
        ChecksProgram.Require(Marshal.SizeOf(type) == size, type.Name + " size differs from the Windows/C ABI.");
        foreach (var field in fields)
            ChecksProgram.Require(Marshal.OffsetOf(type, field.Name).ToInt64() == field.Offset,
                type.Name + "." + field.Name + " offset differs from the Windows/C ABI.");
    }

    internal static void MemoryLayouts(Assembly actor)
    {
        Layout(Nested(Gate(actor), "MemoryBasicInformation32"), 28,
            ("BaseAddress", 0), ("AllocationBase", 4), ("AllocationProtect", 8), ("RegionSize", 12),
            ("State", 16), ("Protect", 20), ("Type", 24));
        Layout(Nested(Gate(actor), "MemoryBasicInformation64"), 48,
            ("BaseAddress", 0), ("AllocationBase", 8), ("AllocationProtect", 16), ("Alignment1", 20),
            ("RegionSize", 24), ("State", 32), ("Protect", 36), ("Type", 40), ("Alignment2", 44));
    }

    internal static void Normalize32(Assembly actor)
    {
        Type gate = Gate(actor), sourceType = Nested(gate, "MemoryBasicInformation32");
        object source = Activator.CreateInstance(sourceType)!;
        var expected = new Dictionary<string, uint>
        {
            ["BaseAddress"] = 0xF0012000, ["AllocationBase"] = 0xE0000000,
            ["AllocationProtect"] = 0x104, ["RegionSize"] = 0x80001000,
            ["State"] = 0x1000, ["Protect"] = 0x204, ["Type"] = 0x20000
        };
        foreach (var pair in expected)
            sourceType.GetField(pair.Key, BindingFlags.NonPublic | BindingFlags.Instance)!.SetValue(source, pair.Value);
        MethodInfo method = gate.GetMethod("NormalizeMemoryInformation32", PrivateStatic)!;
        object normalized = ChecksProgram.Invoke(method, source)!;
        Type targetType = normalized.GetType();
        foreach (var pair in expected)
        {
            object value = targetType.GetField(pair.Key, BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(normalized)!;
            ChecksProgram.Require(Convert.ToUInt64(value) == pair.Value, "MBI32 field was truncated, signed or changed: " + pair.Key);
        }
        foreach (string name in new[] { "Alignment1", "Alignment2" })
            ChecksProgram.Require((uint)targetType.GetField(name, BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(normalized)! == 0,
                "MBI32 normalization introduced nonzero alignment data.");
    }

    internal static void DebuggerLayouts(Assembly core)
    {
        Type native = ChecksProgram.Type(core, "SmoNativeValidator.Core.Win32Native");
        bool x64 = IntPtr.Size == 8;
        Layout(Nested(native, "DebugEvent"), x64 ? 176 : 96,
            ("Code", 0), ("ProcessId", 4), ("ThreadId", 8), ("Info", x64 ? 16 : 12));
        Layout(Nested(native, "ExceptionRecord"), x64 ? 152 : 80,
            ("ExceptionCode", 0), ("NestedRecord", 8), ("ExceptionAddress", x64 ? 16 : 12),
            ("NumberParameters", x64 ? 24 : 16), ("ExceptionInformation0", x64 ? 32 : 20));
        Layout(Nested(native, "StartupInfo"), x64 ? 104 : 68);
        Layout(Nested(native, "ProcessInformation"), x64 ? 24 : 16);
        Layout(Nested(native, "X86Context"), 716);
    }

    internal static void QueryImports(Assembly actor)
    {
        Type gate = Gate(actor);
        foreach (var item in new[] { (Method: "VirtualQueryEx", Structure: "MemoryBasicInformation64"),
            (Method: "VirtualQueryEx32", Structure: "MemoryBasicInformation32") })
        {
            MethodInfo method = gate.GetMethod(item.Method, PrivateStatic)!;
            DllImportAttribute import = method.GetCustomAttribute<DllImportAttribute>()!;
            ChecksProgram.Require(import.Value == "kernel32.dll" && import.EntryPoint == "VirtualQueryEx" && import.SetLastError,
                "Memory query lost its exact Windows entry point/error contract.");
            ParameterInfo[] parameters = method.GetParameters();
            ChecksProgram.Require(parameters.Length == 4 && parameters[2].IsOut &&
                parameters[2].ParameterType.GetElementType() == Nested(gate, item.Structure) &&
                parameters[3].ParameterType == typeof(UIntPtr), "Memory query out-layout/native size argument changed.");
        }
    }

    internal static void PoseLayout(Assembly actor)
    {
        Type interpolator = ChecksProgram.Type(actor, "WinxCoop.RuntimeProbe.NativePoseInterpolator");
        Layout(Nested(interpolator, "NativeTransform"), 60,
            ("X", 0), ("Y", 4), ("Z", 8), ("Sx", 12), ("Sy", 16), ("Sz", 20),
            ("M00", 24), ("M01", 28), ("M02", 32), ("M10", 36), ("M11", 40),
            ("M12", 44), ("M20", 48), ("M21", 52), ("M22", 56));
    }
}
