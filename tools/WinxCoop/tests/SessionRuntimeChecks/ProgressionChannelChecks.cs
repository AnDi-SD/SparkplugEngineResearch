using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using WinxCoop.SessionRuntime;
using P = WinxCoop.SessionRuntime.ProgressionNative;

namespace WinxCoop.SessionRuntimeChecks;

internal static class ProgressionChannelChecks
{
    private static P.Digest32 Digest(ulong value) => new() { Word0 = value, Word1 = value + 1, Word2 = value + 2, Word3 = value + 3 };
    private static P.Descriptor Descriptor => new() { Level = 28, Entry = 0, Digest = Digest(428) };
    private static P.Channels Channels(ulong value) => new()
    {
        SessionId = value + 1, InputToken = value + 2, WorldToken = value + 3,
        HostInputNonce = value + 4, HostWorldNonce = value + 5, GuestInputNonce = value + 6, GuestWorldNonce = value + 7
    };
    private static P.CreateArgs Args(P.Role role, ulong timeout = 10000, ulong token = 102) => new()
    {
        Role = role, Connection = new() { SessionId = 100, ScenarioId = 101, Token = token },
        InitialScene = new() { LevelEpoch = 1, Generation = 1 }, InitialDescriptor = Descriptor,
        InitialChannels = Channels(1000), TransitionTimeoutMilliseconds = timeout
    };
    private static P.Decision Decision => new()
    {
        Operation = 1, Previous = new() { LevelEpoch = 1, Generation = 1 },
        Next = new() { LevelEpoch = 2, Generation = 2 }, Reason = P.Reason.Reload, Destination = Descriptor
    };
    private static P.RetirementReceipt Retired => new()
    {
        Scene = Decision.Previous, OriginalExitReturned = 1, InputRetired = 1, WorldRetired = 1,
        BindingsRetired = 1, BorrowedBaselinesRestored = 1, PatchesRestored = 1, NoNativeOwnership = 1,
        NoPendingNativeCallsOrApply = 1, LaterWorldResourcesReleased = 1
    };
    private static P.EntryReceipt Entry(bool host) => new()
    {
        Scene = Decision.Next, Descriptor = Descriptor, Channels = Channels(2000),
        Seed = Digest(2001), Witness = Digest(3001), LocalEntryOrdinal = host ? 2u : 1u,
        OriginalLoadReturned = 1, ActualReadyNativeWorld = 1
    };
    private static P.AppliedReceipt Applied => new()
    {
        Scene = Decision.Next, Channels = Channels(2000), Revision = 42, NativeTick = 17,
        CompleteNativeSnapshotApplied = 1, FreshNeutralInputBoundaryObserved = 1
    };

    private static (Socket Host, Socket Guest) SocketPair()
    {
        using var listener = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
        listener.Bind(new IPEndPoint(IPAddress.Loopback, 0)); listener.Listen(1);
        var guest = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
        try { guest.Connect(listener.LocalEndPoint!); return (listener.Accept(), guest); }
        catch { guest.Dispose(); throw; }
    }

    private sealed class Pair : IDisposable
    {
        internal readonly P.Session Host = new(Args(P.Role.Host)), Guest = new(Args(P.Role.Guest));
        internal readonly ProgressionControlChannel HostChannel, GuestChannel;
        internal ulong Now = 100;
        internal Pair()
        {
            var sockets = SocketPair();
            HostChannel = new(Host, sockets.Host, 7, 5); GuestChannel = new(Guest, sockets.Guest, 7, 5);
        }
        internal void Pump()
        { ChecksProgram.Require(HostChannel.Pump(Now) && GuestChannel.Pump(Now), "Pair became terminal unexpectedly."); }
        internal void Until(Func<bool> condition)
        {
            long end = Environment.TickCount64 + 5000;
            while (!condition())
            {
                ChecksProgram.Require(Environment.TickCount64 < end, "Actual loopback socket fixture exceeded5s.");
                Pump(); Thread.Yield();
            }
        }
        internal void ToLoadPermit()
        {
            Host.Begin(Decision, Now);
            Until(() => Guest.Snapshot.Phase == P.Phase.Preparing);
            ++Now; Host.ObserveRetired(Retired, Now); Guest.ObserveRetired(Retired, Now);
            Until(() => HostChannel.Metrics.MessagesReceived == 1);
            ++Now; Host.ObserveHostEntry(Entry(true), Now);
            P.Message authorization = default;
            Until(() => Guest.TryTakeLoad(Now, out authorization));
            ChecksProgram.Require(authorization.Kind == P.Kind.Load && authorization.Decision.Operation == 1 &&
                !Guest.TryTakeLoad(Now, out _), "Native Load authorization is not exactly once.");
        }
        public void Dispose()
        {
            HostChannel.Dispose(); GuestChannel.Dispose();
            Host.Stop(); Guest.Stop(); Host.Dispose(); Guest.Dispose();
        }
    }

