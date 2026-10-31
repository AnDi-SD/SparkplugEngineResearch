using System.Security.Cryptography;
using SmoNativeValidator.Core;

namespace WinxCoop.RuntimeProbe;

/// <summary>
/// Own bounded hair ownership around the qualified native SMO load and Attach.
/// The existing template Animator discovers the attached hair; no second Actor
/// or manual animation update is created. Dependent callbacks only read game memory.
/// </summary>
internal sealed partial class NativeTemplateProbe
{
    private const string HairAssetSha256 = "AF68FF609294648CB817497868BB6D0F222863F76D62AC19CA9E39D480702654";
    private bool _hairEnabled, _hairPrepared, _hairSourceOwner, _hairAttached, _hairActorBound, _hairResidualDeleted;
    private string _hairAssetPath = "";
    private uint _hairSource, _hairMaster, _hairHead, _hairResidualLight;
    private OwnedRemoteBuffer? _hairData;
    private object? _hairReleasedData, _hairAttachmentWitness, _hairActorWitness;
    private uint[] _hairOwnedNodes = [], _hairOwnedSkins = [];

    private bool HairHasLiveOwnership => _hairSource != 0 || _hairSourceOwner || _hairData is { Released: false };
    private bool HairModeSucceeded => !_hairEnabled || (_hairPrepared && _hairAttached && _hairActorBound &&
        _hairResidualDeleted && !HairHasLiveOwnership);
    private object NativeTemplateHairSummary => new { enabled = _hairEnabled, prepared = _hairPrepared,
        source = Hex(_hairSource), sourceOwner = _hairSourceOwner, masterHistorical = Hex(_hairMaster), headHistorical = Hex(_hairHead),
        attached = _hairAttached, actorBound = _hairActorBound, residualSourceDeleted = _hairResidualDeleted,
        ownNodeCount = _hairOwnedNodes.Length, ownSkinCount = _hairOwnedSkins.Length,
        data = _hairData?.Summary ?? _hairReleasedData, attachment = _hairAttachmentWitness, actor = _hairActorWitness,
        assetSha256 = HairAssetSha256, secondActorCreated = false, manualAnimationCalls = 0 };

    private void ConfigureNativeTemplateHair(ProbeConfiguration config)
    {
        _hairEnabled = config.Mode is "template-bloom-body-hair-init" or "template-bloom-body-hair-idle" or "template-bloom-body-hair-live" or
            "template-bloom-body-hair-walk" or "template-bloom-body-hair-turn" or "template-bloom-body-hair-jump";
        if (!_hairEnabled) return;
        Require(_layout.IsBloomBody, "Hair attachment requires the exact Bloom body fixture.");
        _hairAssetPath = Path.Combine(config.MediaDirectory, "Characters", "Bloom", "bloom_hair.smo");
        Require(Path.IsPathFullyQualified(_hairAssetPath) && _hairAssetPath.All(character => character is >= ' ' and <= '~'),
            "Hair asset requires the explicit ASCII isolated Media path.");
        byte[] bytes = File.ReadAllBytes(_hairAssetPath);
        Require(bytes.Length == 56109 && Convert.ToHexString(SHA256.HashData(bytes)) == HairAssetSha256,
            "Hair asset differs from the pinned original input.");
    }

