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

        var loaded = SmoLoadedResources.Get(document);
        if (loaded.LoadIssue is not null || loaded.SceneIssue is not null)
            throw new InvalidDataException(loaded.LoadIssue ?? loaded.SceneIssue);
        var catalog = SmoRenderableCatalog.Get(document);
        var prepared = includeMeshes ? SmoViewer.Scene.SmoSceneBuilder.Build(document) : null;
        var warnings = new List<string>();
        if (prepared is not null)
        {
            warnings.AddRange(prepared.DecodeErrors);
            warnings.AddRange(prepared.TextureIssues);
        }
        var meshes = new List<SmoExportMesh>();
        var placements = new List<SmoExportMeshPlacement>();
        var textures = new Dictionary<int, SmoExportTexture>();
        SmoExportTexture ExportTexture(SmoTexture texture)
        {
            if (!textures.TryGetValue(texture.ObjectIndex, out var exported))
                textures.Add(texture.ObjectIndex, exported = BuildExportTexture(texture));
            return exported;
        }
        var decodedSkins = includeSkeleton
            ? catalog.ByObjectIndex.Values.Where(value => value.Skin is not null).ToDictionary(value => value.ObjectIndex, value => value.Skin!)
            : new Dictionary<int, SmoSkin>();
        var skins = decodedSkins.Values.Select(skin => new SmoExportSkin(skin.ObjectIndex, skin.Name,
            skin.Bones.Select(bone => bone.NodeObjectIndex).ToArray(),
            skin.Bones.Select(bone => SmoExportCoordinateSystem.ToExportMatrix(bone.InverseBindMatrix)).ToArray())).ToList();
        var allNodes = includeSkeleton ? BuildExportNodes(document, loaded) : new List<SmoExportNode>();
        var geometry = new Dictionary<int, ExportGeometry>();
        var variants = new Dictionary<SmoExportMeshKey, SmoExportMesh>();
        var seenMeshes = new HashSet<int>();
        foreach (var occurrence in prepared?.Meshes ?? Array.Empty<SmoViewer.Scene.SmoSceneMesh>())
        {
            var source = occurrence.Mesh;
            int renderableIndex = occurrence.RenderableObjectIndex
                ?? throw new InvalidDataException("Export occurrence has no actual renderable identity.");
            if (options.SceneMode == SmoExportSceneMode.SeparateMeshes
                && !options.SelectedMeshObjectIndices!.Contains(source.ObjectIndex)) continue;
            // Retain the existing LevelOnly storage-selection mode as a host
            // filter over actual occurrences, never as a native visibility claim.
            if (options.SceneMode == SmoExportSceneMode.LevelOnly
                && (!catalog.TryGetStoredMeshOwner(document.Objects[source.ObjectIndex], out var storedOwner)
                    || storedOwner.ObjectIndex != renderableIndex)) continue;

            bool exportSkin = includeSkeleton && source.HasSkinningData && occurrence.SkinObjectIndex.HasValue;
            if (exportSkin && !decodedSkins.ContainsKey(occurrence.SkinObjectIndex!.Value))
                throw new InvalidDataException($"Skin [{occurrence.SkinObjectIndex}] is unavailable for Model [{renderableIndex}].");
            Matrix4x4 world = options.ApplyWorldTransforms ? occurrence.WorldTransform : Matrix4x4.Identity;
            Matrix4x4 local = world;
            int? parent = null;
            if (includeSkeleton && options.ApplyWorldTransforms && occurrence.RigidNodeObjectIndex is int nodeIndex)
            {
                if (!loaded.NodeWorlds.TryGetValue(nodeIndex, out var parentWorld)
                    || !Matrix4x4.Invert(parentWorld, out var inverseParent))
                    throw new InvalidDataException($"EXPORT_PARENT_MATRIX_SINGULAR: Model [{renderableIndex}] support [{nodeIndex}].");
                parent = nodeIndex;
                local = world * inverseParent;
            }
            var worldExport = SmoExportCoordinateSystem.ToExportMatrix(world);
            var localExport = SmoExportCoordinateSystem.ToExportMatrix(local);
            var key = new SmoExportMeshKey(source.ObjectIndex, renderableIndex);
            if (!variants.TryGetValue(key, out var mesh))
            {
                if (!geometry.TryGetValue(source.ObjectIndex, out var converted))
                    geometry.Add(source.ObjectIndex, converted = ConvertGeometry(source, includeMaterials));
                var texture = includeTextures && occurrence.Texture is not null ? ExportTexture(occurrence.Texture) : null;
                Vector4 color = includeMaterials ? occurrence.LoadedMaterial?.Colors[1] ?? Vector4.One : Vector4.One;
                // Existing target-format color policy is retained here while
                // full engine material/shader execution is migrated separately.
                if (includeMaterials && texture is null && converted.UniformDiffuse is Vector4 diffuse) color = diffuse;
                bool alpha = includeMaterials && (occurrence.UsesAlphaBlend || color.W < 1f || converted.Colors.Any(value => value.W < 1f));
                mesh = new SmoExportMesh(source.ObjectIndex, document.Objects[source.ObjectIndex].Id,
                    source.Name, source.Marker, source.PrimitiveType, source.VertexFormat, source.Stride, source.RuntimeStride,
                    converted.Positions, converted.Normals, converted.Uv0, converted.Uv1, converted.Colors,
                    exportSkin ? converted.Weights : [], exportSkin ? converted.Joints : [], converted.Triangles,
                    texture, null, color, alpha, exportSkin ? occurrence.SkinObjectIndex : null, parent, worldExport, localExport)
                { RenderableObjectIndex = renderableIndex, LoadedMaterial = occurrence.LoadedMaterial };
                variants.Add(key, mesh);meshes.Add(mesh);
            }
            var container = loaded.RenderContainersByObjectIndex[occurrence.OccurrenceKey!.Value.ContainerObjectIndex];
            var sourceModel = loaded.Models[renderableIndex];
            placements.Add(new SmoExportMeshPlacement(renderableIndex, document.Objects[renderableIndex].Name,
                source.ObjectIndex, !seenMeshes.Add(source.ObjectIndex),
                container.Kind == SmoRenderContainerKind.StaticRenderObject ? container.ObjectIndex : null,
                sourceModel.Material?.ObjectIndex, parent, worldExport, localExport)
            { MeshVariantKey = key, OccurrenceKey = occurrence.OccurrenceKey });
        }
        var nodes = includeSkeleton
            ? includeServiceNodes ? allNodes : FilterServiceNodes(allNodes, skins, placements)
            : new List<SmoExportNode>();
        var animations = includeAnimations ? BuildAnimations(options.AnimationPaths, nodes, warnings) : [];
        string sourcePath = document.SourcePath ?? "memory.smo";
        string hash = Convert.ToHexString(SHA256.HashData(document.Data.Span));
        return new SmoExportScene(sourcePath, hash, document.Header.PlatformMask, resources, options.SceneMode,
            meshes, placements, nodes, skins, animations, warnings.Distinct().ToArray());
    }

    private sealed record ExportGeometry(Vector3[] Positions, Vector3[] Normals,
        Vector2[] Uv0, Vector2[] Uv1, Vector4[] Colors, Vector4[] Weights, Vector4[] Joints,
        uint[] Triangles, Vector4? UniformDiffuse);

    private static ExportGeometry ConvertGeometry(SmoMesh mesh, bool includeMaterials)
    {
        var positions = mesh.Positions.Select(value => new Vector3(value.X, value.Y, -value.Z)).ToArray();
        var normals = mesh.HasNormals ? mesh.Normals.Select(value =>
        {
            var converted = new Vector3(value.X, value.Y, -value.Z);
            return converted.LengthSquared() > .000001f ? Vector3.Normalize(converted) : Vector3.UnitY;
        }).ToArray() : [];
        var triangles = mesh.TriangleIndices.ToArray();
        for (int i = 0; i < triangles.Length; i += 3) (triangles[i+1],triangles[i+2]) = (triangles[i+2],triangles[i+1]);
        bool uniform = includeMaterials && mesh.HasDiffuseColors && mesh.DiffuseColorsArgb.Skip(1).All(value => value == mesh.DiffuseColorsArgb[0]);
        bool vertexColors = includeMaterials && mesh.HasDiffuseColors && !uniform && mesh.DiffuseColorsArgb.Any(value => (value & 0xFFFFFF) != 0);
        return new(positions, normals, mesh.TextureCoordinates.ToArray(), mesh.TextureCoordinates1.ToArray(),
            vertexColors ? mesh.DiffuseColorsArgb.Select(DecodeArgb).ToArray() : [], mesh.BlendWeights.ToArray(),
            mesh.BlendIndices.Select(value => new Vector4(value.X,value.Y,value.Z,value.W)).ToArray(), triangles,
            uniform ? DecodeArgb(mesh.DiffuseColorsArgb[0]) : null);
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
        IReadOnlyList<SmoExportMeshPlacement> placements)
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

        foreach (SmoExportMeshPlacement placement in placements)
        {
            if (placement.ParentNodeObjectIndex is int parentNodeObjectIndex)
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
        SmoDocument document, SmoLoadedResources loaded)
    {
        List<SmoExportNode> result = [];
        foreach (var node in loaded.Nodes.Values)
        {
            var entry = document.Objects[node.ObjectIndex];
            int? parent = node.ParentObjectIndex;
            Matrix4x4 world = node.World;
            Matrix4x4 local = world;
            if (parent is int parentIndex)
            {
                // Target-format local coordinates derived from actual game
                // worlds; this is not a second implementation of Node FK.
                if (!loaded.NodeWorlds.TryGetValue(parentIndex, out var parentWorld)
                    || !Matrix4x4.Invert(parentWorld, out var inverseParent))
                    throw new InvalidDataException($"EXPORT_PARENT_MATRIX_SINGULAR: Node [{entry.Index}] parent [{parentIndex}] cannot be represented by this export hierarchy.");
                local = world * inverseParent;
            }
            result.Add(new SmoExportNode(
                entry.Index,
                entry.Name,
                parent,
                SmoExportCoordinateSystem.ToExportMatrix(world),
                SmoExportCoordinateSystem.ToExportMatrix(local)));
        }
        return result;
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