    internal static void FragmentedTransition()
    {
        using var pair = new Pair(); pair.Host.Begin(Decision, pair.Now);
        ChecksProgram.Require(pair.HostChannel.Pump(pair.Now), "First fragment failed.");
        ChecksProgram.Require(pair.HostChannel.Metrics.PendingSendBytes == 237 &&
            pair.HostChannel.Metrics.MessagesSent == 0 && pair.Host.Snapshot.OutboundCount == 1,
            "Partial TCP send committed the native record prematurely.");
        pair.Until(() => pair.Guest.Snapshot.Phase == P.Phase.Preparing);
        ++pair.Now; pair.Host.ObserveRetired(Retired, pair.Now); pair.Guest.ObserveRetired(Retired, pair.Now);
        pair.Until(() => pair.HostChannel.Metrics.MessagesReceived == 1);
        ++pair.Now; pair.Host.ObserveHostEntry(Entry(true), pair.Now);
        pair.Until(() => pair.Guest.TryTakeLoad(pair.Now, out _));
        ChecksProgram.Require(!pair.Guest.TryTakeLoad(pair.Now, out _), "Duplicate Load permit.");
        pair.Guest.ObserveGuestApplied(Entry(false), Applied, pair.Now);
        pair.Until(() => pair.Host.Snapshot.Phase == P.Phase.ResumePending);
        ChecksProgram.Require(pair.Host.Snapshot.InputAllowed == 0 && pair.Guest.Snapshot.InputAllowed == 0,
            "Input opened before native Resume commit/receive.");
        pair.Until(() => pair.Host.Snapshot.InputAllowed == 1 && pair.Guest.Snapshot.InputAllowed == 1);
        ChecksProgram.Require(pair.Host.Snapshot.CompletedOperation == pair.Guest.Snapshot.CompletedOperation &&
            pair.Host.Snapshot.CompletedOperation == 1 && pair.HostChannel.Metrics.MessagesSent == 3 &&
            pair.GuestChannel.Metrics.MessagesSent == 2 && pair.HostChannel.Metrics.PartialSocketCalls > 0 &&
            pair.GuestChannel.Metrics.PartialSocketCalls > 0, "Fragmented native transition did not complete exactly once.");
    }

    private static byte[] NativePrepare(P.Session source)
    {
        source.Begin(Decision, 100);
        ChecksProgram.Require(source.TryPeekWire(100, out byte[] wire, out var view) && view.Kind == P.Kind.Prepare,
            "Native fixture did not produce Prepare wire.");
        return wire;
    }
    private static void Send(Socket peer, byte[] wire, int bytes)
    {
        int sent = 0;
        while (sent < bytes) { int n = peer.Send(wire, sent, Math.Min(13, bytes - sent), SocketFlags.None); ChecksProgram.Require(n > 0, "Raw fixture peer send failed."); sent += n; }
    }
    private static void Until(ProgressionControlChannel channel, Func<bool> done)
    {
        long end = Environment.TickCount64 + 5000;
        while (!done())
        {
            ChecksProgram.Require(Environment.TickCount64 < end, "Raw TCP fixture exceeded5s.");
            if (!channel.Pump(100)) break;
            Thread.Yield();
        }
    }

    internal static void PrefixThenEof()
    {
        using var source = new P.Session(Args(P.Role.Host)); using var receiver = new P.Session(Args(P.Role.Guest));
        var sockets = SocketPair(); using var peer = sockets.Host; using var channel = new ProgressionControlChannel(receiver, sockets.Guest, 7, 5);
        Send(peer, NativePrepare(source), 41); peer.Shutdown(SocketShutdown.Send);
        Until(channel, () => channel.Metrics.PeerEof);
        ChecksProgram.Require(channel.Metrics.PeerEof && channel.Metrics.BufferedReceiveBytes == 41 &&
            channel.Metrics.MessagesReceived == 0 && receiver.Snapshot.Terminal == P.Terminal.Disconnected &&
            receiver.Snapshot.InputAllowed == 0, "EOF promoted an incomplete244-byte native record.");
    }

