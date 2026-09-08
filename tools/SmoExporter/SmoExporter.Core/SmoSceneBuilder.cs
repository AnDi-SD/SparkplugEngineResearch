using System.Numerics;
using System.Security.Cryptography;
using SmoViewer.Core;

namespace SmoExporter.Core;

public static class SmoSceneBuilder
{
    public static SmoExportScene Build(
        SmoDocument document,
        SmoExportOptions? options = null)
    {
        ArgumentNullException.ThrowIfNull(document);
        options ??= new SmoExportOptions();
        SmoExportResourceTypes resources = options.Resources;
        ValidateResources(resources);
        bool includeMeshes = Includes(resources, SmoExportResourceTypes.Meshes);
        bool includeSkeleton = Includes(resources, SmoExportResourceTypes.Skeleton);
        bool includeMaterials = Includes(resources, SmoExportResourceTypes.Materials);
        bool includeTextures = Includes(resources, SmoExportResourceTypes.Textures);
        bool includeAnimations = Includes(resources, SmoExportResourceTypes.Animations);
        bool includeServiceNodes = Includes(resources, SmoExportResourceTypes.ServiceNodes);
        ValidateSceneMode(options.SceneMode, options.SelectedMeshObjectIndices);

        var warnings = new List<string>();
        var meshes = new List<SmoExportMesh>();
        var exportTextures = new Dictionary<int, SmoExportTexture>();
        SmoExportTexture GetExportTexture(SmoTexture texture)
        {
            if (!exportTextures.TryGetValue(texture.ObjectIndex, out SmoExportTexture? exported))
            {
                exported = BuildExportTexture(texture);
                exportTextures.Add(texture.ObjectIndex, exported);
            }
            return exported;
        }
        var meshPlacements = new List<SmoExportMeshPlacement>();
        IReadOnlyDictionary<int, SmoTextureBinding> materialBindings =
            includeMaterials
                ? SmoTextureBindingResolver.ResolveAll(document)
                : new Dictionary<int, SmoTextureBinding>();
        IReadOnlyDictionary<int, uint> materialColors =
            includeMaterials
                ? SmoMaterialColorResolver.ResolveAll(document)
                : new Dictionary<int, uint>();
        IReadOnlyDictionary<int, uint> materialFlags =
            includeMaterials
                ? SmoMaterialRenderState.ResolveAll(document)
                : new Dictionary<int, uint>();
        Dictionary<int, SmoSkin> decodedSkins = [];
        Dictionary<int, string> skinDecodeErrors = [];
        Dictionary<int, Matrix4x4> nodeWorld = [];
        List<SmoExportNode> allNodes = [];
        List<SmoExportNode> nodes = [];
        List<SmoExportSkin> skins = [];
        if (includeSkeleton)
        {
            SmoNodeHierarchy hierarchy = SmoNodeHierarchy.Decode(document);
            IReadOnlyDictionary<int, Matrix4x4> bindWorld =
                SmoSkinBindingResolver.ResolveBindWorldMatrices(document);
            foreach (SmoObjectEntry skinEntry in document.Objects.Where(
                         entry => entry.TypeHash == SmoClassIds.Skin))
            {
                if (SmoSkinDecoder.TryDecode(
                        document, skinEntry, out SmoSkin? skin, out string skinError) &&
                    skin is not null)
                {
                    decodedSkins[skin.ObjectIndex] = skin;
                }
                else
                {
                    skinDecodeErrors[skinEntry.Index] = skinError;
                    warnings.Add(
                        $"Skin [{skinEntry.Index}] {skinEntry.Name}: {skinError}");
                }
            }
            nodeWorld = BuildNodeWorldMatrices(document, hierarchy, bindWorld);
            allNodes = BuildExportNodes(document, hierarchy, nodeWorld);
            skins = decodedSkins.Values.Select(skin => new SmoExportSkin(
                skin.ObjectIndex,
                skin.Name,
                skin.Bones.Select(bone => bone.NodeObjectIndex).ToArray(),
                skin.Bones.Select(bone =>
                    SmoExportCoordinateSystem.ToExportMatrix(
                        bone.InverseBindMatrix)).ToArray())).ToList();
        }

        IEnumerable<SmoObjectEntry> meshEntries = includeMeshes
            ? document.Objects.Where(item => item.TypeHash == SmoClassIds.MeshData)
            : Enumerable.Empty<SmoObjectEntry>();
        if (options.SceneMode == SmoExportSceneMode.SeparateMeshes)
        {
            IReadOnlySet<int> selected = options.SelectedMeshObjectIndices!;
            meshEntries = meshEntries.Where(entry => selected.Contains(entry.Index));
        }
        foreach (SmoObjectEntry entry in meshEntries)
        {
            if (!SmoMeshDecoder.TryDecode(document, entry, out SmoMesh? mesh, out string error) ||
                mesh is null)
            {
                warnings.Add(error);
                continue;
            }

            int? skinObjectIndex = includeSkeleton
                ? FindAncestorObjectIndex(document.Objects, entry, SmoClassIds.Skin)
                : null;
            if (includeSkeleton && mesh.HasSkinningData && skinObjectIndex is null)
            {
                throw new InvalidDataException(
                    $"Skinned mesh [{entry.Index}] {entry.Name} has no owning skin object; " +
                    "exporting it as a static mesh would change the model.");
            }
            if (includeSkeleton && mesh.HasSkinningData &&
                skinObjectIndex is int requiredSkinIndex &&
                !decodedSkins.ContainsKey(requiredSkinIndex))
            {
                string detail = skinDecodeErrors.GetValueOrDefault(
                    requiredSkinIndex, "the referenced skin was not decoded");
                throw new InvalidDataException(
                    $"Skinned mesh [{entry.Index}] {entry.Name} requires skin " +
                    $"[{requiredSkinIndex}], but it is unavailable: {detail}. " +
                    "Exporting it as a static mesh would change the model.");
            }
            bool exportSkin = includeSkeleton && mesh.HasSkinningData &&
                              skinObjectIndex is int skinIndex &&
                              decodedSkins.ContainsKey(skinIndex);

            int? parentNodeObjectIndex = null;
            Matrix4x4 world = Matrix4x4.Identity;
            Matrix4x4 local = Matrix4x4.Identity;
            if (options.ApplyWorldTransforms && !exportSkin)
            {
                world = SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, entry);
                int? rigidNodeObjectIndex = includeSkeleton && !mesh.HasSkinningData
                    ? SmoRigidBindingResolver.ResolveAnimationNodeObjectIndex(document, entry)
                    : null;
                if (rigidNodeObjectIndex is int rigidIndex &&
                    nodeWorld.TryGetValue(rigidIndex, out Matrix4x4 parentWorld) &&
                    Matrix4x4.Invert(parentWorld, out Matrix4x4 inverseParent))
                {
                    parentNodeObjectIndex = rigidIndex;
                    local = world * inverseParent;
                }
                else
                {
                    local = world;
                }
            }

            Vector3[] positions = mesh.Positions.Select(value =>
                new Vector3(value.X, value.Y, -value.Z)).ToArray();
            Vector3[] normals = mesh.HasNormals
                ? mesh.Normals.Select(value =>
                {
                    Vector3 transformed = new(value.X, value.Y, -value.Z);
                    return transformed.LengthSquared() > 0.000001f
                        ? Vector3.Normalize(transformed)
                        : Vector3.UnitY;
                }).ToArray()
                : [];
            uint[] triangles = mesh.TriangleIndices.ToArray();
            for (int index = 0; index < triangles.Length; index += 3)
                (triangles[index + 1], triangles[index + 2]) =
                    (triangles[index + 2], triangles[index + 1]);

            SmoExportTexture? texture = null;
            SmoExportTexture? effectTexture = null;
            bool usesAlphaBlend = includeMaterials &&
                materialFlags.TryGetValue(entry.Index, out uint flags) &&
                SmoMaterialRenderState.UsesAlphaBlend(flags);
            SmoTextureBinding? binding = materialBindings.GetValueOrDefault(entry.Index);
            if (includeMaterials && binding is not null)
                usesAlphaBlend |= binding.UsesAlphaBlend;
            if (includeTextures && binding is not null)
            {
                if (binding.Issue is not null)
                    warnings.Add(binding.Issue);
                else if (binding.Texture is not null)
                {
                    SmoTexture source = binding.BaseTexture ?? binding.Texture;
                    texture = GetExportTexture(source);
                    if (binding.BaseTexture is not null)
                    {
                        SmoTexture effect = binding.Texture;
                        effectTexture = GetExportTexture(effect);
                        if (binding.AnimationFrames is { Count: > 1 })
                        {
                            warnings.Add(
                                $"ANIMATED_TEXTURE_FIRST_FRAME_ONLY: Mesh [{entry.Index}] " +
                                $"\"{entry.Name}\" exports the first of " +
                                $"{binding.AnimationFrames.Count} material frames.");
                        }
                    }
                }
            }

            Vector4 materialColor = Vector4.One;
            if (includeMaterials)
            {
                if (materialColors.TryGetValue(entry.Index, out uint argb))
                    materialColor = DecodeArgb(argb);
                else if (binding?.DiffuseArgb is uint inheritedArgb)
                    materialColor = DecodeArgb(inheritedArgb);
            }
            bool hasUniformDiffuse = includeMaterials && mesh.HasDiffuseColors &&
                mesh.DiffuseColorsArgb.Skip(1)
                    .All(color => color == mesh.DiffuseColorsArgb[0]);
            if (texture is null && hasUniformDiffuse)
                materialColor = DecodeArgb(mesh.DiffuseColorsArgb[0]);
            // Some skinned assets serialize an all-zero diffuse channel as a
            // placeholder. glTF COLOR_0 multiplies baseColorTexture, so exporting
            // that placeholder would turn a valid textured model completely black.
            // Keep COLOR_0 only when it carries actual RGB information.
            bool hasRenderableDiffuse = includeMaterials && mesh.HasDiffuseColors &&
                !hasUniformDiffuse &&
                mesh.DiffuseColorsArgb.Any(color => (color & 0x00FFFFFF) != 0);
            Vector4[] colors = hasRenderableDiffuse
                ? mesh.DiffuseColorsArgb.Select(DecodeArgb).ToArray()
                : [];
            // glTF ignores every alpha source while alphaMode remains OPAQUE.
            // Texture atlases may contain unused/service alpha, so they still
            // require the confirmed material blend state above. An explicit
            // material factor or an exported COLOR_0 alpha, however, is already
            // part of this mesh's rendered colour and must enable blending.
            usesAlphaBlend |= materialColor.W < 1f ||
                              colors.Any(color => color.W < 1f);
            var exportMesh = new SmoExportMesh(
                entry.Index,
                entry.Id,
                mesh.Name,
                mesh.Marker,
                mesh.PrimitiveType,
                mesh.VertexFormat,
                mesh.Stride,
                mesh.RuntimeStride,
                positions,
                normals,
                mesh.TextureCoordinates.ToArray(),
                mesh.TextureCoordinates1.ToArray(),
                colors,
                exportSkin ? mesh.BlendWeights.ToArray() : [],
                exportSkin ? mesh.BlendIndices.Select(value => new Vector4(
                    value.X, value.Y, value.Z, value.W)).ToArray() : [],
                triangles,
                texture,
                effectTexture,
                materialColor,
                usesAlphaBlend,
                exportSkin ? skinObjectIndex : null,
                parentNodeObjectIndex,
                SmoExportCoordinateSystem.ToExportMatrix(world),
                SmoExportCoordinateSystem.ToExportMatrix(local));
            meshes.Add(exportMesh);
            meshPlacements.Add(new SmoExportMeshPlacement(
                entry.Index,
                entry.Name,
                entry.Index,
                IsSharedInstance: false,
                StaticObjectIndex: FindAncestorObjectIndex(
                    document.Objects, entry, SmoClassIds.StaticRenderObject),
                MaterialObjectIndex: FindAncestorObjectIndex(
                    document.Objects, entry, SmoClassIds.MaterialData),
                parentNodeObjectIndex,
                exportMesh.BindWorldMatrix,
                exportMesh.BindLocalMatrix));
        }

