using System.ComponentModel;

namespace SmoNativeValidator.Core;

internal delegate void BreakpointCallback(
    uint threadId,
    uint address,
    ref Win32Native.X86Context context);

internal readonly record struct BreakpointThreadCleanup(
    int RemovedReturnProbes,
    bool RemovedPendingRearm);

internal sealed class SoftwareBreakpointManager : IDisposable
{
    private const byte Int3 = 0xCC;
    private const uint TrapFlag = 0x00000100;

    private readonly IntPtr _process;
    private readonly Dictionary<uint, BreakpointSlot> _slots = [];
    private readonly Dictionary<uint, uint> _pendingRearmsByThread = [];
    private bool _disposed;

    internal SoftwareBreakpointManager(IntPtr process)
    {
        _process = process;
    }

    internal IEnumerable<uint> Addresses => _slots.Keys;

    internal void AddStatic(
        uint address,
        ExecutableCheckpoint checkpoint,
        BreakpointCallback callback)
    {
        ArgumentNullException.ThrowIfNull(checkpoint);
        ExecutableBytePattern pattern = checkpoint.VerificationPattern;
        byte[] actual = Win32Native.ReadMemory(_process, address, pattern.Length);
        if (!pattern.IsMatch(actual))
        {
            throw new InvalidDataException(
                $"Checkpoint {checkpoint.Id} signature mismatch at 0x{address:X8}: " +
                $"expected masked pattern '{pattern.Text}', actual {Convert.ToHexString(actual)}.");
        }

        BreakpointSlot slot = GetOrCreate(address);
        slot.StaticCheckpoint = checkpoint;
        slot.StaticCallback = callback;
    }

    internal void AddReturn(uint address, uint threadId, string name, BreakpointCallback callback)
    {
        BreakpointSlot slot = GetOrCreate(address);
        slot.ReturnProbes.Add(new ReturnProbe(threadId, name, callback));
    }

    internal bool Contains(uint address) => _slots.ContainsKey(address);

    internal bool TryHandleBreakpoint(
        uint threadId,
        uint address,
        ref Win32Native.X86Context context)
    {
        if (!_slots.TryGetValue(address, out BreakpointSlot? slot))
            return false;

        RestoreOriginal(slot);
        context.Eip = address;

        slot.StaticCallback?.Invoke(threadId, address, ref context);

        int returnIndex = slot.ReturnProbes.FindLastIndex(probe => probe.ThreadId == threadId);
        if (returnIndex >= 0)
        {
            ReturnProbe probe = slot.ReturnProbes[returnIndex];
            slot.ReturnProbes.RemoveAt(returnIndex);
            probe.Callback(threadId, address, ref context);
        }

        bool shouldRearm = slot.StaticCallback is not null || slot.ReturnProbes.Count > 0;
        if (shouldRearm)
        {
            context.EFlags |= TrapFlag;
            _pendingRearmsByThread[threadId] = address;
        }
        else
        {
            _slots.Remove(address);
        }

        Win32Native.SetX86Context(threadId, ref context);
        return true;
    }

    internal bool TryHandleSingleStep(uint threadId, ref Win32Native.X86Context context)
    {
        if (!_pendingRearmsByThread.Remove(threadId, out uint address))
            return false;

        context.EFlags &= ~TrapFlag;
        if (_slots.TryGetValue(address, out BreakpointSlot? slot))
            Arm(slot);
        Win32Native.SetX86Context(threadId, ref context);
        return true;
    }

    internal BreakpointThreadCleanup RemoveThread(uint threadId)
    {
        bool removedPendingRearm = _pendingRearmsByThread.Remove(
            threadId,
            out uint pendingAddress);
        int removedReturnProbes = 0;
        List<uint> emptySlots = [];

        foreach ((uint address, BreakpointSlot slot) in _slots)
        {
            removedReturnProbes += slot.ReturnProbes.RemoveAll(
                probe => probe.ThreadId == threadId);

            bool stillNeeded = slot.StaticCallback is not null || slot.ReturnProbes.Count > 0;
            if (!stillNeeded)
            {
                RestoreOriginal(slot);
                emptySlots.Add(address);
                continue;
            }

            if (removedPendingRearm && address == pendingAddress && !slot.Armed)
                Arm(slot);
        }

        foreach (uint address in emptySlots)
            _slots.Remove(address);

        return new BreakpointThreadCleanup(removedReturnProbes, removedPendingRearm);
    }

    private BreakpointSlot GetOrCreate(uint address)
    {
        if (_slots.TryGetValue(address, out BreakpointSlot? existing))
            return existing;
        byte original = Win32Native.ReadMemory(_process, address, 1)[0];
        BreakpointSlot slot = new(address, original);
        _slots.Add(address, slot);
        Arm(slot);
        return slot;
    }

    private void Arm(BreakpointSlot slot)
    {
        Win32Native.WriteMemory(_process, slot.Address, [Int3], executable: true);
        slot.Armed = true;
    }

    private void RestoreOriginal(BreakpointSlot slot)
    {
        if (!slot.Armed)
            return;
        Win32Native.WriteMemory(_process, slot.Address, [slot.OriginalByte], executable: true);
        slot.Armed = false;
    }

    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        foreach (BreakpointSlot slot in _slots.Values)
        {
            try
            {
                RestoreOriginal(slot);
            }
            catch (Win32Exception)
            {
                // The owned process may already have exited.
            }
        }
        _slots.Clear();
        _pendingRearmsByThread.Clear();
    }

    private sealed class BreakpointSlot(uint address, byte originalByte)
    {
        internal uint Address { get; } = address;
        internal byte OriginalByte { get; } = originalByte;
        internal bool Armed { get; set; }
        internal ExecutableCheckpoint? StaticCheckpoint { get; set; }
        internal BreakpointCallback? StaticCallback { get; set; }
        internal List<ReturnProbe> ReturnProbes { get; } = [];
    }

    private sealed record ReturnProbe(uint ThreadId, string Name, BreakpointCallback Callback);
}
