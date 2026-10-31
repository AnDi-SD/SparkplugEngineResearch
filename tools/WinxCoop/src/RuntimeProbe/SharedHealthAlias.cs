using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own bounded adapter: one canonical native PlayerTraits HP transaction and a
/// derived second-body view. Original damage/frontends and their source-body
/// registers remain native. This is not a second implementation of damage.
/// </summary>
internal sealed class SharedHealthAlias
{
    private const uint PlayerTraitsTable = 0x6F6990, CharacterTable = 0x6F6438;
    private const uint MachineTable = 0x6FA288, DyingTable = 0x6F8210;
    private const uint Setter = 0x4FCD50, MaximumSetter = 0x4FCDD0;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly int _ownerThread;
    private readonly List<Pair> _pending = [];
    private readonly Dictionary<string, uint> _counts = [];
    private readonly List<object> _samples = [];
    private Owner? _primary, _secondary;
    private uint _profile, _scene, _primaryDying, _secondaryDying, _sequence;
    private uint _worlds, _mirrorWriteAttempts, _mirrorWrites, _retireEntry;
    private bool _bound, _retired, _terminalFailure, _automaticDeathCommandPinned;

    private sealed record Owner(uint Record, uint Traits, uint Character, uint Machine,
        uint Root, uint Command, uint Type);
    private sealed record Health(uint Current, uint Maximum, uint Minimum, uint Previous);
    private sealed class Pair(uint id, string kind, uint receiver, uint esp, uint caller)
    {
        internal readonly uint Id = id, Receiver = receiver, Esp = esp, Caller = caller;
        internal readonly string Kind = kind;
        internal bool NativeResetWriteReached, SecondaryCompletionSkipped;
    }

