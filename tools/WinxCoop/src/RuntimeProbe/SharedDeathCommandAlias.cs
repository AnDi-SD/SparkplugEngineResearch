using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own bounded shared-death policy. After one real native lethal frontend has
/// produced its own dead command, publish that actual byte to the other body.
/// HP, pending-hurt state, FSM consumption and dying animations stay native.
/// </summary>
internal sealed class SharedDeathCommandAlias
{
    private const uint Frontend = 0x4FE750, ProducedDead = 0x4FE91C, FrontendCaller = 0x4FE274;
    private const uint TraitsTable = 0x6F6990, CharacterTable = 0x6F6438, MachineTable = 0x6FA288;
    private const uint CommandTable = 0x703090;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly int _ownerThread;
    private readonly Dictionary<string, uint> _counts = [];
    private readonly List<object> _samples = [];
    private Owner? _primary, _secondary;
    private SharedHealthAlias? _health;
    private Frame? _pending;
    private uint _profile, _scene, _sequence, _worlds, _publications, _writeAttempts, _writes, _retireEntry;
    private bool _bound, _retired, _failed;

    private sealed record Owner(uint Instance, uint Traits, uint Character, uint Machine, uint Root, uint Command);
    private sealed class Frame(uint id, Owner source, Owner other, uint esp, uint message, uint sender,
        uint health, uint commits, Win32Native.X86Context c)
    {
        internal readonly uint Id = id, Esp = esp, Message = message, Sender = sender;
        internal readonly uint HealthBefore = health, CommitsBefore = commits;
        internal readonly uint Esi = c.Esi, Edi = c.Edi, Ebx = c.Ebx, Ebp = c.Ebp;
        internal readonly Owner Source = source, Other = other;
        internal bool ProducerReached;
    }