        if (includeMeshes && options.SceneMode is
            SmoExportSceneMode.All or
            SmoExportSceneMode.LevelWithBakedObjects or
            SmoExportSceneMode.LevelWithInstances)
        {
            Dictionary<int, SmoExportMesh> meshesByObjectIndex = meshes
                .ToDictionary(mesh => mesh.ObjectIndex);
            foreach (SmoSharedMeshInstanceInfo instance in
                     SmoSharedMeshInstanceResolver.ResolveAll(document))
            {
                if (!meshesByObjectIndex.TryGetValue(
                        instance.SourceMeshObjectIndex, out SmoExportMesh? sourceMesh))
                {
                    warnings.Add(
                        $"SHARED_MESH_INSTANCE_SOURCE_MISSING: Model " +
                        $"[{instance.ModelObjectIndex}] {instance.ModelObjectName} references " +
                        $"mesh [{instance.SourceMeshObjectIndex}], but that mesh was not exported.");
                    continue;
                }
                if (sourceMesh.SkinObjectIndex is not null)
                {
                    throw new InvalidDataException(
                        $"Shared level placement [{instance.ModelObjectIndex}] " +
                        $"{instance.ModelObjectName} references skinned mesh " +
                        $"[{sourceMesh.ObjectIndex}] {sourceMesh.Name}. A rigid instance cannot " +
                        "preserve that skin binding without duplicating geometry.");
                }

                Matrix4x4 world = options.ApplyWorldTransforms
                    ? SmoExportCoordinateSystem.ToExportMatrix(
                        instance.WorldTransform)
                    : Matrix4x4.Identity;
                string placementName = string.IsNullOrWhiteSpace(instance.ModelObjectName)
                    ? instance.StaticObjectName
                    : instance.ModelObjectName;
                meshPlacements.Add(new SmoExportMeshPlacement(
                    instance.ModelObjectIndex,
                    placementName,
                    instance.SourceMeshObjectIndex,
                    IsSharedInstance: true,
                    instance.StaticObjectIndex,
                    instance.MaterialObjectIndex,
                    ParentNodeObjectIndex: null,
                    world,
                    world));
            }
        }

