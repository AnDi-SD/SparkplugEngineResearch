using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

internal sealed record NativeCharacterHurtWitness(uint StateIndex, uint State, uint Table,
    uint Getter, uint TypeRecord, uint ClassId, uint Selector, uint Owner, uint Animator);

/// <summary>Read-only original Hurt identity for the bounded ordinary-input neutral fence.</summary>
internal sealed class NativeCharacterHurtState
{
    internal const uint HurtClassId = 0x37C025BF, HurtTable = 0x6F8518, HurtGetter = 0x5159D0;
    private const uint HurtTypeRecord = 0x74F868;
    private readonly DebuggerHost _host;
    private readonly int _ownerThread = Environment.CurrentManagedThreadId;

    internal NativeCharacterHurtState(DebuggerHost host, PEReader image)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ImageBase == 0x400000,
            "Native Hurt identity requires the stopped original image.");
        Pin(0x6CE750, "6A0068E027400068A8F4740068949A6D0068C27B814468BF25C037B968F87400E87B48D4FFC3");
        Pin(0x406A02, "C70618856F00C6463C00C746100A0000008BC65EC3");
        Pin(HurtGetter, "B868F87400C3");
        Pin(HurtTable + 0x10, "D0595100");
        Require(U(HurtTypeRecord) == HurtClassId, "Original Hurt RTTI record has another registered class ID.");

        void Pin(uint address, string expectedHex)
        {
            byte[] expected = Convert.FromHexString(expectedHex);
            Require(image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, expected.Length).ToArray()
                .SequenceEqual(expected) && Read(address, expected.Length).SequenceEqual(expected),
                $"Original Hurt semantic pin changed at0x{address:X8}.");
        }
    }

    internal NativeCharacterHurtWitness? ReadAtWorld(uint tid, in Win32Native.X86Context context,
        uint machine, uint animator, uint currentIndex)
    {
        Require(_host.IsAtDebugStop && tid == _host.MainThreadId && context.Eip == NativeCallDispatcher.WorldReturn &&
            Environment.CurrentManagedThreadId == _ownerThread, "Native Hurt observation requires the owned WorldRET.");
        // Only the original qualified Hurt10 slot is admitted. An arbitrary
        // non-idle state is never treated as an ordinary-input exception.
        if (currentIndex != 10) return null;
        byte[] machineState = Read(checked(machine + 0x130), 0x44);
        Require(Word(machineState, 0) == currentIndex, "Native current FSM index changed within its stopped Hurt observation.");
        uint state = Word(machineState, 0x40);
        if (state < 0x10000 || (state & 3) != 0) return null;
        byte[] native = Read(state, 0x1C);
        uint table = Word(native, 0), selector = Word(native, 0x10);
        if (table != HurtTable) return null;
        uint owner = Word(native, 0x14), boundAnimator = Word(native, 0x18),
            actualGetter = U(HurtTable + 0x10), actualClassId = U(HurtTypeRecord);
        Require(selector == 10 && owner == machine && boundAnimator == animator &&
            actualGetter == HurtGetter && actualClassId == HurtClassId,
            "Typed native Hurt state lost its original selector/owner/Animator/RTTI identity.");
        return new(currentIndex, state, table, HurtGetter, HurtTypeRecord, HurtClassId, selector, owner, boundAnimator);
    }

    private uint U(uint address) => Win32Native.ReadUInt32(_host.ProcessHandle, address);
    private byte[] Read(uint address, int count)
    {
        Require(address >= 0x10000 && count is > 0 and <= 512, "Native Hurt read exceeds its bounded original object.");
        return Win32Native.ReadMemory(_host.ProcessHandle, address, count);
    }
    private static uint Word(byte[] bytes, int offset) => BitConverter.ToUInt32(bytes, offset);
    private static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
}
