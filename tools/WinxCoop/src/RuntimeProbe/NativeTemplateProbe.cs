using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own bounded original-template construction diagnostic. This is not an NPC
/// startup, scheduler, movement backend or replacement game serializer.
/// </summary>
internal sealed class NativeTemplateProbe
{
    internal const string StockSha256 = "617EF85C33E9E447565CBF83A08FA2476D22F1C24570B59A6D21D4BEA61A7296";
    private const string SchemaInspectorSha256 = "839EB8269BDB598EAFA960AC2D38BC12DAF9E0537BF0CDAF276E0537D53523D3";
    private const int PayloadBytes = 4298;
    private readonly PEReader _image;
    private readonly Action<object> _log;
    private readonly byte[] _payload;
    private readonly bool _initialize;
    private readonly uint _expectedMachineKind;
    private readonly Descriptor[] _descriptors;
    private DebuggerHost _host = null!;
    private IntPtr _process;
    private uint _pid;
    private int _ownerThread;
    private int _stage;
    private uint _stream, _template, _instance, _constructing, _buffer;
    private uint _profile, _record, _localRoot, _parent, _scene, _input, _command, _device, _camera;
    private uint _templateManager, _gameManager, _fileManager, _engine, _flow, _level, _active;
    private uint _character, _move, _animator, _machine, _traits, _ownRoot;
    private uint _localAnimator, _localActor, _ownNotify1C, _ownMoveInit, _ownAnimatorInit;
    private ushort _streamRefs, _templateRefs, _instanceRefs;
    private uint _foreignWitnesses, _ownNotify1D, _ownCharacterInit, _buildWitnesses;
    private readonly HashSet<uint> _localNodes = [];
    private uint[] _ownEntities = [];
    private object? _constructionWitness;
    private object? _initializedWitness;
    private bool _cleaned, _payloadWritten;
    private bool _dependentInterval;

    private sealed record Descriptor(uint Id, uint Type, uint ClassId, int Offset, int PropertyOffset, int PropertyBytes);
    private sealed record Component(uint Address, uint Table, uint ClassId, uint Owner, uint BoundNode);
    private sealed record Relationship(string Operand, string Actual, string Expected, bool Matched);
    private sealed record Membership(uint[] Counts, uint MoveCount, uint MainListCount, uint SecondaryListCount,
        uint[] MainListMatches, uint[] SecondaryListMatches, uint[] TableMatches, uint[] Matches, string TablesSha256);

    internal NativeTemplateProbe(ProbeConfiguration config, PEReader image, Action<object> log)
    {
        _image = image; _log = log;
        _initialize = config.Mode == "template-init";
        _payload = ReadVerifiedPayload(config);
        // Pinned StaticSeresa Animation Group property (DWORD at file2669).
        // Native setter555060/getter555070 access machine+128; it is a kind
        // value, not the Animator pointer held by Character+128.
        _expectedMachineKind = BitConverter.ToUInt32(_payload, 2669);
        Require(_expectedMachineKind == 0x40, "Pinned Students Animation Group property mismatch.");
        _descriptors =
        [
            new(5, 0, 0x695C0F65, 76, 0, 0),
            new(1, 1, 0x0003CC73, 532, 988, 215),
            new(2, 1, 0x1BC6108E, 1203, 1659, 400),
            new(0, 1, 0x78F35E7F, 2059, 2515, 182),
            new(4, 1, 0xCD2B2876, 2697, 3153, 127),
            new(3, 1, 0x680838E4, 3280, 3736, 562)
        ];
    }

    internal bool ModeSucceeded => _cleaned;
    internal bool HasLiveOwnership => _stream != 0 || _template != 0 || _instance != 0 || _constructing != 0;
    internal object Summary => new { stage = _stage, cleaned = _cleaned, hasLiveOwnership = HasLiveOwnership,
        templateSha256 = StockSha256, schemaInspectorSha256 = SchemaInspectorSha256,
        stream = Hex(_stream), template = Hex(_template), instance = Hex(_instance),
        ownNotify1D = _ownNotify1D, ownCharacterInit = _ownCharacterInit, buildWitnesses = _buildWitnesses,
        foreignWitnesses = _foreignWitnesses, construction = _constructionWitness, initialized = _initializedWitness,
        scopedInitializationMode = _initialize, dependentIntervalActive = _dependentInterval,
        ownNotify1C = _ownNotify1C, ownMoveInit = _ownMoveInit, ownAnimatorInit = _ownAnimatorInit,
        nativeScopedInitCallsObserved = _ownNotify1C == 0 ? 0 : 1,
        independentMovementQualified = false, schedulerAbsenceClaimed = false,
        manualInitOrSchedulerCalls = 0, nativeEntityFieldWrites = 0, ownedPayloadBytesWritten = _payloadWritten ? PayloadBytes : 0 };

    internal static byte[] ReadVerifiedPayload(ProbeConfiguration config)
    {
        string path = config.TemplateFile ?? Path.Combine(config.MediaDirectory, "Characters", "Bloom", "StaticSeresa.spt");
        string expected = config.TemplateSha256 ?? StockSha256;
        if (!Path.IsPathFullyQualified(path) || path.Any(c => c is < ' ' or > '~') || expected != StockSha256)
            throw new InvalidDataException("Template probe requires an absolute ASCII path and the exact stock StaticSeresa SHA256.");
        var info = new FileInfo(path);
        if (!info.Exists || info.Length is < 1 or > 65536 || info.Length != PayloadBytes)
            throw new InvalidDataException("Expected the bounded 4298-byte stock StaticSeresa template.");
        byte[] bytes = File.ReadAllBytes(path);
        if (bytes.Length != PayloadBytes || Convert.ToHexString(SHA256.HashData(bytes)) != expected ||
            BitConverter.ToUInt32(bytes, 0) != 0xDAB33F00 || BitConverter.ToUInt32(bytes, 0x44) != 6)
            throw new InvalidDataException("Stock template bytes/hash/header mismatch.");
        return bytes;
    }

