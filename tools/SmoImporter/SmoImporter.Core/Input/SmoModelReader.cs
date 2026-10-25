using System.Numerics;
using SmoExporter.Core;
using SmoViewer.Core;

namespace SmoImporter.Core;

/// <summary>
/// Decodes a model SMO into the immutable intermediate scene used by the
/// preview and geometry-analysis tools. Native SMO-to-SMO replacement does not
/// use this representation: it transfers the donor render forest directly.
/// </summary>
public static class SmoModelReader
{
    private const float WeightEpsilon = 0.000001f;

    public static ImportedScene Read(
        string path,
        CancellationToken cancellationToken = default)
        => ReadCore(path, cancellationToken, includeMaterials: true);

    /// <summary>
    /// Geometry-only input for the native forest transfer window. Its writer
    /// consumes the original SmoDocument; this preview does not flatten native
    /// passes, layers or animated textures into ImportedMaterial.
    /// </summary>
    public static ImportedScene ReadNativeTransferGeometryPreview(
        string path,
        CancellationToken cancellationToken = default)
        => ReadCore(path, cancellationToken, includeMaterials: false);

    private static ImportedScene ReadCore(
        string path,
        CancellationToken cancellationToken,
        bool includeMaterials)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        cancellationToken.ThrowIfCancellationRequested();
        string fullPath = Path.GetFullPath(path);
        if (!Path.GetExtension(fullPath).Equals(
                ".smo", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException("SmoModelReader accepts only .smo files.");
        }

        SmoDocument document = SmoDocument.Load(fullPath);
        if (document.HasErrors)
        {
            throw new InvalidDataException(
                "The donor SMO has parser errors: " +
                string.Join(" | ", document.Diagnostics
                    .Where(item => item.Severity == SmoDiagnosticSeverity.Error)
                    .Select(item => item.Message)));
        }

        cancellationToken.ThrowIfCancellationRequested();
        HashSet<int> activeMeshIndices = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData &&
                            SmoActiveVisualResourceResolver.IsMeshActive(
                                document, entry))
            .Select(entry => entry.Index)
            .ToHashSet();
        if (activeMeshIndices.Count == 0)
            throw new InvalidDataException("The donor SMO has no active mesh resources.");

