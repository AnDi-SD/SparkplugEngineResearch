using System.Buffers.Binary;
using System.Numerics;
using SmoViewer.Core;

namespace SmoImporter.Core;

internal sealed record SmoVisualTransplantResult(
    byte[] Data,
    int MeshCount,
    int TextureCount,
    int DonorTextureCount,
    int VertexCount,
    int TriangleCount);

/// <summary>
/// Keeps the target FFPS object graph and replaces only its mesh and texture
/// leaf objects. Donor triangles are regrouped against the target's existing
/// skin palettes, and packed joint indices are remapped by exact bone name.
/// </summary>
internal static partial class SmoVisualTransplanter
{
    private const float WeightEpsilon = 0.000001f;

    public static SmoVisualTransplantResult Transplant(
        SmoDocument target,
        SmoDocument donor)
    {
        SmoBoneRemapPlan boneMapping = SmoBoneMappingPlanner.Build(target, donor);
        if (boneMapping.Errors.Count > 0)
            throw new InvalidOperationException(string.Join(" ", boneMapping.Errors));
        IReadOnlyDictionary<int, SmoTextureBinding> originalTargetBindings =
            SmoTextureBindingResolver.ResolveAll(target);
        IReadOnlyDictionary<int, SmoTextureBinding> donorBindings =
            SmoTextureBindingResolver.ResolveAll(donor);
        VisualGroup[] originalTargetGroups = BuildGroups(target, originalTargetBindings);
        VisualGroup[] donorGroups = BuildGroups(donor, donorBindings);
        if (originalTargetGroups.Length < donorGroups.Length)
            throw new InvalidOperationException(
                $"В target недостаточно texture groups: target={originalTargetGroups.Length}, " +
                $"donor={donorGroups.Length}.");

        IReadOnlyList<GroupPair> originalPairs = MatchGroups(
            originalTargetGroups, donorGroups);
        SmoDocument patchedTarget = PatchReferencePalettes(
            target, donor, originalPairs, boneMapping.DonorToTarget);
        VisualGroup[] targetGroups = BuildGroups(
            patchedTarget, SmoTextureBindingResolver.ResolveAll(patchedTarget));
        IReadOnlyList<GroupPair> pairs = MatchGroups(targetGroups, donorGroups);

        var replacements = new List<ObjectReplacement>();
        int vertices = 0;
        int triangles = 0;
        var matchedTargetTextures = new HashSet<int>();
        var matchedDonorTextures = new HashSet<int>();

        foreach (GroupPair pair in pairs)
        {
            VisualGroup targetGroup = pair.Target;
            VisualGroup donorGroup = pair.Donor;
            matchedTargetTextures.Add(targetGroup.TextureObjectIndex);
            matchedDonorTextures.UnionWith(donorGroup.SourceTextureObjectIndices);
            replacements.Add(new ObjectReplacement(
                patchedTarget.Objects[targetGroup.TextureObjectIndex],
                donorGroup.SerializedTextureData));

            IReadOnlyList<MeshPayload> payloads = BuildMeshPayloads(
                patchedTarget, targetGroup, donor, donorGroup,
                boneMapping.DonorToTarget);
            foreach (MeshPayload payload in payloads)
            {
                replacements.Add(new ObjectReplacement(payload.Target, payload.Data));
                vertices += payload.VertexCount;
                triangles += payload.TriangleCount;
            }
        }

        // The target graph may contain additional visual slots (Bloom eyes are a
        // common example). They cannot be removed without changing object IDs and
        // references, so keep the slots but replace their geometry with a valid
        // invisible triangle. Their texture identities stay in the graph while
        // their serialized payload receives the nearest donor texture as well.
        VisualGroup[] unmatchedGroups = targetGroups.Where(group =>
                !matchedTargetTextures.Contains(group.TextureObjectIndex))
            .ToArray();
        foreach (VisualGroup unmatched in unmatchedGroups)
        {
            VisualGroup donorTextureSource = donorGroups
                .OrderBy(group => Math.Abs(
                    group.TextureWidth * group.TextureHeight -
                    unmatched.TextureWidth * unmatched.TextureHeight))
                .ThenBy(group => group.TextureObjectIndex)
                .First();
            replacements.Add(new ObjectReplacement(
                patchedTarget.Objects[unmatched.TextureObjectIndex],
                donorTextureSource.SerializedTextureData));
            foreach (MeshPayload payload in BuildEmptyMeshPayloads(patchedTarget, unmatched))
                replacements.Add(new ObjectReplacement(payload.Target, payload.Data));
        }

        int donorTextureCount = donor.Objects.Count(entry =>
            entry.TypeHash == SmoClassIds.TextureData);
        if (matchedDonorTextures.Count != donorTextureCount)
            throw new InvalidOperationException(
                "Не все texture-объекты донора имеют однозначную mesh binding.");

        byte[] output = Repack(patchedTarget, replacements);
        return new SmoVisualTransplantResult(
            output,
            target.Objects.Count(item => item.TypeHash == SmoClassIds.MeshData),
            target.Objects.Count(item => item.TypeHash == SmoClassIds.TextureData),
            donorTextureCount,
            vertices,
            triangles);
    }