    private bool TryPrepareNativeTemplateHair(uint tid, ref Win32Native.X86Context ctx, NativeCallDispatcher calls, Action resume)
    {
        if (!_hairEnabled || _hairPrepared) return false;
        Require(_stage == 5 && !_dependentInterval && _instance == 0 && _constructing == 0 && !calls.Pending,
            "Hair preparation requires the world stop before template Create.");
        if (_hairSource == 0)
        {
            Require(_hairData is null, "Hair load already owns an uncompleted data buffer.");
            _hairData = new OwnedRemoteBuffer(_host, 1024);
            uint name = _hairData.WriteAsciiZ(0, _hairAssetPath);
            // Same captured singleton operand as the qualified A/P SMO loader;
            // no unverified manager vtable/layout is inferred here.
            uint sceneManager = Pointer(0x75DB78);
            Call(tid, ref ctx, calls, resume, 0x458260, sceneManager, [name], false, value =>
            {
                Require(value >= 0x10000 && !_localNodes.Contains(value), "Fresh hair SMO load failed or aliases local nodes.");
                _hairSource = value;
                _log(new { @event = "template-hair-source-loaded", source = Hex(value), assetSha256 = HairAssetSha256,
                    originalInstanceNotCreated = true, secondActorCreated = false });
            });
            return true;
        }
        VisualGraphReadResult hair = new VisualGraphReader(_host.ProcessHandle).Read(_hairSource, _localNodes, inspectSkins: true);
        Require(hair.Success && U(_hairSource + 0x2C) == 0 && U(_hairSource + 0x3C) == 0 && Refs(_hairSource) == 0,
            "Fresh hair graph/ownership rejected: " + hair.Error);
        var nodes = hair.Graph.Nodes.ToDictionary(node => node.Address);
        _hairMaster = UniqueHairNamedNode(hair.Graph, "Hair_Master");
        _hairResidualLight = UniqueHairNamedNode(hair.Graph, "Ambient01");
        VisualNodeRead residualLight = nodes[_hairResidualLight];
        Require(hair.Graph.Nodes.Count == 21 && nodes[_hairSource].Children.Count == 2 &&
            nodes[_hairSource].Children.Contains(_hairMaster) && nodes[_hairSource].Children.Contains(_hairResidualLight) &&
            HairIdentity(nodes[_hairSource].LocalTransform) && residualLight.Kind == NativeVisualNodeKind.DXLight &&
            residualLight.Parent == _hairSource && residualLight.Scene == 0 && residualLight.LightType == 3 &&
            residualLight.Children.Count == 0 && residualLight.Collisions.Items.Count == 0,
            "Hair source does not match its qualified identity root, Hair_Master and detached Ambient01.");
        var owned = new HashSet<uint>(); var pending = new Stack<uint>(); pending.Push(_hairMaster);
        while (pending.Count != 0)
        {
            uint address = pending.Pop(); Require(owned.Add(address), "Hair subtree repeats a node.");
            VisualNodeRead node = nodes[address];
            Require(node.Scene == 0 && node.Collisions.Items.Count == 0 &&
                node.Kind is not (NativeVisualNodeKind.Light or NativeVisualNodeKind.LightData or NativeVisualNodeKind.DXLight or NativeVisualNodeKind.AudioSound) &&
                (node.Flags & 0x2000) == 0, "Fresh hair subtree already owns a nonvisual/Actor relation.");
            foreach (uint child in node.Children) pending.Push(child);
        }
        Require(owned.Count == 19 && !owned.Contains(_hairResidualLight) && hair.Graph.Skins.Count == 1 &&
            hair.Graph.Skins.All(skin => skin.BoneCount > 0 && skin.BoneNodes.All(owned.Contains)) &&
            hair.Graph.Nodes.Where(node => node.Kind == NativeVisualNodeKind.RenderNode).All(node => owned.Contains(node.Address)),
            "Hair Skin/render/skeleton owners extend outside Hair_Master.");
        _hairOwnedNodes = owned.ToArray(); _hairOwnedSkins = hair.Graph.Skins.Select(skin => skin.Address).ToArray();
        // Same explicit single adapter owner as the accepted A/P hair loader.
        SetHairSourceReferences(1); _hairSourceOwner = true; _hairPrepared = true;
        _log(new { @event = "template-hair-source-prepared", source = Hex(_hairSource), master = Hex(_hairMaster),
            incomingReferenceCount = Refs(_hairSource), ownNodes = _hairOwnedNodes.Select(Hex).ToArray(),
            ownSkins = _hairOwnedSkins.Select(Hex).ToArray(), graph = hair.Graph,
            adapterRetainsExactlyOneSourceOwner = true, originalInstanceNotCreated = true });
        return true;
    }

    private NativePreInitAttachmentPlan NativeTemplateHairAttachmentPlan()
    {
        Require(_hairEnabled && _hairPrepared && _hairSourceOwner && !_hairAttached && !_hairResidualDeleted,
            "Hair attachment requires one prepared detached source owner.");
        VerifyFunction(0x421A60);
        return new(0x421A60, _hairMaster, SelectNativeTemplateHairParent, ObserveNativeTemplateHairAttached);
    }

