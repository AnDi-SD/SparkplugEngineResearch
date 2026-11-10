using System.Reflection.PortableExecutable;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using Microsoft.Win32.SafeHandles;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Thin process-local ABI over the shared C++ RemotePoseInterpolator. No game
/// pointers, profile state, networking or interpolation mathematics live here.
/// The exact module stays loaded for the owning Actor process lifetime.
/// </summary>
internal sealed class NativePoseInterpolator : IDisposable
{
    private const uint AbiVersion = 1, TransformBytes = 60;
    private static readonly Lazy<NativeApi> SharedApi = new(NativeApi.Load);
    private static int _moduleReported;
    private readonly NativeApi _api;
    private readonly OwnedPose _handle;

    internal NativePoseInterpolator(Action<object> log)
    {
        _api = SharedApi.Value;
        IntPtr value = IntPtr.Zero;
        int result = _api.Create(AbiVersion, TransformBytes, ref value);
        if (result != 1 || value == IntPtr.Zero || value == new IntPtr(-1))
            throw new InvalidDataException($"Native pose creation rejected: {result}.");
        _handle = new(_api, value);
        if (Interlocked.Exchange(ref _moduleReported, 1) == 0)
            log(new { @event = "native-pose-interpolator-loaded", _api.Path, _api.Sha256,
                abiVersion = AbiVersion, transformBytes = TransformBytes,
                processArchitecture = RuntimeInformation.ProcessArchitecture.ToString(),
                sharedCppInterpolator = true, nativeGameCallsAdded = 0 });
    }

    internal void Reset(WorldPresentationActorState state, long localMilliseconds)
    {
        NativeTransform pose = NativeTransform.From(state);
        ulong now = Clock(localMilliseconds);
        Invoke(value => _api.Reset(value, in pose, now), "reset");
    }

    internal void SetTarget(WorldPresentationActorState state, long localMilliseconds, uint durationMilliseconds)
    {
        NativeTransform pose = NativeTransform.From(state);
        ulong now = Clock(localMilliseconds);
        Invoke(value => _api.SetTarget(value, in pose, now, durationMilliseconds), "target");
    }

    internal WorldPresentationActorState Sample(WorldPresentationActorState metadata, long localMilliseconds)
    {
        NativeTransform pose = default;
        ulong now = Clock(localMilliseconds);
        Invoke(value => _api.Sample(value, now, ref pose), "sample");
        // These are copy-owned ABI values. HP, state, visibility and animation
        // remain the latest acknowledged server values; only display PRS change.
        return metadata with
        {
            Position = [pose.X, pose.Y, pose.Z],
            Scale = [pose.Sx, pose.Sy, pose.Sz],
            Rotation = [pose.M00, pose.M01, pose.M02, pose.M10, pose.M11, pose.M12, pose.M20, pose.M21, pose.M22]
        };
    }

    internal void Clear() => Invoke(value => _api.Clear(value), "clear");
    public void Dispose() => _handle.Dispose();

    private void Invoke(Func<IntPtr, int> operation, string name)
    {
        bool retained = false;
        try
        {
            _handle.DangerousAddRef(ref retained);
            int result = operation(_handle.DangerousGetHandle());
            if (result != 1) throw new InvalidDataException($"Native pose {name} rejected: {result}.");
        }
        finally { if (retained) _handle.DangerousRelease(); }
    }

    private static ulong Clock(long value) => value >= 0 ? checked((ulong)value) :
        throw new InvalidDataException("Native pose clock must be local nonnegative monotonic milliseconds.");

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    private struct NativeTransform
    {
        internal float X, Y, Z, Sx, Sy, Sz;
        internal float M00, M01, M02, M10, M11, M12, M20, M21, M22;
        internal static NativeTransform From(WorldPresentationActorState state)
        {
            state.Validate();
            return new()
            {
                X = state.Position[0], Y = state.Position[1], Z = state.Position[2],
                Sx = state.Scale[0], Sy = state.Scale[1], Sz = state.Scale[2],
                M00 = state.Rotation[0], M01 = state.Rotation[1], M02 = state.Rotation[2],
                M10 = state.Rotation[3], M11 = state.Rotation[4], M12 = state.Rotation[5],
                M20 = state.Rotation[6], M21 = state.Rotation[7], M22 = state.Rotation[8]
            };
        }
    }

