using System.Buffers.Binary;
using System.Numerics;
using System.Text;
using System.Text.Json;

namespace SmoExporter.Core;

public static class GlbExporter
{
    public static void Export(SmoExportScene scene, string outputPath)
    {
        ArgumentNullException.ThrowIfNull(scene);
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outputPath))!);

        bool includeMeshes = Includes(scene, SmoExportResourceTypes.Meshes);
        bool includeSkeleton = Includes(scene, SmoExportResourceTypes.Skeleton);
        bool includeMaterials = includeMeshes &&
            Includes(scene, SmoExportResourceTypes.Materials);
        bool includeTextures = includeMaterials &&
            Includes(scene, SmoExportResourceTypes.Textures);
        bool includeAnimations = includeSkeleton &&
            Includes(scene, SmoExportResourceTypes.Animations);
        IReadOnlyList<SmoExportNode> sourceNodes = includeSkeleton ? scene.Nodes : [];
        IReadOnlyList<SmoExportSkin> sourceSkins = includeSkeleton ? scene.Skins : [];
        IReadOnlyList<SmoExportMesh> sourceMeshes = includeMeshes ? scene.Meshes : [];
        IReadOnlyList<SmoExportAnimation> sourceAnimations = includeAnimations
            ? scene.Animations
            : [];

        using var binary = new MemoryStream();
        var views = new List<object>();
        var accessors = new List<object>();
        var images = new List<object>();
        var textures = new List<object>();
        var materials = new List<object>();
        var gltfMeshes = new List<object>();
        var nodes = new List<object>();
        var gltfSkins = new List<object>();
        var gltfAnimations = new List<object>();
        var textureIndices = new Dictionary<(int ObjectIndex, bool OpaqueRgb), int>();
        var nodeObjects = new Dictionary<int, Dictionary<string, object>>();
        var nodeChildren = new Dictionary<int, List<int>>();
        var gltfParents = new List<int?>();
        Dictionary<int, int> nodeIndices = sourceNodes
            .Select((node, index) => (node.ObjectIndex, index))
            .ToDictionary(item => item.ObjectIndex, item => item.index);
        Dictionary<int, SmoExportNode> sourceNodesByIndex = sourceNodes
            .ToDictionary(node => node.ObjectIndex);
        Dictionary<int, List<int>> animationNodeIndices = sourceNodes
            .ToDictionary(node => node.ObjectIndex,
                node => new List<int> { nodeIndices[node.ObjectIndex] });
        Dictionary<int, int[]> children = sourceNodes
            .Where(node => node.ParentObjectIndex is not null)
            .GroupBy(node => node.ParentObjectIndex!.Value)
            .ToDictionary(group => group.Key, group => group
                .Select(node => node.ObjectIndex).ToArray());
        foreach (SmoExportNode node in sourceNodes)
        {
            if (!Matrix4x4.Decompose(node.BindLocalMatrix,
                    out Vector3 scale, out Quaternion rotation, out Vector3 translation))
                throw new InvalidDataException(
                    $"Node {node.ObjectIndex} ({node.Name}) has a non-decomposable bind transform.");
            ValidateTransform(scale, rotation, translation,
                $"Node {node.ObjectIndex} ({node.Name})");
            rotation = Quaternion.Normalize(rotation);
            var value = new Dictionary<string, object>
            {
                ["name"] = CleanName(node.Name, $"bone_{node.ObjectIndex}"),
                ["translation"] = new[] { translation.X, translation.Y, translation.Z },
                ["rotation"] = new[] { rotation.X, rotation.Y, rotation.Z, rotation.W },
                ["scale"] = new[] { scale.X, scale.Y, scale.Z },
                ["extras"] = new { sparkplugObjectIndex = node.ObjectIndex }
            };
            List<int> gltfChildren = children.TryGetValue(
                    node.ObjectIndex, out int[]? sourceChildren)
                ? sourceChildren.Where(nodeIndices.ContainsKey)
                    .Select(index => nodeIndices[index]).ToList()
                : [];
            if (gltfChildren.Count > 0)
                value["children"] = gltfChildren;
            nodeObjects[node.ObjectIndex] = value;
            nodeChildren[node.ObjectIndex] = gltfChildren;
            nodes.Add(value);
            gltfParents.Add(node.ParentObjectIndex is int parentObjectIndex &&
                            nodeIndices.TryGetValue(parentObjectIndex, out int parentNodeIndex)
                ? parentNodeIndex
                : null);
        }
        int AddPaletteClone(int objectIndex, int skinObjectIndex, int sourceSlot)
        {
            SmoExportNode source = sourceNodesByIndex[objectIndex];
            if (!Matrix4x4.Decompose(source.BindLocalMatrix,
                    out Vector3 scale, out Quaternion rotation, out Vector3 translation))
            {
                throw new InvalidDataException(
                    $"Palette clone source {objectIndex} ({source.Name}) has a " +
                    "non-decomposable bind transform.");
            }
            ValidateTransform(scale, rotation, translation,
                $"Palette clone source {objectIndex} ({source.Name})");
            rotation = Quaternion.Normalize(rotation);

            int cloneIndex = nodes.Count;
            var clone = new Dictionary<string, object>
            {
                ["name"] = CleanName(source.Name, $"bone_{objectIndex}") +
                    $"__palette_{skinObjectIndex}_{sourceSlot}",
                ["translation"] = new[] { translation.X, translation.Y, translation.Z },
                ["rotation"] = new[] { rotation.X, rotation.Y, rotation.Z, rotation.W },
                ["scale"] = new[] { scale.X, scale.Y, scale.Z },
                ["extras"] = new
                {
                    sparkplugObjectIndex = objectIndex,
                    sparkplugPaletteClone = true,
                    sparkplugSkinObjectIndex = skinObjectIndex,
                    sparkplugSourcePaletteSlot = sourceSlot
                }
            };
            nodes.Add(clone);

            int? parentIndex = source.ParentObjectIndex is int parentObjectIndex &&
                               nodeIndices.TryGetValue(parentObjectIndex, out int resolvedParentIndex)
                ? resolvedParentIndex
                : null;
            gltfParents.Add(parentIndex);
            if (parentIndex.HasValue &&
                source.ParentObjectIndex is int parentObject &&
                nodeObjects.TryGetValue(parentObject, out Dictionary<string, object>? parentNode))
            {
                List<int> parentChildren = nodeChildren[parentObject];
                parentChildren.Add(cloneIndex);
                parentNode["children"] = parentChildren;
            }
            animationNodeIndices[objectIndex].Add(cloneIndex);
            return cloneIndex;
        }

        Dictionary<int, int> skinIndices = new();
        Dictionary<int, SkinPalette> skinPalettes = new();
        foreach (SmoExportSkin skin in sourceSkins)
        {
            if (skin.JointObjectIndices.Count == 0)
            {
                // An empty, unreferenced skin carries no exportable binding. A
                // mesh that references it is rejected below instead of becoming static.
                continue;
            }
            if (skin.JointObjectIndices.Count != skin.InverseBindMatrices.Count)
            {
                throw new InvalidDataException(
                    $"Skin {skin.ObjectIndex} ({skin.Name}) has " +
                    $"{skin.JointObjectIndices.Count} joints but " +
                    $"{skin.InverseBindMatrices.Count} inverse bind matrices.");
            }
            int[] missingJoints = skin.JointObjectIndices
                .Where(index => !nodeIndices.ContainsKey(index))
                .Distinct()
                .ToArray();
            if (missingJoints.Length > 0)
            {
                throw new InvalidDataException(
                    $"Skin {skin.ObjectIndex} ({skin.Name}) references unavailable " +
                    $"joint nodes: {string.Join(", ", missingJoints)}.");
            }
            if (skinIndices.ContainsKey(skin.ObjectIndex))
            {
                throw new InvalidDataException(
                    $"Skin object index {skin.ObjectIndex} occurs more than once.");
            }

            SkinPalette palette = BuildSkinPalette(
                skin, sourceMeshes, nodeIndices, AddPaletteClone);
            int inverseBindAccessor = AddMatrix4Accessor(
                binary, views, accessors, palette.InverseBindMatrices);
            skinIndices[skin.ObjectIndex] = gltfSkins.Count;
            skinPalettes[skin.ObjectIndex] = palette;
            var gltfSkin = new Dictionary<string, object>
            {
                ["name"] = CleanName(skin.Name, $"skin_{skin.ObjectIndex}"),
                ["joints"] = palette.JointNodeIndices,
                ["inverseBindMatrices"] = inverseBindAccessor
            };
            int? skeletonRoot = FindClosestCommonAncestor(
                palette.JointNodeIndices, gltfParents);
            if (skeletonRoot.HasValue)
                gltfSkin["skeleton"] = skeletonRoot.Value;
            gltfSkins.Add(gltfSkin);
        }

        int? AddTexture(SmoExportTexture? texture, bool opaqueRgb)
        {
            if (texture is null)
                return null;
            bool useOpaqueRgb = opaqueRgb &&
                                texture.OpacityMaskPngBytes is not null &&
                                texture.OpaqueRgbPngBytes is not null;
            var key = (texture.ObjectIndex, useOpaqueRgb);
            if (textureIndices.TryGetValue(key, out int existing))
                return existing;

            byte[] pngBytes = useOpaqueRgb
                ? texture.OpaqueRgbPngBytes!
                : texture.PngBytes;
            int imageView = AddBytes(binary, views, pngBytes, null);
            int imageIndex = images.Count;
            images.Add(new
            {
                name = CleanName(
                    useOpaqueRgb ? texture.Name + "_opaque" : texture.Name,
                    $"texture_{texture.ObjectIndex}" + (useOpaqueRgb ? "_opaque" : string.Empty)),
                mimeType = "image/png",
                bufferView = imageView
            });
            existing = textures.Count;
            textures.Add(new { source = imageIndex });
            textureIndices[key] = existing;
            return existing;
        }

        foreach (SmoExportMesh mesh in sourceMeshes)
        {
            int positionAccessor = AddVector3Accessor(
                binary, views, accessors, mesh.Positions, includeBounds: true, target: 34962);
            var attributes = new Dictionary<string, int> { ["POSITION"] = positionAccessor };
            if (mesh.Normals.Length == mesh.Positions.Length)
                attributes["NORMAL"] = AddVector3Accessor(
                    binary, views, accessors, mesh.Normals, false, 34962);
            if (mesh.TextureCoordinates0.Length == mesh.Positions.Length)
                attributes["TEXCOORD_0"] = AddVector2Accessor(
                    binary, views, accessors, mesh.TextureCoordinates0, 34962);
            if (mesh.TextureCoordinates1.Length == mesh.Positions.Length)
                attributes["TEXCOORD_1"] = AddVector2Accessor(
                    binary, views, accessors, mesh.TextureCoordinates1, 34962);
            if (mesh.Colors.Length == mesh.Positions.Length)
                attributes["COLOR_0"] = AddVector4Accessor(
                    binary, views, accessors, mesh.Colors, 34962);
            int? gltfSkinIndex = null;
            SkinPalette? meshPalette = null;
            if (mesh.SkinObjectIndex is int skinObjectIndex)
            {
                if (!skinIndices.TryGetValue(skinObjectIndex, out int resolvedSkinIndex) ||
                    !skinPalettes.TryGetValue(skinObjectIndex, out meshPalette))
                {
                    throw new InvalidDataException(
                        $"Mesh {mesh.ObjectIndex} ({mesh.Name}) references invalid or " +
                        $"unavailable skin {skinObjectIndex}.");
                }
                if (mesh.BlendWeights.Length != mesh.Positions.Length ||
                    mesh.JointIndices.Length != mesh.Positions.Length)
                {
                    throw new InvalidDataException(
                        $"Mesh {mesh.ObjectIndex} ({mesh.Name}) has incomplete skinning data.");
                }
                gltfSkinIndex = resolvedSkinIndex;
            }
            if (gltfSkinIndex.HasValue)
            {
                (Vector4[] remappedJoints, Vector4[] remappedWeights) =
                    RemapSkinning(mesh, meshPalette!);
                attributes["WEIGHTS_0"] = AddVector4Accessor(
                    binary, views, accessors, remappedWeights, 34962);
                attributes["JOINTS_0"] = AddJointAccessor(
                    binary, views, accessors, remappedJoints, 34962);
            }

            int indexAccessor = AddIndicesAccessor(
                binary, views, accessors, mesh.TriangleIndices);

            var primitive = new Dictionary<string, object>
            {
                ["attributes"] = attributes,
                ["indices"] = indexAccessor,
                ["mode"] = 4
            };
            if (includeMaterials)
            {
                int? textureIndex = includeTextures
                    ? AddTexture(mesh.Texture, opaqueRgb: !mesh.UsesAlphaBlend)
                    : null;
                int? effectTextureIndex = includeTextures
                    ? AddTexture(mesh.EffectTexture, opaqueRgb: true)
                    : null;
                int materialIndex = materials.Count;
                var pbr = new Dictionary<string, object>
                {
                    ["baseColorFactor"] = ToArray(mesh.MaterialColor),
                    ["metallicFactor"] = 0f,
                    ["roughnessFactor"] = 1f
                };
                if (textureIndex.HasValue)
                    pbr["baseColorTexture"] = new { index = textureIndex.Value };
                var gltfMaterial = new Dictionary<string, object>
                {
                    ["name"] = CleanName(mesh.Name, $"material_{mesh.ObjectIndex}"),
                    ["pbrMetallicRoughness"] = pbr,
                    ["doubleSided"] = true
                };
                if (mesh.UsesAlphaBlend)
                    gltfMaterial["alphaMode"] = "BLEND";
                if (effectTextureIndex.HasValue)
                {
                    gltfMaterial["emissiveTexture"] = new
                    {
                        index = effectTextureIndex.Value,
                        texCoord = 1
                    };
                    gltfMaterial["emissiveFactor"] = new[] { 1f, 1f, 1f };
                }
                materials.Add(gltfMaterial);
                primitive["material"] = materialIndex;
            }
            gltfMeshes.Add(new
            {
                name = CleanName(mesh.Name, $"mesh_{mesh.ObjectIndex}"),
                primitives = new[] { primitive },
                extras = new
                {
                    sparkplugObjectIndex = mesh.ObjectIndex,
                    sparkplugObjectId = mesh.ObjectId,
                    sparkplugMarker = $"0x{mesh.Marker:X2}",
                    sparkplugPrimitiveType = mesh.PrimitiveType,
                    sparkplugVertexFormat = $"0x{mesh.VertexFormat:X4}",
                    sparkplugSerializedStride = mesh.SerializedStride,
                    sparkplugRuntimeStride = mesh.RuntimeStride
                }
            });
            var meshNode = new Dictionary<string, object>
            {
                ["name"] = CleanName(mesh.Name, $"node_{mesh.ObjectIndex}"),
                ["mesh"] = gltfMeshes.Count - 1
            };
            if (!Matrix4x4.Decompose(mesh.BindLocalMatrix,
                    out Vector3 meshScale,
                    out Quaternion meshRotation,
                    out Vector3 meshTranslation))
            {
                throw new InvalidDataException(
                    $"Mesh node {mesh.ObjectIndex} ({mesh.Name}) has a " +
                    "non-decomposable bind transform.");
            }
            ValidateTransform(meshScale, meshRotation, meshTranslation,
                $"Mesh node {mesh.ObjectIndex} ({mesh.Name})");
            meshRotation = Quaternion.Normalize(meshRotation);
            meshNode["translation"] = new[]
                { meshTranslation.X, meshTranslation.Y, meshTranslation.Z };
            meshNode["rotation"] = new[]
                { meshRotation.X, meshRotation.Y, meshRotation.Z, meshRotation.W };
            meshNode["scale"] = new[] { meshScale.X, meshScale.Y, meshScale.Z };
            if (gltfSkinIndex.HasValue)
                meshNode["skin"] = gltfSkinIndex.Value;
            int meshNodeIndex = nodes.Count;
            nodes.Add(meshNode);
            int? meshParentIndex = null;
            if (mesh.ParentNodeObjectIndex is int parentObjectIndex &&
                nodeObjects.TryGetValue(parentObjectIndex, out Dictionary<string, object>? parentNode))
            {
                List<int> parentChildren = nodeChildren[parentObjectIndex];
                parentChildren.Add(meshNodeIndex);
                parentNode["children"] = parentChildren;
                meshParentIndex = nodeIndices[parentObjectIndex];
            }
            gltfParents.Add(meshParentIndex);
        }

        var animationBuffer = new PackedBufferView(binary, views);
        var animationInputAccessors =
            new Dictionary<float[], int>(FloatArrayComparer.Instance);
        foreach (SmoExportAnimation animation in sourceAnimations)
        {
            object? gltfAnimation = BuildAnimation(
                animation, animationNodeIndices, animationBuffer,
                animationInputAccessors, accessors);
            if (gltfAnimation is not null)
                gltfAnimations.Add(gltfAnimation);
        }

        int binaryLength = checked((int)binary.Length);
        int[] sceneRoots = gltfParents
            .Select((parent, index) => (parent, index))
            .Where(item => item.parent is null)
            .Select(item => item.index)
            .ToArray();
        var root = new Dictionary<string, object>
        {
            ["asset"] = new { version = "2.0", generator = "Sparkplug SmoExporter" },
            ["scene"] = 0,
            ["scenes"] = new[]
            {
                new
                {
                    name = Path.GetFileNameWithoutExtension(scene.SourcePath),
                    nodes = sceneRoots
                }
            },
            ["extras"] = new
            {
                sparkplugSource = Path.GetFileName(scene.SourcePath),
                sparkplugSourceSha256 = scene.SourceSha256,
                sparkplugPlatformFlags = scene.PlatformFlags,
                sparkplugResources = scene.Resources.ToString(),
                warnings = scene.Warnings
            }
        };
        if (nodes.Count > 0) root["nodes"] = nodes;
        if (gltfMeshes.Count > 0) root["meshes"] = gltfMeshes;
        if (gltfSkins.Count > 0) root["skins"] = gltfSkins;
        if (gltfAnimations.Count > 0) root["animations"] = gltfAnimations;
        if (materials.Count > 0) root["materials"] = materials;
        if (textures.Count > 0) root["textures"] = textures;
        if (images.Count > 0) root["images"] = images;
        if (accessors.Count > 0) root["accessors"] = accessors;
        if (views.Count > 0) root["bufferViews"] = views;
        if (binaryLength > 0)
            root["buffers"] = new[] { new { byteLength = binaryLength } };

        byte[] json = JsonSerializer.SerializeToUtf8Bytes(root, new JsonSerializerOptions
        {
            WriteIndented = false,
            DefaultIgnoreCondition = System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull
        });
        json = Pad(json, 0x20);
        byte[]? bin = binaryLength > 0 ? Pad(binary.ToArray(), 0x00) : null;

        using FileStream output = File.Create(outputPath);
        Span<byte> header = stackalloc byte[12];
        BinaryPrimitives.WriteUInt32LittleEndian(header, 0x46546C67);
        BinaryPrimitives.WriteUInt32LittleEndian(header[4..], 2);
        BinaryPrimitives.WriteUInt32LittleEndian(
            header[8..], checked((uint)(12 + 8 + json.Length +
                (bin is null ? 0 : 8 + bin.Length))));
        output.Write(header);
        WriteChunk(output, json, 0x4E4F534A);
        if (bin is not null)
            WriteChunk(output, bin, 0x004E4942);
    }

    private sealed record SkinPalette(
        int[] SourceToExportSlots,
        int[] JointNodeIndices,
        Matrix4x4[] InverseBindMatrices);

    private static SkinPalette BuildSkinPalette(
        SmoExportSkin skin,
        IReadOnlyList<SmoExportMesh> meshes,
        IReadOnlyDictionary<int, int> nodeIndices,
        Func<int, int, int, int> addPaletteClone)
    {
        for (int slot = 0; slot < skin.InverseBindMatrices.Count; slot++)
        {
            if (!IsFinite(skin.InverseBindMatrices[slot]))
            {
                throw new InvalidDataException(
                    $"Skin {skin.ObjectIndex} ({skin.Name}) has a non-finite " +
                    $"inverse bind matrix in palette slot {slot}.");
            }
        }

        var activeSlots = new HashSet<int>();
        foreach (SmoExportMesh mesh in meshes.Where(
                     mesh => mesh.SkinObjectIndex == skin.ObjectIndex &&
                             mesh.BlendWeights.Length == mesh.JointIndices.Length))
        {
            for (int vertex = 0; vertex < mesh.BlendWeights.Length; vertex++)
            for (int component = 0; component < 4; component++)
            {
                float weight = GetComponent(mesh.BlendWeights[vertex], component);
                float joint = GetComponent(mesh.JointIndices[vertex], component);
                if (float.IsFinite(weight) && weight > 0 &&
                    TryGetJointSlot(joint, skin.JointObjectIndices.Count, out int slot))
                {
                    activeSlots.Add(slot);
                }
            }
        }

        int[] representativeBySourceSlot = new int[skin.JointObjectIndices.Count];
        foreach (IGrouping<int, int> group in Enumerable.Range(
                     0, skin.JointObjectIndices.Count)
                 .GroupBy(slot => skin.JointObjectIndices[slot]))
        {
            int[] sourceSlots = group.ToArray();
            int[] contributingSlots = sourceSlots.Where(activeSlots.Contains).ToArray();
            if (contributingSlots.Length == 0)
                contributingSlots = [sourceSlots[0]];

            var representatives = new List<int>();
            foreach (int slot in contributingSlots)
            {
                if (!representatives.Any(existing => MatrixEquals(
                        skin.InverseBindMatrices[existing],
                        skin.InverseBindMatrices[slot])))
                {
                    representatives.Add(slot);
                }
            }

            foreach (int slot in sourceSlots)
            {
                int matching = representatives.FirstOrDefault(existing => MatrixEquals(
                    skin.InverseBindMatrices[existing],
                    skin.InverseBindMatrices[slot]), -1);
                // A matrix absent from the representatives belongs only to an
                // unused padding slot. Mapping it cannot affect deformation.
                representativeBySourceSlot[slot] = matching >= 0
                    ? matching
                    : representatives[0];
            }
        }

        int[] representativeSlots = representativeBySourceSlot
            .Distinct()
            .OrderBy(slot => slot)
            .ToArray();
        var representativeToExportSlot = new Dictionary<int, int>();
        var retainedPerObject = new Dictionary<int, int>();
        int[] jointNodeIndices = new int[representativeSlots.Length];
        Matrix4x4[] inverseBindMatrices = new Matrix4x4[representativeSlots.Length];
        for (int exportSlot = 0; exportSlot < representativeSlots.Length; exportSlot++)
        {
            int sourceSlot = representativeSlots[exportSlot];
            int objectIndex = skin.JointObjectIndices[sourceSlot];
            int occurrence = retainedPerObject.GetValueOrDefault(objectIndex);
            jointNodeIndices[exportSlot] = occurrence == 0
                ? nodeIndices[objectIndex]
                : addPaletteClone(objectIndex, skin.ObjectIndex, sourceSlot);
            retainedPerObject[objectIndex] = occurrence + 1;
            inverseBindMatrices[exportSlot] = skin.InverseBindMatrices[sourceSlot];
            representativeToExportSlot[sourceSlot] = exportSlot;
        }

        int[] sourceToExportSlots = representativeBySourceSlot
            .Select(representative => representativeToExportSlot[representative])
            .ToArray();
        return new SkinPalette(
            sourceToExportSlots, jointNodeIndices, inverseBindMatrices);
    }

    private static (Vector4[] Joints, Vector4[] Weights) RemapSkinning(
        SmoExportMesh mesh, SkinPalette palette)
    {
        Vector4[] joints = new Vector4[mesh.JointIndices.Length];
        Vector4[] weights = new Vector4[mesh.BlendWeights.Length];
        Span<int> mappedSlots = stackalloc int[4];
        Span<float> mappedWeights = stackalloc float[4];
        for (int vertex = 0; vertex < weights.Length; vertex++)
        {
            mappedSlots.Clear();
            mappedWeights.Clear();
            int mappedCount = 0;
            float totalWeight = 0;
            for (int component = 0; component < 4; component++)
            {
                float weight = GetComponent(mesh.BlendWeights[vertex], component);
                if (!float.IsFinite(weight) || weight < 0)
                {
                    throw new InvalidDataException(
                        $"Mesh {mesh.ObjectIndex} ({mesh.Name}) has an invalid skin " +
                        $"weight at vertex {vertex}, component {component}.");
                }
                if (weight == 0)
                    continue;

                float sourceJoint = GetComponent(mesh.JointIndices[vertex], component);
                if (!TryGetJointSlot(
                        sourceJoint, palette.SourceToExportSlots.Length, out int sourceSlot))
                {
                    throw new InvalidDataException(
                        $"Mesh {mesh.ObjectIndex} ({mesh.Name}) references palette slot " +
                        $"{sourceJoint} at vertex {vertex}, component {component}; the skin " +
                        $"has {palette.SourceToExportSlots.Length} source slots.");
                }
                int exportSlot = palette.SourceToExportSlots[sourceSlot];
                int existing = -1;
                for (int index = 0; index < mappedCount; index++)
                {
                    if (mappedSlots[index] == exportSlot)
                    {
                        existing = index;
                        break;
                    }
                }
                if (existing >= 0)
                {
                    mappedWeights[existing] += weight;
                }
                else
                {
                    mappedSlots[mappedCount] = exportSlot;
                    mappedWeights[mappedCount] = weight;
                    mappedCount++;
                }
                totalWeight += weight;
            }

            if (!float.IsFinite(totalWeight) || totalWeight <= 0)
            {
                throw new InvalidDataException(
                    $"Mesh {mesh.ObjectIndex} ({mesh.Name}) has no usable skin weights " +
                    $"at vertex {vertex}.");
            }
            // Match the game's skinning path: normalize materially non-unit
            // sums, while preserving already-normalized source weights exactly.
            if (MathF.Abs(totalWeight - 1f) > 0.0001f)
            {
                for (int index = 0; index < mappedCount; index++)
                    mappedWeights[index] /= totalWeight;
            }
            joints[vertex] = new Vector4(
                mappedSlots[0], mappedSlots[1], mappedSlots[2], mappedSlots[3]);
            weights[vertex] = new Vector4(
                mappedWeights[0], mappedWeights[1], mappedWeights[2], mappedWeights[3]);
        }
        return (joints, weights);
    }

    private static int? FindClosestCommonAncestor(
        IReadOnlyList<int> joints, IReadOnlyList<int?> parents)
    {
        if (joints.Count == 0)
            return null;
        foreach (int candidate in EnumerateAncestors(joints[0], parents))
        {
            if (joints.All(joint => IsAncestor(candidate, joint, parents)))
                return candidate;
        }
        return null;
    }

    private static IEnumerable<int> EnumerateAncestors(
        int node, IReadOnlyList<int?> parents)
    {
        var visited = new HashSet<int>();
        while ((uint)node < (uint)parents.Count && visited.Add(node))
        {
            yield return node;
            if (parents[node] is not int parent)
                yield break;
            node = parent;
        }
    }

    private static bool IsAncestor(
        int candidate, int node, IReadOnlyList<int?> parents) =>
        EnumerateAncestors(node, parents).Contains(candidate);

    private static object? BuildAnimation(
        SmoExportAnimation animation,
        IReadOnlyDictionary<int, List<int>> animationNodeIndices,
        PackedBufferView animationBuffer,
        IDictionary<float[], int> inputAccessors,
        List<object> accessors)
    {
        var samplers = new List<object>();
        var channels = new List<object>();
        var emittedTargets = new HashSet<(int Node, string Path)>();
        foreach (SmoExportAnimationTrack track in animation.Tracks)
        {
            if (!animationNodeIndices.TryGetValue(
                    track.NodeObjectIndex, out List<int>? targetNodes))
                continue;
            Add(track.Positions.Select(key => key.Time).ToArray(),
                track.Positions.Select(key => key.Value).ToArray(), "translation");
            Add4(track.Rotations.Select(key => key.Time).ToArray(),
                track.Rotations.Select(key => new Vector4(
                    key.Value.X, key.Value.Y, key.Value.Z, key.Value.W)).ToArray(), "rotation");
            Add(track.Scales.Select(key => key.Time).ToArray(),
                track.Scales.Select(key => key.Value).ToArray(), "scale");

            void Add(float[] times, Vector3[] values, string path)
            {
                if (times.Length == 0 || times.Length != values.Length) return;
                int[] nodesForChannel = targetNodes
                    .Where(node => !emittedTargets.Contains((node, path))).ToArray();
                if (nodesForChannel.Length == 0) return;
                ValidateAnimationValues(animation.Name, track.NodeName, path, times, values);
                int input = GetInputAccessor(times);
                int output = AddPackedVector3Accessor(
                    animationBuffer, accessors, values);
                int sampler = samplers.Count;
                samplers.Add(new { input, output, interpolation = "LINEAR" });
                foreach (int node in nodesForChannel)
                {
                    emittedTargets.Add((node, path));
                    channels.Add(new { sampler, target = new { node, path } });
                }
            }
            void Add4(float[] times, Vector4[] values, string path)
            {
                if (times.Length == 0 || times.Length != values.Length) return;
                int[] nodesForChannel = targetNodes
                    .Where(node => !emittedTargets.Contains((node, path))).ToArray();
                if (nodesForChannel.Length == 0) return;
                ValidateAnimationValues(animation.Name, track.NodeName, path, times, values);
                int input = GetInputAccessor(times);
                int output = AddPackedVector4Accessor(
                    animationBuffer, accessors, values);
                int sampler = samplers.Count;
                samplers.Add(new { input, output, interpolation = "LINEAR" });
                foreach (int node in nodesForChannel)
                {
                    emittedTargets.Add((node, path));
                    channels.Add(new { sampler, target = new { node, path } });
                }
            }
        }

        int GetInputAccessor(float[] times)
        {
            if (inputAccessors.TryGetValue(times, out int existing))
                return existing;
            int accessor = AddPackedScalarAccessor(
                animationBuffer, accessors, times);
            inputAccessors[times] = accessor;
            return accessor;
        }
        return channels.Count == 0
            ? null
            : new { name = CleanName(animation.Name, "animation"), samplers, channels };
    }

    private sealed class PackedBufferView
    {
        private readonly MemoryStream _binary;
        private readonly List<object> _views;
        private Dictionary<string, object>? _view;
        private int _viewIndex = -1;
        private int _start;

        public PackedBufferView(MemoryStream binary, List<object> views)
        {
            _binary = binary;
            _views = views;
        }

        public (int View, int Offset) Write(byte[] bytes)
        {
            Align(_binary);
            if (_view is null)
            {
                _start = checked((int)_binary.Position);
                _viewIndex = _views.Count;
                _view = new Dictionary<string, object>
                {
                    ["buffer"] = 0,
                    ["byteOffset"] = _start,
                    ["byteLength"] = 0
                };
                _views.Add(_view);
            }

            int offset = checked((int)_binary.Position - _start);
            _binary.Write(bytes);
            _view["byteLength"] = checked((int)_binary.Position - _start);
            return (_viewIndex, offset);
        }
    }

    private static int AddPackedScalarAccessor(
        PackedBufferView buffer, List<object> accessors, float[] values)
    {
        byte[] bytes = new byte[checked(values.Length * 4)];
        for (int i = 0; i < values.Length; i++) WriteSingle(bytes, i * 4, values[i]);
        (int view, int offset) = buffer.Write(bytes);
        int result = accessors.Count;
        accessors.Add(new
        {
            bufferView = view, byteOffset = offset,
            componentType = 5126, count = values.Length, type = "SCALAR",
            min = new[] { values.Min() }, max = new[] { values.Max() }
        });
        return result;
    }

    private static int AddPackedVector3Accessor(
        PackedBufferView buffer, List<object> accessors, Vector3[] values)
    {
        byte[] bytes = new byte[checked(values.Length * 12)];
        for (int index = 0; index < values.Length; index++)
        {
            WriteSingle(bytes, index * 12, values[index].X);
            WriteSingle(bytes, index * 12 + 4, values[index].Y);
            WriteSingle(bytes, index * 12 + 8, values[index].Z);
        }
        (int view, int offset) = buffer.Write(bytes);
        int result = accessors.Count;
        accessors.Add(new
        {
            bufferView = view, byteOffset = offset,
            componentType = 5126, count = values.Length, type = "VEC3"
        });
        return result;
    }

    private static int AddPackedVector4Accessor(
        PackedBufferView buffer, List<object> accessors, Vector4[] values)
    {
        byte[] bytes = new byte[checked(values.Length * 16)];
        for (int index = 0; index < values.Length; index++)
        {
            WriteSingle(bytes, index * 16, values[index].X);
            WriteSingle(bytes, index * 16 + 4, values[index].Y);
            WriteSingle(bytes, index * 16 + 8, values[index].Z);
            WriteSingle(bytes, index * 16 + 12, values[index].W);
        }
        (int view, int offset) = buffer.Write(bytes);
        int result = accessors.Count;
        accessors.Add(new
        {
            bufferView = view, byteOffset = offset,
            componentType = 5126, count = values.Length, type = "VEC4"
        });
        return result;
    }

    private static int AddMatrix4Accessor(
        MemoryStream binary, List<object> views, List<object> accessors, Matrix4x4[] values)
    {
        byte[] bytes = new byte[checked(values.Length * 64)];
        for (int i = 0; i < values.Length; i++)
        {
            float[] matrix = ToArray(values[i]);
            for (int component = 0; component < 16; component++)
                WriteSingle(bytes, i * 64 + component * 4, matrix[component]);
        }
        int view = AddBytes(binary, views, bytes, null);
        int result = accessors.Count;
        accessors.Add(new { bufferView = view, componentType = 5126, count = values.Length, type = "MAT4" });
        return result;
    }

    private static int AddJointAccessor(
        MemoryStream binary, List<object> views, List<object> accessors,
        Vector4[] values, int target)
    {
        byte[] bytes = new byte[checked(values.Length * 8)];
        for (int i = 0; i < values.Length; i++)
        {
            BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(i * 8), checked((ushort)values[i].X));
            BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(i * 8 + 2), checked((ushort)values[i].Y));
            BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(i * 8 + 4), checked((ushort)values[i].Z));
            BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(i * 8 + 6), checked((ushort)values[i].W));
        }
        int view = AddBytes(binary, views, bytes, target);
        int result = accessors.Count;
        accessors.Add(new { bufferView = view, componentType = 5123, count = values.Length, type = "VEC4" });
        return result;
    }

    private static int AddVector3Accessor(
        MemoryStream binary, List<object> views, List<object> accessors,
        Vector3[] values, bool includeBounds, int? target)
    {
        byte[] bytes = new byte[checked(values.Length * 12)];
        for (int i = 0; i < values.Length; i++)
        {
            WriteSingle(bytes, i * 12, values[i].X);
            WriteSingle(bytes, i * 12 + 4, values[i].Y);
            WriteSingle(bytes, i * 12 + 8, values[i].Z);
        }
        int view = AddBytes(binary, views, bytes, target);
        int result = accessors.Count;
        if (includeBounds)
        {
            Vector3 min = values.Aggregate(Vector3.Min);
            Vector3 max = values.Aggregate(Vector3.Max);
            accessors.Add(new { bufferView = view, componentType = 5126, count = values.Length, type = "VEC3", min = ToArray(min), max = ToArray(max) });
        }
        else
            accessors.Add(new { bufferView = view, componentType = 5126, count = values.Length, type = "VEC3" });
        return result;
    }

    private static int AddVector2Accessor(MemoryStream binary, List<object> views, List<object> accessors, Vector2[] values, int target)
    {
        byte[] bytes = new byte[checked(values.Length * 8)];
        for (int i = 0; i < values.Length; i++)
        {
            WriteSingle(bytes, i * 8, values[i].X);
            WriteSingle(bytes, i * 8 + 4, values[i].Y);
        }
        int view = AddBytes(binary, views, bytes, target);
        int result = accessors.Count;
        accessors.Add(new { bufferView = view, componentType = 5126, count = values.Length, type = "VEC2" });
        return result;
    }

    private static int AddVector4Accessor(
        MemoryStream binary, List<object> views, List<object> accessors,
        Vector4[] values, int? target)
    {
        byte[] bytes = new byte[checked(values.Length * 16)];
        for (int i = 0; i < values.Length; i++)
        {
            WriteSingle(bytes, i * 16, values[i].X);
            WriteSingle(bytes, i * 16 + 4, values[i].Y);
            WriteSingle(bytes, i * 16 + 8, values[i].Z);
            WriteSingle(bytes, i * 16 + 12, values[i].W);
        }
        int view = AddBytes(binary, views, bytes, target);
        int result = accessors.Count;
        accessors.Add(new { bufferView = view, componentType = 5126, count = values.Length, type = "VEC4" });
        return result;
    }

    private static int AddIndicesAccessor(MemoryStream binary, List<object> views, List<object> accessors, uint[] values)
    {
        byte[] bytes = new byte[checked(values.Length * 4)];
        for (int i = 0; i < values.Length; i++)
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(i * 4), values[i]);
        int view = AddBytes(binary, views, bytes, 34963);
        int result = accessors.Count;
        accessors.Add(new { bufferView = view, componentType = 5125, count = values.Length, type = "SCALAR" });
        return result;
    }

    private static int AddBytes(MemoryStream binary, List<object> views, byte[] bytes, int? target)
    {
        Align(binary);
        int offset = checked((int)binary.Position);
        binary.Write(bytes);
        int result = views.Count;
        views.Add(target.HasValue
            ? new { buffer = 0, byteOffset = offset, byteLength = bytes.Length, target = target.Value }
            : (object)new { buffer = 0, byteOffset = offset, byteLength = bytes.Length });
        return result;
    }

    private static void Align(MemoryStream stream)
    {
        while ((stream.Position & 3) != 0)
            stream.WriteByte(0);
    }

    private static void WriteSingle(Span<byte> bytes, int offset, float value) =>
        BinaryPrimitives.WriteInt32LittleEndian(bytes[offset..], BitConverter.SingleToInt32Bits(value));

    private static void ValidateTransform(
        Vector3 scale, Quaternion rotation, Vector3 translation, string owner)
    {
        float rotationLengthSquared = rotation.LengthSquared();
        if (!IsFinite(scale) || !IsFinite(rotation) || !IsFinite(translation) ||
            !float.IsFinite(rotationLengthSquared) ||
            rotationLengthSquared <= 0.000000000001f)
        {
            throw new InvalidDataException($"{owner} has a non-finite or invalid transform.");
        }
    }

    private static void ValidateAnimationValues(
        string animation, string node, string path,
        IReadOnlyList<float> times, IReadOnlyList<Vector3> values)
    {
        ValidateAnimationTimes(animation, node, path, times);
        if (values.Any(value => !IsFinite(value)))
        {
            throw new InvalidDataException(
                $"Animation {animation}, node {node}, path {path} contains non-finite values.");
        }
    }

    private static void ValidateAnimationValues(
        string animation, string node, string path,
        IReadOnlyList<float> times, IReadOnlyList<Vector4> values)
    {
        ValidateAnimationTimes(animation, node, path, times);
        if (values.Any(value => !IsFinite(value)))
        {
            throw new InvalidDataException(
                $"Animation {animation}, node {node}, path {path} contains non-finite values.");
        }
        if (path == "rotation" && values.Any(value =>
                MathF.Abs(value.LengthSquared() - 1f) > 0.0001f))
        {
            throw new InvalidDataException(
                $"Animation {animation}, node {node} contains a non-unit rotation.");
        }
    }

    private static void ValidateAnimationTimes(
        string animation, string node, string path, IReadOnlyList<float> times)
    {
        for (int index = 0; index < times.Count; index++)
        {
            if (!float.IsFinite(times[index]) || times[index] < 0 ||
                (index > 0 && times[index] <= times[index - 1]))
            {
                throw new InvalidDataException(
                    $"Animation {animation}, node {node}, path {path} has invalid or " +
                    "non-increasing key times.");
            }
        }
    }

    private static bool TryGetJointSlot(float value, int count, out int slot)
    {
        slot = 0;
        if (!float.IsFinite(value))
            return false;
        float rounded = MathF.Round(value);
        if (MathF.Abs(value - rounded) > 0.0001f || rounded < 0 || rounded >= count)
            return false;
        slot = (int)rounded;
        return true;
    }

    private static float GetComponent(Vector4 value, int component) => component switch
    {
        0 => value.X,
        1 => value.Y,
        2 => value.Z,
        3 => value.W,
        _ => throw new ArgumentOutOfRangeException(nameof(component))
    };

    private static bool MatrixEquals(Matrix4x4 left, Matrix4x4 right) =>
        left.Equals(right);

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);

    private static bool IsFinite(Vector4 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) &&
        float.IsFinite(value.Z) && float.IsFinite(value.W);

    private static bool IsFinite(Quaternion value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) &&
        float.IsFinite(value.Z) && float.IsFinite(value.W);

    private static bool IsFinite(Matrix4x4 value) =>
        float.IsFinite(value.M11) && float.IsFinite(value.M12) &&
        float.IsFinite(value.M13) && float.IsFinite(value.M14) &&
        float.IsFinite(value.M21) && float.IsFinite(value.M22) &&
        float.IsFinite(value.M23) && float.IsFinite(value.M24) &&
        float.IsFinite(value.M31) && float.IsFinite(value.M32) &&
        float.IsFinite(value.M33) && float.IsFinite(value.M34) &&
        float.IsFinite(value.M41) && float.IsFinite(value.M42) &&
        float.IsFinite(value.M43) && float.IsFinite(value.M44);

    private sealed class FloatArrayComparer : IEqualityComparer<float[]>
    {
        public static FloatArrayComparer Instance { get; } = new();

        public bool Equals(float[]? left, float[]? right)
        {
            if (ReferenceEquals(left, right)) return true;
            if (left is null || right is null || left.Length != right.Length) return false;
            for (int index = 0; index < left.Length; index++)
            {
                if (BitConverter.SingleToInt32Bits(left[index]) !=
                    BitConverter.SingleToInt32Bits(right[index]))
                    return false;
            }
            return true;
        }

        public int GetHashCode(float[] values)
        {
            var hash = new HashCode();
            foreach (float value in values)
                hash.Add(BitConverter.SingleToInt32Bits(value));
            return hash.ToHashCode();
        }
    }

    private static float[] ToArray(Vector2 value) => [value.X, value.Y];
    private static float[] ToArray(Vector3 value) => [value.X, value.Y, value.Z];
    private static float[] ToArray(Vector4 value) => [value.X, value.Y, value.Z, value.W];
    private static float[] ToArray(Matrix4x4 value) =>
    [
        value.M11, value.M12, value.M13, value.M14,
        value.M21, value.M22, value.M23, value.M24,
        value.M31, value.M32, value.M33, value.M34,
        value.M41, value.M42, value.M43, value.M44
    ];

    private static string CleanName(string value, string fallback) =>
        string.IsNullOrWhiteSpace(value) ? fallback : value.Replace('\0', '_');

    private static bool Includes(
        SmoExportScene scene, SmoExportResourceTypes resource) =>
        (scene.Resources & resource) != 0;

    private static byte[] Pad(byte[] value, byte padding)
    {
        int length = (value.Length + 3) & ~3;
        if (length == value.Length)
            return value;
        byte[] padded = new byte[length];
        value.CopyTo(padded, 0);
        padded.AsSpan(value.Length).Fill(padding);
        return padded;
    }

    private static void WriteChunk(Stream output, byte[] bytes, uint type)
    {
        Span<byte> header = stackalloc byte[8];
        BinaryPrimitives.WriteUInt32LittleEndian(header, (uint)bytes.Length);
        BinaryPrimitives.WriteUInt32LittleEndian(header[4..], type);
        output.Write(header);
        output.Write(bytes);
    }
}