    private uint SelectNativeTemplateHairParent(uint instance)
    {
        Require(_dependentInterval && _stage == 6 && instance == _instance && _hairPrepared && _hairSourceOwner &&
            !_hairAttached && Refs(_hairSource) == 1 && U(_hairMaster + 0x2C) == _hairSource,
            "Hair parent selection escaped the created private checkpoint.");
        VisualGraphReadResult body = new VisualGraphReader(_host.ProcessHandle).Read(_ownRoot,
            _localNodes.Concat(_hairOwnedNodes).Append(_hairSource).ToHashSet(), inspectSkins: true);
        Require(body.Success, "Body graph before hair attachment rejected: " + body.Error);
        _hairHead = UniqueHairNamedNode(body.Graph, "Head");
        Require(U(_animator + 0x128) == 0 && _ownAnimatorInit == 0, "Hair must be attached before this Animator creates its Actor.");
        _log(new { @event = "template-hair-parent-selected", instance = Hex(instance), head = Hex(_hairHead),
            master = Hex(_hairMaster), actorNotCreated = true, checkpointGameMemoryReadOnly = true });
        return _hairHead;
    }

    private void ObserveNativeTemplateHairAttached(uint instance, uint parent)
    {
        Require(_dependentInterval && _stage == 6 && instance == _instance && parent == _hairHead &&
            _ownAnimatorInit == 0 && U(_animator + 0x128) == 0 && U(_hairMaster + 0x2C) == _hairHead &&
            Refs(_hairSource) == 1 && U(_hairSource + 0x2C) == 0,
            "Native pre-init Attach did not preserve own hair/source/Animator identities.");
        VisualGraphReadResult source = new VisualGraphReader(_host.ProcessHandle).Read(_hairSource, _localNodes, inspectSkins: true);
        Require(IsQualifiedHairResidual(source),
            "Native Attach did not leave the qualified detached root and unused Ambient01.");
        _hairAttached = true;
        VisualGraphReadResult combined = new VisualGraphReader(_host.ProcessHandle).Read(_ownRoot, _localNodes, inspectSkins: true);
        Require(combined.Success, "Combined pre-init body/hair graph rejected: " + combined.Error);
        _hairAttachmentWitness = ObserveNativeTemplateHairBinding(combined.Graph, initialized: false, actor: 0);
        _log(new { @event = "template-hair-native-attached", witness = _hairAttachmentWitness,
            sourceRemainingCount = source.Graph.Nodes.Count, sourceIncomingReferences = Refs(_hairSource),
            checkpointGameMemoryReadOnly = true, originalAnimatorInitializationStillPending = true });
    }

    private object ObserveNativeTemplateHairBinding(VisualGraphSnapshot graph, bool initialized, uint actor)
    {
        Require(_hairEnabled && _hairPrepared && _layout.IsBloomBody, "Hair graph observation requires its prepared body cohort.");
        var nodes = graph.Nodes.ToDictionary(node => node.Address);
        if (!_hairAttached)
        {
            Require(!initialized && _hairOwnedNodes.All(address => !nodes.ContainsKey(address)),
                "Unattached hair unexpectedly appears inside the body graph.");
            return new { attached = false, initialized, hairNodeCount = 0, actorBound = false };
        }
        Require(_hairOwnedNodes.All(nodes.ContainsKey) && !nodes.ContainsKey(_hairSource) &&
            UniqueHairNamedNode(graph, "Head") == _hairHead && UniqueHairNamedNode(graph, "Hair_Master") == _hairMaster &&
            nodes[_hairMaster].Parent == _hairHead && _hairOwnedSkins.All(address => graph.Skins.Any(skin => skin.Address == address)),
            "Combined graph does not retain the exact own Head/Hair_Master/Skin graph.");
        var animated = _hairOwnedNodes.Select(address => nodes[address]).Where(node => (node.Flags & 0x800) != 0).ToArray();
        if (initialized)
        {
            Table(actor, 0x703F80);
            Require(actor == U(_animator + 0x128) && actor != _localActor && animated.Length > 0 &&
                animated.All(node => node.NamePointer != 0 && (node.Flags & 0x2000) != 0),
                "Native Animator did not discover the named animated hair nodes in its own Actor.");
            _hairActorBound = true;
        }
        object witness = new { attached = true, initialized, sourceHistorical = Hex(_hairSource), head = Hex(_hairHead), master = Hex(_hairMaster),
            ownHairNodeCount = _hairOwnedNodes.Length, ownHairSkinCount = _hairOwnedSkins.Length,
            animatedHairNodes = animated.Select(node => new { address = Hex(node.Address), flags = Hex(node.Flags) }).ToArray(),
            actor = Hex(actor), sameTemplateAnimator = true, secondActorCreated = false,
            actorDiscoveryFlagsVerified = initialized, skinBonesRemainInCombinedOwnGraph = true };
        if (initialized) _hairActorWitness = witness;
        return witness;
    }

