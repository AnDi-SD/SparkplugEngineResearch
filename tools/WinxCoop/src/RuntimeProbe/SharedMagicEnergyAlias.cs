using System.Reflection.PortableExecutable;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own bounded policy: one canonical native magic resource, actual-value second
/// view, one primary regen source, and each body's original costs. Unintegrated
/// until a separate actual-game witness; no resource arithmetic is reimplemented.
/// </summary>
internal sealed class SharedMagicEnergyAlias
{
    private const uint TraitsTable = 0x6F6990, CharacterTable = 0x6F6438, MachineTable = 0x6FA288;
    private const uint Hundred = 0x42C80000, Setter = 0x4FDAC0;
    private readonly DebuggerHost _host;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly int _ownerThread;
    private readonly List<Pair> _pending = [];
    private readonly Dictionary<string, uint> _counts = [];
    private readonly List<object> _samples = [];
    private Owner? _primary, _secondary;
    private uint _profile, _scene, _sequence, _worlds, _writeAttempts, _writes, _retireEntry;
    private bool _bound, _retired, _failed;

    private sealed record Owner(uint Instance, uint Traits, uint Character, uint Machine, uint Root, uint Command);
    private sealed record Resource(uint Current, uint Maximum, uint Minimum, uint Previous);
    private sealed class Pair(uint id, string kind, Owner owner, Win32Native.X86Context c, uint caller, uint delta, Resource before)
    {
        internal readonly uint Id = id, Esp = c.Esp, Caller = caller, Delta = delta;
        internal readonly uint Esi = c.Esi, Edi = c.Edi, Ebx = c.Ebx, Ebp = c.Ebp;
        internal readonly string Kind = kind;
        internal readonly Owner Owner = owner;
        internal readonly Resource Before = before;
        internal uint Requested, Message, Payload, Tail, Sender, SenderTable, SourceIndex, Setters, Notifies;
        internal byte Enabled;
        internal bool ResourceReached, RegenReached, RegenSuppressed, Changed;
    }

    internal SharedMagicEnergyAlias(DebuggerHost host, PEReader image, Action<object> log)
    {
        _host = host ?? throw new ArgumentNullException(nameof(host));
        _image = image ?? throw new ArgumentNullException(nameof(image));
        _log = log ?? throw new ArgumentNullException(nameof(log));
        _ownerThread = Environment.CurrentManagedThreadId;
    }

    internal bool Active => _bound && !_retired && !_failed;
    internal bool Retired => _retired;
    internal bool HasPendingNativePair => _pending.Count != 0;
    internal uint CommittedSpendTransactions => Count("spend.primary-committed") + Count("spend.secondary-committed");
    internal uint SecondaryCommittedSpendTransactions => Count("spend.secondary-committed");
    internal object Summary => new
    {
        bound = _bound, active = Active, retiredBeforeOwnDelete = _retired, terminalFailure = _failed,
        primary = Describe(_primary), secondary = Describe(_secondary), scene = Hex(_scene),
        verifiedWorlds = _worlds, pendingNativePairs = _pending.Count, maximumPendingPairs = 8,
        retireEntry = Hex(_retireEntry), counts = new Dictionary<string, uint>(_counts), samples = _samples.ToArray(),
        committedSpendTransactions = CommittedSpendTransactions,
        secondaryCommittedSpendTransactions = SecondaryCommittedSpendTransactions,
        derivedDwordWriteAttempts = _writeAttempts, derivedDwordWritesReadbackVerified = _writes,
        policy = "primary regen only; no primary regen while secondary state8; each native cost retained",
        magicEnableBytesWritten = 0, resourceFloatArithmeticReimplemented = false,
        privateNativeCallsOrPages = 0, duplicateHudDispatches = 0, wholeUpdateReceiverRedirected = false,
        constructorResetHookOwnedElsewhere = "SharedHealthAlias/root must retain 4FD7E0 guard",
        supportedRefill = "exact original 5064A8 send -> Notify2792 -> 4FE2CB setter",
        unsupported = new[] { "max4FDB30", "272A enable/disable", "property-current4FD720", "minimum563230",
            "copy4FD9C0", "unpaired/script/refill507425/refill5BBF6E setters", "unmapped direct magic writers" },
        nativeReadyOrEnabledManufactured = false, runtimeSharedMagicQualified = false, fullE3Complete = false
    };

