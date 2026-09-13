using SmoNativeValidator.Core;
using System.Runtime.InteropServices;

int assertions = 0;

void True(bool condition, string message)
{
    assertions++;
    if (!condition)
        throw new InvalidOperationException($"Assertion failed: {message}");
}

void Equal<T>(T expected, T actual, string message)
{
    assertions++;
    if (!EqualityComparer<T>.Default.Equals(expected, actual))
    {
        throw new InvalidOperationException(
            $"Assertion failed: {message}. Expected '{expected}', actual '{actual}'.");
    }
}

void Throws<TException>(Action action, string message)
    where TException : Exception
{
    assertions++;
    try
    {
        action();
    }
    catch (TException)
    {
        return;
    }
    throw new InvalidOperationException(
        $"Assertion failed: {message}. Expected {typeof(TException).Name}.");
}

NativeValidationEvent Event(
    long sequence,
    NativeValidationEventKind kind,
    IReadOnlyDictionary<string, string?>? data = null,
    string? checkpoint = null) => new()
{
    Sequence = sequence,
    TimestampUtc = new DateTimeOffset(2026, 8, 14, 10, 0, 0, TimeSpan.Zero).AddSeconds(sequence),
    Elapsed = TimeSpan.FromSeconds(sequence),
    Kind = kind,
    Stage = NativeValidationStage.ResourceLoad,
    Severity = NativeValidationSeverity.Information,
    Message = $"event-{sequence}",
    Checkpoint = checkpoint,
    Data = data ?? new Dictionary<string, string?>()
};

async Task TestExecutableSignatureResolution()
{
    ExecutableBytePattern wildcard = ExecutableBytePattern.Parse("56 57 E8 ?? ?? ?? ?? 8B F0");
    True(wildcard.IsMatch(new byte[] { 0x56, 0x57, 0xE8, 1, 2, 3, 4, 0x8B, 0xF0 }),
        "masked signature ignores relative call displacement");
    True(!wildcard.IsMatch(new byte[] { 0x56, 0x57, 0x90, 1, 2, 3, 4, 0x8B, 0xF0 }),
        "masked signature still verifies fixed opcodes");

    string repositoryRoot = FindRepositoryRoot();
    string[] candidates =
    [
        Path.Combine(repositoryRoot, "local-data", "pc-pristine", "WinxClub.exe"),
        Path.Combine(repositoryRoot, "local-data", "Winx Club", "WinxClub.exe"),
        Path.Combine(repositoryRoot, "local-data", "WinxClubWithDebugMenu", "WinxClub.exe"),
        Path.Combine(repositoryRoot, "local-data", "dubteam", "Winx Club", "WinxClub.exe")
    ];
    string[] existing = candidates.Where(File.Exists).ToArray();
    if (existing.Length == 0)
    {
        Console.WriteLine("SKIP: local WinxClub.exe signature fixtures are absent");
        return;
    }

    List<ExecutableIdentification> identifications = [];
    foreach (string executable in existing)
    {
        ExecutableIdentification identification =
            await ExecutableProfileCatalog.IdentifyAsync(executable);
        identifications.Add(identification);
        True(identification.IsSupported,
            $"internal loader signatures resolve for {executable}: {identification.Error}");
        Equal(ExecutableProfileCatalog.SignatureProfileId, identification.Profile?.Id,
            "all patched builds use the same signature-resolved engine profile");
        True(identification.Profile!.Checkpoints
                .Where(checkpoint => checkpoint.Id is "CP02" or "CP03" or
                    "FFPS01" or "FFPS02" or "FFPS03")
                .All(checkpoint => checkpoint.Required),
            "every pass-critical runtime checkpoint remains required");
        True(identification.Profile!.Checkpoints
                .Where(checkpoint => checkpoint.Id == "CP01" || checkpoint.BloomSpecific)
                .All(checkpoint => !checkpoint.Required),
            "diagnostic and Bloom-specific runtime checkpoints remain optional");
        True(identification.Profile!.Checkpoints.Any(checkpoint =>
                checkpoint.StringComparisonMode == StringComparisonCallMode.CdeclImportThunk),
            "shared case-insensitive import thunk is available for opt-in string probes");
        True(Enum.GetValues<StringComparisonFunction>()
                .Where(function => function != StringComparisonFunction.None)
                .All(function => identification.Profile!.Checkpoints.Any(checkpoint =>
                    checkpoint.StringComparisonFunction == function)),
            "all imported case-insensitive string APIs are available for opt-in probes");
        Equal(0x014Cu, (uint)(identification.PeMachine ?? 0), "x86 PE machine accepted");
        Equal(0x00192C50u, CheckpointRva(identification, "CP02"),
            "BuildAssetPath is resolved by code, not by build identity");
        Equal(0x00058260u, CheckpointRva(identification, "CP03"),
            "ResourceLoad is resolved by code, not by build identity");
        Equal(0x00022260u, CheckpointRva(identification, "FFPS01"),
            "FFPS header validator entry is resolved");
        Equal(0x000222D6u, CheckpointRva(identification, "FFPS02"),
            "accepted-magic checkpoint follows the decoded JE target");
        Equal(0x00022345u, CheckpointRva(identification, "FFPS03"),
            "accepted-version checkpoint follows the decoded JE target");
        ExecutableCheckpoint? transformBinding = identification.Profile!.Checkpoints
            .SingleOrDefault(checkpoint => checkpoint.Id == "CP10");
        True(transformBinding is not null,
            "optional spTransformTrackEval binding-write probe is signature-resolved");
        Equal(0x00054628u, transformBinding!.RelativeVirtualAddress,
            "transform binding probe stops before evaluator +0x10 is overwritten");
        True(!transformBinding.Required && transformBinding.StringComparisonSpecific,
            "transform binding checkpoint remains opt-in research instrumentation");
        ExecutableCheckpoint? transformEvaluate = identification.Profile!.Checkpoints
            .SingleOrDefault(checkpoint => checkpoint.Id == "CP11");
        True(transformEvaluate is not null,
            "optional spTransformTrackEval evaluation probe is signature-resolved");
        Equal(0x001FEBB0u, transformEvaluate!.RelativeVirtualAddress,
            "transform evaluation probe resolves the track evaluator virtual method");
        True(!transformEvaluate.Required && transformEvaluate.StringComparisonSpecific,
            "transform evaluation checkpoint remains opt-in research instrumentation");
        (string Id, ExecutableCheckpointKind Kind, uint Rva)[] sceneCheckpoints =
        [
            ("CP12", ExecutableCheckpointKind.GameFlowStateEntered, 0x001960A0u),
            ("CP13", ExecutableCheckpointKind.GameFlowStateExiting, 0x0019656Fu),
            ("CP14", ExecutableCheckpointKind.GameFlowStateResumed, 0x001965D3u)
        ];
        foreach ((string id, ExecutableCheckpointKind kind, uint rva) in sceneCheckpoints)
        {
            ExecutableCheckpoint sceneCheckpoint = identification.Profile!.Checkpoints
                .Single(checkpoint => checkpoint.Id == id);
            Equal(kind, sceneCheckpoint.Kind,
                $"{id} resolves the intended native game-flow transition");
            Equal(rva, sceneCheckpoint.RelativeVirtualAddress,
                $"{id} resolves to the verified PC game-flow instruction");
            True(!sceneCheckpoint.Required && sceneCheckpoint.SceneReadySpecific,
                $"{id} is installed only for opt-in scene-ready validation");
        }
        True(identification.SignatureResolutions
                .Where(resolution => resolution.Required)
                .All(resolution => resolution.ResolvedRva is not null),
            "every required semantic signature is unique");
    }
    if (identifications.Count > 1)
    {
        True(identifications.Select(identification => identification.Sha256)
                .Distinct(StringComparer.OrdinalIgnoreCase).Count() > 1,
            "different patched hashes resolve to the same internal-code profile");
    }

    string tempDirectory = Path.Combine(
        Path.GetTempPath(), "SmoNativeValidator.Tests", Guid.NewGuid().ToString("N"));
    Directory.CreateDirectory(tempDirectory);
    try
    {
        byte[] source = await File.ReadAllBytesAsync(existing[0]);
        string overlayPath = Path.Combine(tempDirectory, "WinxClub-overlay.exe");
        byte[] overlay = new byte[source.Length + 37];
        source.CopyTo(overlay, 0);
        "hash-independent-overlay"u8.CopyTo(overlay.AsSpan(source.Length));
        await File.WriteAllBytesAsync(overlayPath, overlay);
        ExecutableIdentification overlayIdentification =
            await ExecutableProfileCatalog.IdentifyAsync(overlayPath);
        True(overlayIdentification.IsSupported,
            "file length and SHA changes outside internal code do not gate validation");
        True(!string.Equals(
                overlayIdentification.Sha256,
                identifications[0].Sha256,
                StringComparison.OrdinalIgnoreCase),
            "mutated overlay has a different diagnostic SHA");

        byte[] missing = source.ToArray();
        int cp02Offset = FindSequence(missing,
            new byte[] { 0x81, 0xEC, 0x30, 0x01, 0x00, 0x00, 0xA1 });
        True(cp02Offset >= 0, "CP02 fixture bytes found before missing-signature mutation");
        missing[cp02Offset] ^= 0x01;
        string missingPath = Path.Combine(tempDirectory, "WinxClub-missing.exe");
        await File.WriteAllBytesAsync(missingPath, missing);
        ExecutableIdentification missingIdentification =
            await ExecutableProfileCatalog.IdentifyAsync(missingPath);
        True(!missingIdentification.IsSupported,
            "a missing required loader signature blocks unsafe instrumentation");
        ExecutableSignatureResolution missingCp02 = missingIdentification.SignatureResolutions
            .Single(resolution => resolution.Id == "CP02");
        Equal(0, missingCp02.CandidateRvas.Count, "missing signature reports no candidates");
        True(missingIdentification.Error?.Contains("CP02", StringComparison.Ordinal) == true,
            "missing signature diagnostic names internal checkpoint, not hash");
        True(missingIdentification.Error?.Contains("SHA", StringComparison.OrdinalIgnoreCase) == false,
            "missing signature diagnostic never reports a hash mismatch");

        byte[] ambiguous = source.ToArray();
        int executableCave = FindExecutableCodeCave(ambiguous, 96, cp02Offset);
        True(executableCave >= 0, "executable code cave found for ambiguity fixture");
        int cp02PatternLength = 43;
        ambiguous.AsSpan(cp02Offset, cp02PatternLength)
            .CopyTo(ambiguous.AsSpan(executableCave, cp02PatternLength));
        string ambiguousPath = Path.Combine(tempDirectory, "WinxClub-ambiguous.exe");
        await File.WriteAllBytesAsync(ambiguousPath, ambiguous);
        ExecutableIdentification ambiguousIdentification =
            await ExecutableProfileCatalog.IdentifyAsync(ambiguousPath);
        True(!ambiguousIdentification.IsSupported,
            "duplicate required signature blocks choosing a guessed breakpoint");
        ExecutableSignatureResolution ambiguousCp02 = ambiguousIdentification.SignatureResolutions
            .Single(resolution => resolution.Id == "CP02");
        Equal(2, ambiguousCp02.CandidateRvas.Count,
            "ambiguous signature exposes both candidate RVAs");
        True(ambiguousCp02.Diagnostic?.Contains("ambiguous", StringComparison.OrdinalIgnoreCase) == true,
            "ambiguity has a precise instrumentation diagnostic");
    }
    finally
    {
        if (Directory.Exists(tempDirectory))
            Directory.Delete(tempDirectory, recursive: true);
    }
}

