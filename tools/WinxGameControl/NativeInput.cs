using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;

namespace WinxGameControl;

/// <summary>
/// Explicit, finite OS scan-code/relative-mouse input for the selected foreground game.
/// SendInput is global Windows input; neither this API nor OS readback proves game consumption.
/// </summary>
public static class NativeInput
{
    private const uint KeyboardType = 1, ScanCode = 8, Extended = 1, KeyUp = 2;
    private const uint MouseMove = 1, LeftDown = 2, LeftUp = 4, RightDown = 8, RightUp = 16;
    private static readonly SemaphoreSlim Owner = new(1, 1);
    private static long sends, acceptedEvents, releaseFailures;
    private static int lastCount, lastError;
    private static readonly int InputBytes = Marshal.SizeOf<Input>();

    static NativeInput()
    {
        int expected = IntPtr.Size == 8 ? 40 : 28, expectedOffset = IntPtr.Size == 8 ? 8 : 4;
        if (InputBytes != expected || Marshal.OffsetOf<Input>(nameof(Input.Data)).ToInt32() != expectedOffset)
            throw new TypeLoadException("The native INPUT union does not match the Windows ABI.");
    }

    public static object Readback() => new
    {
        pointerBytes = IntPtr.Size, inputBytes = InputBytes,
        unionOffset = Marshal.OffsetOf<Input>(nameof(Input.Data)).ToInt32(),
        sendCalls = Interlocked.Read(ref sends), acceptedEventCount = Interlocked.Read(ref acceptedEvents),
        lastSendInputCount = Volatile.Read(ref lastCount), lastWin32Error = Volatile.Read(ref lastError),
        releaseFailures = Interlocked.Read(ref releaseFailures), gameConsumptionClaimed = false
    };

    public static async Task SendKeyAsync(TargetIdentity target, ushort scan, bool extended,
        int holdMs, int neutralMs, CancellationToken token)
    {
        ValidateTiming(holdMs, neutralMs);
        ValidateScan(scan, extended);
        await Owner.WaitAsync(token).ConfigureAwait(false);
        try
        {
            NativeDesktop.Validate(target, true);
            NativeDesktop.GuardNoWindowsKey();
            NativeDesktop.GuardKeyReleased(scan, extended);
            NativeDesktop.GuardModifiers();
            Exception? failure = null, releaseFailure = null, neutralFailure = null;
            bool releaseRequired = false;
            try
            {
                token.ThrowIfCancellationRequested();
                NativeDesktop.Validate(target, true);
                NativeDesktop.GuardModifiers();
                releaseRequired = true;
                Send(Keyboard(scan, extended, false));
                await DwellAsync(target, holdMs, false, token, scan, extended).ConfigureAwait(false);
            }
            catch (Exception error) { failure = error; }
            finally
            {
                // Release is unconditional after our Down attempt, including focus loss/cancellation.
                // It never activates a window and never releases a key that was already held at admission.
                if (releaseRequired)
                    try { Send(Keyboard(scan, extended, true)); }
                    catch (Exception error) { Interlocked.Increment(ref releaseFailures); releaseFailure = error; }
            }
            try { await DwellAsync(target, neutralMs, false, token).ConfigureAwait(false); }
            catch (Exception error) { neutralFailure = error; }
            Complete(failure, releaseFailure, neutralFailure);
        }
        finally { Owner.Release(); }
    }

    public static async Task SendMouseAsync(TargetIdentity target, int dx, int dy, string? button,
        int holdMs, int neutralMs, CancellationToken token)
    {
        ValidateTiming(holdMs, neutralMs);
        if (dx is < -4096 or > 4096 || dy is < -4096 or > 4096)
            throw new ArgumentOutOfRangeException(nameof(dx), "Relative mouse deltas must be within -4096..4096.");
        bool? right = ParseButton(button);
        if (dx == 0 && dy == 0 && !right.HasValue) throw new ArgumentException("A relative movement or left/right button is required.");
        await Owner.WaitAsync(token).ConfigureAwait(false);
        try
        {
            NativeDesktop.Validate(target, true);
            NativeDesktop.GuardModifiers();
            if (right.HasValue) { NativeDesktop.GuardMouseButton(target); NativeDesktop.GuardMouseReleased(right.Value); }
            Exception? failure = null, releaseFailure = null, neutralFailure = null;
            bool releaseRequired = false;
            try
            {
                token.ThrowIfCancellationRequested();
                if (dx != 0 || dy != 0)
                {
                    NativeDesktop.Validate(target, true);
                    NativeDesktop.GuardModifiers();
                    Send(Mouse(dx, dy, MouseMove));
                }
                if (right.HasValue)
                {
                    // Check the actual OS position again after relative motion; clipping is never assumed.
                    NativeDesktop.GuardMouseButton(target);
                    NativeDesktop.GuardModifiers();
                    releaseRequired = true;
                    Send(Mouse(0, 0, right.Value ? RightDown : LeftDown));
                }
                await DwellAsync(target, holdMs, right.HasValue, token).ConfigureAwait(false);
            }
            catch (Exception error) { failure = error; }
            finally
            {
                if (releaseRequired)
                    try { Send(Mouse(0, 0, right!.Value ? RightUp : LeftUp)); }
                    catch (Exception error) { Interlocked.Increment(ref releaseFailures); releaseFailure = error; }
            }
            try { await DwellAsync(target, neutralMs, false, token).ConfigureAwait(false); }
            catch (Exception error) { neutralFailure = error; }
            Complete(failure, releaseFailure, neutralFailure);
        }
        finally { Owner.Release(); }
    }