    internal void BindAtWorld(uint tid, ref Win32Native.X86Context context,
        uint primaryInstance, uint primaryTraits, uint primaryCharacter, uint primaryMachine,
        uint secondaryInstance, uint secondaryTraits, uint secondaryCharacter, uint secondaryMachine)
    {
        Ensure(tid);
        Require(!_bound && !_retired && !_failed && context.Eip == NativeCallDispatcher.WorldReturn &&
            _host.ImageBase == 0x400000 && _host.ProcessHandle != IntPtr.Zero,
            "Shared magic requires its first owned original WorldRET.");
        try
        {
            _profile = Pointer(0x765AD4);
            _primary = Capture(primaryInstance, primaryTraits, primaryCharacter, primaryMachine);
            _secondary = Capture(secondaryInstance, secondaryTraits, secondaryCharacter, secondaryMachine);
            _scene = Pointer(Primary.Root + 0x3C);
            VerifyOwners(); EqualViews();
            Pin(NativeCallDispatcher.WorldReturn, "C3");
            Pin(0x4FDAC0, "D981A8010000D991C0010000D9442404D8159CBA6D00DFE0F6C441");
            Pin(0x4FDAFC, "D991A8010000DAE9DFE0F6C4447B1E");
            Pin(0x4FDB17, "6A076888270000C70000000000E8D710F1FFC20400");
            Pin(0x4FDDD0, "D981A8010000D85C2404DFE0F6C4057A05");
            Pin(0x4FDDE1, "32C0C20400");
            Pin(0x4FDDE6, "D981A801000051D8642408D91C24E8C7FCFFFFB001C20400");
            Pin(0x4FDE00, "8A814C02000084C075648B818C0100008B902401000083BA3001000008744F");
            Pin(0x4FDE1F, "8B4424048B501889542404D9442404D889AC010000D8A9A8010000");
            Pin(0x4FDE47, "C781A801000000000000C2040089542404D9442404D889AC010000D8A9A8010000D999A8010000C20400");
            Pin(0x4FE5C0, "5153568BF1");
            Pin(0x4FE6B4, "389EA00100000F84860000008B86A80100008B8E8C0100008B91240100008944240883BA30010000087531");
            Pin(0x4FE6F2, "D9402851D80DB8CB6D008BCED86C240CD91C24E8B6F3FFFF");
            Pin(0x4FE70A, "B0015E5B59C3");
            Pin(0x4FE710, "D9442408D89EAC010000DFE0F6C4057A25");
            Pin(0x4FE734, "D944240851D840288BCED91C24E87AF3FFFF5EB0015B59C3");
            Pin(0x4FE207, "8B06FF50485F5EC20400");
            Pin(0x4FE324, "8B06578BCEFF50645F5EC20400");
            Pin(0x518821, "8B43148B88240100008B5424108B893801000052E89655FEFF84C0");
            Pin(0x518B54, "8B4B148B81240100008B80380100008B542410528BC8E86152FEFF84C0");
            Pin(0x518770, "C701E08B6F00"); Pin(0x518AB0, "C701488C6F00");
            Pin(0x506485, "518BC451C700000000008B84BE400100008BCC89018B862C0100006892270000508BCEE883F8F2FF");
            Pin(0x4FE2AD, "8B168BCEFF52508B47188944240CDB44240C518BCED886A8010000D91C24E8F0F7FFFF5F5EC20400");
            Pin(0x40EC23, "894C2410"); Pin(0x55F03D, "8B4110D980A8010000");
            Pin(0x6DCBB8, "0000A040"); Pin(0x6DBA9C, "00000000");
            Require(ImageWord(TraitsTable + 4) == 0x4FE1D0 && U(TraitsTable + 4) == 0x4FE1D0 &&
                ImageWord(TraitsTable + 0x48) == 0x4FE5C0 && U(TraitsTable + 0x48) == 0x4FE5C0 &&
                ImageWord(TraitsTable + 0x64) == 0x4FDE00 && U(TraitsTable + 0x64) == 0x4FDE00,
                "Shared magic requires original full PlayerTraits Notify/Update/drain slots.");
            Require(_host.Breakpoints.Contains(0x4FD7E0),
                "Shared magic requires the existing root/SharedHealth constructor-reset guard; no duplicate hook is installed.");
            _bound = true;
            Hook(Setter, "D981A8010000D991C0010000", "setter", SetterEntry);
            Hook(0x4FDDD0, "D981A8010000", "spend", SpendEntry);
            Hook(0x4FE5C0, "5153568BF1", "update", UpdateEntry);
            Hook(0x4FE710, "D9442408D89EAC010000", "regen", Regen);
            Hook(0x4FDE00, "8A814C02000084C0", "direct-drain", DrainEntry);
            Hook(0x4FDE1F, "8B4424048B5018", "direct-drain-resource", DrainResource);
            Hook(0x5064A8, "E883F8F2FF", "refill-send", RefillSend);
            Hook(0x4FE1D0, "56578B7C240C8B07", "notify", NotifyEntry);
            Hook(0x4FDB30, "FF25C4303B01", "unsupported-max", RejectBound);
            Hook(0x4FD720, "D9442404D899AC010000", "unsupported-property-current", RejectBound);
            Hook(0x563230, "8B4424048981B0010000", "unsupported-minimum", RejectBound);
            Hook(0x4FD9C0, "5657", "unsupported-copy", RejectCopy);
            Sample("bound", new { primary = Describe(Primary), secondary = Describe(Secondary),
                resource = View(Primary.Traits), primaryEnabled = Byte(Primary.Traits + 0x1A0),
                secondaryEnabled = Byte(Secondary.Traits + 0x1A0), noResourceOrEnableWritesAtBind = true });
        }
        catch { _failed = true; throw; }
    }