uint CheckpointRva(ExecutableIdentification identification, string id) =>
    identification.Profile!.Checkpoints.Single(checkpoint => checkpoint.Id == id)
        .RelativeVirtualAddress;

string FindRepositoryRoot()
{
    DirectoryInfo? directory = new(AppContext.BaseDirectory);
    while (directory is not null)
    {
        if (Directory.Exists(Path.Combine(directory.FullName, "local-data")) &&
            Directory.Exists(Path.Combine(directory.FullName, "tools")))
        {
            return directory.FullName;
        }
        directory = directory.Parent;
    }
    return Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", ".."));
}

int FindSequence(byte[] data, byte[] sequence)
{
    for (int offset = 0; offset <= data.Length - sequence.Length; offset++)
    {
        if (data.AsSpan(offset, sequence.Length).SequenceEqual(sequence))
            return offset;
    }
    return -1;
}

int FindExecutableCodeCave(byte[] image, int minimumLength, int excludedOffset)
{
    int peOffset = BitConverter.ToInt32(image, 0x3C);
    int sectionCount = BitConverter.ToUInt16(image, peOffset + 6);
    int optionalSize = BitConverter.ToUInt16(image, peOffset + 20);
    int sectionTable = peOffset + 24 + optionalSize;
    for (int sectionIndex = 0; sectionIndex < sectionCount; sectionIndex++)
    {
        int section = sectionTable + sectionIndex * 40;
        int rawSize = checked((int)BitConverter.ToUInt32(image, section + 16));
        int rawOffset = checked((int)BitConverter.ToUInt32(image, section + 20));
        uint characteristics = BitConverter.ToUInt32(image, section + 36);
        if ((characteristics & 0x20000000) == 0)
            continue;
        int run = 0;
        for (int offset = rawOffset; offset < rawOffset + rawSize; offset++)
        {
            run = image[offset] == 0xCC ? run + 1 : 0;
            if (run >= minimumLength)
            {
                int result = offset - run + 1;
                if (Math.Abs(result - excludedOffset) > minimumLength)
                    return result;
            }
        }
    }
    return -1;
}