    internal static void CompleteThenEof()
    {
        using var source = new P.Session(Args(P.Role.Host)); using var receiver = new P.Session(Args(P.Role.Guest));
        var sockets = SocketPair(); using var peer = sockets.Host; using var channel = new ProgressionControlChannel(receiver, sockets.Guest, 7, 5);
        Send(peer, NativePrepare(source), P.WireBytes); peer.Shutdown(SocketShutdown.Send);
        Until(channel, () => channel.Metrics.MessagesReceived == 1);
        ChecksProgram.Require(channel.Metrics.MessagesReceived == 1 && !channel.Metrics.PeerEof &&
            receiver.Snapshot.Phase == P.Phase.Preparing && receiver.Snapshot.InputAllowed == 0,
            "Positive complete record was lost before EOF.");
        Until(channel, () => channel.Metrics.PeerEof);
        ChecksProgram.Require(channel.Metrics.PeerEof && receiver.Snapshot.Terminal == P.Terminal.Disconnected,
            "Actual EOF did not stop the native borrowed session.");
    }

    internal static void WrongIdentity()
    {
        using var source = new P.Session(Args(P.Role.Host)); using var receiver = new P.Session(Args(P.Role.Guest, token: 999));
        var sockets = SocketPair(); using var peer = sockets.Host; using var channel = new ProgressionControlChannel(receiver, sockets.Guest, 7, 5);
        Send(peer, NativePrepare(source), P.WireBytes);
        ChecksProgram.Reject(() => Until(channel, () => channel.Metrics.MessagesReceived != 0), "Wrong native connection identity was accepted.");
        ChecksProgram.Require(receiver.Snapshot.Terminal == P.Terminal.Identity && receiver.Snapshot.InputAllowed == 0,
            "Identity failure permitted a Resume.");
        ChecksProgram.Reject(() => receiver.TryTakeLoad(100, out _), "Identity failure permitted a Load.");
    }

    internal static void IncompleteAppliedReceipt()
    {
        using var pair = new Pair(); pair.ToLoadPermit(); var applied = Applied; applied.FreshNeutralInputBoundaryObserved = 0;
        ChecksProgram.Reject(() => pair.Guest.ObserveGuestApplied(Entry(false), applied, pair.Now), "Incomplete actual-applied fixture receipt accepted.");
        ChecksProgram.Require(pair.Guest.Snapshot.Terminal == P.Terminal.NativeReceipt && pair.Guest.Snapshot.InputAllowed == 0 &&
            pair.Host.Snapshot.InputAllowed == 0, "Incomplete native receipt opened input.");
    }

    internal static void NativeTimeout()
    {
        using var host = new P.Session(Args(P.Role.Host, 100)); using var guest = new P.Session(Args(P.Role.Guest, 100));
        var sockets = SocketPair(); using var hc = new ProgressionControlChannel(host, sockets.Host, 7, 5); using var gc = new ProgressionControlChannel(guest, sockets.Guest, 7, 5);
        host.Begin(Decision, 100);
        long end = Environment.TickCount64 + 5000;
        while (guest.Snapshot.Phase != P.Phase.Preparing)
        { ChecksProgram.Require(Environment.TickCount64 < end && hc.Pump(100) && gc.Pump(100), "Timeout fixture Prepare failed."); Thread.Yield(); }
        ChecksProgram.Require(!hc.Pump(200) && host.Snapshot.Terminal == P.Terminal.Timeout && host.Snapshot.InputAllowed == 0 &&
            !hc.Metrics.PeerEof && !hc.Metrics.TransportStopped, "Native timeout was replaced with transport completion/EOF.");
        hc.Dispose(); hc.Dispose(); host.Stop();
        ChecksProgram.Require(host.Snapshot.Terminal == P.Terminal.Timeout, "Later cleanup overwrote native Timeout.");
    }

    internal static void BorrowedSessionLifetime()
    {
        using var core = new P.Session(Args(P.Role.Host)); var sockets = SocketPair(); using var peer = sockets.Guest;
        var channel = new ProgressionControlChannel(core, sockets.Host);
        ChecksProgram.Require(channel.Pump(100), "Empty nonblocking socket became EOF.");
        ChecksProgram.Require(!channel.Metrics.PeerEof && !channel.Metrics.TransportStopped && core.Snapshot.Terminal == P.Terminal.None,
            "WouldBlock was treated as disconnect.");
        channel.Dispose(); channel.Dispose();
        ChecksProgram.Require(channel.Metrics.SocketDisposed && core.Snapshot.Terminal == P.Terminal.None,
            "Channel Dispose destroyed/stopped its borrowed native Session.");
        core.Stop(); core.Dispose(); core.Dispose();
    }
}
