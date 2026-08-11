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
        var warnings = new List<string>();
        var meshes = new List<SmoExportMesh>();
        IReadOnlyDictionary<int, SmoTextureBinding> textures =
            SmoTextureBindingResolver.ResolveAll(document);
        IReadOnlyDictionary<int, uint> materialColors =
            SmoMaterialColorResolver.ResolveAll(document);
        SmoNodeHierarchy hierarchy = SmoNodeHierarchy.Decode(document);
        IReadOnlyDictionary<int, Matrix4x4> bindWorld =
            SmoSkinBindingResolver.ResolveBindWorldMatrices(document);
        Dictionary<int, SmoSkin> decodedSkins = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Skin)
            .Select(entry => SmoSkinDecoder.TryDecode(document, entry, out SmoSkin? skin, out _)
                ? skin : null)
            .Where(skin => skin is not null).Cast<SmoSkin>()
            .ToDictionary(skin => skin.ObjectIndex);
        Dictionary<int, Matrix4x4> nodeWorld = BuildNodeWorldMatrices(
            document, hierarchy, bindWorld);
        List<SmoExportNode> nodes = BuildExportNodes(document, hierarchy, nodeWorld);
        List<SmoExportSkin> skins = decodedSkins.Values.Select(skin => new SmoExportSkin(
            skin.ObjectIndex,
            skin.Name,
            skin.Bones.Select(bone => bone.NodeObjectIndex).ToArray(),
            skin.Bones.Select(bone => ReflectMatrix(bone.InverseBindMatrix)).ToArray())).ToList();

        foreach (SmoObjectEntry entry in document.Objects.Where(
                     item => item.TypeHash == SmoClassIds.MeshData))
        {
            if (!SmoMeshDecoder.TryDecode(document, entry, out SmoMesh? mesh, out string error) ||
                mesh is null)
            {
                warnings.Add(error);
                continue;
            }

            int? skinObjectIndex = FindAncestorObjectIndex(
                document.Objects, entry, SmoClassIds.Skin);
            bool exportSkin = mesh.HasSkinningData && skinObjectIndex is int skinIndex &&
                              decodedSkins.ContainsKey(skinIndex);

            Matrix4x4 world = options.ApplyWorldTransforms && !exportSkin
                ? SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, entry)
                : Matrix4x4.Identity;
            Matrix4x4 normalMatrix = world;
            if (Matrix4x4.Invert(world, out Matrix4x4 inverse))
                normalMatrix = Matrix4x4.Transpose(inverse);

            Vector3[] positions = mesh.Positions.Select(value =>
            {
                Vector3 transformed = Vector3.Transform(value, world);
                return new Vector3(transformed.X, transformed.Y, -transformed.Z);
            }).ToArray();
            Vector3[] normals = mesh.HasNormals
                ? mesh.Normals.Select(value =>
                {
                    Vector3 transformed = Vector3.TransformNormal(value, normalMatrix);
                    transformed.Z = -transformed.Z;
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
            if (textures.TryGetValue(entry.Index, out SmoTextureBinding? binding))
            {
                if (binding.Issue is not null)
                    warnings.Add(binding.Issue);
                else if (binding.Texture is not null)
                {
                    SmoTexture source = binding.BaseTexture ?? binding.Texture;
                    texture = new SmoExportTexture(
                        source.ObjectIndex,
                        source.Name,
                        source.Width,
                        source.Height,
                        PngEncoder.EncodeBgra32(
                            source.Width, source.Height, source.Bgra32Pixels.Span));
                }
            }

            Vector4 materialColor = materialColors.TryGetValue(entry.Index, out uint argb)
                ? DecodeArgb(argb)
                : Vector4.One;
            // Some skinned assets serialize an all-zero diffuse channel as a
            // placeholder. glTF COLOR_0 multiplies baseColorTexture, so exporting
            // that placeholder would turn a valid textured model completely black.
            // Keep COLOR_0 only when it carries actual RGB information.
            bool hasRenderableDiffuse = mesh.HasDiffuseColors &&
                mesh.DiffuseColorsArgb.Any(color => (color & 0x00FFFFFF) != 0);
            Vector4[] colors = hasRenderableDiffuse
                ? mesh.DiffuseColorsArgb.Select(DecodeArgb).ToArray()
                : [];
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
                materialColor,
                exportSkin ? skinObjectIndex : null));
        }

        List<SmoExportAnimation> animations = BuildAnimations(options.AnimationPaths, nodes, warnings);

        string sourcePath = document.SourcePath ?? "memory.smo";
        string hash = Convert.ToHexString(SHA256.HashData(document.Data.Span));
        return new SmoExportScene(
            sourcePath, hash, document.Header.Version, meshes, nodes, skins, animations, warnings);
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
            SmoExportAnimationTrack[] tracks = clip.Tracks
                .Where(track => byName.ContainsKey(track.NodeName))
                .Select(track => new SmoExportAnimationTrack(
                    byName[track.NodeName].ObjectIndex, track.NodeName,
                    track.Positions.Select(key => new SmoAnimationKey<Vector3>(
                        key.Time, new Vector3(key.Value.X, key.Value.Y, -key.Value.Z))).ToArray(),
                    track.Rotations.Select(key => new SmoAnimationKey<Quaternion>(
                        key.Time, new Quaternion(-key.Value.X, -key.Value.Y, key.Value.Z, key.Value.W))).ToArray(),
                    track.Scales.ToArray())).ToArray();
            result.Add(new SmoExportAnimation(
                Path.GetFileNameWithoutExtension(path), clip.Duration, tracks));
        }
        return result;
    }

    private static List<SmoExportNode> BuildExportNodes(
        SmoDocument document, SmoNodeHierarchy hierarchy,
        IReadOnlyDictionary<int, Matrix4x4> worlds)
    {
        List<SmoExportNode> result = [];
        foreach (SmoObjectEntry entry in document.Objects.Where(item => item.TypeHash == SmoClassIds.Node))
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
        foreach (SmoObjectEntry entry in document.Objects.Where(item => item.TypeHash == SmoClassIds.Node))
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

    private static Vector4 DecodeArgb(uint argb) => new(
        ((argb >> 16) & 0xFF) / 255f,
        ((argb >> 8) & 0xFF) / 255f,
        (argb & 0xFF) / 255f,
        (argb >> 24) / 255f);
}