void TestLocatorResolver()
{
    string preferred = Path.GetFullPath(Path.Combine("test-root", "preferred", "WinxClub.exe"));
    string environment = Path.GetFullPath(Path.Combine("test-root", "environment", "WinxClub.exe"));
    string saved = Path.GetFullPath(Path.Combine("test-root", "saved", "WinxClub.exe"));
    WinxClubLocationResult result = WinxClubLocator.ResolveCandidates(
    [
        (preferred, WinxClubLocationSource.Preferred),
        (environment, WinxClubLocationSource.EnvironmentVariable),
        (saved, WinxClubLocationSource.SavedManualSelection),
        (environment.ToUpperInvariant(), WinxClubLocationSource.CommonInstallDirectory)
    ],
    ["registry unavailable in unit test"],
    path => path.Equals(environment, StringComparison.OrdinalIgnoreCase) ||
        path.Equals(saved, StringComparison.OrdinalIgnoreCase));

    Equal(environment, result.SelectedPath, "first existing source wins");
    Equal(3, result.Candidates.Count, "case-insensitive duplicates collapse");
    Equal(WinxClubLocationSource.Preferred, result.Candidates[0].Source,
        "preferred candidate retains highest priority");
    True(!result.Candidates[0].Exists, "missing preferred path is reported");
    True(result.Candidates[1].Exists, "existing environment path is reported");
    Equal(1, result.Diagnostics.Count, "locator diagnostics survive resolution");
}

void TestLogicalMatcher()
{
    True(LogicalAssetMatcher.IsMatch(
        @"D:\Game\Media\Characters\Bloom\bloom_jeans.smo",
        @"Characters/Bloom/bloom_jeans.smo"),
        "logical slot matches an absolute game path suffix");
    True(LogicalAssetMatcher.IsMatch(
        @"D:\Game\Media\Characters\Bloom\bloom_jeans.smo",
        "bloom_jeans.smo"),
        "file-name-only slot matches");
    True(!LogicalAssetMatcher.IsMatch(
        @"D:\Game\Media\Characters\Bloom\bloom_jeans.smo.bak",
        "bloom_jeans.smo"),
        "non-SMO suffix does not match");
    True(!LogicalAssetMatcher.IsMatch(
        @"D:\Game\Media\Characters\Flora\bloom_jeans.smo",
        @"Characters\Bloom\bloom_jeans.smo"),
        "different logical directory does not match");
}

void TestResourceLoadTargetClassifier()
{
    const string redirect = @"C:\Users\Tester\AppData\Local\Temp\SmoNV\s-123\Alfea02.smo";
    const string trigger = @"Levels\Alfea\Alfea02.smo";

    Equal(
        ResourceLoadTargetRoute.DirectLogicalPath,
        ResourceLoadTargetClassifier.Classify(
            @"X:\Games\Winx Club\Media\Levels\Alfea\Alfea02.smo",
            redirect,
            trigger),
        "direct ResourceLoad of a logical level slot is recognized before redirection");
    Equal(
        ResourceLoadTargetRoute.AlreadyRedirected,
        ResourceLoadTargetClassifier.Classify(redirect.ToUpperInvariant(), redirect, trigger),
        "staged ResourceLoad argument is recognized after BuildAssetPath redirection");
    Equal(
        ResourceLoadTargetRoute.None,
        ResourceLoadTargetClassifier.Classify(
            @"X:\Games\Winx Club\Media\Levels\Alfea\Alfea01.smo",
            redirect,
            trigger),
        "a different direct level remains outside target context");
}

void TestValidationRouting()
{
    NativeValidationRequest contextual = new()
    {
        ExecutablePath = @"D:\Winx\WinxClub.exe",
        AssetPath = @"D:\Models\Troll.smo",
        LogicalGameAssetPath = @"Characters\Troll\Troll.smo",
        Route = NativeValidationRoute.Contextual
    };
    True(contextual.UseIsolatedLaunchWorkspace,
        "all native validation requests default to an isolated windowed workspace");
    ResolvedNativeValidationRoute contextualRoute = NativeValidationRouting.Resolve(contextual);
    Equal(@"Characters\Troll\Troll.smo", contextualRoute.TriggerGameAssetPath,
        "contextual route triggers on the original logical slot");
    Equal<int?>(null, contextualRoute.RequestedStartLevel,
        "contextual route does not inject a start level");

    NativeValidationRequest fast = contextual with
    {
        Route = NativeValidationRoute.FastGeneric,
        UseIsolatedLaunchWorkspace = true
    };
    ResolvedNativeValidationRoute fastRoute = NativeValidationRouting.Resolve(fast);
    Equal(NativeValidationDefaults.FastTriggerGameAssetPath,
        fastRoute.TriggerGameAssetPath,
        "fast route uses the shared early mouse cursor trigger");
    Equal(@"Characters\Troll\Troll.smo", fast.LogicalGameAssetPath,
        "fast routing preserves original context metadata");

    ResolvedNativeValidationRoute zeroStart = NativeValidationRouting.Resolve(contextual with
    {
        StartLevel = 0
    });
    Equal<int?>(null, zeroStart.RequestedStartLevel,
        "startLevel zero is normalized to normal startup");

    ResolvedNativeValidationRoute accelerated = NativeValidationRouting.Resolve(contextual with
    {
        UseIsolatedLaunchWorkspace = true,
        StartLevel = NativeValidationDefaults.ContextualStartLevel
    });
    Equal<int?>(2, accelerated.RequestedStartLevel,
        "contextual preset applies the shared accelerated level");

    Throws<NativeAssetPathException>(() => NativeValidationRouting.Resolve(fast with
    {
        TriggerGameAssetPath = @"Menus\another.smo"
    }), "fast trigger cannot silently become an arbitrary slot");
    Throws<NativeAssetPathException>(() => NativeValidationRouting.Resolve(contextual with
    {
        TriggerGameAssetPath = @"Characters\Bloom\bloom.smo"
    }), "contextual trigger must remain the original logical slot");
    Throws<ArgumentException>(() => NativeValidationRouting.Resolve(contextual with
    {
        UseIsolatedLaunchWorkspace = false,
        StartLevel = 2
    }), "the core rejects every non-isolated launch before routing");
    Throws<ArgumentException>(() => NativeValidationRouting.Resolve(fast with
    {
        StartLevel = 2
    }), "fast route rejects contextual startLevel acceleration");
    Throws<ArgumentException>(() => NativeValidationRouting.Resolve(contextual with
    {
        UseIsolatedLaunchWorkspace = true,
        WorkingDirectory = @"D:\Custom"
    }), "an explicit working directory cannot override the owned launch workspace");
    Throws<ArgumentOutOfRangeException>(() => NativeValidationRouting.Resolve(contextual with
    {
        Route = (NativeValidationRoute)99
    }), "undefined route values are rejected");

    Throws<NativeAssetPathException>(() => NativeValidationRouting.Resolve(contextual with
    {
        LogicalGameAssetPath = "Troll.smo"
    }), "file-name-only logical paths are rejected");
}

