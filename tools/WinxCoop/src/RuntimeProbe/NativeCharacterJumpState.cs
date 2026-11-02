using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal sealed record NativeCharacterJumpWitness(uint StateIndex, uint State, uint Table,
    uint Getter, uint TypeRecord, uint ClassId, uint Selector, uint Owner, uint Animator);

/// <summary>Original inline Jumping-state identity, independent of its FSM array index.</summary>
internal sealed class NativeCharacterJumpState
{
    internal const uint JumpClassId = 0x756B123A, JumpTable = 0x6F82B0, JumpGetter = 0x5139D0;
    private const uint JumpTypeRecord = 0x74F5C8;
    private readonly DebuggerHost _host;
    private readonly int _ownerThread = Environment.CurrentManagedThreadId;

    internal NativeCharacterJumpState(DebuggerHost host, PEReader image)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ImageBase == 0x400000,
            "Native Jumping identity requires the stopped original image.");
        // Original registration names wxJumpingState and passes its CRC/base,
        // while the original constructor tail explicitly writes selector1.
        Pin(0x6CE600, "6A00684025400068A8F47400680C9A6D0068C27B8144683A126B75B9C8F57400E8CB49D4FFC3");
        Pin(0x4046CE, "C706B0826F00C74610010000008BC65EC3");
        Pin(JumpGetter, "B8C8F57400C3");
        Pin(JumpTable + 0x10, "D0395100");
        Require(U(JumpTypeRecord) == JumpClassId, "Original Jumping RTTI record has another registered class ID.");

        void Pin(uint address, string expectedHex)
        {
            byte[] expected = Convert.FromHexString(expectedHex);
            Require(image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, expected.Length).ToArray()
                .SequenceEqual(expected) && Read(address, expected.Length).SequenceEqual(expected),
                $"Original Jumping semantic pin changed at0x{address:X8}.");
        }
    }

    internal NativeCharacterJumpWitness? ReadAtWorld(uint tid, in Win32Native.X86Context context,
        uint machine, uint animator, uint currentIndex)
    {
        Require(_host.IsAtDebugStop && tid == _host.MainThreadId && context.Eip == NativeCallDispatcher.WorldReturn &&
            Environment.CurrentManagedThreadId == _ownerThread, "Native Jumping observation requires the owned WorldRET.");
        // The bound covers only the original inline state array. An unknown
        // index remains unmapped; index18 is never itself a Jumping claim.
        if (currentIndex >= 64) return null;
        byte[] machineState = Read(checked(machine + 0x130), checked(0x1C + (int)currentIndex * 4));
        Require(Word(machineState, 0) == currentIndex, "Native current FSM index changed within its stopped observation.");
        uint state = Word(machineState, checked(0x18 + (int)currentIndex * 4));
        if (state < 0x10000 || (state & 3) != 0) return null;
        byte[] native = Read(state, 0x1C);
        uint table = Word(native, 0), selector = Word(native, 0x10);
        if (table != JumpTable) return null;
        uint owner = Word(native, 0x14), boundAnimator = Word(native, 0x18),
            actualGetter = U(JumpTable + 0x10), actualClassId = U(JumpTypeRecord);
        Require(selector == 1 && owner == machine && boundAnimator == animator &&
            actualGetter == JumpGetter && actualClassId == JumpClassId,
            $"Typed native Jumping identity changed: index={currentIndex}, state=0x{state:X8}, table=0x{table:X8}, " +
            $"selector=0x{selector:X8}/expected0x00000001, owner14=0x{owner:X8}/expected0x{machine:X8}, " +
            $"animator18=0x{boundAnimator:X8}/expected0x{animator:X8}, getter=0x{actualGetter:X8}/expected0x{JumpGetter:X8}, " +
            $"classId=0x{actualClassId:X8}/expected0x{JumpClassId:X8}.");
        return new(currentIndex, state, table, JumpGetter, JumpTypeRecord, JumpClassId, selector, machine, animator);
    }

    private uint U(uint address) => Win32Native.ReadUInt32(_host.ProcessHandle, address);
    private byte[] Read(uint address, int count)
    {
        Require(address >= 0x10000 && count is > 0 and <= 512, "Native Jumping read exceeds its bounded original object.");
        return Win32Native.ReadMemory(_host.ProcessHandle, address, count);
    }
    private static uint Word(byte[] bytes, int offset) => BitConverter.ToUInt32(bytes, offset);
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
