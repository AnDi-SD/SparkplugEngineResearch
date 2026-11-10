using System.Net.Sockets;

namespace WinxCoop.SessionRuntime;

internal sealed record ProgressionControlChannelMetrics(ulong MessagesSent, ulong MessagesReceived,
    ulong SendCalls, ulong ReceiveCalls, ulong PartialSocketCalls, int PendingSendBytes,
    int PendingReceiveBytes, int BufferedReceiveBytes, bool PeerEof, bool TransportStopped,
    bool SocketDisposed);

// One serial owner, one borrowed native Session and one owned connected TCP
// socket. Native code alone supplies/consumes each fixed-size wire record.
// Receipts, transition decisions and socket connection policy belong to callers.
internal sealed class ProgressionControlChannel : IDisposable
{
    private readonly Socket _socket;
    private readonly int _maximumSendBytesPerCall, _maximumReceiveBytesPerCall;
    private readonly byte[] _incoming = new byte[ProgressionNative.WireBytes];
    private byte[]? _outgoing;
    private int _sent, _received;
    private ulong _messagesSent, _messagesReceived, _sendCalls, _receiveCalls, _partialCalls;
    private bool _peerEof, _transportStopped, _disposed;

    internal ProgressionNative.Session Core { get; }
    // PendingReceiveBytes is the remaining suffix of an already buffered
    // record, or zero when there is no prefix. No buffer contents are exposed.
    internal ProgressionControlChannelMetrics Metrics => new(_messagesSent, _messagesReceived,
        _sendCalls, _receiveCalls, _partialCalls, _outgoing is null ? 0 : _outgoing.Length - _sent,
        _received == 0 ? 0 : _incoming.Length - _received, _received, _peerEof, _transportStopped, _disposed);

    // Ownership of socket transfers only after successful construction. Core
    // remains borrowed and must outlive this channel, including Dispose.
    internal ProgressionControlChannel(ProgressionNative.Session borrowedCore, Socket ownedConnectedSocket,
        int maximumSendBytesPerCall = ProgressionNative.WireBytes,
        int maximumReceiveBytesPerCall = ProgressionNative.WireBytes)
    {
        ArgumentNullException.ThrowIfNull(borrowedCore);
        ArgumentNullException.ThrowIfNull(ownedConnectedSocket);
        if (maximumSendBytesPerCall is < 1 or > ProgressionNative.WireBytes ||
            maximumReceiveBytesPerCall is < 1 or > ProgressionNative.WireBytes)
            throw new ArgumentOutOfRangeException(nameof(maximumSendBytesPerCall), "Socket work must be bounded to one native wire record.");
        if (ownedConnectedSocket.SocketType != SocketType.Stream ||
            ownedConnectedSocket.ProtocolType != ProtocolType.Tcp || !ownedConnectedSocket.Connected)
            throw new ArgumentException("A connected TCP stream is required.", nameof(ownedConnectedSocket));
        ownedConnectedSocket.NoDelay = true;
        ownedConnectedSocket.Blocking = false;
        Core = borrowedCore; _socket = ownedConnectedSocket;
        _maximumSendBytesPerCall = maximumSendBytesPerCall;
        _maximumReceiveBytesPerCall = maximumReceiveBytesPerCall;
    }

    // Caller supplies its monotonic native-session clock. At most one
    // nonblocking Send and Receive occur per pump; no polling or sleep loop.
    // false preserves an existing native terminal or reports actual TCP EOF.
    internal bool Pump(ulong now)
    {
#if NET6_0
        if (_disposed) throw new ObjectDisposedException(GetType().FullName);
#else
        ObjectDisposedException.ThrowIf(_disposed, this);
#endif
        if (_transportStopped || !Core.Poll(now)) return false;
        if (_outgoing is null && Core.TryPeekWire(now, out byte[] wire, out _))
        { _outgoing = wire; _sent = 0; }
        if (_outgoing is { } output)
        {
            int remaining = output.Length - _sent;
            try
            {
                ++_sendCalls;
                int count = _socket.Send(output, _sent, Math.Min(_maximumSendBytesPerCall, remaining), SocketFlags.None);
                if (count == 0) { StopForTransport(false); return false; }
                if (count < remaining) ++_partialCalls;
                _sent += count;
                if (_sent == output.Length)
                {
                    Core.Commit(now);
                    _outgoing = null; _sent = 0; ++_messagesSent;
                }
            }
            catch (SocketException error) when (error.SocketErrorCode == SocketError.WouldBlock) { }
            catch (SocketException) { StopForTransport(false); throw; }
        }
        try
        {
            int remaining = _incoming.Length - _received;
            ++_receiveCalls;
            int count = _socket.Receive(_incoming, _received, Math.Min(_maximumReceiveBytesPerCall, remaining), SocketFlags.None);
            if (count == 0) { StopForTransport(true); return false; }
            if (count < remaining) ++_partialCalls;
            _received += count;
            if (_received == _incoming.Length)
            {
                // A complete positive read reaches native Receive before a
                // subsequent pump may observe EOF. Reading never creates ACKs.
                Core.Receive(_incoming, now);
                _received = 0; ++_messagesReceived;
            }
        }
        catch (SocketException error) when (error.SocketErrorCode == SocketError.WouldBlock) { }
        catch (SocketException) { StopForTransport(false); throw; }
        return true;
    }

    private void StopForTransport(bool eof)
    {
        if (_transportStopped) return;
        _transportStopped = true; _peerEof = eof;
        // Do not overwrite a native timeout or protocol failure with a later
        // transport diagnosis. WouldBlock never enters this path.
        if (Core.Snapshot.Terminal == ProgressionNative.Terminal.None)
            Core.Stop(ProgressionNative.Terminal.Disconnected);
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true; _socket.Dispose();
        // The caller owns Stop/Dispose of its borrowed native Session.
    }
}
