using System.Runtime.InteropServices;

namespace WinxGameControl;

// Read-only Windows SDK XINPUT_GAMEPAD12/XINPUT_STATE16 observation. Index
// ownership is supplied by the ViGEm controller SDK, never guessed from slot0.
public static class XInputReadback
{
    public sealed record Slot(uint UserIndex, uint? ErrorCode, bool StateValid,
        uint PacketNumber, ushort Buttons, byte LeftTrigger, byte RightTrigger,
        short LeftThumbX, short LeftThumbY, short RightThumbX, short RightThumbY,
        bool? ExactNeutral, string? Error);
    public sealed record Snapshot(string Boundary, string Utc, long Tick,
        bool AbiVerified, IReadOnlyList<Slot> Slots);

    [StructLayout(LayoutKind.Sequential)]
    private struct Gamepad
    {
        public ushort Buttons;
        public byte LeftTrigger, RightTrigger;
        public short LeftThumbX, LeftThumbY, RightThumbX, RightThumbY;
    }
    [StructLayout(LayoutKind.Sequential)]
    private struct State { public uint PacketNumber; public Gamepad Gamepad; }

    public static Snapshot Read(string boundary)
    {
        bool abi = Marshal.SizeOf<Gamepad>() == 12 && Marshal.SizeOf<State>() == 16 &&
            Marshal.OffsetOf<State>(nameof(State.Gamepad)).ToInt32() == 4 &&
            Marshal.OffsetOf<Gamepad>(nameof(Gamepad.LeftTrigger)).ToInt32() == 2 &&
            Marshal.OffsetOf<Gamepad>(nameof(Gamepad.LeftThumbX)).ToInt32() == 4;
        var slots = new List<Slot>(4);
        for (uint index = 0; index < 4; ++index)
        {
            State state = default; uint? code = null; string? error = null;
            try
            {
                if (!abi) throw new InvalidDataException("XInput ABI differs from the SDK12/16-byte layouts.");
                code = XInputGetState(index, ref state);
            }
            catch (Exception exception) { error = exception.GetType().Name + ": " + exception.Message; }
            Gamepad value = state.Gamepad;
            bool valid = code == 0;
            bool neutral = value.Buttons == 0 && value.LeftTrigger == 0 && value.RightTrigger == 0 &&
                value.LeftThumbX == 0 && value.LeftThumbY == 0 && value.RightThumbX == 0 && value.RightThumbY == 0;
            slots.Add(new(index, code, valid, state.PacketNumber, value.Buttons,
                value.LeftTrigger, value.RightTrigger, value.LeftThumbX, value.LeftThumbY,
                value.RightThumbX, value.RightThumbY, valid ? neutral : null, error));
        }
        return new(boundary, DateTime.UtcNow.ToString("o"), Environment.TickCount64, abi, slots.AsReadOnly());
    }

    [DllImport("xinput1_4.dll", ExactSpelling = true, CallingConvention = CallingConvention.StdCall)]
    [DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
    private static extern uint XInputGetState(uint index, ref State state);
}
