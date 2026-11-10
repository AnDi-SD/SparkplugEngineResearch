using System.Net;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
using P = WinxCoop.SessionRuntime.ProgressionNative;

namespace WinxCoop.SessionHost;

// Typed local ABI only. The native peer owns Packet/MemoryStream framing,
// bootstrap assembly/hash checks and the TCP carrier. No wire codec lives here.
internal sealed class BootstrapPeer : IDisposable
{
    private const string Library = "WinxCoop.SessionBootstrapNative.dll";
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct CreateArgs
    {
        internal uint AbiVersion, StructBytes, Role, BindIpv4, BindPort, ServerIpv4, ServerPort, Reserved;
        internal P.Digest32 Profile;
        internal uint ConnectTimeoutMilliseconds, TransferTimeoutMilliseconds, HeartbeatMilliseconds, IdleTimeoutMilliseconds;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct Assignment
    {
        public uint AbiVersion, StructBytes;
        public ulong SessionId, ScenarioId, InputToken, WorldToken;
        public ulong HostInputNonce, HostWorldNonce, GuestInputNonce, GuestWorldNonce;
        public uint LevelEpoch, Generation, Level, Entry, ControlPort, InputPort, WorldPort, CheckpointBytes;
        public P.Digest32 PayloadDigest;
        internal P.Scene Scene => new() { LevelEpoch = LevelEpoch, Generation = Generation };
        internal P.Channels Channels => new()
        {
            SessionId = SessionId, InputToken = InputToken, WorldToken = WorldToken,
            HostInputNonce = HostInputNonce, HostWorldNonce = HostWorldNonce,
            GuestInputNonce = GuestInputNonce, GuestWorldNonce = GuestWorldNonce
        };
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct Status
    {
        public uint AbiVersion, StructBytes, Phase, Terminal, Role, ProfileMatched, BootstrapAvailable, BootstrapAcknowledged;
        public uint BoundIpv4, BoundPort, PeerIpv4, PeerPort;
        public ulong ConnectionId, TransferId, TotalBytes, ReceivedBytes;
        public uint ControlQueued, PendingOutbound, OwnedSockets, Reserved;
        public ulong SocketsOpened, SocketsClosed, WsaStarts, WsaCleanups, LastPumpMilliseconds, LastProgressMilliseconds;
        public Assignment Assignment;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    private struct ReadyReceipt
    {
        internal uint AbiVersion, StructBytes, Role, FullSnapshotApplied;
        internal ulong SessionId;
        internal uint LevelEpoch, Generation, FreshNeutralInput, Reserved;
        internal ulong Revision, NativeTick;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct ReadyState
    {
        public uint AbiVersion, StructBytes, LocalReady, PeerReady, BothReady, PeerRole, LevelEpoch, Generation;
        public ulong PeerRevision, PeerNativeTick;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct OwnerNotice
    {
        public uint AbiVersion, StructBytes, Role, Kind;
        public ulong SessionId;
        public uint OldEpoch, OldGeneration;
        public ulong OperationId;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    private struct AbiLayout
    {
        internal uint AbiVersion, StructBytes, CreateArgsBytes, AssignmentBytes, StatusBytes;
        internal uint AssignmentNonceOffset, AssignmentEpochOffset, AssignmentDigestOffset, StatusAssignmentOffset;
        internal uint SeedWitnessBytes, MaximumCheckpointBytes, MaximumBootstrapBytes;
    }
    private sealed class PeerHandle : SafeHandleZeroOrMinusOneIsInvalid
    {
        public PeerHandle() : base(true) { }
        protected override bool ReleaseHandle() { Destroy(handle); return true; }
    }
    private readonly PeerHandle _handle;
    private readonly int _ownerThread = Environment.CurrentManagedThreadId;
    private bool _disposed;
    private Status _lastStatus;

    internal BootstrapPeer(SessionConfiguration config, byte[] compatibilityDigest)
    {
        CheckLayout();
        var args = new CreateArgs
        {
            AbiVersion = 1, StructBytes = (uint)Marshal.SizeOf<CreateArgs>(), Role = config.Host ? 1u : 2u,
            BindIpv4 = Ipv4(config.BindAddress), BindPort = config.Host ? config.Port : 0u,
            ServerIpv4 = config.Host ? 0u : Ipv4(config.ServerAddress), ServerPort = config.Host ? 0u : config.Port,
            Profile = P.Digest32.FromBytes(compatibilityDigest), ConnectTimeoutMilliseconds = 180000,
            TransferTimeoutMilliseconds = 180000, HeartbeatMilliseconds = 500, IdleTimeoutMilliseconds = 15000
        };
        Check(Create(ref args, out _handle), "create bootstrap peer");
        try { Check(Start(_handle, Now), "start bootstrap peer"); _lastStatus = Snapshot; }
        catch { _handle.Dispose(); throw; }
    }

    internal static ulong Now => checked((ulong)Environment.TickCount64);
    internal Status Snapshot
    {
        get
        {
            Owner();
            if (_disposed) return _lastStatus;
            Check(ReadStatus(_handle, out Status value, (uint)Marshal.SizeOf<Status>()), "read bootstrap status");
            _lastStatus = value; return value;
        }
    }
    internal bool Pump()
    {
        Owner(); Require(!_disposed, "Bootstrap peer is disposed.");
        int result = NativePump(_handle, Now);
        if (result < 0) return false;
        return Snapshot.Terminal == 0;
    }
    internal bool Publish(Assignment assignment, byte[] seed, byte[] witness, byte[]? checkpoint = null)
    {
        Owner(); Require(seed.Length == 448 && witness.Length == 64 &&
            (checkpoint is null || checkpoint.Length is >= 349 and <= 65536), "Invalid bounded bootstrap payload.");
        byte[] payload = new byte[512 + (checkpoint?.Length ?? 0)];
        seed.CopyTo(payload, 0); witness.CopyTo(payload, 448); checkpoint?.CopyTo(payload, 512);
        assignment.AbiVersion = 1; assignment.StructBytes = (uint)Marshal.SizeOf<Assignment>();
        assignment.CheckpointBytes = (uint)(checkpoint?.Length ?? 0);
        int result = NativePublish(_handle, ref assignment, payload, (uint)payload.Length, Now);
        if (result == -6) return false;
        Check(result, "publish authoritative bootstrap"); return true;
    }
    internal bool TryRead(out Assignment assignment, out byte[] seed, out byte[] witness, out byte[] checkpoint)
    {
        Owner(); assignment = default; seed = witness = checkpoint = [];
        Status status = Snapshot;
        if (status.BootstrapAvailable == 0) return false;
        Require(status.TotalBytes is >= 512 and <= 66048, "Native bootstrap size escaped its ABI bound.");
        byte[] payload = new byte[(int)status.TotalBytes];
        int result = NativeRead(_handle, out assignment, (uint)Marshal.SizeOf<Assignment>(), payload, (uint)payload.Length, out uint bytes);
        if (result == 0) return false;
        Check(result, "read authoritative bootstrap");
        Require(bytes == payload.Length && bytes == 512 + assignment.CheckpointBytes, "Native bootstrap output size differs.");
        seed = payload[..448]; witness = payload[448..512]; checkpoint = payload[512..]; return true;
    }
    internal bool SendControl(byte[] wire)
    {
        Owner(); Require(wire.Length == P.WireBytes, "Exactly one native progression record is required.");
        int result = NativeSendControl(_handle, wire, (uint)wire.Length, Now);
        if (result == -6) return false;
        Check(result, "enqueue native progression frame"); return true;
    }
    internal bool TryReadControl(out byte[] wire)
    {
        Owner(); wire = new byte[P.WireBytes];
        int result = NativeReadControl(_handle, wire, (uint)wire.Length);
        if (result == 0 || result == -2 && Snapshot.Terminal == 3) return false;
        Check(result, "receive native progression frame"); return true;
    }
    internal void ObserveReady(Assignment assignment, bool guest, ulong actualRevision, ulong actualNativeTick,
        bool actualFreshNeutralInput)
    {
        Owner();
        Require(actualRevision != 0 && actualNativeTick != 0 && actualFreshNeutralInput,
            "Initial readiness requires a real native revision/tick and separately observed fresh neutral input.");
        var receipt = new ReadyReceipt
        {
            AbiVersion = 1, StructBytes = (uint)Marshal.SizeOf<ReadyReceipt>(), Role = guest ? 2u : 1u,
            FullSnapshotApplied = guest ? 1u : 0u, SessionId = assignment.SessionId,
            LevelEpoch = assignment.LevelEpoch, Generation = assignment.Generation,
            FreshNeutralInput = 1, Revision = actualRevision, NativeTick = actualNativeTick
        };
        Check(NativeObserveReady(_handle, ref receipt, Now), "publish actual owner readiness");
    }
    internal ReadyState Readiness
    {
        get
        {
            Owner();
            Check(NativeReadyState(_handle, out ReadyState value, (uint)Marshal.SizeOf<ReadyState>()), "read owner readiness");
            return value;
        }
    }
    internal bool SendNotice(Assignment assignment, bool guest, uint kind, ulong operation)
    {
        Owner();
        var notice = new OwnerNotice
        {
            AbiVersion = 1, StructBytes = (uint)Marshal.SizeOf<OwnerNotice>(), Role = guest ? 2u : 1u,
            Kind = kind, SessionId = assignment.SessionId, OldEpoch = assignment.LevelEpoch,
            OldGeneration = assignment.Generation, OperationId = operation
        };
        int result = NativeSendNotice(_handle, ref notice, Now);
        if (result == -6) return false;
        Check(result, "enqueue actual owner lifecycle notice"); return true;
    }
    internal bool TryReadNotice(out OwnerNotice notice)
    {
        Owner();
        int result = NativeReadNotice(_handle, out notice, (uint)Marshal.SizeOf<OwnerNotice>());
        // ABI v1 returns FAILED for an empty queue after transport termination.
        // Positive queued notices still come first. Preserve terminal status so
        // the Worker independently accepts only its fully retired SessionEnded
        // boundary; other terminal failures remain exceptions here.
        if (result == 0 || result == -2 && Snapshot.Terminal == 3) return false;
        Check(result, "read owner lifecycle notice"); return true;
    }
    internal void Stop()
    {
        Owner(); if (_disposed) return;
        NativeStop(_handle, 1); _lastStatus = Snapshot;
    }
    public void Dispose()
    {
        Owner(); if (_disposed) return;
        Stop(); _handle.Dispose(); _disposed = true;
    }
    private static void CheckLayout()
    {
        Check(Layout(out AbiLayout layout, (uint)Marshal.SizeOf<AbiLayout>()), "query bootstrap ABI");
        Require(layout.AbiVersion == 1 && layout.StructBytes == Marshal.SizeOf<AbiLayout>() &&
            layout.CreateArgsBytes == Marshal.SizeOf<CreateArgs>() &&
            layout.AssignmentBytes == Marshal.SizeOf<Assignment>() && layout.StatusBytes == Marshal.SizeOf<Status>() &&
            layout.AssignmentNonceOffset == (uint)Marshal.OffsetOf<Assignment>(nameof(Assignment.HostInputNonce)).ToInt32() &&
            layout.AssignmentEpochOffset == (uint)Marshal.OffsetOf<Assignment>(nameof(Assignment.LevelEpoch)).ToInt32() &&
            layout.AssignmentDigestOffset == (uint)Marshal.OffsetOf<Assignment>(nameof(Assignment.PayloadDigest)).ToInt32() &&
            layout.StatusAssignmentOffset == (uint)Marshal.OffsetOf<Status>(nameof(Status.Assignment)).ToInt32() &&
            layout.SeedWitnessBytes == 512 && layout.MaximumCheckpointBytes == 65536 && layout.MaximumBootstrapBytes == 66048,
            "Native bootstrap ABI size/offset/version differs.");
        Require(ReadyReceiptBytes() == Marshal.SizeOf<ReadyReceipt>() && ReadyStateBytes() == Marshal.SizeOf<ReadyState>(),
            "Native startup-ready ABI differs.");
        Require(Marshal.SizeOf<OwnerNotice>() == 40, "Native owner-notice ABI differs from the version1 header.");
    }
    internal static uint Ipv4(string text)
    { byte[] bytes = IPAddress.Parse(text).GetAddressBytes(); Require(bytes.Length == 4, "IPv4 is required."); return ((uint)bytes[0] << 24) | ((uint)bytes[1] << 16) | ((uint)bytes[2] << 8) | bytes[3]; }
    internal static string Ipv4Text(uint value) => $"{value >> 24}.{(value >> 16) & 255}.{(value >> 8) & 255}.{value & 255}";
    private void Owner() => Require(Environment.CurrentManagedThreadId == _ownerThread, "Bootstrap peer owner thread changed.");
    private static void Require(bool value, string message) => SessionConfiguration.Require(value, message);
    private static void Check(int result, string operation)
    { if (result != 1) throw new InvalidDataException(operation + " failed: " + result); }

    [DllImport(Library, EntryPoint = "wc_bootstrap_get_abi_layout", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Layout(out AbiLayout layout, uint bytes);
    [DllImport(Library, EntryPoint = "wc_bootstrap_create", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Create(ref CreateArgs args, out PeerHandle handle);
    [DllImport(Library, EntryPoint = "wc_bootstrap_start", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Start(PeerHandle handle, ulong now);
    [DllImport(Library, EntryPoint = "wc_bootstrap_pump", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativePump(PeerHandle handle, ulong now);
    [DllImport(Library, EntryPoint = "wc_bootstrap_publish", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativePublish(PeerHandle handle, ref Assignment assignment, byte[] payload, uint bytes, ulong now);
    [DllImport(Library, EntryPoint = "wc_bootstrap_try_read_bootstrap", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeRead(PeerHandle handle, out Assignment assignment, uint assignmentBytes, [Out] byte[] payload, uint capacity, out uint bytes);
    [DllImport(Library, EntryPoint = "wc_bootstrap_send_control", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeSendControl(PeerHandle handle, byte[] wire, uint bytes, ulong now);
    [DllImport(Library, EntryPoint = "wc_bootstrap_try_read_control", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeReadControl(PeerHandle handle, [Out] byte[] wire, uint capacity);
    [DllImport(Library, EntryPoint = "wc_bootstrap_ready_receipt_bytes", CallingConvention = CallingConvention.Cdecl)]
    private static extern uint ReadyReceiptBytes();
    [DllImport(Library, EntryPoint = "wc_bootstrap_ready_state_bytes", CallingConvention = CallingConvention.Cdecl)]
    private static extern uint ReadyStateBytes();
    [DllImport(Library, EntryPoint = "wc_bootstrap_observe_ready", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeObserveReady(PeerHandle handle, ref ReadyReceipt receipt, ulong now);
    [DllImport(Library, EntryPoint = "wc_bootstrap_ready_state", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeReadyState(PeerHandle handle, out ReadyState state, uint bytes);
    [DllImport(Library, EntryPoint = "wc_bootstrap_send_notice", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeSendNotice(PeerHandle handle, ref OwnerNotice notice, ulong now);
    [DllImport(Library, EntryPoint = "wc_bootstrap_try_read_notice", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeReadNotice(PeerHandle handle, out OwnerNotice notice, uint bytes);
    [DllImport(Library, EntryPoint = "wc_bootstrap_status", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ReadStatus(PeerHandle handle, out Status status, uint bytes);
    [DllImport(Library, EntryPoint = "wc_bootstrap_stop", CallingConvention = CallingConvention.Cdecl)]
    private static extern void NativeStop(PeerHandle handle, uint terminal);
    [DllImport(Library, EntryPoint = "wc_bootstrap_destroy", CallingConvention = CallingConvention.Cdecl)]
    private static extern void Destroy(IntPtr handle);
}