async Task TestWindowedLaunchGuard()
{
    string logPath = Path.Combine(
        Path.GetTempPath(),
        "SmoNativeValidator.Tests",
        $"windowed-guard-{Guid.NewGuid():N}.jsonl");
    try
    {
        NativeValidationReport report = await new WinxClubNativeValidator().ValidateAsync(
            new NativeValidationRequest
            {
                ExecutablePath = @"D:\Missing\WinxClub.exe",
                AssetPath = @"D:\Missing\model.smo",
                LogicalGameAssetPath = @"Characters\Troll\Troll.smo",
                Route = NativeValidationRoute.Contextual,
                UseIsolatedLaunchWorkspace = false,
                LogFilePath = logPath
            });
        Equal(NativeValidationStatus.LaunchFailed, report.Status,
            "an explicit non-isolated request is rejected before launch");
        True(report.Summary.Contains("isolated windowed", StringComparison.OrdinalIgnoreCase),
            "the rejection explains the mandatory windowed workspace");
        True(report.Events.All(validationEvent =>
                validationEvent.Kind != NativeValidationEventKind.ProcessStarted),
            "a rejected non-isolated request never starts WinxClub.exe");
    }
    finally
    {
        if (File.Exists(logPath))
            File.Delete(logPath);
        string? directory = Path.GetDirectoryName(logPath);
        if (directory is not null && Directory.Exists(directory) &&
            !Directory.EnumerateFileSystemEntries(directory).Any())
            Directory.Delete(directory);
    }

    string sceneLogPath = Path.Combine(
        Path.GetTempPath(),
        "SmoNativeValidator.Tests",
        $"scene-route-guard-{Guid.NewGuid():N}.jsonl");
    try
    {
        NativeValidationReport report = await new WinxClubNativeValidator().ValidateAsync(
            new NativeValidationRequest
            {
                ExecutablePath = @"D:\Missing\WinxClub.exe",
                AssetPath = @"D:\Missing\model.smo",
                LogicalGameAssetPath = @"Menus\mousecursor.smo",
                Route = NativeValidationRoute.FastGeneric,
                UseIsolatedLaunchWorkspace = true,
                RequireSceneReady = true,
                LogFilePath = sceneLogPath
            });
        Equal(NativeValidationStatus.LaunchFailed, report.Status,
            "scene-ready requirement is rejected on the fast generic route");
        True(report.Summary.Contains("contextual", StringComparison.OrdinalIgnoreCase),
            "scene-ready route rejection names the required contextual route");
        True(report.Events.All(validationEvent =>
                validationEvent.Kind != NativeValidationEventKind.ProcessStarted),
            "an invalid scene-ready route never starts WinxClub.exe");
    }
    finally
    {
        if (File.Exists(sceneLogPath))
            File.Delete(sceneLogPath);
    }
}

void TestLevelCatalog()
{
    Equal(50, WinxClubLevelCatalog.Levels.Count,
        "catalog covers every native startLevel descriptor");
    Equal(50, WinxClubLevelCatalog.Levels.Select(level => level.Id).Distinct().Count(),
        "startLevel IDs are unique");
    True(WinxClubLevelCatalog.TryGet(2, out WinxClubLevelInfo? gardenia02),
        "default contextual start level exists");
    Equal("Gardenia02", gardenia02!.InternalName,
        "default contextual start level has its native name");
    True(gardenia02.CanStart, "Gardenia02 is startable");
    True(WinxClubLevelCatalog.CanStart(37), "last debug-menu level is startable");
    True(WinxClubLevelCatalog.CanStart(41) && WinxClubLevelCatalog.CanStart(49),
        "hidden challenge start levels are represented as startable");
    for (int id = 38; id <= 40; id++)
    {
        WinxClubLevelInfo unavailable = WinxClubLevelCatalog.Get(id);
        Equal(WinxClubLevelAvailability.Unavailable, unavailable.Availability,
            $"empty native descriptor {id} is unavailable");
        True(!unavailable.CanStart && unavailable.Warning is not null,
            $"empty native descriptor {id} carries a blocking warning");
    }
    WinxClubLevelInfo incomplete = WinxClubLevelCatalog.Get(50);
    Equal("Gardenia04", incomplete.InternalName,
        "incomplete final descriptor retains its native name");
    Equal(WinxClubLevelAvailability.Incomplete, incomplete.Availability,
        "Gardenia04 is marked incomplete because its SMO is absent");
    True(!WinxClubLevelCatalog.CanStart(0) && !WinxClubLevelCatalog.CanStart(51),
        "out-of-range start levels cannot launch");
    True(WinxClubLevelCatalog.TryResolveStartLevelForLogicalSmo(
            @"Levels\Alfea\Alfea02.smo", out int alfea02) && alfea02 == 28,
        "Alfea02 resolves to its native startLevel instead of the old default 2");
    True(WinxClubLevelCatalog.TryResolveStartLevelForLogicalSmo(
            @"Media/Levels/Challenges/BATTLE_03.smo", out int battle03) && battle03 == 49,
        "logical SMO resolution accepts Media prefix, slash variants and case variants");
    True(!WinxClubLevelCatalog.TryResolveStartLevelForLogicalSmo(
            @"Characters\Bloom\bloom_jeans.smo", out _),
        "character resources remain manual contextual routes, not false level routes");
}

void TestSceneReadyTracker()
{
    var tracker = new NativeSceneReadyTracker(28);
    True(!tracker.Observe(
            new NativeGameFlowSnapshot(28, 28, 0, 28),
            targetAccepted: true) &&
            tracker.TargetStateObserved &&
            !tracker.TransitionQueueIdle &&
            tracker.ActiveStackStateMatched,
        "the requested level is not ready while its transition remains pending");
    True(!tracker.Observe(
            new NativeGameFlowSnapshot(74, 0, 1, 74),
            targetAccepted: true) &&
            !tracker.TargetStateObserved,
        "an active loading state is not mistaken for the requested level state");
    True(!tracker.Observe(
            new NativeGameFlowSnapshot(28, 0, 0, 28),
            targetAccepted: false),
        "a complete level state before target acceptance is not target evidence");
    True(tracker.Observe(
            new NativeGameFlowSnapshot(28, 0, 0, 28),
            targetAccepted: true) && tracker.SceneReady,
        "matching current and active states with an idle transition queue are scene-ready");

    var mismatchedStack = new NativeSceneReadyTracker(1);
    True(!mismatchedStack.Observe(
            new NativeGameFlowSnapshot(1, 0, 0, 74),
            targetAccepted: true) &&
            !mismatchedStack.ActiveStackStateMatched,
        "a stale current-state field cannot pass when the active stack object differs");
}