    internal SharedDeathCommandAlias(DebuggerHost host, PEReader image, Action<object> log)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _image = image ?? throw new ArgumentNullException(nameof(image));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _ownerThread = Environment.CurrentManagedThreadId;
    }

    internal bool Active => _bound && !_retired && !_failed;
    internal bool HasPendingNativePair => _pending is not null;
    internal uint CompletedPublications => _publications;
    internal object Summary => new
    {
        bound = _bound, active = Active, retiredBeforeOwnDelete = _retired, terminalFailure = _failed,
        primary = Describe(_primary), secondary = Describe(_secondary), profile = Hex(_profile), scene = Hex(_scene),
        pendingNativePairs = _pending is null ? 0 : 1, maximumPendingPairs = 1, verifiedWorlds = _worlds,
        damageSourceProfile = "original2717/damage1 frontend; sender remains opaque",
        completedPublications = _publications, maximumLethalTransactions = 1,
        derivedDeadByteWriteAttempts = _writeAttempts, derivedDeadByteWritesReadbackVerified = _writes,
        retireEntry = Hex(_retireEntry), counts = new Dictionary<string, uint>(_counts), samples = _samples.ToArray(),
        policy = "copy only actual native source command5F=1 after full lethal frontend return",
        healthOrPendingFlagWrites = 0, damageOrNativeCallsAdded = 0, contextWrites = 0,
        hurtPending1D0Mirrored = false, nativeDeathConsumptionQualified = false,
        runtimeSharedDeathQualified = false, fullE3Complete = false
    };

    internal void BindAtWorld(uint tid, ref Win32Native.X86Context context,
        uint primaryInstance, uint primaryTraits, uint primaryCharacter, uint primaryMachine,
        uint secondaryInstance, uint secondaryTraits, uint secondaryCharacter, uint secondaryMachine,
        SharedHealthAlias health)
    {
        Ensure(tid); ArgumentNullException.ThrowIfNull(health);
        Require(!_bound && !_retired && !_failed && context.Eip == NativeCallDispatcher.WorldReturn &&
            _host.ImageBase == 0x400000 && _host.ProcessHandle != IntPtr.Zero && health.Active,
            "Shared death command requires its first original WorldRET and active common HP owner.");
        try
        {
            Require(!_host.Breakpoints.Contains(Frontend) && !_host.Breakpoints.Contains(ProducedDead) &&
                !_host.Breakpoints.Contains(FrontendCaller), "Shared death command native frame already has another hook owner.");
            _health = health; _profile = Pointer(0x765AD4);
            _primary = Capture(primaryInstance, primaryTraits, primaryCharacter, primaryMachine);
            _secondary = Capture(secondaryInstance, secondaryTraits, secondaryCharacter, secondaryMachine);
            _scene = Pointer(Primary.Root + 0x3C); VerifyOwners(); EqualHealth();
            Require(U(Primary.Traits + 0x124) > 0, "Shared death command binding requires a living common resource.");
            Pin(Frontend, "A19852750083EC3485C0535556578BF1");
            Pin(0x4FE26C, "8B16578BCEFF52545F5EC20400");
            Pin(0x4FE8CB, "E8606A04008B501C0396D80100008996D40100008B471C8B4F1850516818270000568BCEE83C74F3FF");
            Pin(0x4FE8F4, "8B868C01000085C00F84A30000008B883001000085C90F84950000008B962401000085D2B301770588595FEB7E");
            Pin(0x4FE99F, "889ED0010000");
            Pin(0x4FE9E2, "5F5E5D5B83C434C20400");
            Pin(NativeCallDispatcher.WorldReturn, "C3");
            _bound = true;
            Hook(Frontend, "A19852750083EC3485C0535556578BF1", "frontend", FrontendEntry);
            Hook(ProducedDead, "88595FEB7E", "native-dead-producer", NativeProducer);
            Sample("bound", new { primary = Describe(Primary), secondary = Describe(Secondary),
                writesAtBind = 0, actualNativeProducer = Hex(ProducedDead), actualReturnCaller = Hex(FrontendCaller) });
        }
        catch { _failed = true; throw; }
    }

    internal void VerifyAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid); Require(context.Eip == NativeCallDispatcher.WorldReturn, "Shared death command verification requires WorldRET.");
        if (_retired) return;
        Require(Active && _health?.Active == true && _pending is null, "Shared death command reached WorldRET with an unmatched frame.");
        VerifyOwners(); EqualHealth(); Require(++_worlds <= 36000, "Shared death command world budget exceeded.");
    }

    internal void RetireBeforeOwnDelete(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid);
        Require(Active && _health?.Active == true && _pending is null && context.Eip is
            NativeCallDispatcher.WorldReturn or 0x5F2300 or 0x5F86C0 or 0x4E8AA0 or 0x4E8C80 or 0x59AE80 or 0x59B190,
            "Shared death command must retire before its common HP peer and native own deletion.");
        VerifyOwners(); EqualHealth(); _retireEntry = context.Eip; _retired = true;
        Sample("retired-before-own-delete", new { entry = Hex(context.Eip), pendingNativePairs = 0,
            callbacksDormantBeforeDelete = true, historicalObjectReadsNowForbidden = true });
    }

    private void FrontendEntry(uint tid, ref Win32Native.X86Context c)
    {
        Owner? source = c.Ecx == Primary.Traits ? Primary : c.Ecx == Secondary.Traits ? Secondary : null;
        if (source is null) { Add("frontend.unowned-pass-through"); return; }
        Require(_pending is null && _health?.Active == true && c.Esp >= 0x10044 && (c.Esp & 3) == 0 &&
            U(c.Esp) == FrontendCaller && !_host.Breakpoints.Contains(FrontendCaller),
            "Shared death command rejects nested or unowned original frontend frames.");
        VerifyOwners(); EqualHealth(); uint message = Pointer(c.Esp + 4), sender = U(message + 0x10);
        // This native producer depends on actual own HP/Character/command,
        // not on sender class. Physics eligibility stays with its own adapter.
        Require(U(message) == 0x2717 && U(message + 0x18) == 1 &&
            c.Esi == source.Traits && c.Edi == message,
            "Shared death command is not the exact original damage1 frontend packet/receiver.");
        Owner other = source == Primary ? Secondary : Primary;
        Frame frame = new(++_sequence, source, other, c.Esp, message, sender,
            U(Primary.Traits + 0x124), _health!.CommittedDamageTransactions, c);
        Require(_sequence <= 256, "Shared death command frontend budget exceeded.");
        _pending = frame; Add("frontend.entered");
        _host.Breakpoints.AddReturn(FrontendCaller, tid, "SharedDeathCommandAlias.frontend.return",
            (uint rt, uint at, ref Win32Native.X86Context returned) => FrontendReturn(rt, at, ref returned, frame));
        Sample("frontend-entry", new { sequence = frame.Id, sourceTraits = Hex(source.Traits),
            sourceCommand = Hex(source.Command), otherCommand = Hex(other.Command), message = Hex(message),
            sender = Hex(sender), entryEsp = Hex(c.Esp), caller = Hex(FrontendCaller),
            canonicalHealthBefore = frame.HealthBefore, committedDamageBefore = frame.CommitsBefore });
    }

    private void NativeProducer(uint tid, ref Win32Native.X86Context c)
    {
        if (c.Esi != Primary.Traits && c.Esi != Secondary.Traits) return;
        Frame frame = _pending ?? throw new InvalidDataException("Native dead command has no paired shared-death frontend.");
        VerifyOwners(); EqualHealth();
        Require(!frame.ProducerReached && c.Esi == frame.Source.Traits && c.Edi == frame.Message &&
            c.Ecx == frame.Source.Command && (c.Ebx & 255) == 1 && c.Esp == checked(frame.Esp - 0x44) &&
            U(frame.Esp) == FrontendCaller && U(frame.Esp + 4) == frame.Message &&
            U(c.Esp) == frame.Edi && U(c.Esp + 4) == frame.Esi && U(c.Esp + 8) == frame.Ebp && U(c.Esp + 12) == frame.Ebx &&
            U(frame.Message) == 0x2717 && U(frame.Message + 0x10) == frame.Sender &&
            U(frame.Message + 0x18) == 1 && U(Primary.Traits + 0x124) == 0 &&
            (frame.HealthBefore == 0 || frame.HealthBefore == 1 &&
                _health!.CommittedDamageTransactions == checked(frame.CommitsBefore + 1)),
            "Native dead command producer lost its exact frame, lethal actual resource or owned source.");
        frame.ProducerReached = true; Add("native-dead.producer-reached");
        Sample("native-dead-producer", new { sequence = frame.Id, sourceTraits = Hex(c.Esi),
            sourceCommand = Hex(c.Ecx), nativeEsp = Hex(c.Esp), actualBl = c.Ebx & 255,
            canonicalHealth = U(Primary.Traits + 0x124), originalStoreDeferred = true, registersChanged = false });
    }

    private void FrontendReturn(uint tid, uint at, ref Win32Native.X86Context c, Frame frame)
    {
        Ensure(tid);
        try
        {
            Require(Active && _health?.Active == true && ReferenceEquals(_pending, frame) &&
                at == FrontendCaller && c.Eip == at && c.Esp == checked(frame.Esp + 8) &&
                c.Esi == frame.Esi && c.Edi == frame.Edi && c.Ebx == frame.Ebx && c.Ebp == frame.Ebp,
                "Shared death command frontend RET4 lost caller, ESP, saved registers or lifetime.");
            VerifyOwners(); EqualHealth();
            uint after = U(Primary.Traits + 0x124), commits = _health!.CommittedDamageTransactions;
            Require(commits >= frame.CommitsBefore && commits - frame.CommitsBefore <= 1 && after <= frame.HealthBefore,
                "Shared death command observed an unsupported resource transaction inside its frontend.");
            Sample("frontend-return", new { sequence = frame.Id, sourceTraits = Hex(frame.Source.Traits),
                returnedCaller = Hex(at), returnedEsp = Hex(c.Esp), nativeEspDelta = 8,
                preservedSourceRegisters = true, frame.ProducerReached, canonicalHealthAfter = after,
                committedDamageAfter = commits, actualSourceDeadByte = Byte(frame.Source.Command + 0x5F) });
            bool lethal = frame.HealthBefore == 1 && after == 0 && commits == checked(frame.CommitsBefore + 1);
            if (lethal)
            {
                byte actual = Byte(frame.Source.Command + 0x5F), oldOther = Byte(frame.Other.Command + 0x5F);
                Require(_publications == 0 && frame.ProducerReached && actual == 1 && oldOther <= 1,
                    "Shared death command has no one-time actual native lethal byte to publish.");
                if (oldOther == 0)
                {
                    ++_writeAttempts;
                    try
                    {
                        Win32Native.WriteMemory(_host.ProcessHandle, frame.Other.Command + 0x5F, [actual], executable: false);
                        Require(Byte(frame.Other.Command + 0x5F) == actual, "Shared death command derived-byte readback differs.");
                        ++_writes;
                    }
                    catch
                    {
                        _failed = true;
                        Sample("derived-dead-byte-write-failed", new { sequence = frame.Id,
                            otherCommand = Hex(frame.Other.Command), requestedActualNativeByte = actual,
                            actualOtherDeadByte = TryByte(frame.Other.Command + 0x5F), nativeContinuationForbidden = true });
                        throw;
                    }
                }
                Require(Byte(frame.Source.Command + 0x5F) == actual && Byte(frame.Other.Command + 0x5F) == actual,
                    "Shared death command actual source/derived views changed during publication.");
                ++_publications; Add("lethal.actual-dead-byte-published");
                Sample("lethal-dead-byte-published", new { sequence = frame.Id,
                    sourceTraits = Hex(frame.Source.Traits), sourceCommand = Hex(frame.Source.Command),
                    otherCommand = Hex(frame.Other.Command), actualNativeDeadByte = actual, previousOtherDeadByte = oldOther,
                    derivedByteWritesThisTransaction = oldOther == 0 ? 1 : 0,
                    canonicalHealthBefore = frame.HealthBefore, canonicalHealthAfter = after,
                    committedDamageBefore = frame.CommitsBefore, committedDamageAfter = commits,
                    actualProducer = Hex(ProducedDead), returnedCaller = Hex(at), returnedEsp = Hex(c.Esp),
                    nativeEspDelta = 8, pending1D0ChangedByAdapter = false, nativeConsumptionStillRequired = true });
            }
            else
            {
                Require(!frame.ProducerReached || frame.HealthBefore == 0 && after == 0 && commits == frame.CommitsBefore &&
                    Byte(frame.Source.Command + 0x5F) == 1,
                    "Nonlethal frontend unexpectedly produced a new unqualified dead command.");
                Add("frontend.no-new-lethal-publication");
            }
            Add("frontend.returned"); _pending = null;
        }
        catch { _failed = true; throw; }
    }

    private Owner Capture(uint instance, uint traits, uint character, uint machine) =>
        new(instance, traits, character, machine, Pointer(instance + 0x24), Pointer(character + 0x130));
    private void VerifyOwners()
    {
        Require(U(0x765AD4) == _profile && U(_profile) == 0x6F5A44 && U(_profile + 0x2B4) == Primary.Instance &&
            Primary.Instance != Secondary.Instance && Primary.Traits != Secondary.Traits &&
            Primary.Character != Secondary.Character && Primary.Machine != Secondary.Machine &&
            Primary.Root != Secondary.Root && Primary.Command != Secondary.Command,
            "Shared death command lost its canonical Profile or separate body graphs.");
        foreach (Owner owner in new[] { Primary, Secondary })
            Require(U(owner.Instance) == 0x710F80 && U(owner.Instance + 0x24) == owner.Root &&
                U(owner.Root) == 0x6DC4F4 && U(owner.Root + 0x3C) == _scene &&
                U(owner.Traits) == TraitsTable && U(owner.Traits + 0x24) == owner.Instance &&
                U(owner.Traits + 0x18C) == owner.Character && U(owner.Traits + 0x190) == 0 &&
                U(owner.Character) == CharacterTable && U(owner.Character + 0x24) == owner.Instance &&
                U(owner.Character + 0x18) == owner.Root && U(owner.Character + 0x138) == owner.Traits &&
                U(owner.Character + 0x124) == owner.Machine && U(owner.Character + 0x130) == owner.Command &&
                U(owner.Machine) == MachineTable && U(owner.Machine + 0x24) == owner.Instance &&
                U(owner.Machine + 0x124) == owner.Character && U(owner.Machine + 0x12C) == owner.Command &&
                U(owner.Command) == CommandTable,
                "Shared death command typed owner/Traits/Character/FSM/command graph changed.");
    }
    private void EqualHealth() => Require(U(Primary.Traits + 0x124) == U(Secondary.Traits + 0x124),
        "Shared death command requires both actual common-health views before publication.");
    private delegate void Stop(uint tid, ref Win32Native.X86Context context);
    private void Hook(uint address, string hex, string name, Stop callback)
    {
        byte[] expected = Convert.FromHexString(hex); Pin(address, hex);
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            Ensure(tid); if (_retired || _failed) return;
            Require(Active && at == address && c.Eip == at && Read(at, expected.Length).SequenceEqual(expected),
                "Shared death command hook signature or lifetime differs.");
            try { callback(tid, ref c); } catch { _failed = true; throw; }
            if (Active) _host.Breakpoints.AddReturn(address, tid, "SharedDeathCommandAlias." + name, repeat);
        };
        _host.Breakpoints.AddReturn(address, _host.MainThreadId, "SharedDeathCommandAlias." + name, repeat);
    }
    private void Pin(uint at, string hex)
    {
        byte[] expected = Convert.FromHexString(hex);
        Require(_image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, expected.Length).ToArray().SequenceEqual(expected) &&
            Read(at, expected.Length).SequenceEqual(expected), $"Shared death command native pin differs at {Hex(at)}.");
    }
    private void Add(string name)
    { uint value = _counts.GetValueOrDefault(name) + 1; Require(value <= 100000, "Shared death command callback budget exceeded."); _counts[name] = value; }
    private void Sample(string name, object data)
    { if (_samples.Count >= 128) return; _samples.Add(new { name, data }); _log(new { @event = "shared-death-command-alias", name, data }); }
    private Owner Primary => _primary ?? throw new InvalidDataException("Shared death primary owner is not bound.");
    private Owner Secondary => _secondary ?? throw new InvalidDataException("Shared death secondary owner is not bound.");
    private static object? Describe(Owner? owner) => owner is null ? null : new { instance = Hex(owner.Instance), traits = Hex(owner.Traits),
        character = Hex(owner.Character), machine = Hex(owner.Machine), root = Hex(owner.Root), command = Hex(owner.Command) };
    private uint Pointer(uint at) { uint value = U(at); Require(value >= 0x10000, "Shared death command pointer is null."); return value; }
    private uint U(uint at) => BitConverter.ToUInt32(Read(at, 4));
    private byte Byte(uint at) => Read(at, 1)[0];
    private byte? TryByte(uint at) { try { return Byte(at); } catch (System.ComponentModel.Win32Exception) { return null; } }
    private byte[] Read(uint at, int count)
    { Ensure(_host.MainThreadId); Require(at >= 0x10000 && count is > 0 and <= 256, "Shared death command read exceeds its bounds."); return Win32Native.ReadMemory(_host.ProcessHandle, at, count); }
    private void Ensure(uint tid) => Require(_host.IsAtDebugStop && tid == _host.MainThreadId &&
        Environment.CurrentManagedThreadId == _ownerThread, "Shared death command requires the owned main-thread debug stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
