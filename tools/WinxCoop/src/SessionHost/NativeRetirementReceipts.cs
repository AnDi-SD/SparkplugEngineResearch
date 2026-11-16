using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;
using WinxCoop.SessionRuntime;
using P = WinxCoop.SessionRuntime.ProgressionNative;

namespace WinxCoop.SessionHost;

// Trusted local owner attestations derived from completed native observers.
// The native Progression ABI validates these PODs; it cannot inspect the game.
internal static class NativeRetirementReceipts
{
    internal static P.Descriptor Descriptor => new()
    {
        Level = 28, Entry = 0,
        Digest = P.Digest32.FromBytes(SHA256.HashData(Encoding.ASCII.GetBytes("WinxCoop.PC.3F022480.Alfea28.entry0")))
    };

    internal static P.Descriptor DescriptorFor(uint level, bool campaign)
    {
        if (!campaign) { Require(level == 28, "Alfea descriptor changed its native level."); return Descriptor; }
        var admitted = new CampaignSceneProfile();
        Require(admitted.Admits(level), "Campaign descriptor lacks its admitted native scene.");
        return new() { Level = level, Entry = 0,
            Digest = P.Digest32.FromBytes(Convert.FromHexString(admitted.DescriptorSha256)) };
    }

    internal static P.RetirementReceipt Build(RoleActor actor, P.Scene scene)
    {
        Require(actor.Epoch == scene.LevelEpoch && actor.Generation == scene.Generation,
            "Retirement receipt belongs to another local Actor generation.");
        JsonObject report = actor.SuccessfulReport(), observer = Object(report, "observer");
        JsonObject world = Object(observer, "worldState"), profile = Object(world, "profile"), ipc = Object(world, "ipc");
        JsonObject exit = Object(observer, "progressionOriginalExit"), calls = Object(observer, "nativeCalls");
        Require(B(world, "retired") && B(world, "goalPassed") && B(ipc, "retired") && !B(ipc, "pendingNativeApply") &&
            B(profile, "retired") && !B(profile, "active") && !B(profile, "pendingApply") && !B(profile, "terminalFailure") &&
            U(profile, "levelEpoch") == scene.LevelEpoch && U(profile, "bodyGeneration") == scene.Generation &&
            U(exit, "actualOriginalExitReturns") > 0 && !B(exit, "pendingOriginalExitReturn") && !B(exit, "failed"),
            "Native retirement lacks its exact scene, paired original Exit, World/profile or pending-apply proof.");
        Require(U(calls, "callsBegun") == U(calls, "callsCompleted") &&
            U(calls, "callsCompleted") == checked(U(calls, "pagesFreedAtLaterWorldStop") + U(calls, "pagesFreedAtProgressionExit")) &&
            !B(calls, "pending") && !B(calls, "retiredPage") && !B(calls, "terminalFailure") &&
            !B(Object(calls, "dependentLifecycle"), "active"), "Native retirement retains a call, dependent interval or executable lease.");
        if (actor.Role == "host")
        {
            JsonObject host = Object(observer, "progressionHost"), template = Object(observer, "template");
            JsonObject input = Object(template, "networkInputFixture"), life = Object(template, "sharedBattleLifetime");
            Require(B(host, "retirementReady") && B(host, "completed") && !B(host, "failed") &&
                U(host, "oldEpoch") == scene.LevelEpoch && U(host, "oldGeneration") == scene.Generation &&
                B(observer, "progressionRequestedOriginalExitReturned") && !B(template, "hasLiveOwnership") &&
                S(template, "instance") == "0x00000000" && S(template, "template") == "0x00000000" && S(template, "stream") == "0x00000000" &&
                B(input, "retired") && B(input, "endpointReleased") && !B(input, "nativeIntervalPending") &&
                B(life, "nativeBatchCompleted") && B(life, "resourcesReleasedAtLaterWorld") && B(life, "progressionNativeResourcesRetired") &&
                !B(life, "templateAdapterOwnerHeld") && U(life, "templateAdapterReferenceWrites") == 2 &&
                !B(life, "oldPlayerOrOwnObjectReadsAfterUnload"),
                "Host retirement retains original borrowers, controller, body/template references or later-World resources.");
        }
        else
        {
            JsonObject guest = Object(observer, "progressionGuest"), input = Object(observer, "nativeInputCapture");
            JsonObject presentation = Object(observer, "clientPresentation"), gate = Object(presentation, "gate"), poses = Object(world, "poses");
            Require(B(guest, "retirementReady") && B(guest, "originalRequestReturned") && B(guest, "actualOriginalExitReturned") &&
                B(guest, "inputRetiredBeforeNativeRequest") && !B(guest, "failed") && B(input, "retired") && !B(input, "failed") &&
                B(presentation, "afterUnload") && B(presentation, "guestCaptureRetiredBeforeUnload") && B(presentation, "nativeResourcesRetired") &&
                !B(presentation, "localEntityReadAfterUnload") && B(gate, "restored") && B(gate, "released") &&
                B(poses, "retired") && U(poses, "bindings") == 0,
                "Guest retirement retains input, bindings, original policy patches or executable policy pages.");
        }
        // These are the nine composition proofs above, not aliases for a
        // request returning or a socket closing. Host borrowed template holds
        // are balanced; Guest's native quarantine baselines/patches are restored.
        return new()
        {
            Scene = scene, OriginalExitReturned = 1, InputRetired = 1, WorldRetired = 1, BindingsRetired = 1,
            BorrowedBaselinesRestored = 1, PatchesRestored = 1, NoNativeOwnership = 1,
            NoPendingNativeCallsOrApply = 1, LaterWorldResourcesReleased = 1
        };
    }