void TestLaunchWorkspace()
{
    string sourceDirectory = Path.Combine(
        Path.GetTempPath(),
        "SmoNativeValidator.Tests",
        Guid.NewGuid().ToString("N"));
    Directory.CreateDirectory(sourceDirectory);
    string executable = Path.Combine(sourceDirectory, "WinxClub.exe");
    File.WriteAllBytes(executable, [0x4D, 0x5A]);
    string shaderDirectory = Path.Combine(sourceDirectory, "Shaders");
    string mediaDirectory = Path.Combine(sourceDirectory, "Media");
    Directory.CreateDirectory(mediaDirectory);
    File.WriteAllText(Path.Combine(mediaDirectory, "marker.bin"), "media");
    string nestedShaderDirectory = Path.Combine(shaderDirectory, "Nested");
    Directory.CreateDirectory(nestedShaderDirectory);
    File.WriteAllText(Path.Combine(shaderDirectory, "Fixed.rfx"), "fixed");
    File.WriteAllBytes(Path.Combine(nestedShaderDirectory, "Basic.vsh"), [1, 2, 3]);

    string? workspaceDirectory = null;
    try
    {
        using (NativeLaunchWorkspace workspace = NativeLaunchWorkspace.Create(
                   executable,
                   shaderSearchDirectory: null,
                   NativeValidationDefaults.ContextualStartLevel))
        {
            workspaceDirectory = workspace.SessionDirectory;
            True(Directory.Exists(workspaceDirectory),
                "owned launch workspace exists during validation");
            Equal<int?>(2, workspace.StartLevel,
                "workspace records the actually written startLevel");
            Equal(Path.GetFullPath(mediaDirectory), workspace.MediaSourceDirectory,
                "workspace binds Media to the directory beside the selected executable");
            Equal(2, workspace.CopiedShaderFiles.Count,
                "all shader files are copied into the isolated workspace");
            True(File.ReadAllBytes(Path.Combine(
                    workspaceDirectory, "Shaders", "Nested", "Basic.vsh"))
                .SequenceEqual(new byte[] { 1, 2, 3 }),
                "nested shader bytes are unchanged");

            byte[] winxBytes = File.ReadAllBytes(workspace.WinxIniPath);
            True(winxBytes.Length < 3 ||
                !(winxBytes[0] == 0xEF && winxBytes[1] == 0xBB && winxBytes[2] == 0xBF),
                "legacy winx.ini is UTF-8/ASCII without a BOM");
            string winx = File.ReadAllText(workspace.WinxIniPath);
            string[] winxLines = File.ReadAllLines(workspace.WinxIniPath);
            Equal("fullScreen=false", winxLines.Single(line =>
                    line.StartsWith("fullScreen=", StringComparison.OrdinalIgnoreCase)),
                "isolated winx.ini has exactly one explicit windowed-mode key");
            True(winx.Contains("fullScreen=false\r\n", StringComparison.Ordinal) &&
                winx.Contains("showCinematics=false\r\n", StringComparison.Ordinal) &&
                winx.Contains("startLevel=2\r\n", StringComparison.Ordinal),
                "isolated winx.ini is windowed, skips cinematics and applies the level");
            True(!winx.Replace("\r\n", string.Empty, StringComparison.Ordinal).Contains('\n'),
                "isolated winx.ini uses only CRLF line endings");
            Equal("[Input]\r\nmouse_exclusive = false\r\n",
                File.ReadAllText(workspace.ConfigIniPath),
                "isolated config.ini releases exclusive mouse input");
            True(File.Exists(Path.Combine(sourceDirectory, "Shaders", "Fixed.rfx")),
                "source game shaders remain untouched");
        }

        True(workspaceDirectory is not null && !Directory.Exists(workspaceDirectory),
            "disposing the workspace removes only its GUID-owned directory");

        using NativeLaunchWorkspace normalStartup = NativeLaunchWorkspace.Create(
            executable,
            shaderSearchDirectory: null,
            startLevel: null);
        Equal("fullScreen=false", File.ReadAllLines(normalStartup.WinxIniPath).Single(line =>
                line.StartsWith("fullScreen=", StringComparison.OrdinalIgnoreCase)),
            "normal isolated startup remains explicitly windowed");
        True(!File.ReadAllText(normalStartup.WinxIniPath).Contains(
                "startLevel=", StringComparison.OrdinalIgnoreCase),
            "normal isolated startup omits the startLevel key");
        Throws<ArgumentOutOfRangeException>(() => NativeLaunchWorkspace.Create(
            executable,
            shaderSearchDirectory: null,
            startLevel: 50),
            "workspace rejects a level whose model data is incomplete");
    }
    finally
    {
        if (workspaceDirectory is not null && Directory.Exists(workspaceDirectory))
            Directory.Delete(workspaceDirectory, recursive: true);
        if (Directory.Exists(sourceDirectory))
            Directory.Delete(sourceDirectory, recursive: true);
    }
}

void TestMediaPathMapper()
{
    string root = Path.GetFullPath(Path.Combine(Path.GetTempPath(), "SmoNV-Media"));
    True(NativeMediaPathMapper.TryMap(
            @"D:\Working Winx\Media\Characters\Bloom\bloom_jeans.smo",
            @"Characters\Bloom\bloom_jeans.smo",
            root,
            out string mapped),
        "registry-backed BuildAssetPath result can be rebound to selected Media");
    Equal(
        Path.GetFullPath(Path.Combine(root, "Characters", "Bloom", "bloom_jeans.smo")),
        mapped,
        "media remap preserves the Media-relative suffix");
    True(NativeMediaPathMapper.TryMap(
            string.Empty,
            @"Menus\mousecursor.smo",
            root,
            out string requestedFallback),
        "relative request is a safe fallback when the observed root is unavailable");
    Equal(
        Path.GetFullPath(Path.Combine(root, "Menus", "mousecursor.smo")),
        requestedFallback,
        "request fallback remains below the selected Media root");
    True(!NativeMediaPathMapper.TryMap(
            string.Empty,
            @"..\outside.smo",
            root,
            out _),
        "media remap rejects traversal outside the selected root");
}