        if (includeSkeleton)
        {
            nodes = includeServiceNodes
                ? allNodes
                : FilterServiceNodes(allNodes, skins, meshes);
        }

        List<SmoExportAnimation> animations = includeAnimations
            ? BuildAnimations(options.AnimationPaths, nodes, warnings)
            : [];

        string sourcePath = document.SourcePath ?? "memory.smo";
        string hash = Convert.ToHexString(SHA256.HashData(document.Data.Span));
        return new SmoExportScene(
            sourcePath, hash, document.Header.PlatformMask, resources, options.SceneMode,
            meshes, meshPlacements, nodes, skins, animations, warnings);
    }

    private static bool Includes(
        SmoExportResourceTypes resources,
        SmoExportResourceTypes value) => (resources & value) == value;

    private static void ValidateResources(SmoExportResourceTypes resources)
    {
        if (resources == SmoExportResourceTypes.None ||
            (resources & ~SmoExportResourceTypes.All) != 0)
        {
            throw new ArgumentOutOfRangeException(
                nameof(SmoExportOptions.Resources), resources,
                "Export resources must contain one or more known resource types.");
        }

        if (Includes(resources, SmoExportResourceTypes.Materials) &&
            !Includes(resources, SmoExportResourceTypes.Meshes))
        {
            throw new ArgumentException(
                "Exporting materials requires meshes.",
                nameof(SmoExportOptions.Resources));
        }

        if (Includes(resources, SmoExportResourceTypes.Textures) &&
            !Includes(resources, SmoExportResourceTypes.Materials))
        {
            throw new ArgumentException(
                "Exporting textures requires materials.",
                nameof(SmoExportOptions.Resources));
        }

        if (Includes(resources, SmoExportResourceTypes.Animations) &&
            !Includes(resources, SmoExportResourceTypes.Skeleton))
        {
            throw new ArgumentException(
                "Exporting animations requires a skeleton.",
                nameof(SmoExportOptions.Resources));
        }

        if (Includes(resources, SmoExportResourceTypes.ServiceNodes) &&
            !Includes(resources, SmoExportResourceTypes.Skeleton))
        {
            throw new ArgumentException(
                "Exporting service nodes requires a skeleton.",
                nameof(SmoExportOptions.Resources));
        }
    }

    private static void ValidateSceneMode(
        SmoExportSceneMode mode,
        IReadOnlySet<int>? selectedMeshObjectIndices)
    {
        if (!Enum.IsDefined(mode))
            throw new ArgumentOutOfRangeException(nameof(mode), mode, "Unknown scene mode.");
        if (mode == SmoExportSceneMode.SeparateMeshes &&
            (selectedMeshObjectIndices is null || selectedMeshObjectIndices.Count == 0))
        {
            throw new ArgumentException(
                "Separate mesh export requires at least one selected mesh object index.",
                nameof(SmoExportOptions.SelectedMeshObjectIndices));
        }
    }

    private static List<SmoExportNode> FilterServiceNodes(
        IReadOnlyList<SmoExportNode> nodes,
        IReadOnlyList<SmoExportSkin> skins,
        IReadOnlyList<SmoExportMesh> meshes)
    {
        Dictionary<int, SmoExportNode> nodesByObjectIndex =
            nodes.ToDictionary(node => node.ObjectIndex);
        HashSet<int> requiredObjectIndices = [];

        void AddAncestorClosure(int objectIndex)
        {
            while (nodesByObjectIndex.TryGetValue(objectIndex, out SmoExportNode? node) &&
                   requiredObjectIndices.Add(objectIndex) &&
                   node.ParentObjectIndex is int parentObjectIndex)
            {
                objectIndex = parentObjectIndex;
            }
        }

        foreach (SmoExportSkin skin in skins)
        {
            foreach (int jointObjectIndex in skin.JointObjectIndices)
                AddAncestorClosure(jointObjectIndex);
        }

        foreach (SmoExportMesh mesh in meshes)
        {
            if (mesh.SkinObjectIndex is null &&
                mesh.ParentNodeObjectIndex is int parentNodeObjectIndex)
            {
                AddAncestorClosure(parentNodeObjectIndex);
            }
        }

        return nodes.Where(node => requiredObjectIndices.Contains(node.ObjectIndex)).ToList();
    }

    private static List<SmoExportAnimation> BuildAnimations(
        IReadOnlyList<string>? paths, IReadOnlyList<SmoExportNode> nodes,
        ICollection<string> warnings)
    {
        if (paths is null || paths.Count == 0) return [];
        Dictionary<string, SmoExportNode[]> byName = nodes
            .GroupBy(node => node.Name, StringComparer.Ordinal)
            .ToDictionary(group => group.Key, group => group.ToArray(), StringComparer.Ordinal);
        List<SmoExportAnimation> result = [];
        int keyBudget = SmoAnimationBaker.MaximumOutputKeys;
        foreach (string path in paths.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            if (!SmoAnimationDecoder.TryDecode(path, out SmoAnimationClip? clip, out string error) || clip is null)
            {
                throw new SmoFormatException($"Animation {Path.GetFileName(path)}: {error}");
            }
            int discardedKeys = 0;
            var tracks = new List<SmoExportAnimationTrack>();
            IReadOnlyDictionary<string, SmoAnimationTrack> bound = SmoAnimationBinding.BindByName(clip.Tracks, warnings);
            SmoAnimationTrack[] matchedTracks = bound.Values.Where(track => byName.ContainsKey(track.NodeName)).ToArray();
            float minimumTime = matchedTracks.SelectMany(track => track.Positions.Select(key => key.Time)
                .Concat(track.Rotations.Select(key => key.Time)).Concat(track.Scales.Select(key => key.Time)))
                .DefaultIfEmpty(0).Min();
            float timeOffset = -Math.Min(0, minimumTime);
            foreach (SmoAnimationTrack sourceTrack in matchedTracks)
            {
                SmoExportNode[] targets = byName[sourceTrack.NodeName];
                SmoAnimationTrack sampled = SmoAnimationBaker.Bake(sourceTrack, clip.Duration, ref keyBudget, timeOffset);
                SmoAnimationKey<Vector3>[] positions = SanitizeVectorCurve(sampled.Positions,
                    value => new Vector3(value.X, value.Y, -value.Z), ref discardedKeys);
                SmoAnimationKey<Quaternion>[] rotations = SanitizeQuaternionCurve(sampled.Rotations, ref discardedKeys);
                SmoAnimationKey<Vector3>[] scales = SanitizeVectorCurve(sampled.Scales, value => value, ref discardedKeys);
                if (positions.Length > 0 || rotations.Length > 0 || scales.Length > 0)
                {
                    long additionalKeys = (long)(positions.Length+rotations.Length+scales.Length)*(targets.Length-1);
                    if (additionalKeys > keyBudget)
                        throw new SmoFormatException("SAN duplicate-target expansion exceeds the 500000-key export limit.");
                    keyBudget -= (int)additionalKeys;
                    foreach (SmoExportNode node in targets)
                        tracks.Add(new SmoExportAnimationTrack(
                            node.ObjectIndex, node.Name, positions, rotations, scales));
                    if (targets.Length > 1)
                        warnings.Add($"Animation {Path.GetFileName(path)}: '{sourceTrack.NodeName}' binds to all {targets.Length} matching nodes.");
                }
            }
            if (tracks.Count == 0)
                throw new SmoFormatException($"Animation {Path.GetFileName(path)} has no non-empty tracks matching the exported node names (case sensitive).");
            warnings.Add($"Animation {Path.GetFileName(path)}: sampled from PC SAN curves at {SmoAnimationBaker.FramesPerSecond} fps plus source key boundaries; target interpolation between exported keys is an approximation.");

            if (timeOffset > 0)
            {
                warnings.Add(
                    $"Animation {Path.GetFileName(path)}: shifted key times by " +
                    $"{timeOffset:G9}s so the glTF/FBX timeline starts at zero.");
            }
            if (discardedKeys > 0)
            {
                warnings.Add(
                    $"Animation {Path.GetFileName(path)}: discarded {discardedKeys} " +
                    "non-finite, zero-quaternion, or duplicate-time keys.");
            }
            float maximumTime = tracks
                .SelectMany(EnumerateTrackTimes)
                .DefaultIfEmpty(0)
                .Max();
            float duration = float.IsFinite(clip.Duration)
                ? Math.Max(0, clip.Duration - Math.Min(0, minimumTime))
                : maximumTime;
            duration = Math.Max(duration, maximumTime);
            result.Add(new SmoExportAnimation(
                Path.GetFileNameWithoutExtension(path), duration, tracks));
        }
        return result;
    }

    private static SmoAnimationKey<Vector3>[] SanitizeVectorCurve(
        IReadOnlyList<SmoAnimationKey<Vector3>> keys,
        Func<Vector3, Vector3> convert,
        ref int discardedKeys)
    {
        var valid = new List<SmoAnimationKey<Vector3>>(keys.Count);
        foreach (SmoAnimationKey<Vector3> key in keys)
        {
            Vector3 value = convert(key.Value);
            if (!float.IsFinite(key.Time) || !IsFinite(value))
            {
                discardedKeys++;
                continue;
            }
            valid.Add(new SmoAnimationKey<Vector3>(key.Time, value));
        }
        return SortAndDeduplicate(valid, ref discardedKeys);
    }

    private static SmoAnimationKey<Quaternion>[] SanitizeQuaternionCurve(
        IReadOnlyList<SmoAnimationKey<Quaternion>> keys,
        ref int discardedKeys)
    {
        var valid = new List<SmoAnimationKey<Quaternion>>(keys.Count);
        foreach (SmoAnimationKey<Quaternion> key in keys)
        {
            Quaternion value = new(
                -key.Value.X, -key.Value.Y, key.Value.Z, key.Value.W);
            float lengthSquared = value.LengthSquared();
            if (!float.IsFinite(key.Time) || !IsFinite(value) ||
                !float.IsFinite(lengthSquared) ||
                lengthSquared <= 0.000000000001f)
            {
                discardedKeys++;
                continue;
            }
            valid.Add(new SmoAnimationKey<Quaternion>(
                key.Time, Quaternion.Normalize(value)));
        }
        SmoAnimationKey<Quaternion>[] result =
            SortAndDeduplicate(valid, ref discardedKeys);
        for (int index = 1; index < result.Length; index++)
        {
            if (Quaternion.Dot(result[index - 1].Value, result[index].Value) < 0)
            {
                Quaternion value = result[index].Value;
                result[index] = result[index] with
                {
                    Value = new Quaternion(-value.X, -value.Y, -value.Z, -value.W)
                };
            }
        }
        return result;
    }

    private static SmoAnimationKey<T>[] SortAndDeduplicate<T>(
        IReadOnlyList<SmoAnimationKey<T>> keys,
        ref int discardedKeys)
    {
        SmoAnimationKey<T>[] ordered = keys
            .Select((key, order) => (key, order))
            .OrderBy(item => item.key.Time)
            .ThenBy(item => item.order)
            .Select(item => item.key)
            .ToArray();
        var result = new List<SmoAnimationKey<T>>(ordered.Length);
        foreach (SmoAnimationKey<T> key in ordered)
        {
            if (result.Count > 0 && key.Time == result[^1].Time)
            {
                discardedKeys++;
                continue;
            }
            result.Add(key);
        }
        return result.ToArray();
    }

    private static IEnumerable<float> EnumerateTrackTimes(SmoExportAnimationTrack track) =>
        track.Positions.Select(key => key.Time)
            .Concat(track.Rotations.Select(key => key.Time))
            .Concat(track.Scales.Select(key => key.Time));

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);

    private static bool IsFinite(Quaternion value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) &&
        float.IsFinite(value.Z) && float.IsFinite(value.W);

    private static List<SmoExportNode> BuildExportNodes(
        SmoDocument document, SmoNodeHierarchy hierarchy,
        IReadOnlyDictionary<int, Matrix4x4> worlds)
    {
        List<SmoExportNode> result = [];
        foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                     item.TypeHash is SmoClassIds.Node or SmoClassIds.RenderNode))
        {
            int? parent = GetLogicalParent(entry.Index, document.Objects, hierarchy);
            Matrix4x4 world = worlds.GetValueOrDefault(entry.Index, Matrix4x4.Identity);
            Matrix4x4 local = world;
            if (parent is int parentIndex && worlds.TryGetValue(parentIndex, out Matrix4x4 parentWorld) &&
                Matrix4x4.Invert(parentWorld, out Matrix4x4 inverseParent))
                local = world * inverseParent;
            result.Add(new SmoExportNode(
                entry.Index,
                entry.Name,
                parent,
                SmoExportCoordinateSystem.ToExportMatrix(world),
                SmoExportCoordinateSystem.ToExportMatrix(local)));
        }
        return result;
    }

    private static Dictionary<int, Matrix4x4> BuildNodeWorldMatrices(
        SmoDocument document, SmoNodeHierarchy hierarchy,
        IReadOnlyDictionary<int, Matrix4x4> bindWorld)
    {
        Dictionary<int, Matrix4x4> result = [];
        HashSet<int> resolving = [];
        Matrix4x4 Resolve(int index)
        {
            if (result.TryGetValue(index, out Matrix4x4 cached)) return cached;
            if (bindWorld.TryGetValue(index, out Matrix4x4 bind)) return result[index] = bind;
            if (!resolving.Add(index)) return Matrix4x4.Identity;
            SmoObjectEntry entry = document.Objects[index];
            Matrix4x4 local = SmoNodeTransformDecoder.TryDecode(
                document, entry, out SmoNodeTransform? transform) && transform is not null
                    ? transform.LocalMatrix : Matrix4x4.Identity;
            int? parent = GetLogicalParent(index, document.Objects, hierarchy);
            Matrix4x4 world = parent is int parentIndex && (uint)parentIndex < (uint)document.Objects.Count
                ? local * Resolve(parentIndex) : local;
            resolving.Remove(index);
            return result[index] = world;
        }
        foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                     item.TypeHash is SmoClassIds.Node or SmoClassIds.RenderNode))
            Resolve(entry.Index);
        return result;
    }

    private static int? GetLogicalParent(
        int index, IReadOnlyList<SmoObjectEntry> entries, SmoNodeHierarchy hierarchy) =>
        hierarchy.ParentsByChild.TryGetValue(index, out IReadOnlyList<int>? parents) && parents.Count == 1
            ? parents[0] : entries[index].ParentIndex;

    private static int? FindAncestorObjectIndex(
        IReadOnlyList<SmoObjectEntry> entries, SmoObjectEntry entry, uint typeHash)
    {
        SmoObjectEntry? cursor = entry;
        while (cursor.ParentIndex is int parentIndex && (uint)parentIndex < (uint)entries.Count)
        {
            cursor = entries[parentIndex];
            if (cursor.TypeHash == typeHash) return cursor.Index;
        }
        return null;
    }

    private static SmoExportTexture BuildExportTexture(SmoTexture source)
    {
        byte[]? opacityMask = PngEncoder.EncodeOpacityMaskBgra32(
            source.Width, source.Height, source.Bgra32Pixels.Span);
        return new SmoExportTexture(
            source.ObjectIndex,
            source.Name,
            source.Width,
            source.Height,
            PngEncoder.EncodeBgra32(
                source.Width, source.Height, source.Bgra32Pixels.Span),
            opacityMask,
            opacityMask is null
                ? null
                : PngEncoder.EncodeBgr24(
                    source.Width, source.Height, source.Bgra32Pixels.Span),
            source.Bgra32Pixels);
    }

    private static Vector4 DecodeArgb(uint argb) => new(
        ((argb >> 16) & 0xFF) / 255f,
        ((argb >> 8) & 0xFF) / 255f,
        (argb & 0xFF) / 255f,
        (argb >> 24) / 255f);
}
