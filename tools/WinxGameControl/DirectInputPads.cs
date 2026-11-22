using System.Runtime.InteropServices;

namespace WinxGameControl;

// The existing E5 DirectInput8W enumeration ABI. No Poll/Acquire/state write.
public static class DirectInputPads
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct DeviceInstance
    {
        public uint Size;
        public Guid Instance, Product;
        public uint DeviceType;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)] public string InstanceName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)] public string ProductName;
        public Guid ForceFeedbackDriver;
        public ushort UsagePage, Usage;
    }
    public sealed record Pad(Guid Instance, Guid Product, string Name, uint DeviceType);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int EnumCallback(IntPtr instance, IntPtr context);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int EnumDevices(IntPtr self, uint deviceClass, EnumCallback callback, IntPtr context, uint flags);

    public static Pad[] Read()
    {
        Guid iid = new("BF798031-483A-4DA2-AA99-5D64ED369700");
        IntPtr input = IntPtr.Zero;
        try
        {
            Marshal.ThrowExceptionForHR(DirectInput8Create(GetModuleHandle(IntPtr.Zero), 0x0800, ref iid, out input, IntPtr.Zero));
            var result = new List<Pad>(); Exception? callbackError = null;
            EnumCallback callback = (pointer, _) =>
            {
                try
                {
                    var value = Marshal.PtrToStructure<DeviceInstance>(pointer);
                    if (value.Size < Marshal.SizeOf<DeviceInstance>())
                        throw new InvalidDataException("DirectInput device-instance size is below the SDK layout.");
                    result.Add(new(value.Instance, value.Product, value.InstanceName, value.DeviceType));
                    return 1;
                }
                catch (Exception exception) { callbackError = exception; return 0; }
            };
            var enumerate = Marshal.GetDelegateForFunctionPointer<EnumDevices>(Marshal.ReadIntPtr(Marshal.ReadIntPtr(input), 4 * IntPtr.Size));
            int hr = enumerate(input, 4, callback, IntPtr.Zero, 1); // GAMECTRL, ATTACHEDONLY
            GC.KeepAlive(callback); Marshal.ThrowExceptionForHR(hr);
            if (callbackError is not null) throw new InvalidDataException("DirectInput enumeration callback failed.", callbackError);
            return result.ToArray();
        }
        finally { if (input != IntPtr.Zero) Marshal.Release(input); }
    }

    [DllImport("dinput8.dll", ExactSpelling = true)]
    [DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
    private static extern int DirectInput8Create(IntPtr instance, uint version, ref Guid iid, out IntPtr result, IntPtr outer);
    [DllImport("kernel32.dll", EntryPoint = "GetModuleHandleW", ExactSpelling = true)]
    private static extern IntPtr GetModuleHandle(IntPtr name);
}