void TestTracker()
{
    NativeValidationTracker tracker = new();
    tracker.Apply(Event(1, NativeValidationEventKind.PathRedirected,
        checkpoint: "CP02.return"));
    tracker.Apply(Event(2, NativeValidationEventKind.CheckpointEnter,
        new Dictionary<string, string?> { ["target"] = "true" },
        "CP03"));
    tracker.Apply(Event(3, NativeValidationEventKind.CheckpointEnter,
        new Dictionary<string, string?> { ["targetContext"] = "true" },
        "FFPS01"));
    tracker.Apply(Event(4, NativeValidationEventKind.CheckpointEnter,
        new Dictionary<string, string?> { ["targetContext"] = "true" },
        "FFPS02"));
    tracker.Apply(Event(5, NativeValidationEventKind.CheckpointEnter,
        new Dictionary<string, string?> { ["targetContext"] = "true" },
        "FFPS03"));
    tracker.Apply(Event(6, NativeValidationEventKind.CheckpointReturn,
        new Dictionary<string, string?>
        {
            ["target"] = "true",
            ["accepted"] = "true"
        },
        "CP03.return"));
    tracker.Apply(Event(7, NativeValidationEventKind.CheckpointEnter,
        new Dictionary<string, string?> { ["targetContext"] = "false" },
        "BACKGROUND"));
    tracker.Apply(Event(8, NativeValidationEventKind.SceneReady));

    Equal(8, tracker.Events.Count, "tracker retains full ordered event stream");
    Equal(1, tracker.RedirectCount, "tracker counts redirects");
    True(tracker.TargetRedirected, "tracker sees target redirect");
    True(tracker.TargetLoadEntered, "tracker sees target ResourceLoad enter");
    True(tracker.TargetLoadReturned, "tracker sees target ResourceLoad return");
    True(tracker.TargetLoadAccepted, "tracker records non-null ResourceLoad result");
    True(tracker.TargetFfpsHeaderEntered, "tracker records target FFPS header entry");
    True(tracker.TargetFfpsMagicAccepted, "tracker records target FFPS magic acceptance");
    True(tracker.TargetFfpsVersionAccepted, "tracker records target FFPS version acceptance");
    True(tracker.SceneReadyReached, "tracker records the explicit scene-ready event");
    Equal("FFPS03", tracker.LastTargetCheckpoint,
        "background events do not replace the deepest target checkpoint");
    Equal("BACKGROUND", tracker.LastCheckpoint,
        "global checkpoint still reflects the complete debug stream");
    Equal(Event(8, NativeValidationEventKind.Diagnostic).TimestampUtc,
        tracker.LastProgressUtc, "last-progress time follows events");

    NativeValidationTracker rejected = new();
    rejected.Apply(Event(1, NativeValidationEventKind.CheckpointReturn,
        new Dictionary<string, string?>
        {
            ["target"] = "true",
            ["accepted"] = "false"
        }));
    True(rejected.TargetLoadReturned && !rejected.TargetLoadAccepted,
        "null ResourceLoad result remains rejected");
}

void TestNativeThreadLoadState()
{
    const uint targetThread = 17;
    const uint otherThread = 29;
    NativeThreadLoadState state = new();

    state.MarkTargetPathRedirected(targetThread);
    True(state.ConsumePendingTargetRedirect(targetThread),
        "same-thread ResourceLoad consumes diagnostic redirect correlation");
    True(!state.ConsumePendingTargetRedirect(targetThread),
        "redirect correlation is one-shot");

    ResourceLoadFrame outerTarget = state.Push(targetThread, target: true);
    True(state.IsCurrentTarget(targetThread),
        "outer target ResourceLoad owns target checkpoint context");
    ResourceLoadFrame nestedBackground = state.Push(targetThread, target: false);
    True(!state.IsCurrentTarget(targetThread),
        "nested non-target ResourceLoad cannot borrow the outer target identity");
    True(!state.IsCurrentTarget(otherThread),
        "target context never crosses debuggee threads");

    ResourceLoadFramePopResult nestedReturn = state.Pop(
        targetThread,
        nestedBackground.Id);
    True(nestedReturn.Found && nestedReturn.AbandonedNestedFrames == 0,
        "normal nested return removes exactly its own frame");
    True(state.IsCurrentTarget(targetThread),
        "returning from nested non-target load restores outer target context");

    ResourceLoadFrame nestedTarget = state.Push(targetThread, target: true);
    ResourceLoadFramePopResult unwind = state.Pop(targetThread, outerTarget.Id);
    True(unwind.Found && unwind.AbandonedNestedFrames == 1,
        "an outer return clears frames abandoned by a native unwind");
    True(!state.IsCurrentTarget(targetThread),
        "unwound nested frame cannot leak target context");
    True(!state.Pop(targetThread, nestedTarget.Id).Found,
        "a stale return probe cannot complete an already-unwound frame");

    state.MarkTargetPathRedirected(targetThread);
    state.Push(targetThread, target: true);
    state.Push(otherThread, target: false);
    NativeThreadLoadCleanup cleanup = state.RemoveThread(targetThread);
    Equal(1, cleanup.RemovedFrames,
        "EXIT_THREAD cleanup removes all ResourceLoad frames owned by that thread");
    True(cleanup.RemovedPendingRedirect,
        "EXIT_THREAD cleanup removes pending path correlation");
    True(!state.IsCurrentTarget(targetThread) &&
        !state.ConsumePendingTargetRedirect(targetThread),
        "exited thread leaves no reusable target or path state");
    True(!state.IsCurrentTarget(otherThread),
        "cleaning one thread preserves another thread's independent frame");
    Equal(1, state.RemoveThread(otherThread).RemovedFrames,
        "the remaining thread can be cleaned independently");
}