        SmoExportScene source = SmoSceneBuilder.Build(
            document,
            new SmoExportOptions(
                ApplyWorldTransforms: true,
                AnimationPaths: null,
                Resources: SmoExportResourceTypes.Meshes |
                           SmoExportResourceTypes.Skeleton |
                           (includeMaterials ? SmoExportResourceTypes.Materials |
                            SmoExportResourceTypes.Textures : default),
                SceneMode: SmoExportSceneMode.SeparateMeshes,
                SelectedMeshObjectIndices: activeMeshIndices));
        cancellationToken.ThrowIfCancellationRequested();
        return Convert(source, cancellationToken, includeMaterials);
    }

    public static ImportedScene ReadGeometryOnly(
        string path,
        CancellationToken cancellationToken = default)
    {
        ImportedScene scene = Read(path, cancellationToken);
        return scene with
        {
            Meshes = scene.Meshes
                .Select(mesh => mesh with { Skinning = null })
                .ToArray()
        };
    }

    internal static ImportedScene Convert(
        SmoExportScene source,
        CancellationToken cancellationToken = default,
        bool includeMaterials = true)
    {
        ArgumentNullException.ThrowIfNull(source);
        cancellationToken.ThrowIfCancellationRequested();

        var warnings = new List<string>(source.Warnings);
        if (!includeMaterials)
            warnings.Add("NATIVE_GEOMETRY_PREVIEW: only geometry is projected for the transfer window; " +
                "saving transfers the original SMO graph, including all material passes, layers and controllers.");
        // ImportedScene stores flattened occurrences. The shared source scene
        // keeps geometry variants separately from actual support reference slots.
        var variants = source.Meshes.ToDictionary(mesh => mesh.VariantKey);
        SmoExportMesh[] meshes = source.MeshPlacements.Select(placement =>
            {
                if (!variants.TryGetValue(placement.EffectiveMeshKey, out var mesh))
                    throw new InvalidDataException(
                        $"SMO placement {placement.PlacementKey} references unavailable mesh variant {placement.EffectiveMeshKey}.");
                return mesh with
                {
                    Name = placement.Name,
                    ParentNodeObjectIndex = placement.ParentNodeObjectIndex,
                    BindWorldMatrix = placement.WorldMatrix,
                    BindLocalMatrix = placement.LocalMatrix
                };
            })
            .Where(mesh =>
            {
                bool renderable = HasRenderableTriangle(mesh);
                if (!renderable)
                {
                    warnings.Add(
                        $"SMO mesh [{mesh.ObjectIndex}] {mesh.Name} has no " +
                        "non-degenerate triangle and was not imported as visual geometry.");
                }
                return renderable;
            })
            .ToArray();
        if (meshes.Length == 0)
            throw new InvalidDataException("The donor SMO has no renderable mesh geometry.");

        SmoExportMesh[] layered = meshes
            .Where(mesh => mesh.EffectTexture is not null ||
                mesh.LoadedMaterial is { } material &&
                (material.Passes.Count > 1 || material.Passes.Any(pass => pass.Layers.Count > 1)))
            .ToArray();
        if (includeMaterials && layered.Length > 0)
        {
            throw new InvalidDataException(
                "MATERIAL_IMPORT_SHAPE: the donor SMO contains multiple material passes or layers which cannot yet be " +
                "represented by ImportedScene without dropping a texture layer: " +
                string.Join(", ", layered.Select(mesh =>
                    $"[{mesh.ObjectIndex}] {mesh.Name}")) + ".");
        }

        SmoExportTexture[] sourceTextures = meshes
            .Where(_ => includeMaterials)
            .Select(mesh => mesh.Texture)
            .OfType<SmoExportTexture>()
            .GroupBy(texture => texture.ObjectIndex)
            .Select(group => group.First())
            .OrderBy(texture => texture.ObjectIndex)
            .ToArray();
        Dictionary<int, int> textureIndexByObject = sourceTextures
            .Select((texture, index) => (texture.ObjectIndex, index))
            .ToDictionary(item => item.ObjectIndex, item => item.index);
        IReadOnlyDictionary<int, string> textureNames = BuildTextureNames(sourceTextures);
        ImportedTexture[] textures = sourceTextures
            .Select(texture => new ImportedTexture(
                textureNames[texture.ObjectIndex],
                "image/png",
                texture.Width,
                texture.Height,
                texture.PngBytes.ToArray()))
            .ToArray();

        Dictionary<int, SmoExportSkin> skins = source.Skins
            .ToDictionary(skin => skin.ObjectIndex);
        Dictionary<int, SmoExportNode> nodes = source.Nodes
            .ToDictionary(node => node.ObjectIndex);
        var importedSkeletons = new Dictionary<int, ImportedSkinDefinition>();
        var importedMeshes = new ImportedMesh[meshes.Length];
        var materials = new ImportedMaterial[includeMaterials ? meshes.Length : 0];
        for (int meshIndex = 0; meshIndex < meshes.Length; meshIndex++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            SmoExportMesh sourceMesh = meshes[meshIndex];
            int textureIndex = !includeMaterials || sourceMesh.Texture is null
                ? -1
                : textureIndexByObject[sourceMesh.Texture.ObjectIndex];
            if (includeMaterials) materials[meshIndex] = new ImportedMaterial(
                $"{sourceMesh.Name}_material",
                textureIndex >= 0 ? textures[textureIndex].Name : null,
                textureIndex,
                sourceMesh.UsesAlphaBlend
                    ? ImportedMaterialAlphaMode.Blend
                    : ImportedMaterialAlphaMode.Opaque);

            ImportedSkinning? skinning = null;
            if (sourceMesh.SkinObjectIndex is int skinObjectIndex)
            {
                if (!skins.TryGetValue(skinObjectIndex, out SmoExportSkin? sourceSkin))
                {
                    throw new InvalidDataException(
                        $"SMO mesh [{sourceMesh.ObjectIndex}] {sourceMesh.Name} " +
                        $"references unavailable skin [{skinObjectIndex}].");
                }
                if (!importedSkeletons.TryGetValue(
                        skinObjectIndex, out ImportedSkinDefinition? importedSkin))
                {
                    importedSkin = BuildSkeleton(
                        sourceSkin,
                        nodes,
                        meshes.Where(candidate =>
                            candidate.SkinObjectIndex == skinObjectIndex));
                    importedSkeletons.Add(skinObjectIndex, importedSkin);
                }
                skinning = BuildSkinning(sourceMesh, importedSkin);
            }

            Vector3[] positions = sourceMesh.Positions.ToArray();
            Vector3[] normals = sourceMesh.Normals.ToArray();
            if (skinning is null && sourceMesh.BindWorldMatrix != Matrix4x4.Identity)
                BakeRigidTransform(sourceMesh, positions, normals);

            importedMeshes[meshIndex] = new ImportedMesh(
                sourceMesh.Name,
                positions,
                normals,
                sourceMesh.TextureCoordinates0.ToArray(),
                sourceMesh.TriangleIndices.ToArray(),
                includeMaterials ? BuildDiffuseColors(sourceMesh) : null,
                includeMaterials ? meshIndex : -1,
                skinning)
            {
                SecondaryTextureCoordinates =
                    sourceMesh.TextureCoordinates1.ToArray()
            };
        }

        return new ImportedScene(importedMeshes, textures, materials)
        {
            ImportWarnings = warnings.AsReadOnly()
        };
    }

    private sealed record ImportedSkinDefinition(
        ImportedSkeleton Skeleton,
        IReadOnlyList<ushort> SourceToImportedSlots);

    private static ImportedSkinDefinition BuildSkeleton(
        SmoExportSkin skin,
        IReadOnlyDictionary<int, SmoExportNode> nodes,
        IEnumerable<SmoExportMesh> sourceMeshes)
    {
        if (skin.JointObjectIndices.Count != skin.InverseBindMatrices.Count)
        {
            throw new InvalidDataException(
                $"SMO skin [{skin.ObjectIndex}] {skin.Name} has " +
                $"{skin.JointObjectIndices.Count} joints but " +
                $"{skin.InverseBindMatrices.Count} inverse-bind matrices.");
        }

        int[] jointObjects = skin.JointObjectIndices.ToArray();
        HashSet<int> activeSlots = FindActivePaletteSlots(
            skin, sourceMeshes);
        var representativeSlots = new List<int>();
        var sourceToImported = new ushort[jointObjects.Length];
        foreach (IGrouping<int, int> group in Enumerable.Range(0, jointObjects.Length)
                     .GroupBy(slot => jointObjects[slot]))
        {
            int[] slots = group.ToArray();
            int[] active = slots.Where(activeSlots.Contains).ToArray();
            int representative = active.Length > 0 ? active[0] : slots[0];
            if (active.Skip(1).Any(slot =>
                    !skin.InverseBindMatrices[slot].Equals(
                        skin.InverseBindMatrices[representative])))
            {
                throw new InvalidDataException(
                    $"SMO skin [{skin.ObjectIndex}] {skin.Name} uses node " +
                    $"[{group.Key}] through several active palette slots with " +
                    "different inverse-bind matrices. The name-based clean writer " +
                    "cannot preserve that ambiguity.");
            }
            ushort importedSlot = checked((ushort)representativeSlots.Count);
            representativeSlots.Add(representative);
            foreach (int sourceSlot in slots)
                sourceToImported[sourceSlot] = importedSlot;
        }

        int[] importedObjects = representativeSlots
            .Select(slot => jointObjects[slot])
            .ToArray();
        var paletteSlotByObject = importedObjects
            .Select((objectIndex, slot) => (objectIndex, slot))
            .ToDictionary(item => item.objectIndex, item => item.slot);
        string[] names = new string[importedObjects.Length];
        int[] parents = new int[importedObjects.Length];
        Matrix4x4[] worlds = new Matrix4x4[importedObjects.Length];
        Matrix4x4[] locals = new Matrix4x4[importedObjects.Length];
        Matrix4x4[] inverseBind = new Matrix4x4[importedObjects.Length];
        for (int slot = 0; slot < importedObjects.Length; slot++)
        {
            int objectIndex = importedObjects[slot];
            if (!nodes.TryGetValue(objectIndex, out SmoExportNode? node))
            {
                throw new InvalidDataException(
                    $"SMO skin [{skin.ObjectIndex}] {skin.Name} references " +
                    $"missing joint node [{objectIndex}].");
            }
            if (string.IsNullOrWhiteSpace(node.Name))
            {
                throw new InvalidDataException(
                    $"SMO skin [{skin.ObjectIndex}] contains an unnamed joint " +
                    $"at palette slot {slot}.");
            }
            names[slot] = node.Name;
            worlds[slot] = node.BindWorldMatrix;
            inverseBind[slot] = skin.InverseBindMatrices[representativeSlots[slot]];
            parents[slot] = ResolveNearestPaletteParent(
                node.ParentObjectIndex, paletteSlotByObject, nodes);
        }
        if (names.Distinct(StringComparer.Ordinal).Count() != names.Length)
        {
            throw new InvalidDataException(
                $"SMO skin [{skin.ObjectIndex}] {skin.Name} contains duplicate " +
                "joint names and cannot be mapped unambiguously.");
        }

        for (int slot = 0; slot < importedObjects.Length; slot++)
        {
            int parent = parents[slot];
            if (parent >= 0)
            {
                if (!Matrix4x4.Invert(worlds[parent], out Matrix4x4 inverseParent))
                {
                    throw new InvalidDataException(
                        $"SMO joint {names[parent]} has a non-invertible bind matrix.");
                }
                locals[slot] = worlds[slot] * inverseParent;
            }
            else
            {
                locals[slot] = worlds[slot];
            }
        }

        return new ImportedSkinDefinition(
            new ImportedSkeleton(skin.Name, names, inverseBind)
            {
                ParentJointIndices = parents,
                BindWorldMatrices = worlds,
                BindLocalMatrices = locals
            },
            sourceToImported);
    }

    private static HashSet<int> FindActivePaletteSlots(
        SmoExportSkin skin,
        IEnumerable<SmoExportMesh> sourceMeshes)
    {
        var result = new HashSet<int>();
        foreach (SmoExportMesh mesh in sourceMeshes)
        for (int vertex = 0; vertex < mesh.BlendWeights.Length; vertex++)
        for (int component = 0; component < 4; component++)
        {
            float weight = Component(mesh.BlendWeights[vertex], component);
            if (!float.IsFinite(weight) || weight <= WeightEpsilon)
                continue;
            float sourceJoint = Component(mesh.JointIndices[vertex], component);
            int rounded = checked((int)MathF.Round(sourceJoint));
            if (!float.IsFinite(sourceJoint) ||
                MathF.Abs(sourceJoint - rounded) > WeightEpsilon ||
                (uint)rounded >= (uint)skin.JointObjectIndices.Count)
            {
                throw new InvalidDataException(
                    $"SMO mesh [{mesh.ObjectIndex}] {mesh.Name} vertex {vertex} " +
                    $"contains invalid active palette slot {sourceJoint}.");
            }
            result.Add(rounded);
        }
        return result;
    }

    private static int ResolveNearestPaletteParent(
        int? parentObjectIndex,
        IReadOnlyDictionary<int, int> paletteSlotByObject,
        IReadOnlyDictionary<int, SmoExportNode> nodes)
    {
        var visited = new HashSet<int>();
        while (parentObjectIndex is int objectIndex && visited.Add(objectIndex))
        {
            if (paletteSlotByObject.TryGetValue(objectIndex, out int slot))
                return slot;
            parentObjectIndex = nodes.TryGetValue(
                objectIndex, out SmoExportNode? node)
                    ? node.ParentObjectIndex
                    : null;
        }
        return -1;
    }

    private static ImportedSkinning BuildSkinning(
        SmoExportMesh mesh,
        ImportedSkinDefinition importedSkin)
    {
        if (mesh.BlendWeights.Length != mesh.Positions.Length ||
            mesh.JointIndices.Length != mesh.Positions.Length)
        {
            throw new InvalidDataException(
                $"SMO mesh [{mesh.ObjectIndex}] {mesh.Name} has incomplete " +
                "skin streams.");
        }
        ImportedSkeleton skeleton = importedSkin.Skeleton;
        var joints = new ImportedJointIndices[mesh.Positions.Length];
        var weights = new Vector4[mesh.Positions.Length];
        var mappedJoints = new ushort[4];
        var mappedWeights = new float[4];
        for (int vertex = 0; vertex < joints.Length; vertex++)
        {
            Array.Clear(mappedJoints);
            Array.Clear(mappedWeights);
            int count = 0;
            for (int component = 0; component < 4; component++)
            {
                float weight = Component(mesh.BlendWeights[vertex], component);
                if (!float.IsFinite(weight) || weight < 0)
                {
                    throw new InvalidDataException(
                        $"SMO mesh [{mesh.ObjectIndex}] {mesh.Name} vertex {vertex} " +
                        $"contains invalid skin weight {weight}.");
                }
                if (weight <= 0)
                    continue;
                float sourceJoint = Component(mesh.JointIndices[vertex], component);
                int sourceSlot = Joint(
                    sourceJoint,
                    importedSkin.SourceToImportedSlots.Count,
                    mesh,
                    vertex);
                ushort mapped = importedSkin.SourceToImportedSlots[sourceSlot];
                int existing = mappedJoints.AsSpan(0, count).IndexOf(mapped);
                if (existing >= 0)
                {
                    mappedWeights[existing] += weight;
                }
                else
                {
                    mappedJoints[count] = mapped;
                    mappedWeights[count] = weight;
                    count++;
                }
            }
            if (count == 0)
            {
                throw new InvalidDataException(
                    $"SMO mesh [{mesh.ObjectIndex}] {mesh.Name} vertex {vertex} " +
                    "has no positive skin weight.");
            }
            joints[vertex] = new ImportedJointIndices(
                mappedJoints[0], mappedJoints[1], mappedJoints[2], mappedJoints[3]);
            weights[vertex] = new Vector4(
                mappedWeights[0], mappedWeights[1],
                mappedWeights[2], mappedWeights[3]);
        }
        return new ImportedSkinning(skeleton, joints, weights);
    }

    private static int Joint(
        float value,
        int sourceSlotCount,
        SmoExportMesh mesh,
        int vertex)
    {
        int rounded = checked((int)MathF.Round(value));
        if (!float.IsFinite(value) || MathF.Abs(value - rounded) > WeightEpsilon ||
            (uint)rounded >= (uint)sourceSlotCount)
        {
            throw new InvalidDataException(
                $"SMO mesh [{mesh.ObjectIndex}] {mesh.Name} vertex {vertex} " +
                $"contains invalid joint index {value}.");
        }
        return rounded;
    }

    private static float Component(Vector4 value, int component) => component switch
    {
        0 => value.X,
        1 => value.Y,
        2 => value.Z,
        3 => value.W,
        _ => throw new ArgumentOutOfRangeException(nameof(component))
    };

    private static void BakeRigidTransform(
        SmoExportMesh source,
        Vector3[] positions,
        Vector3[] normals)
    {
        for (int index = 0; index < positions.Length; index++)
            positions[index] = Vector3.Transform(positions[index], source.BindWorldMatrix);
        if (normals.Length == 0)
            return;
        if (!Matrix4x4.Invert(
                source.BindWorldMatrix, out Matrix4x4 inverseWorld))
        {
            throw new InvalidDataException(
                $"SMO mesh [{source.ObjectIndex}] {source.Name} has a " +
                "non-invertible rigid world matrix.");
        }
        Matrix4x4 normalMatrix = Matrix4x4.Transpose(inverseWorld);
        for (int index = 0; index < normals.Length; index++)
        {
            Vector3 value = Vector3.TransformNormal(normals[index], normalMatrix);
            normals[index] = value.LengthSquared() > WeightEpsilon
                ? Vector3.Normalize(value)
                : Vector3.UnitY;
        }
    }

    private static uint[]? BuildDiffuseColors(SmoExportMesh mesh)
    {
        bool hasColors = mesh.Colors.Length > 0;
        if (hasColors && mesh.Colors.Length != mesh.Positions.Length)
        {
            throw new InvalidDataException(
                $"SMO mesh [{mesh.ObjectIndex}] {mesh.Name} has an incomplete " +
                "diffuse-color stream.");
        }
        bool whiteMaterial = ApproximatelyOne(mesh.MaterialColor);
        if (!hasColors && whiteMaterial)
            return null;

        var result = new uint[mesh.Positions.Length];
        for (int vertex = 0; vertex < result.Length; vertex++)
        {
            Vector4 color = hasColors ? mesh.Colors[vertex] : Vector4.One;
            color *= mesh.MaterialColor;
            result[vertex] = PackArgb(color);
        }
        return result;
    }

    private static IReadOnlyDictionary<int, string> BuildTextureNames(
        IReadOnlyList<SmoExportTexture> textures)
    {
        HashSet<string> duplicates = textures
            .GroupBy(texture => texture.Name, StringComparer.OrdinalIgnoreCase)
            .Where(group => group.Count() > 1)
            .Select(group => group.Key)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        return textures.ToDictionary(
            texture => texture.ObjectIndex,
            texture => duplicates.Contains(texture.Name)
                ? $"{texture.Name}_{texture.ObjectIndex}"
                : texture.Name);
    }

    private static bool HasRenderableTriangle(SmoExportMesh mesh)
    {
        for (int index = 0; index + 2 < mesh.TriangleIndices.Length; index += 3)
        {
            uint first = mesh.TriangleIndices[index];
            uint second = mesh.TriangleIndices[index + 1];
            uint third = mesh.TriangleIndices[index + 2];
            if (first >= mesh.Positions.Length ||
                second >= mesh.Positions.Length ||
                third >= mesh.Positions.Length)
            {
                throw new InvalidDataException(
                    $"SMO mesh [{mesh.ObjectIndex}] {mesh.Name} contains an " +
                    "out-of-range triangle index.");
            }
            Vector3 cross = Vector3.Cross(
                mesh.Positions[(int)second] - mesh.Positions[(int)first],
                mesh.Positions[(int)third] - mesh.Positions[(int)first]);
            if (float.IsFinite(cross.X) && float.IsFinite(cross.Y) &&
                float.IsFinite(cross.Z) && cross.LengthSquared() > 1e-20f)
            {
                return true;
            }
        }
        return false;
    }

    private static bool ApproximatelyOne(Vector4 value) =>
        MathF.Abs(value.X - 1) <= WeightEpsilon &&
        MathF.Abs(value.Y - 1) <= WeightEpsilon &&
        MathF.Abs(value.Z - 1) <= WeightEpsilon &&
        MathF.Abs(value.W - 1) <= WeightEpsilon;

    private static uint PackArgb(Vector4 color)
    {
        byte red = Byte(color.X);
        byte green = Byte(color.Y);
        byte blue = Byte(color.Z);
        byte alpha = Byte(color.W);
        return (uint)(alpha << 24 | red << 16 | green << 8 | blue);
    }

    private static byte Byte(float value)
    {
        if (!float.IsFinite(value))
            throw new InvalidDataException("SMO material contains a non-finite color.");
        return (byte)Math.Clamp(
            (int)MathF.Round(Math.Clamp(value, 0, 1) * byte.MaxValue),
            0,
            byte.MaxValue);
    }
}