    internal SharedHealthAlias(DebuggerHost host, PEReader image, Action<object> log)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _image = image ?? throw new ArgumentNullException(nameof(image));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _ownerThread = Environment.CurrentManagedThreadId;
    }

    internal bool Active => _bound && !_retired && !_terminalFailure;
    internal bool Retired => _retired;
    internal uint CommittedDamageTransactions => _counts.GetValueOrDefault("setter.primary-committed") +
        _counts.GetValueOrDefault("setter.secondary-committed");
    internal uint SecondaryCommittedDamageTransactions => _counts.GetValueOrDefault("setter.secondary-committed");
    internal bool BoundedDeathCycleObserved =>
        _counts.GetValueOrDefault("dying.primary-entry-returned") == 1 &&
        _counts.GetValueOrDefault("dying.secondary-entry-returned") == 1 &&
        _counts.GetValueOrDefault("primary-dying.global-completion-entered") == 1 &&
        _counts.GetValueOrDefault("secondary-dying.global-fade-skipped") == 1 &&
        _counts.GetValueOrDefault("secondary-dying.global-completion-and-reset-skipped") <= 1 &&
        _counts.GetValueOrDefault("canonical-reset.committed") == 1;
    internal object Summary => new
    {
        bound = _bound, active = Active, retiredBeforeOwnDelete = _retired,
        terminalFailure = _terminalFailure, retireEntry = Hex(_retireEntry),
        pendingNativePairs = _pending.Count, maximumPendingPairs = 8, verifiedWorlds = _worlds,
        primary = Describe(_primary), secondary = Describe(_secondary),
        primaryDying = Hex(_primaryDying), secondaryDying = Hex(_secondaryDying),
        counts = new Dictionary<string, uint>(_counts), samples = _samples.ToArray(),
        derivedViewDwordWriteAttempts = _mirrorWriteAttempts,
        derivedViewDwordWritesReadbackVerified = _mirrorWrites,
        automaticNativeDeathCommandSourcePinned = _automaticDeathCommandPinned,
        boundedDeathCycleObserved = BoundedDeathCycleObserved,
        lethalAllowedOnlyForBothNativeTypeZero = true,
        nativeSetterArithmeticReimplemented = false, nativeSetterCalledTwice = false,
        secondaryHudEmissionAdded = false, bodyLocalDamageReceiverChanged = false,
        privateDeathCommandWrites = 0, nativeTypeWrites = 0, generatedCodeOrPages = 0,
        supportedSetterSources = new[] { "native damage4FCF3F", "native damage4FCFB5" },
        unsupportedResourceFamilies = new[] { "max-health changes", "healing/script/Profile-load setters", "unmapped direct HP writers" },
        runtimeHealthQualificationClaimed = false, runtimeDeathQualificationClaimed = false,
        fullE3Complete = false
    };

    /// <summary>Binding makes no game-field write and requires an actual WorldRET.</summary>
    internal void BindAtWorld(uint tid, ref Win32Native.X86Context context,
        uint primaryRecord, uint primaryTraits, uint primaryCharacter,
        uint secondaryRecord, uint secondaryTraits, uint secondaryCharacter, uint secondaryMachine)
    {
        Ensure(tid);
        Require(!_bound && !_retired && !_terminalFailure && context.Eip == NativeCallDispatcher.WorldReturn,
            "Shared HP binding requires its first exact native WorldRET.");
        Require(_host.ImageBase == 0x400000 && _host.ProcessHandle != IntPtr.Zero,
            "Shared HP requires the owned original x86 image.");
        _profile = Pointer(0x765AD4);
        Require(U(_profile) == 0x6F5A44 && U(_profile + 0x2B4) == primaryRecord,
            "Shared HP canonical owner is not the Profile's native primary Instance.");
        _primary = Capture(primaryRecord, primaryTraits, primaryCharacter, Pointer(primaryCharacter + 0x124));
        _secondary = Capture(secondaryRecord, secondaryTraits, secondaryCharacter, secondaryMachine);
        Require(primaryRecord != secondaryRecord && primaryTraits != secondaryTraits &&
            primaryCharacter != secondaryCharacter && _primary.Machine != secondaryMachine &&
            _primary.Command != _secondary.Command && _primary.Root != _secondary.Root,
            "Shared HP bodies do not own separate native graphs.");
        _scene = Pointer(_primary.Root + 0x3C);
        Require(U(_secondary.Root + 0x3C) == _scene, "Shared HP bodies are not in one native scene.");
        VerifyOwners(); EqualViews();
        Require(View(Primary.Traits).Current > 0 && View(Primary.Traits).Maximum > 0,
            "Shared HP first binding requires an initialized living native resource.");
        Pin(Setter, "FF254C2A3B01");
        Pin(MaximumSetter, "FF25B0313B01");
        Pin(0x4FCF3A, "E811FEFFFF"); Pin(0x4FCFB0, "E89BFDFFFF");
        Pin(0x4FE398, "E8D3E6FFFF");
        // Original PlayerTraits Update resolves its OWN Character/command and
        // writes command+5F when its OWN current HP is zero. Both complete
        // PlayerTraits use this slot; no adapter death-command latch is added.
        Pin(0x4FE643, "8B868C0100003BC374678B88240100003BCB745D8B80300100003BC37453399E24010000764783B9300100000A752088585C8B968C010000889ED00100008B8230010000885860889ED1010000EB22C6405C01389ED101000074168B8E8C0100008B9130010000C6426001EB04C6405F01");
        Require(ImageWord(PlayerTraitsTable + 0x48) == 0x4FE5C0 && U(PlayerTraitsTable + 0x48) == 0x4FE5C0,
            "Shared HP requires the complete native PlayerTraits update slot.");
        _automaticDeathCommandPinned = true;
        Pin(0x513590, "568BF18B46148B88240100008B8948010000");
        Pin(0x513647, "8B4E148B4124");
        Pin(0x5134C8, "6A0068581B00008BC8E82A060800");
        Pin(0x5135CD, "8A463C84C00F8586000000");
        Pin(0x5135D8, "A18C52750085C0750AE81A170800A38C5275006A0068E80300008BC8E867040800A19452750085C0750AE8C9270800A3945275006A016A358BC8E8391E0800A19452750085C0750AE8AB270800A3945275006A006A408BC8E8AB1D0800518BC451C700000000008BC4C700000000008B4E148B412468E9270000508BCEE8D626F2FF");
        Pin(0x51365A, "C6463C01");
        VerifyDyingType(DyingTable);
        _bound = true;
        Hook(Setter, "FF254C2A3B01", "setter", SetterEntry);
        Hook(MaximumSetter, "FF25B0313B01", "unsupported-maximum-setter", RejectMaximum);
        Hook(0x4FD7E0, "B806000000898124010000898128010000", "unsupported-constructor-reset", RejectConstructorReset);
        Hook(0x4FCA70, "56578B7C240C8B07", "base-notify", ResetEntry);
        Hook(0x4FCB45, "8B86280100005F8986240100005E", "direct-reset-write", ResetWrite);
        Hook(0x5134B0, "A18C527500", "dying-entry", DyingEntry);
        Hook(0x513590, "568BF1", "dying-update", DyingUpdate);
        Hook(0x5134C8, "6A0068581B00008BC8E82A060800", "dying-global-fade", DyingFade);
        Hook(0x5135D8, "A18C527500", "dying-global-completion", DyingCompletion);
        Sample("bound", new { primary = Describe(_primary), secondary = Describe(_secondary),
            initialCanonical = View(_primary.Traits), initialSecondary = View(_secondary.Traits),
            ownDerivedViewWritesAtBind = 0, sourcePinnedOnly = true });
    }

    internal void VerifyAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid);
        Require(context.Eip == NativeCallDispatcher.WorldReturn, "Shared HP verification requires WorldRET.");
        if (_retired) return; // No historical owner read is permitted after retirement.
        Require(Active && _pending.Count == 0, "Shared HP reached WorldRET with an inactive or pending transaction.");
        VerifyOwners(); EqualViews();
        Require(++_worlds <= 36000, "Shared HP native-world budget exceeded.");
    }

    internal void RetireBeforeOwnUnload(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid);
        Require(Active && _pending.Count == 0 && context.Eip is
            NativeCallDispatcher.WorldReturn or 0x5F2300 or 0x5F86C0 or 0x4E8AA0 or 0x4E8C80 or 0x59AE80 or 0x59B190,
            "Shared HP must retire at an owned WorldRET/unload boundary before any own deletion.");
        VerifyOwners(); EqualViews();
        _retireEntry = context.Eip; _retired = true;
        Sample("retired-before-own-delete", new { entry = Hex(context.Eip),
            pendingNativePairs = _pending.Count, callbacksDormantBeforeDelete = true,
            historicalOwnerReadsNowForbidden = true });
    }

    private void SetterEntry(uint tid, ref Win32Native.X86Context c)
    {
        uint receiver = c.Ecx;
        if (!BoundTraits(receiver)) { Add("setter.unowned-pass-through"); return; }
        VerifyOwners(); EqualViews(); RequireNoHealthPair();
        uint esp = c.Esp, caller = U(esp), requested = U(esp + 4);
        Require(c.Esi == receiver && caller is 0x4FCF3F or 0x4FCFB5,
            "Shared HP setter source is not the bounded native own-body damage call.");
        Require((caller == 0x4FCF3F && requested == 0) || (caller == 0x4FCFB5 && requested != 0),
            "Shared HP damage branch/request differs from its original native call.");
        if (requested == 0)
            Require(_automaticDeathCommandPinned && Primary.Type == 0 && Secondary.Type == 0,
                "Shared HP lethal path is not qualified for these native Traits types.");
        Health before = View(Primary.Traits), secondaryBefore = View(Secondary.Traits);
        uint sourceEsi = c.Esi;
        Pair pair = BeginPair(tid, "setter", receiver, esp, caller,
            (uint rt, uint at, ref Win32Native.X86Context rc) =>
            {
                VerifyOwners();
                Health actual = View(Primary.Traits);
                Require(rc.Esi == sourceEsi && actual.Maximum == before.Maximum && actual.Minimum == before.Minimum &&
                    actual.Previous == before.Current && View(Secondary.Traits) == secondaryBefore,
                    "Shared HP native setter changed an unsupported field or secondary view before publication.");
                Publish(actual, secondaryBefore, "setter");
                Add(receiver == Secondary.Traits ? "setter.secondary-committed" : "setter.primary-committed");
                Sample("setter-return", new { sequence = _pending[^1].Id, sourceTraits = Hex(receiver),
                    canonicalTraits = Hex(Primary.Traits), nativeCaller = Hex(at), requested,
                    oldHealth = before.Current, newHealth = actual.Current,
                    nativePreviousActual = actual.Previous, canonicalAfter = actual,
                    secondaryReadback = View(Secondary.Traits), sourceEsiAtEntry = Hex(sourceEsi),
                    esiAtReturn = Hex(rc.Esi), nativeEspMatched = true });
            });
        Sample("setter-entry", new { sequence = pair.Id, sourceTraits = Hex(receiver),
            canonicalTraits = Hex(Primary.Traits), nativeCaller = Hex(caller), requested,
            canonicalBefore = before, secondaryBefore, sourceEsi = Hex(sourceEsi),
            onlyEcxRedirected = receiver == Secondary.Traits });
        if (receiver == Secondary.Traits) c.Ecx = Primary.Traits;
    }

    private void RejectMaximum(uint tid, ref Win32Native.X86Context c)
    {
        Require(!BoundTraits(c.Ecx), "Shared HP maximum setter is an unsupported resource transaction; no write admitted.");
    }
    private void RejectConstructorReset(uint tid, ref Win32Native.X86Context c)
    {
        Require(!BoundTraits(c.Ecx), "Shared HP bound PlayerTraits constructor/reset cannot overwrite the canonical resource.");
    }

    private void ResetEntry(uint tid, ref Win32Native.X86Context c)
    {
        if (!BoundTraits(c.Ecx)) return;
        uint message = Pointer(c.Esp + 4);
        if (U(message) != 0x27E9) return;
        VerifyOwners(); EqualViews(); RequireNoHealthPair();
        Require(c.Ecx == Primary.Traits && U(c.Esp) == 0x4FE39D &&
            _primaryDying != 0 && U(message + 0x10) == _primaryDying &&
            DyingOwner(_primaryDying) == Primary && Read(_primaryDying + 0x3C, 1)[0] == 0,
            "Shared HP reset is not the one canonical native dying completion.");
        Health before = View(Primary.Traits), secondaryBefore = View(Secondary.Traits);
        BeginPair(tid, "reset", c.Ecx, c.Esp, U(c.Esp),
            (uint rt, uint at, ref Win32Native.X86Context rc) =>
            {
                VerifyOwners(); Health actual = View(Primary.Traits);
                Require(_pending[^1].NativeResetWriteReached && actual.Current == actual.Maximum &&
                    actual.Maximum == before.Maximum && actual.Minimum == before.Minimum &&
                    actual.Previous == before.Previous && View(Secondary.Traits) == secondaryBefore,
                    "Shared HP canonical reset lacks its exact native write/return witness.");
                Publish(actual, secondaryBefore, "canonical-reset"); Add("canonical-reset.committed");
                Sample("canonical-reset-return", new { actual, secondaryReadback = View(Secondary.Traits),
                    sender = Hex(_primaryDying), nativePreviousUnmodified = true });
            });
    }

    private void ResetWrite(uint tid, ref Win32Native.X86Context c)
    {
        if (!BoundTraits(c.Esi)) return;
        Require(c.Esi == Primary.Traits && _pending.Count > 0 && _pending[^1].Kind == "reset" &&
            _pending[^1].Receiver == c.Esi && U(c.Edi) == 0x27E9 && U(c.Edi + 0x10) == _primaryDying,
            "Shared HP encountered an unowned or secondary direct reset write.");
        _pending[^1].NativeResetWriteReached = true;
    }

    private void DyingEntry(uint tid, ref Win32Native.X86Context c)
    {
        Owner? owner = DyingOwner(c.Ecx); if (owner is null) return;
        VerifyOwners(); EqualViews();
        Require(Primary.Type == 0 && Secondary.Type == 0 && View(Primary.Traits).Current == 0,
            "Shared HP dying entry lacks its canonical type0 zero-health transaction.");
        uint state = c.Ecx;
        if (owner == Primary)
        { Require(_primaryDying is 0 || _primaryDying == state, "Canonical dying state identity changed."); _primaryDying = state; }
        else
        { Require(_secondaryDying is 0 || _secondaryDying == state, "Secondary dying state identity changed."); _secondaryDying = state; }
        BeginPair(tid, "dying-entry", state, c.Esp, U(c.Esp),
            (uint rt, uint at, ref Win32Native.X86Context rc) =>
            {
                Require(DyingOwner(state) == owner, "Native dying entry returned through a different owner.");
                Add(owner == Primary ? "dying.primary-entry-returned" : "dying.secondary-entry-returned");
                Sample("dying-entry-return", new { state = Hex(state), primary = owner == Primary,
                    pendingAnimation = Hex(U(state + 0x24)), onceByte = Read(state + 0x3C, 1)[0],
                    bodyNativeAnimationUnmodified = true });
            });
    }

    private void DyingUpdate(uint tid, ref Win32Native.X86Context c)
    {
        Owner? owner = DyingOwner(c.Ecx); if (owner is null) return;
        uint state = c.Ecx;
        Require(state == (owner == Primary ? _primaryDying : _secondaryDying),
            "Shared HP dying update was not paired with this body's native entry.");
        BeginPair(tid, "dying-update", state, c.Esp, U(c.Esp),
            (uint rt, uint at, ref Win32Native.X86Context rc) =>
            {
                Require(DyingOwner(state) == owner, "Native dying update returned through a different owner.");
                if (_pending[^1].SecondaryCompletionSkipped)
                    Require(Read(state + 0x3C, 1)[0] == 1,
                        "Secondary native completion did not execute its original once-byte store.");
            });
    }

    private void DyingFade(uint tid, ref Win32Native.X86Context c)
    {
        uint state = c.Edi;
        Owner? owner = DyingOwner(state); if (owner != Secondary) return;
        Require(_secondaryDying == state && _pending.Any(p => p.Kind == "dying-entry" && p.Receiver == state),
            "Secondary dying fade lacks its exact owned entry interval.");
        // Skip before argument pushes: every GP register, EFLAGS and ESP keeps
        // its stopped value. Native own animation setup starts at5134D6.
        c.Eip = 0x5134D6; Add("secondary-dying.global-fade-skipped");
    }

    private void DyingCompletion(uint tid, ref Win32Native.X86Context c)
    {
        Owner? owner = DyingOwner(c.Esi);
        if (owner == Primary)
        {
            Require(_primaryDying == c.Esi && _pending.Count > 0 &&
                _pending[^1].Kind == "dying-update" && _pending[^1].Receiver == c.Esi &&
                Read(c.Esi + 0x3C, 1)[0] == 0,
                "Primary completion lacks its native consumed-animation/zero-once boundary.");
            Add("primary-dying.global-completion-entered");
            return;
        }
        if (owner != Secondary) return;
        Require(_secondaryDying == c.Esi && _pending.Count > 0 &&
            _pending[^1].Kind == "dying-update" && _pending[^1].Receiver == c.Esi &&
            Read(c.Esi + 0x3C, 1)[0] == 0,
            "Secondary completion lacks its native consumed-animation/zero-once boundary.");
        // Native completion query and once guard already executed. Skip the
        // exact global triplet and own27E9; retain the original once-byte store.
        _pending[^1].SecondaryCompletionSkipped = true;
        c.Eip = 0x51365A; Add("secondary-dying.global-completion-and-reset-skipped");
    }

    private Pair BeginPair(uint tid, string kind, uint receiver, uint esp, uint caller, BreakpointCallback returned)
    {
        Require(_pending.Count < 8 && esp >= 0x10000 && (esp & 3) == 0 &&
            caller is >= 0x401000 and < 0x6DA000, "Shared HP native pair exceeds its owner/stack bounds.");
        Pair pair = new(++_sequence, kind, receiver, esp, caller); _pending.Add(pair);
        _host.Breakpoints.AddReturn(caller, tid, "SharedHealthAlias." + kind + ".return",
            (uint rt, uint at, ref Win32Native.X86Context rc) =>
            {
                Ensure(rt);
                Require(Active && at == pair.Caller && rc.Eip == at && rc.Esp == checked(pair.Esp + 8) &&
                    _pending.Count > 0 && ReferenceEquals(_pending[^1], pair),
                    "Shared HP native return differs in caller, ESP, lifetime or nesting.");
                try { returned(rt, at, ref rc); _pending.RemoveAt(_pending.Count - 1); }
                catch { _terminalFailure = true; throw; }
            });
        return pair;
    }

    private Owner Capture(uint record, uint traits, uint character, uint machine) =>
        new(record, traits, character, machine, Pointer(record + 0x24), Pointer(character + 0x130), U(traits + 0x190));

    private void VerifyOwners()
    {
        Require(U(0x765AD4) == _profile && U(_profile + 0x2B4) == Primary.Record,
            "Shared HP lost its canonical native Profile/Instance.");
        foreach (Owner owner in new[] { Primary, Secondary })
            Require(U(owner.Record) == 0x710F80 && U(owner.Traits) == PlayerTraitsTable &&
                U(owner.Character) == CharacterTable && U(owner.Machine) == MachineTable &&
                U(owner.Record + 0x24) == owner.Root && U(owner.Root) == 0x6DC4F4 &&
                U(owner.Character + 0x24) == owner.Record && U(owner.Character + 0x18) == owner.Root &&
                U(owner.Character + 0x138) == owner.Traits && U(owner.Character + 0x124) == owner.Machine &&
                U(owner.Character + 0x130) == owner.Command && U(owner.Traits + 0x24) == owner.Record &&
                U(owner.Traits + 0x18C) == owner.Character && U(owner.Traits + 0x190) == owner.Type &&
                owner.Type <= 2 && U(owner.Machine + 0x24) == owner.Record &&
                U(owner.Machine + 0x124) == owner.Character && U(owner.Machine + 0x12C) == owner.Command &&
                (_scene == 0 || U(owner.Root + 0x3C) == _scene),
                "Shared HP native body/component/command ownership changed.");
    }

    private Owner? DyingOwner(uint state)
    {
        uint machine = U(state + 0x14);
        Owner? owner = machine == Primary.Machine ? Primary : machine == Secondary.Machine ? Secondary : null;
        if (owner is null) return null;
        Require(U(state) == DyingTable && U(machine + 0x124) == owner.Character &&
            U(owner.Character + 0x124) == machine && U(machine + 0x24) == owner.Record,
            "Shared HP dying state is not the exact state-machine-Character-Instance graph.");
        return owner;
    }

    private void VerifyDyingType(uint table)
    {
        Require(ImageWord(table + 0x1C) == 0x5134B0 && ImageWord(table + 0x30) == 0x513590 &&
            U(table + 0x1C) == 0x5134B0 && U(table + 0x30) == 0x513590,
            "Shared HP dying state native entry/update slots differ.");
        uint getter = ImageWord(table + 0x10); byte[] literal = Image(getter, 6);
        Require(literal[0] == 0xB8 && literal[5] == 0xC3 && Read(getter, 6).SequenceEqual(literal) &&
            U(BitConverter.ToUInt32(literal, 1)) == 0x15526B2C,
            "Shared HP dying state RTTI is not the exact original class.");
    }

    private Health View(uint traits) => new(U(traits + 0x124), U(traits + 0x128), U(traits + 0x12C), U(traits + 0x130));
    private void EqualViews()
    {
        Health primary = View(Primary.Traits), secondary = View(Secondary.Traits);
        Require(primary.Current == secondary.Current && primary.Maximum == secondary.Maximum &&
            primary.Minimum == secondary.Minimum, "Shared HP native current/max/min views diverged before a transaction.");
    }
    private void Publish(Health actual, Health secondaryBefore, string source)
    {
        Require(View(Secondary.Traits) == secondaryBefore && actual.Maximum == secondaryBefore.Maximum &&
            actual.Minimum == secondaryBefore.Minimum, "Shared HP publication cannot normalize an unqualified secondary view.");
        WriteDerived(Secondary.Traits + 0x124, actual.Current, source + ".current");
        WriteDerived(Secondary.Traits + 0x130, actual.Previous, source + ".previous");
        Require(View(Secondary.Traits) == actual, "Shared HP complete derived view readback differs from canonical actual after-state.");
    }
    private void WriteDerived(uint address, uint value, string source)
    {
        ++_mirrorWriteAttempts;
        try
        {
            Win32Native.WriteMemory(_host.ProcessHandle, address, BitConverter.GetBytes(value), executable: false);
            Require(U(address) == value, "Shared HP derived DWORD write/readback differs."); ++_mirrorWrites;
        }
        catch
        {
            _terminalFailure = true;
            Sample("derived-write-failed", new { source, address = Hex(address), requested = value,
                actualCurrent = TryU(Secondary.Traits + 0x124), actualPrevious = TryU(Secondary.Traits + 0x130),
                nativeContinuationForbidden = true });
            throw;
        }
    }

    private delegate void Stop(uint tid, ref Win32Native.X86Context context);
    private void Hook(uint address, string hex, string name, Stop callback)
    {
        byte[] expected = Convert.FromHexString(hex); Pin(address, hex);
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            Ensure(tid);
            if (_retired) return;
            Require(Active && at == address && c.Eip == address && Read(at, expected.Length).SequenceEqual(expected),
                "Shared HP entry signature/lifetime differs from its original pin.");
            try { Add(name); callback(tid, ref c); }
            catch { _terminalFailure = true; throw; }
            if (Active) _host.Breakpoints.AddReturn(address, tid, "SharedHealthAlias." + name, repeat);
        };
        _host.Breakpoints.AddReturn(address, _host.MainThreadId, "SharedHealthAlias." + name, repeat);
    }

    private void Pin(uint address, string hex)
    {
        byte[] expected = Convert.FromHexString(hex);
        Require(Image(address, expected.Length).SequenceEqual(expected) && Read(address, expected.Length).SequenceEqual(expected),
            $"Shared HP original image/live code pin differs at {Hex(address)}.");
    }
    private void RequireNoHealthPair() => Require(!_pending.Any(p => p.Kind is "setter" or "reset"),
        "Shared HP rejects reentrant resource writers before any unqualified write.");
    private bool BoundTraits(uint value) => value == Primary.Traits || value == Secondary.Traits;
    private Owner Primary => _primary ?? throw new InvalidDataException("Shared HP canonical owner has not been bound.");
    private Owner Secondary => _secondary ?? throw new InvalidDataException("Shared HP secondary owner has not been bound.");
    private static object? Describe(Owner? value) => value is null ? null : new { record = Hex(value.Record),
        traits = Hex(value.Traits), character = Hex(value.Character), machine = Hex(value.Machine),
        root = Hex(value.Root), command = Hex(value.Command), nativeType = value.Type };
    private void Add(string name)
    { uint count = _counts.GetValueOrDefault(name) + 1; Require(count <= 100000, "Shared HP callback budget exceeded."); _counts[name] = count; }
    private void Sample(string name, object data)
    { if (_samples.Count >= 256) return; var sample = new { name, data }; _samples.Add(sample); _log(new { @event = "shared-health-alias", name, data }); }
    private byte[] Image(uint at, int count) => _image.GetSectionData(checked((int)(at - 0x400000))).GetContent(0, count).ToArray();
    private uint ImageWord(uint at) => BitConverter.ToUInt32(Image(at, 4));
    private uint Pointer(uint at) { uint result = U(at); Require(result >= 0x10000, "Shared HP native pointer is null."); return result; }
    private uint U(uint at) => BitConverter.ToUInt32(Read(at, 4));
    private uint? TryU(uint at) { try { return U(at); } catch (System.ComponentModel.Win32Exception) { return null; } }
    private byte[] Read(uint at, int count)
    { Ensure(_host.MainThreadId); Require(at >= 0x10000 && count is > 0 and <= 512, "Shared HP bounded read exceeds its contract."); return Win32Native.ReadMemory(_host.ProcessHandle, at, count); }
    private void Ensure(uint tid) => Require(_host.IsAtDebugStop && tid == _host.MainThreadId &&
        Environment.CurrentManagedThreadId == _ownerThread, "Shared HP requires its owned main-thread debug stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message)
    { if (!condition) throw new InvalidDataException(message); }
}