void TestTargetEngineDiagnosticTracker()
{
    const uint targetThread = 17;
    const string redirected = @"C:\Temp\SmoNV\target.smo";
    NativeThreadLoadState loadState = new();
    NativeTargetEngineDiagnosticTracker tracker = new();

    IReadOnlyList<NativeTargetEngineDiagnostic> beforeTarget = tracker.Observe(
        $"WARNING: Stream reached EOF ({redirected})",
        loadState.IsCurrentTarget(targetThread),
        redirected);
    Equal(0, beforeTarget.Count,
        "matching engine text before target ResourceLoad is ignored");

    ResourceLoadFrame target = loadState.Push(targetThread, target: true);
    ResourceLoadFrame nestedBackground = loadState.Push(targetThread, target: false);
    IReadOnlyList<NativeTargetEngineDiagnostic> nested = tracker.Observe(
        "ERROR: Resource load failed (name background, classID: 0x12345678",
        loadState.IsCurrentTarget(targetThread),
        redirected);
    Equal(0, nested.Count,
        "nested background ResourceLoad cannot reject the outer target");
    True(loadState.Pop(targetThread, nestedBackground.Id).Found,
        "diagnostic scoping fixture restores the target frame");

    IReadOnlyList<NativeTargetEngineDiagnostic> wrongEof = tracker.Observe(
        @"WARNING: Stream reached EOF (C:\Media\background.smo)",
        loadState.IsCurrentTarget(targetThread),
        redirected);
    Equal(0, wrongEof.Count,
        "EOF for a different stream is not attributed to the target");

    string corrupt = $"""
        WARNING: Stream reached EOF ({redirected})
        pStream->Read( uTmp8 ) ERROR: 0x00020003
        ERROR: No empty skin->bone (NULL bone) relation allowed. File corrupt?
        ERROR: Resource load failed (name body-000, classID: 0x681F2043
        ERROR: Cannot create object of this type (class ID: 0x00000062)
        """;
    IReadOnlyList<NativeTargetEngineDiagnostic> added = tracker.Observe(
        corrupt,
        loadState.IsCurrentTarget(targetThread),
        redirected);
    Equal(5, added.Count,
        "all high-confidence target serializer failures are classified");
    Equal(
        "target-stream-eof,target-stream-read-failed,invalid-null-relation," +
        "resource-load-failed,object-creation-failed",
        string.Join(',', tracker.Diagnostics.Select(item => item.Code)),
        "target serializer diagnostic codes are stable and ordered");
    True(tracker.HasDiagnostics,
        "target serializer corruption latches a rejection");
    Equal(0, tracker.Observe(
        corrupt,
        targetContext: true,
        redirectedAssetPath: redirected).Count,
        "repeated and combined debug buffers do not duplicate diagnostics");
    True(tracker.DescribeRejection(returnedNonNull: true).Contains(
            "despite a non-null ResourceLoad return",
            StringComparison.Ordinal),
        "a non-null native pointer cannot hide explicit serializer rejection");
    True(loadState.Pop(targetThread, target.Id).Found,
        "diagnostic scoping fixture removes the target frame");

    NativeTargetEngineDiagnosticTracker benign = new();
    Equal(0, benign.Observe(
        "RENDERER : Direct3D device created successfully.",
        targetContext: true,
        redirectedAssetPath: redirected).Count,
        "benign target debug output is ignored");
    Throws<InvalidOperationException>(
        () => benign.DescribeRejection(returnedNonNull: false),
        "a rejection summary requires concrete engine evidence");
}

void TestCrashAttributionClassification()
{
    (NativeCrashContext Context,
        NativeCrashPhase Phase,
        NativeCrashAttributionConfidence Confidence,
        string EventPhase,
        bool DirectlyAttributed)[] cases =
    [
        (new NativeCrashContext(false, false, false, false, false, false),
            NativeCrashPhase.BeforeTrigger,
            NativeCrashAttributionConfidence.None,
            "before-trigger",
            false),
        (new NativeCrashContext(true, true, true, false, false, false),
            NativeCrashPhase.DuringTargetLoad,
            NativeCrashAttributionConfidence.Direct,
            "during-target-load",
            true),
        (new NativeCrashContext(false, true, true, false, false, false),
            NativeCrashPhase.TargetLoadBackgroundOrUnwind,
            NativeCrashAttributionConfidence.Possible,
            "target-load-background-or-unwind",
            false),
        (new NativeCrashContext(false, true, true, true, true, true),
            NativeCrashPhase.PostReturnSurvivalWindow,
            NativeCrashAttributionConfidence.Possible,
            "post-return-survival",
            false),
        (new NativeCrashContext(false, true, false, false, false, false),
            NativeCrashPhase.AfterTriggerUnattributed,
            NativeCrashAttributionConfidence.Possible,
            "after-trigger-unattributed",
            false)
    ];

    foreach ((NativeCrashContext context,
        NativeCrashPhase expectedPhase,
        NativeCrashAttributionConfidence expectedConfidence,
        string expectedEventPhase,
        bool expectedDirectAttribution) in cases)
    {
        NativeCrashAttribution classification =
            NativeCrashAttributionClassifier.Classify(context);
        Equal(expectedPhase, classification.Phase,
            $"crash phase {expectedEventPhase} is typed");
        Equal(expectedConfidence, classification.Confidence,
            $"crash phase {expectedEventPhase} has explicit attribution confidence");
        Equal(expectedEventPhase, classification.EventPhase,
            $"crash phase {expectedEventPhase} retains its JSONL event value");
        Equal(expectedDirectAttribution, classification.ModelDirectlyAttributed,
            $"legacy modelAttributed semantics are conservative for {expectedEventPhase}");
    }

    NativeCrashAttribution postReturn = NativeCrashAttributionClassifier.Classify(
        new NativeCrashContext(false, true, true, true, true, true));
    True(postReturn.Description.Contains("does not prove", StringComparison.OrdinalIgnoreCase),
        "post-return temporal correlation explicitly avoids confirmed causation");
}

void TestStagingSession()
{
    string sourceDirectory = Path.Combine(
        Path.GetTempPath(),
        "SmoNativeValidator.Tests",
        Guid.NewGuid().ToString("N"));
    Directory.CreateDirectory(sourceDirectory);
    string source = Path.Combine(sourceDirectory, "edited.smo");
    string sourceStx = Path.ChangeExtension(source, ".stx");
    string sourceSpt = Path.ChangeExtension(source, ".spt");
    File.WriteAllBytes(source, [0x46, 0x46, 0x50, 0x53]);
    File.WriteAllText(sourceStx, "stx");
    File.WriteAllText(sourceSpt, "spt");

    string? stagedDirectory = null;
    string? fastStagedDirectory = null;
    try
    {
        using (StagedAssetSession session = StagedAssetSession.Create(
            source,
            @"Characters\Troll\Troll.smo"))
        {
            stagedDirectory = session.SessionDirectory;
            Equal("Troll.smo", Path.GetFileName(session.AssetPath),
                "staging preserves the logical slot file name");
            Equal(3, session.StagedFiles.Count, "SMO and both supported sidecars are staged");
            True(session.StagedFiles.All(File.Exists), "all staged files exist during the session");
            True(StagedAssetSession.IsAscii(session.AssetPath),
                "the native redirect receives an ASCII staging path");
            True(File.ReadAllBytes(session.AssetPath).SequenceEqual(
                    new byte[] { 0x46, 0x46, 0x50, 0x53 }),
                "staged SMO bytes are unchanged");
        }

        True(stagedDirectory is not null && !Directory.Exists(stagedDirectory),
            "disposing the session removes only its owned staging directory");

        NativeValidationRequest fastRequest = new()
        {
            ExecutablePath = @"D:\Winx\WinxClub.exe",
            AssetPath = source,
            LogicalGameAssetPath = @"Characters\Troll\Troll.smo",
            Route = NativeValidationRoute.FastGeneric,
            UseIsolatedLaunchWorkspace = true
        };
        ResolvedNativeValidationRoute fastRoute =
            NativeValidationRouting.Resolve(fastRequest);
        using (StagedAssetSession fastSession = StagedAssetSession.Create(
                   source,
                   fastRoute.TriggerGameAssetPath))
        {
            fastStagedDirectory = fastSession.SessionDirectory;
            Equal(@"Characters\Troll\Troll.smo", fastRequest.LogicalGameAssetPath,
                "fast staging preserves the model's original logical metadata");
            Equal("mousecursor.smo", Path.GetFileName(fastSession.AssetPath),
                "fast staging uses the generic trigger basename");
            True(fastSession.StagedFiles.Select(Path.GetFileName).SequenceEqual(
                    new[] { "mousecursor.smo", "mousecursor.stx", "mousecursor.spt" },
                    StringComparer.OrdinalIgnoreCase),
                "fast staging renames the SMO and both sidecars to the trigger basename");

            foreach (string stagedFile in fastSession.StagedFiles)
                File.SetAttributes(stagedFile, File.GetAttributes(stagedFile) | FileAttributes.ReadOnly);
        }

        True(fastStagedDirectory is not null && !Directory.Exists(fastStagedDirectory),
            "disposing staging normalizes read-only files before removing its owned directory");
    }
    finally
    {
        foreach (string? ownedDirectory in new[] { stagedDirectory, fastStagedDirectory })
        {
            if (ownedDirectory is null || !Directory.Exists(ownedDirectory))
                continue;
            foreach (string file in Directory.EnumerateFiles(
                         ownedDirectory,
                         "*",
                         SearchOption.AllDirectories))
            {
                File.SetAttributes(file, FileAttributes.Normal);
            }
            Directory.Delete(ownedDirectory, recursive: true);
        }
        if (Directory.Exists(sourceDirectory))
            Directory.Delete(sourceDirectory, recursive: true);
    }
}