    private bool TryDeleteNativeTemplateHairResidual(uint tid, ref Win32Native.X86Context ctx,
        NativeCallDispatcher calls, Action resume, bool afterFirstOriginalWorld)
    {
        if (!_hairEnabled || _hairResidualDeleted) return false;
        Require(_hairPrepared && _hairAttached && _hairActorBound && _hairSourceOwner && _hairSource != 0 &&
            !_dependentInterval && !calls.Pending && _hairData is { Released: false }, "Hair residual cleanup lacks its completed private dependency/owner.");
        if (afterFirstOriginalWorld)
            Require(_stage == 6 && _idleActive && _idleWorldStops == 1 && _ownCharacterTicks == 1 && _ownMoveUpdates == 1 &&
                _ownBloomNotifyTicks == 1 && _ownBloomUpdates == 1 && _bloomIntervalVerified,
                "Retained hair source cleanup must follow the first actual counted original World.");
        else
            Require(!_idle && _stage == 7 && _instance == 0 && _ownRoot == 0 && _bloomIntervalVerified,
                "Init-only hair source cleanup must follow the scoped native instance deletion.");
        VisualGraphReadResult source = new VisualGraphReader(_host.ProcessHandle).Read(_hairSource, _localNodes, inspectSkins: true);
        Require(IsQualifiedHairResidual(source) &&
            Refs(_hairSource) == 1 && _hairSourceOwner, "Hair residual source is not an isolated empty single-owner root.");
        uint deleting = Pointer(Pointer(_hairSource)); VerifyFunction(deleting);
        SetHairSourceReferences(0); _hairSourceOwner = false;
        uint removed = _hairSource;
        Call(tid, ref ctx, calls, resume, deleting, removed, [1], false, _ =>
        {
            Require(!calls.Pending, "Hair data may not be released during a native borrower.");
            _hairSource = 0; _hairResidualDeleted = true;
            OwnedRemoteBuffer data = _hairData ?? throw new InvalidDataException("Missing owned hair data after native source deletion.");
            data.Release(calls.Pending); _hairReleasedData = data.Summary; _hairData = null;
            _log(new { @event = "template-hair-residual-source-deleted", deletedSource = Hex(removed),
                afterFirstOriginalWorld, stagePreserved = _stage, originalWorldReturns = _idleWorldStops,
                sourceAdapterOwnerReleased = true, data = _hairReleasedData, freedBytesRead = false });
        });
        return true;
    }

    private void SetHairSourceReferences(ushort count)
    {
        Require(_host.IsAtDebugStop && _hairSource >= 0x10000 && !_localNodes.Contains(_hairSource) && count <= 1,
            "Hair reference mutation requires the stopped own detached source.");
        byte[] bytes = BitConverter.GetBytes(count);
        Win32Native.WriteMemory(_host.ProcessHandle, _hairSource + 8, bytes, executable: false);
        Require(Read(_hairSource + 8, 2).SequenceEqual(bytes), "Hair source reference write/readback mismatch.");
    }
    private bool IsQualifiedHairResidual(VisualGraphReadResult result)
    {
        if (!result.Success || result.Graph.Root != _hairSource || result.Graph.Nodes.Count != 2 || result.Graph.Skins.Count != 0)
            return false;
        var nodes = result.Graph.Nodes.ToDictionary(node => node.Address);
        if (!nodes.TryGetValue(_hairSource, out VisualNodeRead? root) ||
            !nodes.TryGetValue(_hairResidualLight, out VisualNodeRead? light)) return false;
        return root.Parent == 0 && root.Scene == 0 && root.Children.Count == 1 && root.Children[0] == _hairResidualLight &&
            light.Parent == _hairSource && light.Scene == 0 && light.Kind == NativeVisualNodeKind.DXLight &&
            light.LightType == 3 && light.Children.Count == 0 && light.Collisions.Items.Count == 0;
    }
    private uint UniqueHairNamedNode(VisualGraphSnapshot graph, string name)
    {
        uint[] matches = graph.Nodes.Where(node => node.NamePointer != 0 &&
            Win32Native.ReadAnsiString(_host.ProcessHandle, checked(node.NamePointer + 9), 128) == name).Select(node => node.Address).ToArray();
        Require(matches.Length == 1, "Expected one own named node: " + name); return matches[0];
    }
    private static bool HairIdentity(VisualNodeTransform transform) => transform.Position.All(value => value == 0) &&
        transform.Scale.All(value => value == 1) && transform.Orientation.Select((value, index) => value == (index is 0 or 4 or 8 ? 1 : 0)).All(value => value);
}