    private sealed class OwnedPose : SafeHandleZeroOrMinusOneIsInvalid
    {
        private readonly NativeApi _api;
        internal OwnedPose(NativeApi api, IntPtr value) : base(true) { _api = api; SetHandle(value); }
        protected override bool ReleaseHandle() { _api.Destroy(handle); return true; }
    }

    private sealed class NativeApi
    {
        private const string FileName = "WinxCoop.PoseInterpolationNative.dll";
        internal string Path { get; }
        internal string Sha256 { get; }
        // Delegates and this HMODULE have the same process-local lifetime.
        internal IntPtr Module { get; }
        internal CreateDelegate Create { get; }
        internal DestroyDelegate Destroy { get; }
        internal ResetDelegate Reset { get; }
        internal TargetDelegate SetTarget { get; }
        internal SampleDelegate Sample { get; }
        internal ClearDelegate Clear { get; }

        private NativeApi(string path, string sha, IntPtr module)
        {
            Path = path; Sha256 = sha; Module = module;
            T Export<T>(string name) where T : Delegate => Marshal.GetDelegateForFunctionPointer<T>(NativeLibrary.GetExport(module, name));
            var version = Export<UIntDelegate>("wc_pose_abi_version");
            var bytes = Export<UIntDelegate>("wc_pose_transform_bytes");
            if (version() != AbiVersion || bytes() != TransformBytes || Marshal.SizeOf<NativeTransform>() != TransformBytes ||
                Marshal.OffsetOf<NativeTransform>(nameof(NativeTransform.Sx)).ToInt32() != 12 ||
                Marshal.OffsetOf<NativeTransform>(nameof(NativeTransform.M00)).ToInt32() != 24)
                throw new InvalidDataException("Native pose ABI version or 60-byte transform layout differs.");
            Create = Export<CreateDelegate>("wc_pose_create");
            Destroy = Export<DestroyDelegate>("wc_pose_destroy");
            Reset = Export<ResetDelegate>("wc_pose_reset");
            SetTarget = Export<TargetDelegate>("wc_pose_set_target");
            Sample = Export<SampleDelegate>("wc_pose_sample");
            Clear = Export<ClearDelegate>("wc_pose_clear");
        }

        internal static NativeApi Load()
        {
            string path = System.IO.Path.GetFullPath(System.IO.Path.Combine(AppContext.BaseDirectory, FileName));
            if ((File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
                throw new InvalidDataException("Native pose module must be an ordinary file beside its owning Actor.");
            using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
            using var image = new PEReader(stream, PEStreamOptions.LeaveOpen | PEStreamOptions.PrefetchEntireImage);
            Machine machine = image.PEHeaders.CoffHeader.Machine;
            bool qualifiedArchitecture = RuntimeInformation.ProcessArchitecture switch
            {
                Architecture.X64 => machine == Machine.Amd64 && image.PEHeaders.PEHeader?.Magic == PEMagic.PE32Plus,
                Architecture.X86 => machine == Machine.I386 && image.PEHeaders.PEHeader?.Magic == PEMagic.PE32,
                _ => false
            };
            if (!qualifiedArchitecture) throw new InvalidDataException("Native pose module architecture differs from its Actor process.");
            stream.Position = 0;
            string sha = Convert.ToHexString(RuntimeFramework.HashSha256(stream));
            IntPtr module = NativeLibrary.Load(path);
            try { return new(path, sha, module); }
            catch { NativeLibrary.Free(module); throw; }
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate uint UIntDelegate();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate int CreateDelegate(uint abiVersion, uint transformBytes, ref IntPtr output);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void DestroyDelegate(IntPtr handle);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate int ResetDelegate(IntPtr handle, in NativeTransform pose, ulong now);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate int TargetDelegate(IntPtr handle, in NativeTransform pose, ulong now, uint duration);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate int SampleDelegate(IntPtr handle, ulong now, ref NativeTransform output);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate int ClearDelegate(IntPtr handle);
    }
}
