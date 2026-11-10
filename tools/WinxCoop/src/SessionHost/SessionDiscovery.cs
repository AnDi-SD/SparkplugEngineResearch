using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace WinxCoop.SessionHost;

internal sealed record DiscoveredServer(string Address, ushort Port, string Name, bool Occupied)
{
    public override string ToString() => $"{Name} — {Address}:{Port}" + (Occupied ? " (занято)" : "");
}

// A thin UI/worker ABI adapter over the same native UDP discovery used by the
// launcher. The E7 domain/port is distinct; TCP qualifies actual compatibility.
internal sealed class SessionDiscovery : IDisposable
{
    private const string Library = "WinxCoop.SessionDiscoveryNative.dll";
    [StructLayout(LayoutKind.Sequential, Pack = 8)] private struct Args
    { internal uint AbiVersion, StructBytes, DiscoveryPort, Reserved; }
    [StructLayout(LayoutKind.Sequential, Pack = 8)] private struct Layout
    { internal uint AbiVersion, StructBytes, CreateArgsBytes, LobbyBytes, StatusBytes, MaximumLobbies; }
    [StructLayout(LayoutKind.Sequential, Pack = 8)] private struct Lobby
    {
        internal uint AbiVersion, StructBytes, Ipv4, Port, Occupied, NameChars;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 49, ArraySubType = UnmanagedType.U2)] internal ushort[] Name;
        internal ushort Reserved;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct DiscoveryStatus
    {
        public uint AbiVersion, StructBytes, Hosting, Searching, LobbyCount;
        public int LastSocketError;
        public uint OwnedSockets, Reserved;
        public ulong Revision, SocketsOpened, SocketsClosed, ProbesSent, ProbesReceived, RepliesSent, RepliesReceived;
        public ulong InvalidDatagrams, SendErrors, ReceiveErrors, CloseErrors, WsaStarts, WsaCleanups, WsaCleanupErrors;
    }
    private sealed class Handle : SafeHandleZeroOrMinusOneIsInvalid
    {
        public Handle() : base(true) { }
        protected override bool ReleaseHandle() { Destroy(handle); return true; }
    }
    private readonly Handle _handle;
    private readonly int _thread = Environment.CurrentManagedThreadId;
    private bool _disposed;
    private DiscoveryStatus _last;

    internal SessionDiscovery(ushort port = 27046)
    {
        Check(GetLayout(out Layout layout, (uint)Marshal.SizeOf<Layout>()), "query discovery ABI");
        Require(layout.AbiVersion == 1 && layout.CreateArgsBytes == Marshal.SizeOf<Args>() &&
            layout.LobbyBytes == Marshal.SizeOf<Lobby>() && layout.StatusBytes == Marshal.SizeOf<DiscoveryStatus>() &&
            layout.MaximumLobbies == 16, "Discovery ABI layout differs.");
        var args = new Args { AbiVersion = 1, StructBytes = (uint)Marshal.SizeOf<Args>(), DiscoveryPort = port };
        Check(Create(ref args, out _handle), "create discovery owner");
    }
    internal void StartHosting(ushort port, string name)
    {
        Owner(); Require(!_disposed && port != 0 && name.Length is > 0 and <= 48, "Invalid lobby advertisement.");
        ushort[] utf16 = name.Select(c => (ushort)c).ToArray();
        Check(Host(_handle, port, utf16, (uint)utf16.Length), "advertise lobby");
    }
    internal void SetOccupied(bool value)
    { Owner(); Check(Occupied(_handle, value ? 1u : 0u), "set lobby occupancy"); }
    internal void BeginSearch()
    { Owner(); Require(!_disposed, "Discovery owner is disposed."); Check(Search(_handle, BootstrapPeer.Now), "start LAN search"); }
    internal void Pump()
    { Owner(); if (!_disposed) Check(NativePump(_handle, BootstrapPeer.Now), "pump LAN discovery"); }
    internal DiscoveryStatus Status
    {
        get
        {
            Owner(); if (_disposed) return _last;
            Check(ReadStatus(_handle, out DiscoveryStatus status, (uint)Marshal.SizeOf<DiscoveryStatus>()), "read discovery status");
            _last = status; return status;
        }
    }
    internal bool Searching => Status.Searching != 0;
    internal IReadOnlyList<DiscoveredServer> Lobbies
    {
        get
        {
            DiscoveryStatus status = Status;
            Require(status.LobbyCount <= 16, "Native discovery exceeded its lobby bound.");
            var result = new List<DiscoveredServer>((int)status.LobbyCount);
            for (uint index = 0; index < status.LobbyCount; ++index)
            {
                Check(ReadLobby(_handle, index, out Lobby value, (uint)Marshal.SizeOf<Lobby>()), "read discovered lobby");
                Require(value.NameChars <= 48 && value.Port is > 0 and <= ushort.MaxValue && value.Occupied <= 1 &&
                    value.Reserved == 0 && value.Name.Length == 49 && value.Name.Skip((int)value.NameChars).All(c => c == 0),
                    "Native lobby descriptor has invalid bounds.");
                string name = new(value.Name.Take((int)value.NameChars).Select(c => (char)c).ToArray());
                result.Add(new(BootstrapPeer.Ipv4Text(value.Ipv4), (ushort)value.Port, name, value.Occupied != 0));
            }
            return result;
        }
    }
    public void Dispose()
    {
        Owner(); if (_disposed) return;
        Stop(_handle); _last = Status; _handle.Dispose(); _disposed = true;
    }
    private void Owner() => Require(Environment.CurrentManagedThreadId == _thread, "Discovery owner thread changed.");
    private static void Require(bool value, string message) => SessionConfiguration.Require(value, message);
    private static void Check(int result, string operation)
    { if (result != 1) throw new InvalidDataException(operation + " failed: " + result); }

    [DllImport(Library, EntryPoint = "wc_discovery_get_abi_layout", CallingConvention = CallingConvention.Cdecl)]
    private static extern int GetLayout(out Layout layout, uint bytes);
    [DllImport(Library, EntryPoint = "wc_discovery_create", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Create(ref Args args, out Handle handle);
    [DllImport(Library, EntryPoint = "wc_discovery_destroy", CallingConvention = CallingConvention.Cdecl)]
    private static extern void Destroy(IntPtr handle);
    [DllImport(Library, EntryPoint = "wc_discovery_start_hosting", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Host(Handle handle, uint port, ushort[] name, uint chars);
    [DllImport(Library, EntryPoint = "wc_discovery_set_occupied", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Occupied(Handle handle, uint value);
    [DllImport(Library, EntryPoint = "wc_discovery_begin_search", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Search(Handle handle, ulong now);
    [DllImport(Library, EntryPoint = "wc_discovery_pump", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativePump(Handle handle, ulong now);
    [DllImport(Library, EntryPoint = "wc_discovery_status", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ReadStatus(Handle handle, out DiscoveryStatus status, uint bytes);
    [DllImport(Library, EntryPoint = "wc_discovery_try_get_lobby", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ReadLobby(Handle handle, uint index, out Lobby lobby, uint bytes);
    [DllImport(Library, EntryPoint = "wc_discovery_stop", CallingConvention = CallingConvention.Cdecl)]
    private static extern void Stop(Handle handle);
}