void TestLogFormat()
{
    True(NativeValidationSeverity.Success < NativeValidationSeverity.Warning,
        "success events are not treated as warnings by ordinal UI filtering");
    NativeValidationEvent source = Event(7, NativeValidationEventKind.Exception,
        new Dictionary<string, string?>
        {
            ["code"] = "0xC0000005",
            ["lastCheckpoint"] = "FFPS03"
        },
        "FFPS03") with
    {
        Severity = NativeValidationSeverity.Error,
        Address = 0x00422345,
        ThreadId = 17,
        ProgressPercent = 70
    };
    string line = NativeSessionLog.SerializeLine(source);
    True(!line.Contains('\n') && !line.Contains('\r'), "one event serializes to one JSONL line");
    NativeValidationEvent restored = NativeSessionLog.DeserializeLine(line);
    Equal(source.Sequence, restored.Sequence, "log sequence round-trip");
    Equal(source.TimestampUtc, restored.TimestampUtc, "log timestamp round-trip");
    Equal(source.Stage, restored.Stage, "log stage round-trip");
    Equal(source.Severity, restored.Severity, "log severity round-trip");
    Equal(source.Address, restored.Address, "log address round-trip");
    Equal("0xC0000005", restored.Data["code"], "structured log data round-trip");
}

void TestSettingsFormat()
{
    NativeValidatorSettings source = new()
    {
        ManualExecutablePath = @"D:\Winx\WinxClub.exe",
        LogicalGameAssetPath = @"Characters\Bloom\bloom_jeans.smo",
        Route = NativeValidationRoute.Contextual,
        ContextualStartLevel = 14,
        OverallTimeoutSeconds = 180,
        NoProgressTimeoutSeconds = 45,
        SurvivalWindowMilliseconds = 3500,
        IncludeBloomCheckpoints = true
    };
    string json = NativeValidatorSettingsStore.Serialize(source);
    NativeValidatorSettings restored = NativeValidatorSettingsStore.Deserialize(json);
    Equal(source, restored, "settings JSON round-trip");
    True(json.Contains("manualExecutablePath", StringComparison.Ordinal),
        "settings use stable camel-case JSON");
    NativeValidatorSettings defaults = NativeValidatorSettingsStore.Deserialize("{}");
    Equal(NativeValidationRoute.FastGeneric, defaults.Route,
        "new settings recommend the fast generic native route");
    Equal<int?>(NativeValidationDefaults.ContextualStartLevel,
        defaults.ContextualStartLevel,
        "new settings retain the accelerated contextual preset");
    Equal(120, defaults.OverallTimeoutSeconds, "missing settings retain safe default timeout");
    True(NativeValidatorSettingsStore.GetDefaultSettingsFilePath().EndsWith(
        Path.Combine("SparkplugEngineResearch", "SmoNativeValidator", "settings.json"),
        StringComparison.OrdinalIgnoreCase),
        "manual settings live under the application LocalAppData subtree");
}

void TestInteropLayouts()
{
    Type context = typeof(WinxClubNativeValidator).Assembly.GetType(
        "SmoNativeValidator.Core.Win32Native+X86Context", throwOnError: true)!;
    Type debugEvent = typeof(WinxClubNativeValidator).Assembly.GetType(
        "SmoNativeValidator.Core.Win32Native+DebugEvent", throwOnError: true)!;
    Equal(716, Marshal.SizeOf(context), "WOW64/x86 CONTEXT layout size");
    Equal(Environment.Is64BitProcess ? 176 : 96, Marshal.SizeOf(debugEvent),
        "host DEBUG_EVENT layout size");
}

void TestDebugExceptionClassification()
{
    True(NativeDebugExceptionClassifier.IsSoftwareBreakpoint(0x80000003),
        "native STATUS_BREAKPOINT is handled by the breakpoint manager");
    True(NativeDebugExceptionClassifier.IsSoftwareBreakpoint(0x4000001F),
        "WOW64 STATUS_WX86_BREAKPOINT is handled by the breakpoint manager");
    True(NativeDebugExceptionClassifier.IsSingleStep(0x80000004),
        "native STATUS_SINGLE_STEP is used to re-arm breakpoints");
    True(NativeDebugExceptionClassifier.IsSingleStep(0x4000001E),
        "WOW64 STATUS_WX86_SINGLE_STEP is used to re-arm breakpoints");
    True(!NativeDebugExceptionClassifier.IsSoftwareBreakpoint(0xC0000005),
        "access violations are not swallowed as validator breakpoints");
}

await TestExecutableSignatureResolution();
TestLocatorResolver();
TestLogicalMatcher();
TestResourceLoadTargetClassifier();
TestValidationRouting();
await TestWindowedLaunchGuard();
TestLevelCatalog();
TestSceneReadyTracker();
TestTracker();
TestNativeThreadLoadState();
TestTargetEngineDiagnosticTracker();
TestCrashAttributionClassification();
TestLogFormat();
TestSettingsFormat();
TestInteropLayouts();
TestDebugExceptionClassification();
TestStagingSession();
TestLaunchWorkspace();
TestMediaPathMapper();

Console.WriteLine($"PASS: {assertions} assertions");
