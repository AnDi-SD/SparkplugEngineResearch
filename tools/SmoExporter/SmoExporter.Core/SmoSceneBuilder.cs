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

        var warnings = new List<string>();
        var meshes = new List<SmoExportMesh>();
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
                    ReflectMatrix(bone.InverseBindMatrix)).ToArray())).ToList();
        }

        IEnumerable<SmoObjectEntry> meshEntries = includeMeshes
            ? document.Objects.Where(item => item.TypeHash == SmoClassIds.MeshData)
            : Enumerable.Empty<SmoObjectEntry>();
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
                    texture = BuildExportTexture(source);
                    if (binding.BaseTexture is not null)
                    {
                        SmoTexture effect = binding.Texture;
                        effectTexture = BuildExportTexture(effect);
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
            meshes.Add(new SmoExportMesh(
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
                ReflectMatrix(world),
                ReflectMatrix(local)));
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
            sourcePath, hash, document.Header.Version, resources,
            meshes, nodes, skins, animations, warnings);
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
        Dictionary<string, SmoExportNode> byName = nodes
            .GroupBy(node => node.Name, StringComparer.OrdinalIgnoreCase)
            .ToDictionary(group => group.Key, group => group.First(), StringComparer.OrdinalIgnoreCase);
        List<SmoExportAnimation> result = [];
        foreach (string path in paths.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            if (!SmoAnimationDecoder.TryDecode(path, out SmoAnimationClip? clip, out string error) || clip is null)
            {
                warnings.Add($"Animation {Path.GetFileName(path)}: {error}");
                continue;
            }
            int discardedKeys = 0;
            int duplicateChannels = 0;
            var tracks = new List<SmoExportAnimationTrack>();
            foreach (IGrouping<int, SmoAnimationTrack> group in clip.Tracks
                         .Where(track => byName.ContainsKey(track.NodeName))
                         .GroupBy(track => byName[track.NodeName].ObjectIndex))
            {
                SmoExportNode node = nodes.First(item => item.ObjectIndex == group.Key);
                var positionCurves = new List<SmoAnimationKey<Vector3>[]>();
                var rotationCurves = new List<SmoAnimationKey<Quaternion>[]>();
                var scaleCurves = new List<SmoAnimationKey<Vector3>[]>();
                foreach (SmoAnimationTrack sourceTrack in group)
                {
                    SmoAnimationKey<Vector3>[] positionCurve = SanitizeVectorCurve(
                        sourceTrack.Positions,
                        value => new Vector3(value.X, value.Y, -value.Z),
                        ref discardedKeys);
                    SmoAnimationKey<Quaternion>[] rotationCurve = SanitizeQuaternionCurve(
                        sourceTrack.Rotations, ref discardedKeys);
                    SmoAnimationKey<Vector3>[] scaleCurve = SanitizeVectorCurve(
                        sourceTrack.Scales, value => value, ref discardedKeys);
                    if (positionCurve.Length > 0) positionCurves.Add(positionCurve);
                    if (rotationCurve.Length > 0) rotationCurves.Add(rotationCurve);
                    if (scaleCurve.Length > 0) scaleCurves.Add(scaleCurve);
                }

                duplicateChannels += Math.Max(0, positionCurves.Count - 1);
                duplicateChannels += Math.Max(0, rotationCurves.Count - 1);
                duplicateChannels += Math.Max(0, scaleCurves.Count - 1);
                SmoAnimationKey<Vector3>[] positions = positionCurves.FirstOrDefault() ?? [];
                SmoAnimationKey<Quaternion>[] rotations = rotationCurves.FirstOrDefault() ?? [];
                SmoAnimationKey<Vector3>[] scales = scaleCurves.FirstOrDefault() ?? [];
                if (positions.Length > 0 || rotations.Length > 0 || scales.Length > 0)
                {
                    tracks.Add(new SmoExportAnimationTrack(
                        node.ObjectIndex, node.Name, positions, rotations, scales));
                }
            }
            if (tracks.Count == 0)
                continue;

            float minimumTime = tracks
                .SelectMany(EnumerateTrackTimes)
                .DefaultIfEmpty(0)
                .Min();
            if (minimumTime < 0)
            {
                float offset = -minimumTime;
                tracks = tracks.Select(track => ShiftTrack(track, offset)).ToList();
                warnings.Add(
                    $"Animation {Path.GetFileName(path)}: shifted key times by " +
                    $"{offset:G9}s so the glTF/FBX timeline starts at zero.");
            }
            if (discardedKeys > 0)
            {
                warnings.Add(
                    $"Animation {Path.GetFileName(path)}: discarded {discardedKeys} " +
                    "non-finite, zero-quaternion, or duplicate-time keys.");
            }
            if (duplicateChannels > 0)
            {
                warnings.Add(
                    $"Animation {Path.GetFileName(path)}: ignored {duplicateChannels} " +
                    "duplicate node/property curves after the first valid curve.");
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

    private static SmoExportAnimationTrack ShiftTrack(
        SmoExportAnimationTrack track, float offset) => track with
    {
        Positions = track.Positions
            .Select(key => key with { Time = key.Time + offset }).ToArray(),
        Rotations = track.Rotations
            .Select(key => key with { Time = key.Time + offset }).ToArray(),
        Scales = track.Scales
            .Select(key => key with { Time = key.Time + offset }).ToArray()
    };

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
                entry.Index, entry.Name, parent, ReflectMatrix(world), ReflectMatrix(local)));
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

    private static Matrix4x4 ReflectMatrix(Matrix4x4 value)
    {
        Matrix4x4 reflection = Matrix4x4.CreateScale(1, 1, -1);
        return reflection * value * reflection;
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
                    source.Width, source.Height, source.Bgra32Pixels.Span));
    }

    private static Vector4 DecodeArgb(uint argb) => new(
        ((argb >> 16) & 0xFF) / 255f,
        ((argb >> 8) & 0xFF) / 255f,
        (argb & 0xFF) / 255f,
        (argb >> 24) / 255f);
}