    internal void InstallWitnesses(DebuggerHost host)
    {
        Require(host.IsAtDebugStop && host.ProcessHandle != IntPtr.Zero && host.ProcessPid != 0 && host.ImageBase == 0x400000,
            "Template witness installation requires the pinned owned process at its debug stop.");
        _host = host;
        _process = host.ProcessHandle; _pid = host.ProcessPid; _ownerThread = Environment.CurrentManagedThreadId;
        Hook(0x5FB950, "Template.build", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (_template != 0 && _stage == 6 && U(ctx.Ecx + 0x14) == _template)
            {
                Require(ctx.Ecx != _record && U(ctx.Ecx) == 0x710F80 && _constructing == 0,
                    "Unexpected new native template build identity.");
                _constructing = ctx.Ecx; ++_buildWitnesses;
                _log(new { @event = "template-owned-build-entry", threadId = tid, instance = Hex(ctx.Ecx), template = Hex(_template) });
            }
            else Foreign("Template.build", tid, ctx.Ecx, null);
        });
        Hook(0x4F45A0, "Character.Notify", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            uint message = Pointer(ctx.Esp + 4), code = U(message);
            if (OwnReceiver(ctx.Ecx))
            {
                Require(code != 0x1E, "Owned Character received a tick during the bounded construction interval.");
                if (code == 0x1C)
                {
                    Require(_initialize && _dependentInterval && U(message + 0x10) == 0 && U(message + 0x18) <= 7,
                        "Owned Character received an unqualified scoped initialization message.");
                    Require(++_ownNotify1C == 1, "Repeated owned Character1C initialization is unqualified.");
                }
                if (code == 0x1D) ++_ownNotify1D;
                _log(new { @event = "template-owned-character-notify", threadId = tid, receiver = Hex(ctx.Ecx),
                    code = Hex(code), sender = Hex(U(message + 0x10)), instance = Hex(_constructing != 0 ? _constructing : _instance) });
            }
            else Foreign("Character.Notify", tid, ctx.Ecx, code);
        });
        Hook(0x4F3ED0, "Character.init", (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            if (OwnReceiver(ctx.Ecx))
            {
                Require(ClassId(ctx.Ecx) == 0x0003CC73, "Owned Character setup receiver type mismatch.");
                ++_ownCharacterInit;
                Require(_ownCharacterInit == 1, "Repeated owned Character setup is unqualified.");
                _log(new { @event = "template-owned-character-setup-entry", threadId = tid, receiver = Hex(ctx.Ecx),
                    owner = Hex(U(ctx.Ecx + 0x24)), returnAddress = Hex(U(ctx.Esp)), selector148 = U(ctx.Ecx + 0x148) });
            }
            else Foreign("Character.init", tid, ctx.Ecx, null);
        });
        foreach ((uint address, string name) in new[] { (0x4F60D0u, "MoveCtrl.init"), (0x4FB6E0u, "Animator.init") })
            Hook(address, name, (uint tid, uint at, ref Win32Native.X86Context ctx) =>
            {
                if (OwnReceiver(ctx.Ecx))
                {
                    _log(new { @event = "template-owned-component-init-entry", name, receiver = Hex(ctx.Ecx), threadId = tid,
                        scopedInterval = _initialize && _dependentInterval });
                    Require(_initialize && _dependentInterval, "Native owned component initialization occurred outside its scoped qualification interval.");
                    if (at == 0x4F60D0) Require(++_ownMoveInit == 1, "Repeated own MoveCtrl initialization.");
                    else Require(++_ownAnimatorInit == 1, "Repeated own Animator initialization.");
                    return;
                }
                Foreign(name, tid, ctx.Ecx, null);
            });
    }

    internal void Step(DebuggerHost host, uint tid, ref Win32Native.X86Context ctx,
        NativeCallDispatcher calls, Action resume)
    {
        Require(ReferenceEquals(host, _host) && host.IsAtDebugStop && tid == host.MainThreadId && ctx.Eip == NativeCallDispatcher.WorldReturn && !calls.Pending,
            "Template experiment requires the owned main-thread world stop.");
        if (_cleaned) return;
        if (_stage != 0) VerifyLocal();
        switch (_stage)
        {
            case 0:
                CaptureLocal(); _stage = 1;
                Call(tid, ref ctx, calls, resume, 0x465560, 0, [], true, value =>
                {
                    _stream = value; Table(_stream, 0x6E7E50); _streamRefs = Refs(_stream);
                    _log(new { @event = "template-stream-factory-return", stream = Hex(_stream), referenceCount = _streamRefs,
                        prefixHex = Convert.ToHexString(Read(_stream, 0x38)) });
                    Require(_streamRefs == 0, "Fresh stream has unexpected incoming owners.");
                }); break;
            case 1:
                _stage = 2;
                Call(tid, ref ctx, calls, resume, 0x465500, _stream, [(uint)PayloadBytes], false, value =>
                { Require((value & 255) != 0, "Native memory stream resize failed."); VerifyStream(); }); break;
            case 2:
                _stage = 3;
                Call(tid, ref ctx, calls, resume, 0x4CF2F0, _stream, [], false, value =>
                {
                    VerifyStream(); Require(value != 0 && value == U(_stream + 0x30), "Native owned buffer identity mismatch.");
                    _buffer = value;
                    _ = Read(_buffer, PayloadBytes);
                    Win32Native.WriteMemory(_host.ProcessHandle, _buffer, _payload, executable: false);
                    Require(Read(_buffer, PayloadBytes).SequenceEqual(_payload), "Owned stream payload readback mismatch.");
                    _payloadWritten = true;
                    _log(new { @event = "template-owned-payload-written", stream = Hex(_stream), buffer = Hex(_buffer),
                        bytes = PayloadBytes, sha256 = StockSha256, nativeEntityFieldsWritten = 0 });
                }); break;
            case 3:
                _stage = 4;
                Call(tid, ref ctx, calls, resume, 0x59F630, 0, [], true, value =>
                {
                    _template = value; Table(_template, 0x703D24); _templateRefs = Refs(_template);
                    _log(new { @event = "template-factory-return", template = Hex(_template), referenceCount = _templateRefs,
                        baseList04 = Hex(U(_template + 4)), expectedConstructorRegistrationReferences = 1,
                        prefixHex = Convert.ToHexString(Read(_template, 0x60)) });
                    // 59F540 registers the new object through manager Add at
                    // 59F5FD. Stock factory return therefore has one reference,
                    // unlike MemoryStream. Its base +04 list is independent.
                    Require(_templateRefs == 1 && U(_template + 4) == 0,
                        "Template constructor registration baseline/base-list mismatch.");
                }); break;
            case 4:
                VerifyStream(); _stage = 5;
                Call(tid, ref ctx, calls, resume, 0x59E930, _template, [_stream], false, value =>
                { Require((value & 255) != 0, "Native template parse failed; unknown partial state."); InspectTemplate(); }); break;
            case 5:
                InspectTemplate(); Require(Read(Pointer(0x755278) + 0x21C, 1)[0] == 0, "Native1D setup guard is not admitted.");
                _stage = 6;
                if (_initialize)
                {
                    foreach (uint function in new uint[] { 0x59E7A0, 0x5991E0, 0x5FB350 }) VerifyFunction(function);
                    _dependentInterval = true;
                    calls.BeginDependentLifecycle(host, tid, ref ctx, new(0x59E7A0, _template, [1], false), 0x5991E0, 0x5FB350,
                        value =>
                        {
                            _instance = value;
                            Require(_constructing == value && _buildWitnesses == 1, "Dependent Create result does not match its own build.");
                            _constructing = 0; CaptureInstance(initialized: false, allowSecondary: true);
                        },
                        value =>
                        {
                            Require(value == _instance, "Scoped init changed the owned instance identity.");
                            CaptureInstance(initialized: true, allowSecondary: true);
                        },
                        results =>
                        {
                            Require(results.Length == 3 && results[0] == _instance && _initializedWitness is not null,
                                "Dependent lifecycle completion/results mismatch.");
                            uint deleted = _instance; _instance = 0; _ownRoot = 0; _dependentInterval = false;
                            VerifyLocal();
                            Require(Memberships(_ownEntities).Matches.Length == 0,
                                "Own entities survived in game-manager lists/tables after native scoped deletion.");
                            _stage = 7;
                            _log(new { @event = "template-scoped-init-and-delete-returned", deletedInstance = Hex(deleted),
                                privateIntervalResults = results, originalSchedulerResumedBetweenCalls = false, freedBytesRead = false });
                            resume();
                        });
                }
                else
                    Call(tid, ref ctx, calls, resume, 0x59E7A0, _template, [1], false, value =>
                    {
                        _instance = value;
                        Require(_constructing == value && _buildWitnesses == 1, "Create result does not match the observed own build.");
                        _constructing = 0; CaptureInstance();
                    });
                break;
            case 6:
                VerifyOwnedInstance(); Require(Refs(_instance) == _instanceRefs, "Own instance refcount changed before deletion.");
                _stage = 7;
                Call(tid, ref ctx, calls, resume, 0x5FB350, _instance, [1], false, _ =>
                {
                    // Never read deleted instance, components, command or Nodes.
                    uint deleted = _instance; _instance = 0; _ownRoot = 0;
                    Require(Memberships(_ownEntities).Matches.Length == 0, "Deleted own components remain in bounded game-manager tables.");
                    _log(new { @event = "template-instance-delete-returned", deletedInstance = Hex(deleted), freedBytesRead = false });
                }); break;
            case 7:
                Table(_template, 0x703D24); Require(_templateRefs == 1 && Refs(_template) == _templateRefs && U(_template + 4) == 0,
                    "Template constructor registration baseline/base-list changed before deletion.");
                _stage = 8;
                // Original 5FD212/5FD21C parse-failure cleanup directly invokes
                // this deleting destructor at 5FD274 without a manual release.
                // Its 5A0490 body unregisters the template through 462A50.
                Call(tid, ref ctx, calls, resume, 0x5A05B0, _template, [1], false, _ =>
                { uint deleted = _template; _template = 0; _log(new { @event = "template-descriptor-owner-deleted", deletedTemplate = Hex(deleted), instanceAlreadyDeleted = true }); }); break;
            case 8:
                Table(_stream, 0x6E7E50); Require(Refs(_stream) == _streamRefs, "Stream owner changed before deletion.");
                _stage = 9;
                Call(tid, ref ctx, calls, resume, 0x465730, _stream, [1], false, _ =>
                { uint deleted = _stream; _stream = 0; _log(new { @event = "template-input-stream-deleted", deletedStream = Hex(deleted), nativeBorrowersCompleted = true }); }); break;
            case 9:
                VerifyLocal(); Require(!HasLiveOwnership && Memberships(_ownEntities).Matches.Length == 0,
                    "Native template cleanup retained an unexpected owner/membership.");
                _cleaned = true; _stage = 10;
                _log(new { @event = "template-create-cycle-cleaned", localIdentityPreserved = true,
                    boundedConstructionAndCleanupOnly = true, independentMovementQualified = false }); break;
            default: throw new InvalidOperationException("Unknown native template probe phase.");
        }
    }

    private void CaptureLocal()
    {
        _profile = Pointer(0x765AD4); Table(_profile, 0x6F5A44);
        _record = Pointer(_profile + 0x2B4); Table(_record, 0x710F80);
        _localRoot = Pointer(_record + 0x24); Table(_localRoot, VisualGraphReader.NodeVtable);
        _parent = Pointer(_localRoot + 0x2C); _scene = Pointer(_localRoot + 0x3C); Table(_scene, 0x6E7358);
        Require(U(_record + 0x20) == 11, "Expected the typed eleven-component local player.");
        _input = Pointer(0x7552A0); Table(_input, 0x6F5508);
        _command = Pointer(_input + 0x18); Table(_command, 0x703090);
        _device = Pointer(_input + 0x1C); Table(_device, 0x7116CC);
        _engine = Pointer(0x755274); _camera = Pointer(_engine + 0x1C);
        _templateManager = Pointer(0x75DC64); Table(_templateManager, 0x6E77E0);
        _gameManager = Pointer(0x765ACC); Table(_gameManager, 0x70069C);
        _fileManager = Pointer(0x755278); Require(Read(_fileManager + 0x21C, 1)[0] == 0, "File manager suppresses native template setup.");
        _flow = Pointer(0x755294); _level = U(_flow + 0x1B0);
        int stack = unchecked((int)U(_flow + 0x1AC));
        Require(_level is >= 1 and <= 50 && stack is >= 0 and <= 20 && U(_flow + 0x1B8) == 0, "Playing flow is not settled.");
        _active = Pointer(_flow + 0x15C + (uint)stack * 4); Require(U(_active + 0x10) == _level, "Playing flow identity mismatch.");
        Require(U(_templateManager + 0x20) == 0 && U(Pointer(0x75DB90) + 0x20) == 0, "Native parse/world context is active.");
        var graph = new VisualGraphReader(_host.ProcessHandle).Read(_localRoot, inspectSkins: false);
        Require(graph.Success, "Cannot bound local node identities: " + graph.Error);
        foreach (var node in graph.Graph.Nodes) _localNodes.Add(node.Address);
        if (_initialize)
        {
            uint head = Pointer(_record + 0x1C), link = Pointer(head);
            for (int i = 0; i < 11; ++i)
            {
                Require(link != head, "Local Actor inventory ended before eleven components.");
                uint item = Pointer(link + 8);
                if (U(item) == 0x6F670C)
                {
                    Require(_localAnimator == 0 && U(item + 0x24) == _record, "Local Animator identity is not unique/bound.");
                    _localAnimator = item; _localActor = Pointer(item + 0x128); Table(_localActor, 0x703F80);
                }
                link = Pointer(link);
            }
            Require(link == head && _localAnimator != 0, "Local Actor inventory count/type mismatch.");
        }
        _log(new { @event = "template-local-baseline", profile = Hex(_profile), record = Hex(_record), root = Hex(_localRoot),
            parent = Hex(_parent), scene = Hex(_scene), input = Hex(_input), command = Hex(_command), device = Hex(_device), camera = Hex(_camera),
            parseContext = Hex(U(_templateManager + 0x20)), manager = Memberships([]), localNodeCount = _localNodes.Count,
            stockTemplateSha256 = StockSha256, schemaInspectorSha256 = SchemaInspectorSha256 });
    }

    private void VerifyLocal()
    {
        Require(U(0x765AD4) == _profile && U(_profile + 0x2B4) == _record && U(_record) == 0x710F80 &&
            U(_record + 0x24) == _localRoot && U(_record + 0x20) == 11 && U(_localRoot + 0x2C) == _parent && U(_localRoot + 0x3C) == _scene &&
            U(0x7552A0) == _input && U(_input + 0x18) == _command && U(_input + 0x1C) == _device &&
            U(0x755274) == _engine && U(_engine + 0x1C) == _camera && U(0x75DC64) == _templateManager &&
            U(_templateManager + 0x20) == 0 && U(0x765ACC) == _gameManager && U(0x755278) == _fileManager &&
            U(0x755294) == _flow && U(_flow + 0x1B0) == _level && U(_flow + 0x1B8) == 0 && U(_active + 0x10) == _level,
            "Local profile/record/input/device/camera/scene/parse identity changed during native template experiment.");
        int stack = unchecked((int)U(_flow + 0x1AC));
        Require(stack is >= 0 and <= 20 && U(_flow + 0x15C + (uint)stack * 4) == _active &&
            U(Pointer(0x75DB90) + 0x20) == 0 && Read(_fileManager + 0x21C, 1)[0] == 0,
            "Active flow/world/template-setup phase changed during the experiment.");
        Table(_command, 0x703090); Table(_input, 0x6F5508); Table(_device, 0x7116CC);
        if (_initialize) Require(U(_localAnimator + 0x128) == _localActor && U(_localActor) == 0x703F80,
            "Local Animator/Actor identity changed during scoped NPC initialization.");
    }

    private void VerifyStream()
    {
        Table(_stream, 0x6E7E50);
        Require(U(_stream + 0x1C) == PayloadBytes && U(_stream + 0x20) == PayloadBytes && U(_stream + 0x2C) == 0 &&
            Read(_stream + 0x34, 1)[0] == 1 && Refs(_stream) == _streamRefs,
            "Owned stream extent/position/ownership mismatch.");
        _ = Read(Pointer(_stream + 0x30), PayloadBytes);
    }

    private void InspectTemplate()
    {
        VerifyLocal(); Table(_template, 0x703D24);
        Require(U(_template + 0x1C) == 6 && _templateRefs == 1 && Refs(_template) == _templateRefs && U(_template + 4) == 0,
            "Parsed template count/constructor registration/base-list mismatch.");
        uint descriptor = Pointer(_template + 0x14);
        var visited = new HashSet<uint>(); var rows = new List<object>();
        foreach (Descriptor expected in _descriptors)
        {
            Require(visited.Add(descriptor) && ClassId(descriptor) == 0x014E1394 && U(descriptor + 0x20) == expected.Type &&
                U(descriptor + 0x12C) == expected.Id && U(descriptor + 0x130) == uint.MaxValue,
                "Native descriptor identity/type/parent mismatch.");
            string? hash = null;
            if (expected.Type == 1)
            {
                uint stream = Pointer(descriptor + 0x24); Table(stream, 0x6E7E50);
                Require(U(stream + 0x20) == expected.PropertyBytes, "Native property stream size mismatch.");
                byte[] bytes = Read(Pointer(stream + 0x30), expected.PropertyBytes);
                _log(new { @event = "template-native-property-stream", descriptor = Hex(descriptor), id = expected.Id,
                    stream = Hex(stream), bytes = bytes.Length, headerHex = Convert.ToHexString(bytes.AsSpan(0, 24)),
                    actualSha256 = Convert.ToHexString(SHA256.HashData(bytes)),
                    expectedSha256 = Convert.ToHexString(SHA256.HashData(_payload.AsSpan(expected.PropertyOffset, expected.PropertyBytes))) });
                Require(BitConverter.ToUInt32(bytes, 0) == 0x74474539 && BitConverter.ToUInt32(bytes, 4) == 2 &&
                    BitConverter.ToUInt32(bytes, 8) == expected.ClassId &&
                    bytes.SequenceEqual(_payload.AsSpan(expected.PropertyOffset, expected.PropertyBytes).ToArray()),
                    "Native scalar descriptor stream differs from the pinned original payload.");
                hash = Convert.ToHexString(SHA256.HashData(bytes));
            }
            else
            {
                byte[] actualPath = Read(descriptor + 0x2C, 0x100);
                byte[] expectedPath = _payload.AsSpan(expected.Offset + 4, 0x100).ToArray();
                int actualNul = Array.IndexOf(actualPath, (byte)0), expectedNul = Array.IndexOf(expectedPath, (byte)0);
                bool logicalMatch = actualNul >= 0 && actualNul == expectedNul &&
                    actualPath.AsSpan(0, actualNul + 1).SequenceEqual(expectedPath.AsSpan(0, expectedNul + 1));
                _log(new { @event = "template-native-asset-path", descriptor = Hex(descriptor), id = expected.Id,
                    nativePathAddress = Hex(descriptor + 0x2C), sourcePathOffset = expected.Offset + 4,
                    boundedFieldBytes = 256, actualFullHex = Convert.ToHexString(actualPath), expectedFullHex = Convert.ToHexString(expectedPath),
                    actualNulOffset = actualNul, expectedNulOffset = expectedNul,
                    actualPath = actualNul >= 0 ? System.Text.Encoding.Latin1.GetString(actualPath, 0, actualNul) : null,
                    expectedPath = expectedNul >= 0 ? System.Text.Encoding.Latin1.GetString(expectedPath, 0, expectedNul) : null,
                    actualPaddingHex = actualNul >= 0 ? Convert.ToHexString(actualPath.AsSpan(actualNul + 1)) : null,
                    expectedPaddingHex = expectedNul >= 0 ? Convert.ToHexString(expectedPath.AsSpan(expectedNul + 1)) : null,
                    rawFieldMatches = actualPath.SequenceEqual(expectedPath), logicalPathMatches = logicalMatch,
                    comparison = "exact-bytes-through-first-NUL; native5FCF00..5FCF08 does not copy following padding" });
                // Binary reader5FCEF2 selects descriptor+2C, then5FCF00..08
                // copies the source C string including NUL, not all256 bytes.
                // Preserve/report the untouched tail without imposing disk padding.
                Require(logicalMatch, "Native bounded NUL-terminated body asset path differs from the pinned descriptor.");
            }
            rows.Add(new { descriptor = Hex(descriptor), id = expected.Id, type = expected.Type, classId = Hex(expected.ClassId), propertySha256 = hash });
            descriptor = U(descriptor + 0x18);
        }
        Require(descriptor == 0, "Parsed descriptor chain exceeds six stock entries.");
        _log(new { @event = "template-native-parse-witness", template = Hex(_template), referenceCount = _templateRefs,
            descriptorCount = 6, descriptors = rows, parseContext = Hex(U(_templateManager + 0x20)) });
    }

    private void CaptureInstance(bool initialized = false, bool allowSecondary = false)
    {
        VerifyLocal(); Table(_instance, 0x710F80);
        _log(new { @event = "template-create-return-prefix", instance = Hex(_instance), template = Hex(_template),
            prefixHex = Convert.ToHexString(Read(_instance, 0x28)), ownBuildWitnesses = _buildWitnesses, initialized,
            ownNotify1D = _ownNotify1D, ownCharacterInit = _ownCharacterInit });
        Require(_instance != _record && U(_instance + 0x14) == _template && U(_instance + 0x20) == 6,
            "Constructed native instance identity/template/count mismatch.");
        _instanceRefs = Refs(_instance); Require(_instanceRefs == 0, "Fresh instance has unqualified incoming owners.");
        _ownRoot = Pointer(_instance + 0x24); Table(_ownRoot, VisualGraphReader.NodeVtable);
        Require(!_localNodes.Contains(_ownRoot) && U(_ownRoot + 0x2C) == _parent && U(_ownRoot + 0x3C) == _scene,
            "Constructed root scene/parent/identity mismatch.");
        _log(new { @event = "template-create-return-root", root = Hex(_ownRoot), referenceCount = Refs(_ownRoot),
            prefixHex = Convert.ToHexString(Read(_ownRoot, 0xB4)) });
        List<Component> items = Components(_instance, 6);
        _log(new { @event = "template-create-return-components", instance = Hex(_instance), components = items.ToArray() });
        uint Unique(uint id) => items.Single(v => v.ClassId == id).Address;
        _character = Unique(0x0003CC73); _traits = Unique(0x1BC6108E); _machine = Unique(0x78F35E7F);
        _animator = Unique(0xCD2B2876); _move = Unique(0x680838E4);
        _ownEntities = [_character, _traits, _machine, _animator, _move];
        _log(new { @event = "template-create-return-character", character = Hex(_character),
            ownFieldsHex = Convert.ToHexString(Read(_character + 0x124, 0x3C)),
            move160 = Hex(U(_move + 0x160)), animatorActor128 = Hex(U(_animator + 0x128)) });
        Require(items.Count(v => v.ClassId == 0x695C0F65) == 1 && _ownEntities.All(v => U(v + 0x24) == _instance && U(v + 0x18) == _ownRoot),
            "Constructed components lack the real own instance/Node binding.");
        // Instance owns one root reference, scene parent owns one and each of
        // the five stock Entity virtual24 bindings retains that same root.
        Require(Refs(_ownRoot) == 7, "Own root has an unqualified reference-owner count.");
        uint ownCommand = Pointer(_character + 0x130); Table(ownCommand, 0x703090);
        byte[] command = Read(ownCommand, 0x74);
        _log(new { @event = "template-create-return-private-command", command = Hex(ownCommand),
            commandHex = Convert.ToHexString(command), localCommand = Hex(_command) });
        Require(new[] { 4, 8, 12, 16, 20 }.All(o => float.IsFinite(BitConverter.ToSingle(command, o))),
            "Owned command has a nonfinite known float field.");
        Require(U(_character + 0x148) == 1 && ownCommand != _command && U(_character + 0x124) == _machine &&
            U(_character + 0x128) == _animator && U(_character + 0x12C) == _move && U(_character + 0x138) == _traits &&
            U(_character + 0x13C) == 0 && _ownNotify1D == 1 && _ownCharacterInit == 1,
            "Owned selector1/private-command/component setup did not match native typed witnesses.");
        var graph = new VisualGraphReader(_host.ProcessHandle).Read(_ownRoot, _localNodes, inspectSkins: false);
        Require(graph.Success, "Constructed graph is not bounded/disjoint: " + graph.Error);
        Membership membership = Memberships(_ownEntities);
        uint actor = U(_animator + 0x128);
        var witness = new { instance = Hex(_instance), template = Hex(_template), root = Hex(_ownRoot), initialized,
            instanceReferenceCount = _instanceRefs, rootReferenceCount = Refs(_ownRoot), parent = Hex(U(_ownRoot + 0x2C)), scene = Hex(U(_ownRoot + 0x3C)),
            instancePrefixHex = Convert.ToHexString(Read(_instance, 0x28)), components = items.ToArray(),
            characterOwnHex = Convert.ToHexString(Read(_character + 0x124, 0x3C)), privateCommand = Hex(ownCommand),
            commandHex = Convert.ToHexString(command), commandFloatWords = new[] { 4, 8, 12, 16, 20 }.Select(o => FloatWord(command, o)).ToArray(),
            move160 = Hex(U(_move + 0x160)), animatorActor128 = Hex(U(_animator + 0x128)),
            animatorCharacter124 = Hex(U(_animator + 0x124)), machineOwnHex = Convert.ToHexString(Read(_machine + 0x124, 0x24)),
            actorPrefixHex = initialized && actor >= 0x10000 ? Convert.ToHexString(Read(actor, 0x54)) : null,
            nodeCount = graph.Graph.Nodes.Count, manager = membership, ownNotify1D = _ownNotify1D, ownCharacterInit = _ownCharacterInit,
            noSchedulerAbsenceClaim = true, directManualNotify1C = false, scopedNativeInitialization = initialized };
        if (initialized) _initializedWitness = witness; else _constructionWitness = witness;
        _log(new { @event = initialized ? "template-native-initialized-witness" : "template-native-construction-witness", witness });
        if (initialized)
        {
            Relationship[] relationships =
            [
                Compare("scoped-mode", _initialize ? 1u : 0u, 1),
                Compare("private-interval-active", _dependentInterval ? 1u : 0u, 1),
                Compare("Character.Notify1C-count", _ownNotify1C, 1),
                Compare("MoveCtrl.init-count", _ownMoveInit, 1),
                Compare("Animator.init-count", _ownAnimatorInit, 1),
                Compare("MoveCtrl+160.Character", U(_move + 0x160), _character),
                Compare("Animator+124.Character", U(_animator + 0x124), _character),
                Compare("Machine+124.Character", U(_machine + 0x124), _character),
                Compare("Machine+128.AnimationGroup", U(_machine + 0x128), _expectedMachineKind),
                Compare("Machine+12C.private-command", U(_machine + 0x12C), ownCommand),
                Compare("Animator.Actor-readable-pointer", actor >= 0x10000 ? 1u : 0u, 1),
                Compare("Animator.Actor-distinct-from-local", actor != _localActor ? 1u : 0u, 1),
                Compare("Animator.Actor-vtable", actor >= 0x10000 ? U(actor) : 0, 0x703F80)
            ];
            _log(new { @event = "template-scoped-init-relationships", instance = Hex(_instance),
                actor = Hex(actor), localActor = Hex(_localActor), animationGroupSourceOffset = 2669,
                comparisons = relationships, allMatched = relationships.All(value => value.Matched) });
            Require(relationships.All(value => value.Matched),
                "Scoped initialization reciprocal relationships failed: " +
                string.Join(", ", relationships.Where(value => !value.Matched).Select(value => value.Operand)));
            Table(actor, 0x703F80);
        }
        // Admission is observed, not inferred from the lack of explicit Add.
        if (allowSecondary)
            Require(_initialize && _dependentInterval && membership.MainListMatches.Length == 0 && membership.TableMatches.Length == 0 &&
                membership.SecondaryListMatches.Order().SequenceEqual(_ownEntities.Order()),
                "Scoped lifecycle has unexpected own manager admission before gameplay resume.");
        else
            Require(membership.Matches.Length == 0, "Own components were admitted to game-manager tables during construction; stop before an unqualified update.");
    }

    private void VerifyOwnedInstance()
    {
        Table(_instance, 0x710F80); Require(U(_instance + 0x14) == _template && U(_instance + 0x24) == _ownRoot &&
            U(_ownRoot + 0x2C) == _parent && U(_ownRoot + 0x3C) == _scene && U(_instance + 0x20) == 6 &&
            Refs(_ownRoot) == 7 && Memberships(_ownEntities).Matches.Length == 0,
            "Own instance scene/count/scheduling changed before deletion.");
    }

    private List<Component> Components(uint record, uint count)
    {
        uint head = Pointer(record + 0x1C), link = Pointer(head), previous = head;
        var links = new HashSet<uint>(); var addresses = new HashSet<uint>(); var result = new List<Component>();
        for (uint i = 0; i < count; ++i)
        {
            Require(link != head && links.Add(link) && U(link + 4) == previous, "Instance component list is not distinct/reciprocal.");
            uint item = Pointer(link + 8); Require(addresses.Add(item) && !_localNodes.Contains(item), "Instance component aliases another object.");
            uint id = ClassId(item); bool node = id == 0x695C0F65;
            Require(_descriptors.Count(d => d.ClassId == id) == 1, "Unexpected instance component class.");
            result.Add(new(item, U(item), id, node ? 0 : U(item + 0x24), node ? U(item + 0x2C) : U(item + 0x18)));
            uint next = Pointer(link); Require(U(next + 4) == link, "Instance component forward/back link mismatch.");
            previous = link; link = next;
        }
        Require(link == head && U(head + 4) == previous, "Instance list count/sentinel mismatch.");
        return result;
    }

    private Membership Memberships(uint[] own)
    {
        Table(_gameManager, 0x70069C);
        byte[] tables = Read(_gameManager + 0x54, 0x5280);
        uint[] counts = Enumerable.Range(0, 40).Select(i => BitConverter.ToUInt32(tables, i * 4)).ToArray();
        Require(counts.All(v => v <= 130), "Unqualified game-manager group count.");
        byte[] pending = Read(_gameManager + 0x52D4, 200 * 4), move = Read(_gameManager + 0x55F8, 130 * 4);
        uint moveCount = U(_gameManager + 0x5918); Require(moveCount <= 130, "Unqualified MoveCtrl manager count.");
        var values = new HashSet<uint>();
        foreach (byte[] block in new[] { tables.AsSpan(0x140, 40 * 130 * 4).ToArray(), pending, move })
            for (int i = 0; i < block.Length; i += 4) values.Add(BitConverter.ToUInt32(block, i));
        uint mainCount = U(_gameManager + 0x1C), secondaryCount = U(_gameManager + 0x28);
        uint[] main = ManagerList(Pointer(_gameManager + 0x18), mainCount, own);
        uint[] secondary = ManagerList(Pointer(_gameManager + 0x24), secondaryCount, own);
        uint[] tableMatches = own.Where(values.Contains).ToArray();
        return new(counts, moveCount, mainCount, secondaryCount, main, secondary, tableMatches,
            main.Concat(secondary).Concat(tableMatches).Distinct().ToArray(), Convert.ToHexString(SHA256.HashData(tables)));
    }

    private uint[] ManagerList(uint head, uint count, uint[] own)
    {
        Require(count <= 4096, "Unqualified game-manager list count.");
        uint link = Pointer(head), previous = head;
        var visited = new HashSet<uint>(); var matches = new HashSet<uint>();
        for (uint i = 0; i < count; ++i)
        {
            Require(link != head && visited.Add(link) && U(link + 4) == previous, "Game-manager list is not bounded/reciprocal.");
            uint item = Pointer(link + 8); if (own.Contains(item)) matches.Add(item);
            uint next = Pointer(link); Require(U(next + 4) == link, "Game-manager next/back link mismatch.");
            previous = link; link = next;
        }
        Require(link == head && U(head + 4) == previous, "Game-manager list count/sentinel mismatch.");
        return matches.ToArray();
    }

    private bool OwnReceiver(uint receiver) => (_constructing != 0 || _instance != 0) && receiver >= 0x10000 &&
        U(receiver + 0x24) == (_constructing != 0 ? _constructing : _instance);
    private void Foreign(string name, uint thread, uint receiver, uint? code)
    {
        if (++_foreignWitnesses <= 24)
            _log(new { @event = "template-foreign-native-witness", name, threadId = thread, receiver = Hex(receiver), code });
    }
    private void Hook(uint entry, string name, BreakpointCallback callback)
    {
        VerifyFunction(entry);
        BreakpointCallback repeat = null!;
        repeat = (uint tid, uint at, ref Win32Native.X86Context ctx) =>
        {
            Require(tid == _host.MainThreadId && at == entry && ctx.Eip == entry, "Template witness stop/thread identity mismatch.");
            callback(tid, at, ref ctx);
            _host.Breakpoints.AddReturn(entry, tid, name, repeat);
        };
        _host.Breakpoints.AddReturn(entry, _host.MainThreadId, name, repeat);
    }
    private void Call(uint tid, ref Win32Native.X86Context ctx, NativeCallDispatcher calls, Action resume,
        uint function, uint receiver, uint[] args, bool cdecl, Action<uint> completed)
    {
        VerifyFunction(function);
        calls.Begin(_host, tid, ref ctx, function, receiver, args, cdecl, value =>
        { completed(value); VerifyLocal(); resume(); });
    }
    private void VerifyFunction(uint function)
    {
        Require(function is >= 0x401000 and < 0x6DA000, "Native callable is outside the pinned ordinary code image.");
        byte[] expected = _image.GetSectionData(checked((int)(function - 0x400000))).GetContent(0, 6).ToArray();
        Require(Read(function, 6).SequenceEqual(expected), $"Pinned native signature mismatch at {Hex(function)}.");
        _log(new { @event = "template-native-signature", address = Hex(function), sourceBytes6 = Convert.ToHexString(expected) });
    }
    private uint ClassId(uint pointer)
    {
        uint table = Pointer(pointer), getter = Pointer(table + 0x10);
        Require(getter is >= 0x401000 and < 0x6DA000, "Unqualified RTTI getter pointer.");
        byte[] code = _image.GetSectionData(checked((int)(getter - 0x400000))).GetContent(0, 6).ToArray();
        Require(code[0] == 0xB8 && code[5] == 0xC3 && Read(getter, 6).SequenceEqual(code), "RTTI getter is not the pinned simple record accessor.");
        return U(BitConverter.ToUInt32(code, 1));
    }
    private void Table(uint pointer, uint expected) => Require(pointer >= 0x10000 && U(pointer) == expected, $"Native table mismatch at {Hex(pointer)}; expected {Hex(expected)}.");
    private ushort Refs(uint pointer) => BitConverter.ToUInt16(Read(pointer + 8, 2));
    private uint Pointer(uint address) { uint value = U(address); Require(value >= 0x10000, $"Null/low pointer from {Hex(address)}."); return value; }
    private uint U(uint address) => BitConverter.ToUInt32(Read(address, 4));
    private byte[] Read(uint address, int length)
    {
        Require(_host.IsAtDebugStop && _host.ProcessHandle == _process && _host.ProcessPid == _pid &&
            Environment.CurrentManagedThreadId == _ownerThread && address >= 0x10000 && length is >= 1 and <= 65536,
            "Template memory read requires the bounded owned debug stop.");
        _ = checked(address + (uint)length - 1);
        return Win32Native.ReadMemory(_host.ProcessHandle, address, length);
    }
    private static object FloatWord(byte[] bytes, int offset)
    {
        float value = BitConverter.ToSingle(bytes, offset);
        return new { offset = Hex((uint)offset), bits = Hex(BitConverter.ToUInt32(bytes, offset)), finite = float.IsFinite(value), value = float.IsFinite(value) ? (float?)value : null };
    }
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidDataException(message); }
    private static Relationship Compare(string name, uint actual, uint expected) => new(name, Hex(actual), Hex(expected), actual == expected);
    private static string Hex(uint value) => $"0x{value:X8}";
}