    internal static P.EntryReceipt HostEntry(RoleActor actor, EntrySnapshotCapture capture, BootstrapPeer.Assignment assignment)
    {
        Require(actor.Role == "host" && capture.Level == assignment.Level && capture.LevelEpoch == assignment.LevelEpoch &&
            capture.Generation == assignment.Generation && capture.Seed.Length == 448 && capture.Witness.Length == 64,
            "Host entry capture differs from the assigned native generation.");
        return Entry(actor, assignment, capture.Seed, capture.Witness, capture.HostEntryOrdinal, actor.Epoch == 1 ? 1u : 2u);
    }

    internal static P.EntryReceipt GuestEntry(RoleActor actor, BootstrapPeer.Assignment assignment, byte[] seed, byte[] witness)
    {
        Require(actor.Role == "guest", "Guest entry requires the local Guest Actor.");
        JsonObject actual = actor.Last("world-entry-native-qualified") ?? throw new InvalidDataException("Guest has no actual native entry qualification.");
        return Entry(actor, assignment, seed, witness, checked((uint)U(actual, "hostEntryOrdinal")), 1);
    }

    private static P.EntryReceipt Entry(RoleActor actor, BootstrapPeer.Assignment assignment, byte[] seed, byte[] witness,
        uint authoritativeOrdinal, uint localOrdinal)
    {
        Require(actor.Epoch == assignment.LevelEpoch && actor.Generation == assignment.Generation && seed.Length == 448 && witness.Length == 64,
            "Native entry input payload differs from its local assignment.");
        JsonObject actual = actor.Last("world-entry-native-qualified") ?? throw new InvalidDataException("Actual native entry World is missing.");
        JsonObject returned = actor.Last("world-entry-load-returned") ?? throw new InvalidDataException("Actual native Load return is missing.");
        string seedHash = Hash(seed), witnessHash = Hash(witness);
        Require(U(actual, "level") == assignment.Level && U(actual, "levelEpoch") == assignment.LevelEpoch && U(actual, "generation") == assignment.Generation &&
            U(actual, "nativeLocalEntryOrdinal") == localOrdinal && U(actual, "hostEntryOrdinal") == authoritativeOrdinal &&
            S(actual, "preSeedHash") == seedHash && S(actual, "witnessHash") == witnessHash &&
            U(returned, "nativeLocalEntryOrdinal") == localOrdinal && U(returned, "hostEntryOrdinal") == authoritativeOrdinal &&
            B(returned, "nativeLoadSucceeded") && B(returned, "ret4EspMatched") && B(returned, "actualSpawnCallObserved"),
            "Native entry/Load/RET4/spawn witness differs from the exact transferred payload.");
        if (actor.Role == "host" && localOrdinal == 2)
            Require(actor.Last("world-initial-native-save-bootstrap-complete") is not null,
                "Continued Host entry lacks the original whole-checkpoint bootstrap.");
        return new()
        {
            Scene = assignment.Scene, Descriptor = DescriptorFor(assignment.Level, actor.Campaign), Channels = assignment.Channels,
            Seed = P.Digest32.FromBytes(Convert.FromHexString(seedHash)), Witness = P.Digest32.FromBytes(Convert.FromHexString(witnessHash)),
            LocalEntryOrdinal = localOrdinal, OriginalLoadReturned = 1, ActualReadyNativeWorld = 1
        };
    }

    internal static JsonObject Object(JsonObject value, string key) => value[key] as JsonObject ?? throw new InvalidDataException("Missing native object: " + key);
    internal static bool B(JsonObject value, string key) => value[key]?.GetValue<bool>() ?? throw new InvalidDataException("Missing native Boolean: " + key);
    internal static ulong U(JsonObject value, string key) => value[key]?.GetValue<ulong>() ?? throw new InvalidDataException("Missing native integer: " + key);
    internal static string S(JsonObject value, string key) => value[key]?.GetValue<string>() ?? throw new InvalidDataException("Missing native string: " + key);
    internal static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    private static void Require(bool value, string message) => SessionConfiguration.Require(value, message);
}