    private static async Task DwellAsync(TargetIdentity target, int milliseconds, bool mouseHeld, CancellationToken token,
        ushort? ownedScan = null, bool extended = false)
    {
        long start = Stopwatch.GetTimestamp();
        while (true)
        {
            token.ThrowIfCancellationRequested();
            NativeDesktop.Validate(target, true);
            NativeDesktop.GuardModifiers(ownedScan, extended);
            if (mouseHeld) NativeDesktop.GuardMouseButton(target);
            double remaining = milliseconds - Stopwatch.GetElapsedTime(start).TotalMilliseconds;
            if (remaining <= 0) return;
            await Task.Delay(Math.Max(1, (int)Math.Min(10, Math.Ceiling(remaining))), token).ConfigureAwait(false);
        }
    }

    private static void Complete(Exception? failure, Exception? releaseFailure, Exception? neutralFailure)
    {
        // Preserve the first operation's exception and stack; release/neutral diagnostics must not mask it.
        Exception? first = failure ?? releaseFailure ?? neutralFailure;
        if (first is null) return;
        if (releaseFailure is not null && !ReferenceEquals(first, releaseFailure))
            first.Data["WinxGameControl.ReleaseFailure"] = releaseFailure.ToString();
        if (neutralFailure is not null && !ReferenceEquals(first, neutralFailure))
            first.Data["WinxGameControl.NeutralFailure"] = neutralFailure.ToString();
        ExceptionDispatchInfo.Capture(first).Throw();
    }

    private static void ValidateTiming(int holdMs, int neutralMs)
    {
        if (holdMs is < 1 or > 10000) throw new ArgumentOutOfRangeException(nameof(holdMs), "Hold must be 1..10000 milliseconds.");
        if (neutralMs is < 1 or > 10000) throw new ArgumentOutOfRangeException(nameof(neutralMs), "Neutral gap must be 1..10000 milliseconds.");
    }

    private static void ValidateScan(ushort scan, bool extended)
    {
        bool supported = extended
            ? scan is 0x1C or 0x1D or 0x35 or 0x38 or 0x47 or 0x48 or 0x49 or 0x4B or 0x4D or 0x4F or 0x50 or 0x51 or 0x52 or 0x53
            : scan is >= 0x01 and <= 0x53 or 0x56 or 0x57 or 0x58 or 0x64 or 0x65 or 0x66 or 0x70 or 0x73 or 0x79 or 0x7B or 0x7D or 0x7E;
        if (!supported) throw new ArgumentOutOfRangeException(nameof(scan), "Use a supported base DIK scan plus its explicit extended flag; Windows/Super/Menu and media keys are excluded.");
    }

    private static bool? ParseButton(string? button)
    {
        if (button is null) return null;
        if (string.Equals(button, "left", StringComparison.OrdinalIgnoreCase) || string.Equals(button, "L", StringComparison.OrdinalIgnoreCase)) return false;
        if (string.Equals(button, "right", StringComparison.OrdinalIgnoreCase) || string.Equals(button, "R", StringComparison.OrdinalIgnoreCase)) return true;
        throw new ArgumentException("Mouse button must be left, right or null.", nameof(button));
    }

    private static Input Keyboard(ushort scan, bool extended, bool up) => new()
    {
        Type = KeyboardType,
        Data = new InputUnion { Keyboard = new KeyboardInput { Scan = scan, Flags = ScanCode | (extended ? Extended : 0) | (up ? KeyUp : 0) } }
    };
    private static Input Mouse(int dx, int dy, uint flags) => new()
    { Type = 0, Data = new InputUnion { Mouse = new MouseInput { Dx = dx, Dy = dy, Flags = flags } } };

    private static void Send(Input input)
    {
        uint count = SendInput(1, ref input, InputBytes);
        int error = Marshal.GetLastWin32Error();
        Interlocked.Increment(ref sends); Interlocked.Add(ref acceptedEvents, count);
        Volatile.Write(ref lastCount, (int)count); Volatile.Write(ref lastError, count == 1 ? 0 : error);
        if (count != 1) throw new Win32Exception(error, $"SendInput accepted {count}/1 events, Win32 error {error}; focus and integrity require separate evidence.");
    }

    [StructLayout(LayoutKind.Sequential)] private struct Input { public uint Type; public InputUnion Data; }
    [StructLayout(LayoutKind.Explicit)] private struct InputUnion
    {
        [FieldOffset(0)] public MouseInput Mouse;
        [FieldOffset(0)] public KeyboardInput Keyboard;
        [FieldOffset(0)] public HardwareInput Hardware;
    }
    [StructLayout(LayoutKind.Sequential)] private struct MouseInput
    { public int Dx, Dy; public uint MouseData, Flags, Time; public nuint ExtraInfo; }
    [StructLayout(LayoutKind.Sequential)] private struct KeyboardInput
    { public ushort VirtualKey, Scan; public uint Flags, Time; public nuint ExtraInfo; }
    [StructLayout(LayoutKind.Sequential)] private struct HardwareInput
    { public uint Message; public ushort ParameterLow, ParameterHigh; }
    [DllImport("user32.dll", SetLastError = true)] private static extern uint SendInput(uint count, ref Input input, int size);
}