    internal void VerifyAtWorld(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid); Require(context.Eip == NativeCallDispatcher.WorldReturn, "Shared magic verification requires WorldRET.");
        if (_retired) return;
        Require(Active && _pending.Count == 0, "Shared magic has an inactive or unmatched native transaction at WorldRET.");
        VerifyOwners(); EqualViews(); Require(++_worlds <= 36000, "Shared magic world budget exceeded.");
    }

    internal void RetireBeforeOwnDelete(uint tid, ref Win32Native.X86Context context)
    {
        Ensure(tid);
        Require(Active && _pending.Count == 0 && context.Eip is NativeCallDispatcher.WorldReturn or
            0x5F2300 or 0x5F86C0 or 0x4E8AA0 or 0x4E8C80 or 0x59AE80 or 0x59B190,
            "Shared magic must retire at its original pre-delete/unload boundary with no native pair.");
        VerifyOwners(); EqualViews(); _retireEntry = context.Eip; _retired = true;
        Sample("retired-before-own-delete", new { entry = Hex(context.Eip), callbacksDormant = true,
            pendingNativePairs = 0, historicalOwnerReadsNowForbidden = true });
    }

    private void SpendEntry(uint tid, ref Win32Native.X86Context c)
    {
        Owner? owner = ByTraits(c.Ecx); if (owner is null) return;
        VerifyOwners(); EqualViews(); Require(_pending.Count == 0, "Shared magic rejects reentrant native spending.");
        uint caller = U(c.Esp), state = c.Ebx, requested = U(c.Esp + 4);
        uint table = caller == 0x51883A ? 0x6F8BE0u : caller == 0x518B6F ? 0x6F8C48u : 0;
        Require(table != 0 && U(state) == table && U(state + 0x14) == owner.Machine && Finite(requested, 0, 100000),
            "Shared magic spend lacks its exact native spell-state/owner/cost source.");
        Pair pair = Begin(tid, "spend", owner, c, caller, 8, (Pair p, ref Win32Native.X86Context r) =>
        {
            EqualViews();
            Require(p.Setters <= 1 && (r.Eax & 255) == (p.Setters == 1 ? 1u : 0u),
                "Shared magic native spend success differs from its one nested setter.");
            if (p.Setters == 0) Require(View(Primary.Traits) == p.Before, "Rejected native spend changed the shared resource.");
            Add("spend." + Body(p.Owner) + (p.Setters == 0 ? "-rejected" : p.Changed ? "-committed" : "-no-op"));
            Sample("spend-return", new { sequence = p.Id, source = Body(p.Owner), nativeCaller = Hex(p.Caller),
                costBits = Hex(p.Requested), nativeAl = r.Eax & 255, nestedSetters = p.Setters,
                changed = p.Changed, actual = View(Primary.Traits), nativeEspMatched = true });
        });
        pair.Requested = requested;
        if (owner == Secondary) c.Ecx = Primary.Traits;
    }

    private void UpdateEntry(uint tid, ref Win32Native.X86Context c)
    {
        Owner? owner = ByTraits(c.Ecx); if (owner is null) return;
        VerifyOwners(); EqualViews();
        Require(_pending.Count == 0 && U(c.Esp) == 0x4FE20C && c.Esi == owner.Traits && U(c.Edi) == 0x1E,
            "Shared magic Update is not the original own PlayerTraits Notify1E frame.");
        Add("update." + Body(owner) + "-entered");
        Pair pair = Begin(tid, "update", owner, c, 0x4FE20C, 4, (Pair p, ref Win32Native.X86Context r) =>
        {
            EqualViews();
            Require((r.Eax & 255) == 1 && p.Setters <= 1 && Byte(p.Owner.Traits + 0x1A0) == p.Enabled &&
                (p.Enabled == 0 ? p.Setters == 0 && !p.RegenReached : p.RegenReached || p.Setters == 1) &&
                (!p.RegenSuppressed || p.Setters == 0),
                "Shared magic Update changed an unsupported gate or produced multiple costs.");
            if (p.Setters == 0) Require(View(Primary.Traits) == p.Before, "Native Update without an admitted setter changed magic.");
            if (p.Enabled == 0) Add("update." + Body(p.Owner) + "-magic-disabled");
            Add("update." + Body(p.Owner) + "-returned");
            if (Count("update." + Body(p.Owner) + "-returned") <= 2)
                Sample("update-return", new { sequence = p.Id, source = Body(p.Owner), nativeEsp = Hex(r.Esp),
                    nativeEspMatched = true, enabled = p.Enabled, p.RegenReached, p.RegenSuppressed,
                    nativeSetters = p.Setters, actual = View(Primary.Traits) });
        });
        pair.Enabled = Byte(owner.Traits + 0x1A0);
    }

    private void Regen(uint tid, ref Win32Native.X86Context c)
    {
        Owner? owner = ByTraits(c.Esi); if (owner is null) return;
        VerifyOwners(); EqualViews(); Pair pair = Top("update", owner);
        Require(pair.Setters == 0 && !pair.RegenReached && Byte(owner.Traits + 0x1A0) == 1 &&
            State(owner) != 8 && c.Esp == checked(pair.Esp - 12) && c.Ebx == 0 &&
            c.Ecx == owner.Character && c.Edx == owner.Machine && c.Eax == View(Primary.Traits).Current &&
            U(c.Esp) == pair.Esi && U(c.Esp + 4) == pair.Ebx && U(c.Esp + 8) == View(Primary.Traits).Current,
            "Shared magic regen lost its exact source Update frame before native FLD.");
        pair.RegenReached = true;
        if (owner == Secondary || State(Secondary) == 8)
        {
            pair.RegenSuppressed = true; c.Eip = 0x4FE746;
            Add(owner == Secondary ? "regen.secondary-suppressed" : "regen.primary-suppressed-secondary-state8");
        }
        else Add("regen.primary-native-comparison-admitted");
    }

    private void SetterEntry(uint tid, ref Win32Native.X86Context c)
    {
        if (ByTraits(c.Ecx) is null) return;
        VerifyOwners(); EqualViews();
        Require(_pending.Count > 0, "Shared magic runtime setter has no admitted native source; refill/script caller unsupported.");
        Pair parent = _pending[^1]; uint caller = U(c.Esp), requested = U(c.Esp + 4);
        bool spend = parent.Kind == "spend" && caller == 0x4FDDF9 && c.Ecx == Primary.Traits && c.Esp == checked(parent.Esp - 8);
        bool drain = parent.Kind == "update" && caller == 0x4FE70A && !parent.RegenReached &&
            c.Ecx == parent.Owner.Traits && c.Esp == checked(parent.Esp - 20) && State(parent.Owner) == 8;
        bool regen = parent.Kind == "update" && caller == 0x4FE746 && parent.RegenReached && !parent.RegenSuppressed &&
            parent.Owner == Primary && c.Ecx == Primary.Traits && c.Esp == checked(parent.Esp - 20) && State(Secondary) != 8;
        bool refill = parent.Kind == "refill-notify" && caller == 0x4FE2D0 && c.Ecx == parent.Owner.Traits &&
            c.Esp == checked(parent.Esp - 16) && c.Esi == parent.Owner.Traits && c.Edi == parent.Message;
        Require((spend || drain || regen || refill) && parent.Setters == 0 &&
            c.Esi == (spend ? parent.Esi : parent.Owner.Traits) && Finite(requested, -1000000, 1000000) &&
            (parent.Kind != "update" || Byte(parent.Owner.Traits + 0x1A0) == 1),
            "Shared magic setter caller/owner/request/reentrancy is outside its native source cohort.");
        if (refill) VerifyMessage(parent);
        ++parent.Setters;
        Resource before = View(Primary.Traits), secondaryBefore = View(Secondary.Traits);
        string cause = spend ? "spend" : drain ? "state8-drain" : regen ? "regen" : "refill";
        uint causeEvents = Count(cause + "." + Body(parent.Owner) + "-native-committed") + Count(cause + "." + Body(parent.Owner) + "-native-no-op");
        bool sample = spend || refill || causeEvents < 4 || causeEvents % 120 == 0;
        Pair pair = Begin(tid, "setter", parent.Owner, c, caller, 8, (Pair p, ref Win32Native.X86Context r) =>
        {
            Resource actual = View(Primary.Traits); Validate(actual);
            Require(actual.Maximum == before.Maximum && actual.Minimum == before.Minimum && actual.Previous == before.Current &&
                View(Secondary.Traits) == secondaryBefore, "Shared magic native setter changed an unsupported field/view.");
            Publish(actual, secondaryBefore, cause); parent.Changed = actual.Current != before.Current;
            Add(cause + "." + Body(parent.Owner) + (parent.Changed ? "-native-committed" : "-native-no-op"));
            if (sample) Sample("setter-return", new { sequence = p.Id, source = Body(parent.Owner), cause,
                    originalReceiver = Hex(parent.Owner.Traits), canonical = Hex(Primary.Traits), nativeCaller = Hex(caller),
                    requestedBits = Hex(p.Requested), nativeEsp = Hex(r.Esp), before, actual,
                    secondaryReadback = View(Secondary.Traits), nativeEspMatched = true });
        });
        pair.Requested = requested;
        if (sample) Sample("setter-entry", new { sequence = pair.Id, source = Body(parent.Owner), cause,
            originalReceiver = Hex(parent.Owner.Traits), stoppedReceiver = Hex(c.Ecx), canonical = Hex(Primary.Traits),
            nativeCaller = Hex(caller), entryEsp = Hex(c.Esp), requestedBits = Hex(requested), before, secondaryBefore });
        if (c.Ecx == Secondary.Traits) c.Ecx = Primary.Traits;
    }

    private void DrainEntry(uint tid, ref Win32Native.X86Context c)
    {
        Owner? owner = ByTraits(c.Ecx); if (owner is null) return;
        VerifyOwners(); EqualViews(); uint message = Pointer(c.Esp + 4);
        Require(_pending.Count == 0 && U(c.Esp) == 0x4FE32C && c.Esi == owner.Traits && c.Edi == message &&
            U(message) == 0x2776 && Finite(U(message + 0x18), 0, 1),
            "Shared magic direct drain lacks the original struck-body Notify2776 frame/fraction.");
        Pair pair = Begin(tid, "direct-drain", owner, c, 0x4FE32C, 8, (Pair p, ref Win32Native.X86Context r) =>
        {
            VerifyMessage(p);
            Resource actual = View(Primary.Traits); Validate(actual);
            Require(actual.Maximum == p.Before.Maximum && actual.Minimum == p.Before.Minimum && actual.Previous == p.Before.Previous,
                "Shared magic direct drain changed max/min/native previous unexpectedly.");
            if (!p.ResourceReached)
                Require(actual == p.Before && View(Secondary.Traits) == p.Before, "Native rejected drain changed a resource view.");
            else Publish(actual, p.Before, "direct-drain");
            Add("direct-drain." + Body(p.Owner) + (!p.ResourceReached ? "-gated" : actual.Current != p.Before.Current ? "-committed" : "-no-op"));
            Sample("direct-drain-return", new { sequence = p.Id, source = Body(p.Owner), fractionBits = Hex(p.Payload),
                originalSourceGatesRetained = true, resourceSeamReached = p.ResourceReached, before = p.Before, actual,
                nativePreviousUnmodified = true, immediateHudAdded = false, nativeEspMatched = true });
        });
        CaptureMessage(pair, message);
    }

    private void DrainResource(uint tid, ref Win32Native.X86Context c)
    {
        Owner? owner = ByTraits(c.Ecx); if (owner is null) return;
        VerifyOwners(); EqualViews(); Pair pair = Top("direct-drain", owner); VerifyMessage(pair);
        Require(!pair.ResourceReached && c.Esp == pair.Esp && c.Esi == owner.Traits && c.Edi == pair.Message &&
            U(c.Esp + 4) == pair.Message && Byte(owner.Traits + 0x24C) == 0 && State(owner) != 8 &&
            c.Eax == owner.Character && c.Edx == owner.Machine && View(Primary.Traits) == pair.Before,
            "Shared magic direct writer is not after the exact original struck-body gates.");
        pair.ResourceReached = true;
        if (owner == Secondary) c.Ecx = Primary.Traits;
    }

    private void RefillSend(uint tid, ref Win32Native.X86Context c)
    {
        Owner? owner = ByInstance(U(c.Esp)); if (owner is null) return;
        VerifyOwners(); EqualViews();
        Require(_pending.Count == 0 && c.Ecx == c.Esi && c.Edi < 16 && U(c.Esp + 4) == 0x2792 &&
            U(c.Esp + 8) == U(c.Esi + 0x140 + c.Edi * 4) && U(c.Esp + 12) == 0 &&
            U(c.Esi + 0x12C) == owner.Instance && unchecked((int)U(c.Esp + 8)) is >= 0 and <= 100000,
            "Shared magic refill send is outside its pinned original target/value/argument cohort.");
        Pair pair = Begin(tid, "refill-send", owner, c, 0x5064AD, 16, (Pair p, ref Win32Native.X86Context r) =>
        {
            EqualViews(); Require(p.Notifies == 1 && U(p.Sender) == p.SenderTable,
                "Shared magic original refill send did not complete exactly one owned Notify2792.");
            Add("refill-send." + Body(p.Owner) + "-returned");
        });
        pair.Sender = c.Esi; pair.SenderTable = U(c.Esi); pair.Payload = U(c.Esp + 8); pair.SourceIndex = c.Edi;
    }

    private void NotifyEntry(uint tid, ref Win32Native.X86Context c)
    {
        Owner? owner = ByTraits(c.Ecx); if (owner is null) return;
        uint message = Pointer(c.Esp + 4), code = U(message);
        Require(code != 0x272A, "Shared magic bound Notify272A max/enable transition is unsupported.");
        if (code != 0x2792) return;
        VerifyOwners(); EqualViews(); Pair sender = Top("refill-send", owner);
        Require(sender.Notifies == 0 && U(message + 0x10) == sender.Sender && U(message + 0x18) == sender.Payload &&
            U(message + 0x1C) == 0 && U(sender.Sender) == sender.SenderTable,
            "Shared magic refill Notify is not the exact pending native sender/payload.");
        // Do not infer pickup readiness from a full magic bar. Require the
        // existing native per-body FX graph to be attached before its own call.
        uint fx = Pointer(owner.Traits + 0x1DC); _ = Pointer(owner.Traits + 0x1E0);
        Require(U(fx + 0x2C) == owner.Root && U(fx + 0x3C) == _scene,
            "Shared magic refill lacks its original own pickup FX root/scene.");
        ++sender.Notifies;
        Pair pair = Begin(tid, "refill-notify", owner, c, U(c.Esp), 8, (Pair p, ref Win32Native.X86Context r) =>
        {
            VerifyMessage(p); EqualViews(); Require(p.Setters == 1, "Shared magic refill lacks its one native current setter.");
            Add("refill." + Body(p.Owner) + (p.Changed ? "-committed" : "-no-op"));
        });
        CaptureMessage(pair, message);
    }

    private void RejectBound(uint tid, ref Win32Native.X86Context c) =>
        Require(ByTraits(c.Ecx) is null, "Shared magic rejects a bound maximum/property/minimum setter before its native write.");
    private void RejectCopy(uint tid, ref Win32Native.X86Context c) =>
        Require(ByTraits(c.Ecx) is null && ByTraits(U(c.Esp + 4)) is null,
            "Shared magic rejects a bound native PlayerTraits copy before any alias/resource overwrite.");

    private delegate void Completed(Pair pair, ref Win32Native.X86Context context);
    private Pair Begin(uint tid, string kind, Owner owner, Win32Native.X86Context c, uint caller, uint delta, Completed completed)
    {
        Require(_pending.Count < 8 && ++_sequence <= 100000 && c.Esp >= 0x10200 && (c.Esp & 3) == 0 &&
            caller is >= 0x401000 and < 0x6DA000 && !_host.Breakpoints.Contains(caller),
            "Shared magic return pairing has an unowned slot, invalid stack/caller or exceeded budget.");
        Pair pair = new(_sequence, kind, owner, c, caller, delta, View(Primary.Traits)); _pending.Add(pair);
        if (kind != "setter" && (kind != "update" || Count("update." + Body(owner) + "-entered") <= 2))
            Sample("native-entry", new { sequence = pair.Id, kind, source = Body(owner), originalReceiver = Hex(owner.Traits),
                caller = Hex(caller), entryEsp = Hex(c.Esp), expectedReturnEsp = Hex(checked(c.Esp + delta)), before = pair.Before });
        _host.Breakpoints.AddReturn(caller, tid, "SharedMagicEnergyAlias." + kind + ".return",
            (uint rt, uint at, ref Win32Native.X86Context r) =>
            {
                Ensure(rt);
                try
                {
                    Require(Active && _pending.Count > 0 && ReferenceEquals(_pending[^1], pair) && at == caller &&
                        r.Eip == caller && r.Esp == checked(pair.Esp + pair.Delta) && r.Esi == pair.Esi &&
                        r.Edi == pair.Edi && r.Ebx == pair.Ebx && r.Ebp == pair.Ebp,
                        "Shared magic native return lost its exact lifetime, caller, ESP, registers or nesting.");
                    VerifyOwners(); completed(pair, ref r); _pending.RemoveAt(_pending.Count - 1);
                }
                catch { _failed = true; throw; }
            });
        return pair;
    }

    private Pair Top(string kind, Owner owner)
    {
        Require(_pending.Count > 0 && _pending[^1].Kind == kind && _pending[^1].Owner == owner,
            "Shared magic stop lacks its exact owned native parent frame.");
        return _pending[^1];
    }
    private void CaptureMessage(Pair pair, uint message)
    { pair.Message = message; pair.Payload = U(message + 0x18); pair.Tail = U(message + 0x1C); pair.Sender = U(message + 0x10); }
    private void VerifyMessage(Pair pair) => Require(U(pair.Message) == (pair.Kind == "direct-drain" ? 0x2776u : 0x2792u) &&
        U(pair.Message + 0x18) == pair.Payload && U(pair.Message + 0x1C) == pair.Tail && U(pair.Message + 0x10) == pair.Sender,
        "Shared magic native message payload/sender changed within its paired operation.");
    private Owner Capture(uint instance, uint traits, uint character, uint machine) =>
        new(instance, traits, character, machine, Pointer(instance + 0x24), Pointer(character + 0x130));
    private void VerifyOwners()
    {
        Require(U(0x765AD4) == _profile && U(_profile) == 0x6F5A44 && U(_profile + 0x2B4) == Primary.Instance &&
            Primary.Instance != Secondary.Instance && Primary.Traits != Secondary.Traits && Primary.Character != Secondary.Character &&
            Primary.Machine != Secondary.Machine && Primary.Root != Secondary.Root && Primary.Command != Secondary.Command && U(_scene) == 0x6E7358,
            "Shared magic lost canonical Profile, distinct bodies or the original live scene.");
        foreach (Owner owner in new[] { Primary, Secondary })
            Require(U(owner.Instance) == 0x710F80 && U(owner.Traits) == TraitsTable && U(owner.Traits + 0x190) == 0 &&
                U(owner.Character) == CharacterTable && U(owner.Machine) == MachineTable && U(owner.Command) == 0x703090 &&
                U(owner.Instance + 0x24) == owner.Root && U(owner.Root) == 0x6DC4F4 && U(owner.Root + 0x3C) == _scene &&
                U(owner.Traits + 0x24) == owner.Instance && U(owner.Traits + 0x18C) == owner.Character &&
                U(owner.Character + 0x24) == owner.Instance && U(owner.Character + 0x18) == owner.Root &&
                U(owner.Character + 0x138) == owner.Traits && U(owner.Character + 0x124) == owner.Machine &&
                U(owner.Character + 0x130) == owner.Command && U(owner.Machine + 0x24) == owner.Instance &&
                U(owner.Machine + 0x124) == owner.Character && U(owner.Machine + 0x12C) == owner.Command &&
                State(owner) <= 0x18 && Byte(owner.Traits + 0x1A0) <= 1,
                "Shared magic body/control ownership, bounded Bloom state or native enable bool changed.");
    }
    private Resource View(uint traits) => new(U(traits + 0x1A8), U(traits + 0x1AC), U(traits + 0x1B0), U(traits + 0x1C0));
    private static void Validate(Resource value) => Require(value.Maximum == Hundred && value.Minimum == 0 &&
        Finite(value.Current, 0, 100) && Finite(value.Previous, 0, 100), "Shared magic view is not finite native current/previous with stable max100/min0.");
    private void EqualViews()
    { Resource primary = View(Primary.Traits), secondary = View(Secondary.Traits); Validate(primary); Validate(secondary); Require(primary == secondary, "Shared magic current/max/min/previous views diverged before a native transaction."); }
    private void Publish(Resource actual, Resource secondaryBefore, string source)
    {
        Require(View(Secondary.Traits) == secondaryBefore && actual.Maximum == secondaryBefore.Maximum && actual.Minimum == secondaryBefore.Minimum,
            "Shared magic publication cannot normalize an unqualified secondary view.");
        WriteDerived(Secondary.Traits + 0x1A8, actual.Current, source + ".current");
        WriteDerived(Secondary.Traits + 0x1C0, actual.Previous, source + ".native-previous");
        Require(View(Secondary.Traits) == actual, "Shared magic derived native current/previous readback differs.");
    }
    private void WriteDerived(uint address, uint bits, string source)
    {
        ++_writeAttempts;
        try { Win32Native.WriteMemory(_host.ProcessHandle, address, BitConverter.GetBytes(bits), executable: false);
            Require(U(address) == bits, "Shared magic derived DWORD write/readback differs."); ++_writes; }
        catch { _failed = true; Sample("derived-write-failed", new { address = Hex(address), bits = Hex(bits), source,
            actualCurrent = TryU(Secondary.Traits + 0x1A8), actualPrevious = TryU(Secondary.Traits + 0x1C0), nativeContinuationForbidden = true }); throw; }
    }

    private delegate void Stop(uint tid, ref Win32Native.X86Context context);
    private void Hook(uint address, string hex, string name, Stop action)
    {
        byte[] expected = Convert.FromHexString(hex); Pin(address, hex);
        Require(!_host.Breakpoints.Contains(address), "Shared magic hook already has another subscriber; multicast is unsupported.");
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context c) =>
        {
            Ensure(tid); if (_retired) return;
            try { Require(Active && at == address && c.Eip == address && Read(at, expected.Length).SequenceEqual(expected),
                "Shared magic original callback code/lifetime differs."); action(tid, ref c); }
            catch { _failed = true; throw; }
            if (Active) _host.Breakpoints.AddReturn(address, tid, "SharedMagicEnergyAlias." + name, repeat);
        };
        _host.Breakpoints.AddReturn(address, _host.MainThreadId, "SharedMagicEnergyAlias." + name, repeat);
    }
    private void Pin(uint address, string hex)
    { byte[] expected = Convert.FromHexString(hex); Require(Image(address, expected.Length).SequenceEqual(expected) &&
        ReadCode(address, expected.Length).SequenceEqual(expected), $"Shared magic original image/live pin differs at {Hex(address)}."); }
    private byte[] ReadCode(uint address, int count)
    {
        byte[] actual = Read(address, count), original = Image(address, count);
        for (int index = 0; index < count; ++index)
            if (actual[index] == 0xCC && original[index] != 0xCC && _host.Breakpoints.Contains(address + (uint)index)) actual[index] = original[index];
        return actual;
    }
    private Owner Primary => _primary ?? throw new InvalidDataException("Shared magic primary owner is unbound.");
    private Owner Secondary => _secondary ?? throw new InvalidDataException("Shared magic secondary owner is unbound.");
    private Owner? ByTraits(uint traits) => traits == Primary.Traits ? Primary : traits == Secondary.Traits ? Secondary : null;
    private Owner? ByInstance(uint instance) => instance == Primary.Instance ? Primary : instance == Secondary.Instance ? Secondary : null;
    private uint State(Owner owner) => U(owner.Machine + 0x130);
    private string Body(Owner owner) => owner == Secondary ? "secondary" : "primary";
    private uint Count(string name) => _counts.GetValueOrDefault(name);
    private void Add(string name) { uint value = Count(name) + 1; Require(value <= 100000, "Shared magic callback budget exceeded."); _counts[name] = value; }
    private void Sample(string name, object data)
    { if (_samples.Count >= 256) return; _samples.Add(new { name, data }); _log(new { @event = "shared-magic-energy-alias", name, data }); }
    private static bool Finite(uint raw, float minimum, float maximum)
    { float value = BitConverter.Int32BitsToSingle(unchecked((int)raw)); return float.IsFinite(value) && value >= minimum && value <= maximum; }
    private static object? Describe(Owner? owner) => owner is null ? null : new { instance = Hex(owner.Instance), traits = Hex(owner.Traits),
        character = Hex(owner.Character), machine = Hex(owner.Machine), root = Hex(owner.Root), command = Hex(owner.Command) };
    private byte[] Image(uint address, int count) => _image.GetSectionData(checked((int)(address - 0x400000))).GetContent(0, count).ToArray();
    private uint ImageWord(uint address) => BitConverter.ToUInt32(Image(address, 4));
    private uint Pointer(uint address) { uint value = U(address); Require(value >= 0x10000, "Shared magic native pointer is null."); return value; }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private uint? TryU(uint address) { try { return U(address); } catch (System.ComponentModel.Win32Exception) { return null; } }
    private byte Byte(uint address) => Read(address, 1)[0];
    private byte[] Read(uint address, int count)
    { Ensure(_host.MainThreadId); Require(address >= 0x10000 && count is > 0 and <= 512, "Shared magic native read exceeds its bound."); return Win32Native.ReadMemory(_host.ProcessHandle, address, count); }
    private void Ensure(uint tid) => Require(_host.IsAtDebugStop && tid == _host.MainThreadId && Environment.CurrentManagedThreadId == _ownerThread,
        "Shared magic requires its owned main-thread native debug stop.");
    private static string Hex(uint value) => $"0x{value:X8}";
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
}
