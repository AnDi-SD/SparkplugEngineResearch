using System;
using System.Buffers.Binary;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace WinxCoop.SessionRuntime;

// Thin managed ABI adapter. C++ owns the progression FSM and the only wire
// codec. These PODs describe trusted native-owner receipts, never create them.
internal static class ProgressionNative
{
    internal const uint AbiVersion = 1;
    internal const int WireBytes = 244;
    internal enum Role : uint { Host = 1, Guest = 2 }
    internal enum Kind : uint { Prepare = 1, Retired = 2, Load = 3, Loaded = 4, Resume = 5, Abort = 6 }
    internal enum Reason : uint { LevelChange = 1, Reload = 2, Respawn = 3, Checkpoint = 4, Cutscene = 5 }
    internal enum Phase : uint { Running, Preparing, WaitingLoad, Loading, WaitingGuest, WaitingResume, ResumePending, Closed }
    internal enum Terminal : uint
    {
        None, InvalidRecord, Identity, Authority, Sequence, NativeReceipt,
        Timeout, Clock, Backpressure, CounterExhausted, Disconnected
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct Digest32
    {
        public ulong Word0, Word1, Word2, Word3;
        // Local Windows ABI byte storage, not a network serializer.
        internal static Digest32 FromBytes(ReadOnlySpan<byte> bytes)
        {
            if (!BitConverter.IsLittleEndian) throw new PlatformNotSupportedException("Windows little-endian ABI required");
            if (bytes.Length != 32) throw new ArgumentException("Exactly 32 digest bytes required", nameof(bytes));
            return new Digest32
            {
                Word0 = BinaryPrimitives.ReadUInt64LittleEndian(bytes),
                Word1 = BinaryPrimitives.ReadUInt64LittleEndian(bytes[8..]),
                Word2 = BinaryPrimitives.ReadUInt64LittleEndian(bytes[16..]),
                Word3 = BinaryPrimitives.ReadUInt64LittleEndian(bytes[24..])
            };
        }
        internal readonly byte[] ToBytes()
        {
            var result = new byte[32];
            BinaryPrimitives.WriteUInt64LittleEndian(result, Word0);
            BinaryPrimitives.WriteUInt64LittleEndian(result.AsSpan(8), Word1);
            BinaryPrimitives.WriteUInt64LittleEndian(result.AsSpan(16), Word2);
            BinaryPrimitives.WriteUInt64LittleEndian(result.AsSpan(24), Word3);
            return result;
        }
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct Scene { public uint LevelEpoch, Generation; }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct Descriptor { public uint Level, Entry; public Digest32 Digest; }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct Connection { public ulong SessionId, ScenarioId, Token; }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct Channels
    {
        public ulong SessionId, InputToken, WorldToken;
        public ulong HostInputNonce, HostWorldNonce, GuestInputNonce, GuestWorldNonce;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct Decision
    {
        public ulong Operation;
        public Scene Previous, Next;
        public Reason Reason;
        public uint Reserved;
        public Descriptor Destination;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct CreateArgs
    {
        public uint AbiVersion, StructBytes;
        public Role Role;
        public uint Reserved;
        public Connection Connection;
        public Scene InitialScene;
        public Descriptor InitialDescriptor;
        public Channels InitialChannels;
        public ulong TransitionTimeoutMilliseconds;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct RetirementReceipt
    {
        public Scene Scene;
        public uint OriginalExitReturned, InputRetired, WorldRetired, BindingsRetired;
        public uint BorrowedBaselinesRestored, PatchesRestored, NoNativeOwnership;
        public uint NoPendingNativeCallsOrApply, LaterWorldResourcesReleased, Reserved;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct EntryReceipt
    {
        public Scene Scene;
        public Descriptor Descriptor;
        public Channels Channels;
        public Digest32 Seed, Witness;
        public ulong LocalEntryOrdinal;
        public uint OriginalLoadReturned, ActualReadyNativeWorld;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct AppliedReceipt
    {
        public Scene Scene;
        public Channels Channels;
        public ulong Revision, NativeTick;
        public uint CompleteNativeSnapshotApplied, FreshNeutralInputBoundaryObserved;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct Message
    {
        public Kind Kind;
        public Terminal Terminal;
        public Connection Connection;
        public Decision Decision;
        public Channels Channels;
        public Digest32 Seed, Witness;
        public ulong AppliedRevision, NativeTick;
        public uint RetirementMask, Reserved;
    }
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct Status
    {
        public uint AbiVersion, StructBytes;
        public Phase Phase;
        public Terminal Terminal;
        public uint InputAllowed, OutboundCount;
        public Scene CurrentScene;
        public Channels CurrentChannels;
        public ulong CompletedOperation, DeadlineMilliseconds;
    }

    internal sealed class Session : IDisposable
    {
        private readonly OwnedSession _handle;
        internal Session(CreateArgs args)
        {
            RequireSize<Digest32>(32); RequireSize<Scene>(8); RequireSize<Descriptor>(40);
            RequireSize<Connection>(24); RequireSize<Channels>(56); RequireSize<Decision>(72);
            RequireSize<CreateArgs>(152); RequireSize<RetirementReceipt>(48); RequireSize<EntryReceipt>(184);
            RequireSize<AppliedReceipt>(88); RequireSize<Message>(248); RequireSize<Status>(104);
            if (NativeMethods.AbiVersion() != AbiVersion || NativeMethods.WireBytes() != WireBytes)
                throw new InvalidOperationException("Progression native ABI/wire version mismatch");
            args.AbiVersion = AbiVersion;
            args.StructBytes = checked((uint)Marshal.SizeOf<CreateArgs>());
            int code = NativeMethods.Create(in args, out IntPtr value);
            if (code != 1 || value == IntPtr.Zero)
                throw new InvalidOperationException($"Progression create rejected: {code}");
            _handle = new OwnedSession(value);
        }
        internal Status Snapshot
        {
            get
            {
                int code = NativeMethods.Status(_handle, out Status result);
                if (code != 1 || result.AbiVersion != AbiVersion || result.StructBytes != Marshal.SizeOf<Status>())
                    throw new InvalidOperationException($"Progression status ABI rejected: {code}");
                return result;
            }
        }
        internal void Begin(in Decision value, ulong now) => Require(NativeMethods.Begin(_handle, in value, now));
        internal void ObserveRetired(in RetirementReceipt value, ulong now) => Require(NativeMethods.Retired(_handle, in value, now));
        internal void ObserveHostEntry(in EntryReceipt value, ulong now) => Require(NativeMethods.HostEntry(_handle, in value, now));
        internal void ObserveGuestApplied(in EntryReceipt entry, in AppliedReceipt applied, ulong now) =>
            Require(NativeMethods.GuestApplied(_handle, in entry, in applied, now));
        // The returned native view may be a legally ignored stale record. Gate
        // all native side effects with Snapshot, the active operation and the
        // exactly-once TakeLoad authorization, never with Kind alone.
        internal Message Receive(byte[]? wire, ulong now)
        {
            // Let the native owner reject malformed length/null input and close
            // the FSM, preserving its fail-closed behavior across the ABI.
            uint bytes = wire is null ? 0u : checked((uint)wire.Length);
            Require(NativeMethods.ReceiveWire(_handle, wire, bytes, now, out Message result));
            return result;
        }
        internal bool TryTakeLoad(ulong now, out Message authorization)
        {
            int code = NativeMethods.TakeLoad(_handle, now, out authorization);
            if (code == 0) return false;
            Require(code); return true;
        }
        // Retain this exact buffer across short TCP writes. Commit only after
        // all 244 bytes were accepted by the socket; no C# encoding is involved.
        internal bool TryPeekWire(ulong now, out byte[] wire, out Message view)
        {
            wire = new byte[WireBytes];
            int code = NativeMethods.PeekWire(_handle, now, wire, checked((uint)wire.Length), out view);
            if (code == 0) { wire = Array.Empty<byte>(); return false; }
            Require(code); return true;
        }
        internal void Commit(ulong now) => Require(NativeMethods.Commit(_handle, now));
        // Expected timeout is observable as false + Snapshot.Terminal==Timeout.
        internal bool Poll(ulong now)
        {
            int code = NativeMethods.Poll(_handle, now);
            if (code == -2) return false;
            Require(code); return true;
        }
        internal void Stop(Terminal why = Terminal.Disconnected) => NativeMethods.Stop(_handle, why);
        public void Dispose() => _handle.Dispose();
        private void Require(int code)
        {
            if (code == 1) return;
            Status status = Snapshot;
            throw new InvalidOperationException($"Progression rejected: result={code}, phase={status.Phase}, terminal={status.Terminal}");
        }
        private static void RequireSize<T>(int bytes) where T : struct
        {
            if (Marshal.SizeOf<T>() != bytes) throw new InvalidOperationException($"Progression ABI size mismatch for {typeof(T).Name}");
        }
    }

    private sealed class OwnedSession : SafeHandleZeroOrMinusOneIsInvalid
    {
        internal OwnedSession(IntPtr value) : base(true) => SetHandle(value);
        protected override bool ReleaseHandle() { NativeMethods.Destroy(handle); return true; }
    }
    private static class NativeMethods
    {
        private const string Library = "WinxCoop.ProgressionNative.dll";
        [DllImport(Library, EntryPoint = "wc_progression_abi_version", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern uint AbiVersion();
        [DllImport(Library, EntryPoint = "wc_progression_wire_bytes", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern uint WireBytes();
        [DllImport(Library, EntryPoint = "wc_progression_create", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int Create(in CreateArgs args, out IntPtr output);
        [DllImport(Library, EntryPoint = "wc_progression_destroy", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void Destroy(IntPtr handle);
        [DllImport(Library, EntryPoint = "wc_progression_begin", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int Begin(OwnedSession handle, in Decision value, ulong now);
        [DllImport(Library, EntryPoint = "wc_progression_retired", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int Retired(OwnedSession handle, in RetirementReceipt value, ulong now);
        [DllImport(Library, EntryPoint = "wc_progression_host_entry", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int HostEntry(OwnedSession handle, in EntryReceipt value, ulong now);
        [DllImport(Library, EntryPoint = "wc_progression_guest_applied", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int GuestApplied(OwnedSession handle, in EntryReceipt entry, in AppliedReceipt applied, ulong now);
        [DllImport(Library, EntryPoint = "wc_progression_receive_wire", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int ReceiveWire(OwnedSession handle, [In] byte[]? wire, uint bytes, ulong now, out Message accepted);
        [DllImport(Library, EntryPoint = "wc_progression_take_load", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int TakeLoad(OwnedSession handle, ulong now, out Message authorization);
        [DllImport(Library, EntryPoint = "wc_progression_peek_wire", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int PeekWire(OwnedSession handle, ulong now, [Out] byte[] wire, uint capacity, out Message view);
        [DllImport(Library, EntryPoint = "wc_progression_commit", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int Commit(OwnedSession handle, ulong now);
        [DllImport(Library, EntryPoint = "wc_progression_poll", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int Poll(OwnedSession handle, ulong now);
        [DllImport(Library, EntryPoint = "wc_progression_status", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int Status(OwnedSession handle, out Status output);
        [DllImport(Library, EntryPoint = "wc_progression_stop", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void Stop(OwnedSession handle, Terminal terminal);
    }
}