    private static IReadOnlyList<GroupPair> MatchGroups(
        IReadOnlyList<VisualGroup> targets,
        IReadOnlyList<VisualGroup> donors)
    {
        var matchedTargets = new HashSet<int>();
        var result = new List<GroupPair>();
        foreach (VisualGroup donor in donors
                     .OrderBy(group => group.TextureWidth * group.TextureHeight)
                     .ThenBy(group => group.Meshes.Count))
        {
            VisualGroup[] candidates = targets.Where(group =>
                    !matchedTargets.Contains(group.TextureObjectIndex))
                .OrderByDescending(group =>
                    group.TextureWidth == donor.TextureWidth &&
                    group.TextureHeight == donor.TextureHeight)
                .ThenBy(group => Math.Abs(group.Meshes.Count - donor.Meshes.Count))
                .ThenBy(group => Math.Abs(
                    group.TextureWidth * group.TextureHeight -
                    donor.TextureWidth * donor.TextureHeight))
                .ThenBy(group => group.TextureObjectIndex)
                .ToArray();
            if (candidates.Length == 0)
                throw new InvalidOperationException(
                    $"Не удалось сопоставить donor texture " +
                    $"\"{donor.TextureName}\" {donor.TextureWidth}×" +
                    $"{donor.TextureHeight} с texture group целевого SMO.");
            matchedTargets.Add(candidates[0].TextureObjectIndex);
            result.Add(new GroupPair(candidates[0], donor));
        }
        return result;
    }

    private static SmoDocument PatchReferencePalettes(
        SmoDocument target,
        SmoDocument donor,
        IReadOnlyList<GroupPair> pairs,
        IReadOnlyDictionary<string, string> boneRemap)
    {
        byte[] patched = target.Data.ToArray();
        Dictionary<string, SmoObjectEntry> targetNodes = target.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Node &&
                !string.IsNullOrWhiteSpace(entry.Name))
            .GroupBy(entry => entry.Name, StringComparer.Ordinal)
            .Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single(),
                StringComparer.Ordinal);
        var rewrittenSkins = new HashSet<int>();

        foreach (GroupPair pair in pairs.Where(value => value.Target.Meshes.Count > 1))
        {
            IReadOnlyList<IReadOnlyList<string>> palettes = PlanReferencePalettes(
                target, pair.Target, donor, pair.Donor,
                pair.Target.Meshes.Count - 1, boneRemap);
            for (int ordinal = 1; ordinal < pair.Target.Meshes.Count; ordinal++)
            {
                MeshSource targetMesh = pair.Target.Meshes[ordinal];
                if (targetMesh.Skin.Bones.Any(bone => bone.InlineSerializedSize != 0))
                    throw new InvalidOperationException(
                        $"Target skin [{targetMesh.SkinEntry.Index}] содержит inline skeleton " +
                        "и не может быть безопасно перестроен.");
                if (targetMesh.Skin.Bones.Count != palettes[ordinal - 1].Count)
                    throw new InvalidOperationException(
                        $"Размер target skin palette [{targetMesh.SkinEntry.Index}] " +
                        "не совпадает с построенным планом.");
                PatchPalette(
                    patched, target, targetMesh.SkinEntry, palettes[ordinal - 1],
                    donor, targetNodes);
                rewrittenSkins.Add(targetMesh.SkinEntry.Index);
            }
        }
        foreach (SmoObjectEntry skinEntry in target.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Skin &&
                     !rewrittenSkins.Contains(entry.Index)))
        {
            if (!SmoSkinDecoder.TryDecode(
                    target, skinEntry, out SmoSkin? skin, out _) || skin is null)
                continue;
            string[] names = skin.Bones.Select(bone =>
                target.Objects[bone.NodeObjectIndex].Name).ToArray();
            PatchPalette(patched, target, skinEntry, names, donor, targetNodes);
        }
        return SmoDocument.Parse(patched, target.SourcePath);
    }

    private static IReadOnlyList<IReadOnlyList<string>> PlanReferencePalettes(
        SmoDocument targetDocument,
        VisualGroup target,
        SmoDocument donorDocument,
        VisualGroup donor,
        int writablePaletteCount,
        IReadOnlyDictionary<string, string> boneRemap)
    {
        HashSet<string> fixedPalette = target.Meshes[0].Skin.Bones
            .Select(bone => targetDocument.Objects[bone.NodeObjectIndex].Name)
            .ToHashSet(StringComparer.Ordinal);
        HashSet<string>[] triangleSets = donor.Meshes.SelectMany(mesh =>
                Enumerable.Range(0, mesh.Mesh.TriangleIndices.Length / 3)
                    .Select(triangle => GetTriangleBones(
                        donorDocument, mesh,
                        [
                            checked((int)mesh.Mesh.TriangleIndices[triangle * 3]),
                            checked((int)mesh.Mesh.TriangleIndices[triangle * 3 + 1]),
                            checked((int)mesh.Mesh.TriangleIndices[triangle * 3 + 2])
                        ], boneRemap)))
            .Where(bones => !bones.All(fixedPalette.Contains))
            .GroupBy(
                bones => string.Join("\0", bones.Order(StringComparer.Ordinal)),
                StringComparer.Ordinal)
            .Select(group => group.First())
            .OrderByDescending(bones => bones.Count)
            .ThenBy(
                bones => string.Join("\0", bones.Order(StringComparer.Ordinal)),
                StringComparer.Ordinal)
            .ToArray();

        HashSet<string>[] donorPalettes = donor.Meshes.Select(mesh =>
                mesh.Skin.Bones.Select(bone =>
                        donorDocument.Objects[bone.NodeObjectIndex].Name)
                    .Where(boneRemap.ContainsKey)
                    .Select(name => RemapBoneName(name, boneRemap))
                    .ToHashSet(StringComparer.Ordinal))
            .ToArray();
        if (donorPalettes.Length == writablePaletteCount + 1)
        {
            for (int omitted = 0; omitted < donorPalettes.Length; omitted++)
            {
                HashSet<string>[] selected = donorPalettes.Where((_, index) =>
                    index != omitted).ToArray();
                if (triangleSets.All(triangle =>
                        selected.Any(palette => triangle.All(palette.Contains))))
                {
                    return selected.Select(palette =>
                    {
                        var names = palette.Order(StringComparer.Ordinal).ToList();
                        while (names.Count < 16)
                            names.Add(names[0]);
                        return (IReadOnlyList<string>)names;
                    }).ToArray();
                }
            }
        }

        HashSet<string>[] maximalSets = triangleSets
            .Where((candidate, index) => !triangleSets.Where((_, other) => other != index)
                .Any(other => candidate.IsProperSubsetOf(other)))
            .OrderByDescending(set => set.Count)
            .ToArray();
        var bins = Enumerable.Range(0, writablePaletteCount)
            .Select(_ => new HashSet<string>(StringComparer.Ordinal)).ToArray();
        var failedStates = new HashSet<string>(StringComparer.Ordinal);
        if (!TryPack(0))
            throw new InvalidOperationException(
                $"Geometry donor требует больше {writablePaletteCount} изменяемых " +
                "16-bone palettes при сохранении корневой palette target.");

        bool TryPack(int setIndex)
        {
            if (setIndex == maximalSets.Length)
                return true;
            string state = setIndex + "|" + string.Join("|", bins
                .Select(bin => string.Join(",", bin.Order(StringComparer.Ordinal)))
                .Order(StringComparer.Ordinal));
            if (!failedStates.Add(state))
                return false;

            HashSet<string> required = maximalSets[setIndex];
            string? previousSignature = null;
            foreach (int binIndex in Enumerable.Range(0, bins.Length)
                         .Where(index => bins[index].Union(required)
                             .Distinct(StringComparer.Ordinal).Count() <= 16)
                         .OrderBy(index => required.Count(name => !bins[index].Contains(name)))
                         .ThenByDescending(index => required.Count(bins[index].Contains)))
            {
                HashSet<string> bin = bins[binIndex];
                string signature = string.Join(",", bin.Order(StringComparer.Ordinal));
                if (signature == previousSignature)
                    continue;
                previousSignature = signature;
                string[] added = required.Where(name => !bin.Contains(name)).ToArray();
                bin.UnionWith(added);
                if (TryPack(setIndex + 1))
                    return true;
                bin.ExceptWith(added);
            }
            return false;
        }

        string fallback = donor.Meshes.SelectMany(mesh => mesh.Skin.Bones)
            .Select(bone => RemapBoneName(
                donorDocument.Objects[bone.NodeObjectIndex].Name, boneRemap))
            .First(name => targetDocument.Objects.Any(entry =>
                entry.TypeHash == SmoClassIds.Node && entry.Name == name));
        foreach (HashSet<string> bin in bins)
            if (bin.Count == 0)
                bin.Add(fallback);

        return bins.Select(bin =>
        {
            var names = bin.Order(StringComparer.Ordinal).ToList();
            if (names.Count == 0)
                names.Add(fallback);
            while (names.Count < 16)
                names.Add(names[0]);
            return (IReadOnlyList<string>)names;
        }).ToArray();
    }

    private static void PatchPalette(
        Span<byte> output,
        SmoDocument target,
        SmoObjectEntry targetSkin,
        IReadOnlyList<string> paletteNames,
        SmoDocument donor,
        IReadOnlyDictionary<string, SmoObjectEntry> targetNodes)
    {
        ReadOnlySpan<byte> serialized = target.Data.Span.Slice(
            checked((int)targetSkin.PhysicalOffset), checked((int)targetSkin.SerializedSize));
        int fieldOffset = 8;
        SmoDataBlockHeader palette = default;
        bool found = false;
        while (fieldOffset < serialized.Length &&
               SmoDataBlockReader.TryReadHeader(
                   serialized, fieldOffset, out SmoDataBlockHeader header))
        {
            if (header.FieldType == 0 && header.PayloadSize >= 8)
            {
                ReadOnlySpan<byte> payload = serialized.Slice(
                    header.PayloadOffset, checked((int)header.PayloadSize));
                if (BinaryPrimitives.ReadUInt32LittleEndian(payload) == 0 &&
                    BinaryPrimitives.ReadUInt32LittleEndian(payload[4..]) ==
                    paletteNames.Count)
                {
                    palette = header;
                    found = true;
                }
            }
            fieldOffset = checked((int)header.PayloadEnd);
        }
        if (!found)
            throw new InvalidOperationException(
                $"Palette field target skin [{targetSkin.Index}] не найден.");

        IReadOnlyDictionary<string, Matrix4x4> donorMatrices = donor.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Skin)
            .Select(entry => SmoSkinDecoder.TryDecode(
                donor, entry, out SmoSkin? skin, out _) ? skin : null)
            .Where(skin => skin is not null)
            .SelectMany(skin => skin!.Bones)
            .GroupBy(
                bone => donor.Objects[bone.NodeObjectIndex].Name,
                StringComparer.Ordinal)
            .ToDictionary(
                group => group.Key,
                group => group.First().InverseBindMatrix,
                StringComparer.Ordinal);
        int cursor = checked((int)targetSkin.PhysicalOffset + palette.PayloadOffset + 8);
        for (int paletteIndex = 0; paletteIndex < paletteNames.Count; paletteIndex++)
        {
            string name = paletteNames[paletteIndex];
            if (!targetNodes.TryGetValue(name, out SmoObjectEntry? targetNode))
                throw new InvalidOperationException(
                    $"Bone \"{name}\" from donor palette " +
                    "is absent from target node graph.");
            uint inlineSize = BinaryPrimitives.ReadUInt32LittleEndian(output[(cursor + 4)..]);
            WriteUInt32(output, cursor, targetNode.Id);
            cursor = checked(cursor + 8 + (int)inlineSize);
            // A target may contain an animation/helper node that is not weighted by
            // any donor vertex.  In that case there is no donor inverse-bind matrix
            // to transplant and the original target matrix is the only valid value.
            if (donorMatrices.TryGetValue(name, out Matrix4x4 donorMatrix))
            {
                WriteMatrix(output.Slice(cursor, 16 * sizeof(float)), donorMatrix);
            }
            cursor += 16 * sizeof(float);
        }
    }

    private static void WriteMatrix(Span<byte> data, Matrix4x4 value)
    {
        float[] cells =
        [
            value.M11, value.M12, value.M13, value.M14,
            value.M21, value.M22, value.M23, value.M24,
            value.M31, value.M32, value.M33, value.M34,
            value.M41, value.M42, value.M43, value.M44
        ];
        for (int index = 0; index < cells.Length; index++)
            BinaryPrimitives.WriteInt32LittleEndian(
                data[(index * sizeof(float))..],
                BitConverter.SingleToInt32Bits(cells[index]));
    }

    private static VisualGroup[] BuildGroups(
        SmoDocument document,
        IReadOnlyDictionary<int, SmoTextureBinding> bindings)
    {
        var groups = new Dictionary<int, List<MeshSource>>();
        var textures = new Dictionary<int, SmoTexture>();
        foreach (SmoObjectEntry meshEntry in document.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.MeshData))
        {
            if (!bindings.TryGetValue(meshEntry.Index, out SmoTextureBinding? binding) ||
                binding.Issue is not null || binding.Texture is null)
            {
                throw new InvalidOperationException(
                    $"Mesh [{meshEntry.Index}] \"{meshEntry.Name}\" не имеет " +
                    "однозначной texture binding.");
            }
            SmoObjectEntry skinEntry = FindParentSkin(document, meshEntry);
            if (!SmoSkinDecoder.TryDecode(
                    document, skinEntry, out SmoSkin? skin, out string error) || skin is null)
                throw new InvalidOperationException(error);
            SmoMesh mesh = SmoMeshDecoder.Decode(document, meshEntry);
            if (mesh.Marker != SmoMeshDecoder.E1Marker || !mesh.HasSkinningData ||
                !SmoVertexLayoutRegistry.TryGet(mesh.VertexFormat, out SmoVertexLayout? layout) ||
                layout?.BlendIndicesOffset is null || layout.SerializedStride != mesh.Stride)
            {
                throw new InvalidOperationException(
                    $"Mesh [{meshEntry.Index}] должен использовать подтверждённый " +
                    "skinned E1 vertex layout.");
            }

            int textureIndex = binding.Texture.ObjectIndex;
            if (!groups.TryGetValue(textureIndex, out List<MeshSource>? meshes))
            {
                meshes = [];
                groups.Add(textureIndex, meshes);
                textures.Add(textureIndex, binding.Texture);
            }
            meshes.Add(new MeshSource(
                meshEntry, skinEntry, mesh, skin, layout,
                mesh.HasNormals ? mesh.Normals : GenerateSmoothNormals(mesh),
                Vector2.One, Vector2.Zero));
        }

        return groups.Select(pair =>
        {
            SmoTexture texture = textures[pair.Key];
            return new VisualGroup(
                texture.ObjectIndex,
                texture.Name,
                texture.FormatCode,
                texture.SourceLayout,
                texture.Width,
                texture.Height,
                texture.Bgra32Pixels.ToArray(),
                CopyObject(document, document.Objects[texture.ObjectIndex]),
                new HashSet<int> { texture.ObjectIndex },
                pair.Value.OrderBy(item => item.Entry.Index).ToArray());
        })
            .ToArray();
    }

    private static IReadOnlyList<MeshPayload> BuildMeshPayloads(
        SmoDocument targetDocument,
        VisualGroup target,
        SmoDocument donorDocument,
        VisualGroup donor,
        IReadOnlyDictionary<string, string> boneRemap)
    {
        TargetSlot[] slots = target.Meshes.Select(source =>
                new TargetSlot(targetDocument, source, boneRemap))
            .ToArray();
        for (int donorOrdinal = 0; donorOrdinal < donor.Meshes.Count; donorOrdinal++)
        {
            MeshSource donorMesh = donor.Meshes[donorOrdinal];
            for (int index = 0; index < donorMesh.Mesh.TriangleIndices.Length; index += 3)
            {
                int[] sourceVertices =
                [
                    checked((int)donorMesh.Mesh.TriangleIndices[index]),
                    checked((int)donorMesh.Mesh.TriangleIndices[index + 1]),
                    checked((int)donorMesh.Mesh.TriangleIndices[index + 2])
                ];
                HashSet<string> bones = GetTriangleBones(
                    donorDocument, donorMesh, sourceVertices, boneRemap);
                TargetSlot[] candidates = slots.Where(slot =>
                    bones.All(slot.PaletteByName.ContainsKey)).ToArray();
                if (candidates.Length == 0)
                {
                    throw new InvalidOperationException(
                        $"Triangle {index / 3} donor mesh [{donorMesh.Entry.Index}] " +
                        $"использует набор костей, не помещающийся ни в одну target palette: " +
                        string.Join(", ", bones.Order(StringComparer.Ordinal)) + ".");
                }

                TargetSlot selected = candidates
                    .OrderByDescending(slot => slot ==
                        slots[Math.Min(donorOrdinal, slots.Length - 1)])
                    .ThenBy(slot => slot.TriangleCount)
                    .First();
                selected.AddTriangle(donorDocument, donorMesh, sourceVertices);
            }
        }

        var result = new List<MeshPayload>(slots.Length);
        foreach (TargetSlot slot in slots)
        {
            if (slot.TriangleCount == 0)
                slot.AddDegenerate(donorDocument, donor.Meshes[0]);
            byte[] data = BuildMeshObject(slot);
            result.Add(new MeshPayload(
                slot.Source.Entry, data, slot.VertexRecords.Count, slot.RealTriangleCount));
        }
        return result;
    }

    private static Vector3[] GenerateSmoothNormals(SmoMesh mesh)
    {
        var normals = new Vector3[mesh.VertexCount];
        for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
        {
            int first = checked((int)mesh.TriangleIndices[index]);
            int second = checked((int)mesh.TriangleIndices[index + 1]);
            int third = checked((int)mesh.TriangleIndices[index + 2]);
            Vector3 face = Vector3.Cross(
                mesh.Positions[second] - mesh.Positions[first],
                mesh.Positions[third] - mesh.Positions[first]);
            if (!float.IsFinite(face.LengthSquared()) || face.LengthSquared() <= 1e-20f)
                continue;
            normals[first] += face;
            normals[second] += face;
            normals[third] += face;
        }
        for (int index = 0; index < normals.Length; index++)
            normals[index] = normals[index].LengthSquared() > 1e-20f
                ? Vector3.Normalize(normals[index])
                : Vector3.UnitY;
        return normals;
    }

    private static IReadOnlyList<MeshPayload> BuildEmptyMeshPayloads(
        SmoDocument targetDocument,
        VisualGroup target)
    {
        var result = new List<MeshPayload>(target.Meshes.Count);
        foreach (MeshSource source in target.Meshes)
        {
            IReadOnlyDictionary<string, string> identity = source.Skin.Bones
                .Select(bone => targetDocument.Objects[bone.NodeObjectIndex].Name)
                .Distinct(StringComparer.Ordinal)
                .ToDictionary(name => name, name => name, StringComparer.Ordinal);
            var slot = new TargetSlot(targetDocument, source, identity);
            slot.AddDegenerate(targetDocument, source);
            result.Add(new MeshPayload(
                source.Entry, BuildMeshObject(slot), slot.VertexRecords.Count, 0));
        }
        return result;
    }

    private static HashSet<string> GetTriangleBones(
        SmoDocument document,
        MeshSource source,
        IReadOnlyList<int> vertices,
        IReadOnlyDictionary<string, string> boneRemap)
    {
        var result = new HashSet<string>(StringComparer.Ordinal);
        foreach (int vertex in vertices)
        {
            Vector4 weights = source.Mesh.BlendWeights[vertex];
            SmoBlendIndices indices = source.Mesh.BlendIndices[vertex];
            Add(weights.X, indices.X);
            Add(weights.Y, indices.Y);
            Add(weights.Z, indices.Z);
            Add(weights.W, indices.W);
        }
        return result;

        void Add(float weight, byte paletteIndex)
        {
            if (weight <= WeightEpsilon)
                return;
            if (paletteIndex >= source.Skin.Bones.Count)
                throw new InvalidDataException(
                    $"Mesh [{source.Entry.Index}] uses palette slot {paletteIndex} " +
                    $"outside skin [{source.SkinEntry.Index}].");
            SmoSkinBone bone = source.Skin.Bones[paletteIndex];
            result.Add(RemapBoneName(
                document.Objects[bone.NodeObjectIndex].Name, boneRemap));
        }
    }

    private static string RemapBoneName(
        string donorBoneName,
        IReadOnlyDictionary<string, string> boneRemap) =>
        boneRemap.TryGetValue(donorBoneName, out string? targetBoneName)
            ? targetBoneName
            : throw new InvalidOperationException(
                $"Donor bone \"{donorBoneName}\" has no target mapping.");

    private static byte[] BuildMeshObject(TargetSlot slot)
    {
        MeshSource template = slot.Source;
        int vertexCount = slot.VertexRecords.Count;
        int indexCount = slot.Indices.Count;
        if (vertexCount > ushort.MaxValue)
            throw new InvalidOperationException(
                $"Target mesh [{template.Entry.Index}] exceeds 65,535 vertices.");
        int indexBytes = checked(indexCount * sizeof(ushort));
        int vertexBytes = checked(vertexCount * template.Mesh.Stride);
        const int preambleSize = 17;
        const int primitiveHeaderSize = 12;
        const int vertexHeaderSize = 12;
        int payloadSize = checked(
            preambleSize + primitiveHeaderSize + indexBytes + vertexHeaderSize + vertexBytes);
        byte[] result = new byte[checked(8 + 5 + payloadSize + 1)];
        WriteUInt32(result, 0, SmoClassIds.MeshData);
        "SBOO"u8.CopyTo(result.AsSpan(4));
        result[8] = SmoMeshDecoder.E1Marker;
        WriteUInt32(result, 9, (uint)payloadSize);
        int payload = 13;
        WriteUInt32(result, payload, template.Mesh.VertexFormat);
        WriteUInt32(result, payload + 4, (uint)vertexCount);
        WriteUInt32(result, payload + 8,
            checked((uint)(vertexCount * template.Mesh.RuntimeStride)));
        WriteUInt32(result, payload + 12, (uint)indexBytes);
        result[payload + 16] = 0;
        int primitive = payload + preambleSize;
        WriteUInt32(result, primitive, SmoMeshDecoder.TriangleListPrimitive);
        WriteUInt32(result, primitive + 4, checked((uint)(indexCount / 3)));
        WriteUInt32(result, primitive + 8, 0);
        int indices = primitive + primitiveHeaderSize;
        for (int index = 0; index < indexCount; index++)
            WriteUInt16(result, indices + index * sizeof(ushort), slot.Indices[index]);
        int vertexHeader = indices + indexBytes;
        WriteUInt32(result, vertexHeader, template.Mesh.VertexFormat);
        WriteUInt32(result, vertexHeader + 4, (uint)vertexCount);
        WriteUInt32(result, vertexHeader + 8, 0);
        int vertices = vertexHeader + vertexHeaderSize;
        foreach (byte[] record in slot.VertexRecords)
        {
            record.CopyTo(result.AsSpan(vertices));
            vertices += record.Length;
        }
        return result;
    }

    private static byte[] Repack(
        SmoDocument document,
        IReadOnlyList<ObjectReplacement> replacements)
    {
        ObjectReplacement[] ordered = replacements
            .OrderBy(item => item.Entry.PhysicalOffset).ToArray();
        for (int index = 1; index < ordered.Length; index++)
            if (ordered[index].Entry.PhysicalOffset < ordered[index - 1].Entry.PhysicalEnd)
                throw new InvalidOperationException("Visual replacement intervals overlap.");

        byte[] source = document.Data.ToArray();
        long finalLength = source.LongLength + ordered.Sum(item =>
            (long)item.Data.Length - item.Entry.SerializedSize);
        byte[] result = new byte[checked((int)finalLength)];
        int sourceCursor = 0;
        int targetCursor = 0;
        foreach (ObjectReplacement replacement in ordered)
        {
            int start = checked((int)replacement.Entry.PhysicalOffset);
            int end = checked((int)replacement.Entry.PhysicalEnd);
            source.AsSpan(sourceCursor, start - sourceCursor)
                .CopyTo(result.AsSpan(targetCursor));
            targetCursor += start - sourceCursor;
            replacement.Data.CopyTo(result.AsSpan(targetCursor));
            targetCursor += replacement.Data.Length;
            sourceCursor = end;
        }
        source.AsSpan(sourceCursor).CopyTo(result.AsSpan(targetCursor));

        long Map(long oldOffset) => oldOffset + ordered
            .Where(item => item.Entry.PhysicalEnd <= oldOffset)
            .Sum(item => (long)item.Data.Length - item.Entry.SerializedSize);
        HashSet<int> replacedIndices = ordered.Select(item => item.Entry.Index).ToHashSet();
        foreach (SmoObjectEntry entry in document.Objects)
        {
            long newStart = Map(entry.PhysicalOffset);
            long newEnd = Map(entry.PhysicalEnd);
            uint newSize = checked((uint)(newEnd - newStart));
            int logicalOffsetField = entry.TableOffset + sizeof(uint) + sizeof(ushort) +
                entry.NameLength + sizeof(uint);
            WriteUInt32(result, logicalOffsetField,
                checked((uint)(newStart - document.Header.DataStart)));
            WriteUInt32(result, logicalOffsetField + sizeof(uint), newSize);

            if (!replacedIndices.Contains(entry.Index) && newSize != entry.SerializedSize)
            {
                int objectStart = checked((int)newStart);
                ReadOnlySpan<byte> originalObject = source.AsSpan(
                    checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize));
                if (SmoDataBlockReader.TryReadHeader(
                        originalObject, 8, out SmoDataBlockHeader outer) &&
                    outer.PayloadEnd + 1 == entry.SerializedSize)
                {
                    if (outer.SizeKind != SmoDataBlockSizeCode.UInt32)
                        throw new InvalidOperationException(
                            $"Resized parent object [{entry.Index}] has a non-writable size field.");
                    WriteUInt32(result,
                        objectStart + outer.Offset + outer.HeaderSize - sizeof(uint),
                        checked(newSize - (uint)(8 + outer.HeaderSize + 1)));
                }
            }
        }

        // Every ancestor field whose payload contains a replaced leaf changes
        // size. Walk the original top-level fields of each catalog object and
        // patch their encoded payload sizes by the enclosed leaf deltas.
        foreach (SmoObjectEntry entry in document.Objects.Where(item =>
                     !replacedIndices.Contains(item.Index)))
        {
            ReadOnlySpan<byte> serialized = source.AsSpan(
                checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize));
            int fieldOffset = 8;
            while (fieldOffset < serialized.Length &&
                   SmoDataBlockReader.TryReadHeader(
                       serialized, fieldOffset, out SmoDataBlockHeader field))
            {
                long payloadStart = entry.PhysicalOffset + field.PayloadOffset;
                long payloadEnd = entry.PhysicalOffset + field.PayloadEnd;
                long delta = ordered.Where(replacement =>
                        replacement.Entry.PhysicalOffset >= payloadStart &&
                        replacement.Entry.PhysicalEnd <= payloadEnd)
                    .Sum(replacement =>
                        (long)replacement.Data.Length - replacement.Entry.SerializedSize);
                if (delta != 0)
                {
                    uint newPayloadSize = checked((uint)(field.PayloadSize + delta));
                    WritePayloadSize(
                        result,
                        checked((int)Map(entry.PhysicalOffset + field.Offset)),
                        field,
                        newPayloadSize);
                }
                int next = checked((int)field.PayloadEnd);
                if (next <= fieldOffset)
                    break;
                fieldOffset = next;
            }
        }

        // Catalog nesting is also mirrored by inline object-reference prefixes:
        // [object ID][inline serialized size][object bytes]. Patch those sizes;
        // the containing data-block field was handled above (a skin palette can
        // contain several references, so it does not necessarily end at child).
        foreach (SmoObjectEntry entry in document.Objects
                     .OrderByDescending(item => item.NestingDepth))
        {
            long newStart = Map(entry.PhysicalOffset);
            long newEnd = Map(entry.PhysicalEnd);
            uint newSize = checked((uint)(newEnd - newStart));
            if (newSize == entry.SerializedSize || entry.ParentIndex is null ||
                entry.PhysicalOffset < 8)
                continue;

            int oldPrefix = checked((int)entry.PhysicalOffset - 8);
            if (BinaryPrimitives.ReadUInt32LittleEndian(source.AsSpan(oldPrefix)) != entry.Id ||
                BinaryPrimitives.ReadUInt32LittleEndian(source.AsSpan(oldPrefix + 4)) !=
                entry.SerializedSize)
                continue; // Referenced, rather than serialized inline.

            int newPrefix = checked((int)Map(oldPrefix));
            WriteUInt32(result, newPrefix + 4, newSize);
        }
        WriteUInt32(result, 0x0C, checked((uint)result.Length));
        WriteUInt32(result, 0x18,
            checked((uint)result.Length - document.Header.DataStart));
        return result;
    }

    private static void WritePayloadSize(
        Span<byte> data,
        int headerOffset,
        SmoDataBlockHeader original,
        uint value)
    {
        int sizeOffset = headerOffset + original.HeaderSize;
        switch (original.SizeKind)
        {
            case SmoDataBlockSizeCode.UInt8:
                data[sizeOffset - 1] = checked((byte)value);
                break;
            case SmoDataBlockSizeCode.UInt16:
                BinaryPrimitives.WriteUInt16LittleEndian(
                    data[(sizeOffset - sizeof(ushort))..], checked((ushort)value));
                break;
            case SmoDataBlockSizeCode.UInt32:
                WriteUInt32(data, sizeOffset - sizeof(uint), value);
                break;
            default:
                throw new InvalidOperationException(
                    $"Resized inline field uses non-writable size form {original.SizeKind}.");
        }
    }

    private static SmoObjectEntry FindParentSkin(
        SmoDocument document,
        SmoObjectEntry mesh)
    {
        SmoObjectEntry cursor = mesh;
        while (cursor.ParentIndex is int parentIndex)
        {
            cursor = document.Objects[parentIndex];
            if (cursor.TypeHash == SmoClassIds.Skin)
                return cursor;
        }
        throw new InvalidOperationException(
            $"Mesh [{mesh.Index}] is not nested in an spSkin object.");
    }

    private static byte[] CopyObject(SmoDocument document, SmoObjectEntry entry) =>
        document.Data.Span.Slice(
            checked((int)entry.PhysicalOffset), checked((int)entry.SerializedSize)).ToArray();

    private static void WriteUInt16(Span<byte> data, int offset, ushort value) =>
        BinaryPrimitives.WriteUInt16LittleEndian(data[offset..], value);

    private static void WriteUInt32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], value);

    private sealed record ObjectReplacement(SmoObjectEntry Entry, byte[] Data);

    private sealed record MeshPayload(
        SmoObjectEntry Target,
        byte[] Data,
        int VertexCount,
        int TriangleCount);

    private sealed record MeshSource(
        SmoObjectEntry Entry,
        SmoObjectEntry SkinEntry,
        SmoMesh Mesh,
        SmoSkin Skin,
        SmoVertexLayout Layout,
        Vector3[] TransferNormals,
        Vector2 UvScale,
        Vector2 UvOffset);

    private sealed record VisualGroup(
        int TextureObjectIndex,
        string TextureName,
        ushort TextureFormat,
        SmoTextureLayout TextureLayout,
        int TextureWidth,
        int TextureHeight,
        byte[] BgraPixels,
        byte[] SerializedTextureData,
        IReadOnlySet<int> SourceTextureObjectIndices,
        IReadOnlyList<MeshSource> Meshes);

    private sealed record GroupPair(VisualGroup Target, VisualGroup Donor);

    private sealed class TargetSlot
    {
        private readonly Dictionary<(int Mesh, int Vertex), ushort> _vertices = [];
        private readonly IReadOnlyDictionary<string, string> _boneRemap;

        public TargetSlot(
            SmoDocument document,
            MeshSource source,
            IReadOnlyDictionary<string, string> boneRemap)
        {
            Source = source;
            _boneRemap = boneRemap;
            PaletteByName = source.Skin.Bones
                .GroupBy(
                    bone => document.Objects[bone.NodeObjectIndex].Name,
                    StringComparer.Ordinal)
                .ToDictionary(
                    group => group.Key,
                    group => checked((byte)group.First().PaletteIndex),
                    StringComparer.Ordinal);
        }

        public MeshSource Source { get; }
        public IReadOnlyDictionary<string, byte> PaletteByName { get; }
        public List<byte[]> VertexRecords { get; } = [];
        public List<ushort> Indices { get; } = [];
        public int TriangleCount => Indices.Count / 3;
        public int RealTriangleCount { get; private set; }

        public void AddTriangle(
            SmoDocument donorDocument,
            MeshSource donor,
            IReadOnlyList<int> vertices)
        {
            foreach (int vertex in vertices)
                Indices.Add(GetOrAddVertex(donorDocument, donor, vertex));
            RealTriangleCount++;
        }

        public void AddDegenerate(SmoDocument donorDocument, MeshSource donor)
        {
            int compatibleVertex = Enumerable.Range(0, donor.Mesh.VertexCount)
                .FirstOrDefault(vertex => VertexFits(donorDocument, donor, vertex));
            ushort index = GetOrAddVertex(
                donorDocument, donor, compatibleVertex, allowRigidFallback: true);
            Indices.Add(index);
            Indices.Add(index);
            Indices.Add(index);
        }

        private bool VertexFits(
            SmoDocument document, MeshSource donor, int vertex)
        {
            Vector4 weights = donor.Mesh.BlendWeights[vertex];
            SmoBlendIndices indices = donor.Mesh.BlendIndices[vertex];
            return Fits(weights.X, indices.X) && Fits(weights.Y, indices.Y) &&
                   Fits(weights.Z, indices.Z) && Fits(weights.W, indices.W);

            bool Fits(float weight, byte slot) => weight <= WeightEpsilon ||
                slot < donor.Skin.Bones.Count && PaletteByName.ContainsKey(
                    RemapBoneName(
                        document.Objects[donor.Skin.Bones[slot].NodeObjectIndex].Name,
                        _boneRemap));
        }

        private ushort GetOrAddVertex(
            SmoDocument donorDocument,
            MeshSource donor,
            int vertex,
            bool allowRigidFallback = false)
        {
            var key = (donor.Entry.Index, vertex);
            if (_vertices.TryGetValue(key, out ushort existing))
                return existing;
            if (VertexRecords.Count >= ushort.MaxValue)
                throw new InvalidOperationException(
                    $"Target mesh [{Source.Entry.Index}] exceeds UInt16 vertex capacity.");

            byte[] record = new byte[Source.Layout.SerializedStride];
            WriteVector3(record, 0, donor.Mesh.Positions[vertex]);
            if (Source.Layout.NormalOffset is int normalOffset)
                WriteVector3(record, normalOffset, donor.TransferNormals[vertex]);
            if (Source.Layout.DiffuseArgbOffset is int diffuseOffset)
            {
                uint diffuse = donor.Mesh.HasDiffuseColors
                    ? donor.Mesh.DiffuseColorsArgb[vertex]
                    : 0xFFFFFFFF;
                WriteUInt32(record, diffuseOffset, diffuse);
            }
            Vector2 uv0 = donor.Mesh.HasTextureCoordinates
                ? donor.Mesh.TextureCoordinates[vertex] * donor.UvScale + donor.UvOffset
                : Vector2.Zero;
            if (Source.Layout.TextureCoordinate0Offset is int uv0Offset)
                WriteVector2(record, uv0Offset, uv0);
            if (Source.Layout.TextureCoordinate1Offset is int uv1Offset)
            {
                Vector2 uv1 = donor.Mesh.HasTextureCoordinates1
                    ? donor.Mesh.TextureCoordinates1[vertex] * donor.UvScale + donor.UvOffset
                    : uv0;
                WriteVector2(record, uv1Offset, uv1);
            }

            int weightsOffset = Source.Layout.BlendWeightsOffset!.Value;
            int indicesOffset = Source.Layout.BlendIndicesOffset!.Value;
            Vector4 weights = donor.Mesh.BlendWeights[vertex];
            SmoBlendIndices indices = donor.Mesh.BlendIndices[vertex];
            var mapped = new Dictionary<byte, float>();
            if (allowRigidFallback && !VertexFits(donorDocument, donor, vertex))
            {
                WriteVector4(record, weightsOffset, new Vector4(1, 0, 0, 0));
                record.AsSpan(indicesOffset, 4).Clear();
            }
            else
            {
                AddInfluence(weights.X, indices.X);
                AddInfluence(weights.Y, indices.Y);
                AddInfluence(weights.Z, indices.Z);
                AddInfluence(weights.W, indices.W);
                (byte Slot, float Weight)[] influences = mapped
                    .OrderByDescending(pair => pair.Value)
                    .ThenBy(pair => pair.Key)
                    .Select(pair => (pair.Key, pair.Value))
                    .Take(4)
                    .ToArray();
                float total = influences.Sum(item => item.Weight);
                if (total <= WeightEpsilon)
                    influences = [(0, 1f)];
                else
                    influences = influences.Select(item =>
                        (item.Slot, item.Weight / total)).ToArray();
                float[] packedWeights = new float[4];
                byte[] packedIndices = new byte[4];
                for (int influence = 0; influence < influences.Length; influence++)
                {
                    packedIndices[influence] = influences[influence].Slot;
                    packedWeights[influence] = influences[influence].Weight;
                }
                WriteVector4(record, weightsOffset, new Vector4(
                    packedWeights[0], packedWeights[1],
                    packedWeights[2], packedWeights[3]));
                packedIndices.CopyTo(record, indicesOffset);
            }

            ushort created = checked((ushort)VertexRecords.Count);
            VertexRecords.Add(record);
            _vertices.Add(key, created);
            return created;

            void AddInfluence(float weight, byte donorSlot)
            {
                if (weight <= WeightEpsilon)
                    return;
                SmoSkinBone bone = donor.Skin.Bones[donorSlot];
                string name = RemapBoneName(
                    donorDocument.Objects[bone.NodeObjectIndex].Name, _boneRemap);
                if (!PaletteByName.TryGetValue(name, out byte targetSlot))
                    throw new InvalidOperationException(
                        $"Bone \"{name}\" is absent from target skin " +
                        $"[{Source.SkinEntry.Index}].");
                mapped[targetSlot] = mapped.GetValueOrDefault(targetSlot) + weight;
            }
        }

        private static void WriteVector2(Span<byte> data, int offset, Vector2 value)
        {
            WriteSingle(data, offset, value.X);
            WriteSingle(data, offset + 4, value.Y);
        }

        private static void WriteVector3(Span<byte> data, int offset, Vector3 value)
        {
            WriteSingle(data, offset, value.X);
            WriteSingle(data, offset + 4, value.Y);
            WriteSingle(data, offset + 8, value.Z);
        }

        private static void WriteVector4(Span<byte> data, int offset, Vector4 value)
        {
            WriteSingle(data, offset, value.X);
            WriteSingle(data, offset + 4, value.Y);
            WriteSingle(data, offset + 8, value.Z);
            WriteSingle(data, offset + 12, value.W);
        }

        private static void WriteSingle(Span<byte> data, int offset, float value) =>
            BinaryPrimitives.WriteInt32LittleEndian(
                data[offset..], BitConverter.SingleToInt32Bits(value));
    }
}
