using System.Buffers.Binary;
using System.Numerics;
using System.Text.Json;
using SixLabors.ImageSharp;

namespace SmoImporter.Core;

public static class GlbModelReader
{
    public static ImportedScene Read(
        string path,
        CancellationToken cancellationToken = default) =>
        ReadCore(path, ignoreSkinning: false, cancellationToken);

    /// <summary>
    /// Reads only mesh/material/texture data and deliberately ignores any skin
    /// attributes. This lets the explicit generate-weights mode recover geometry
    /// from a donor whose rig is unusable; the normal reader remains fail-loud.
    /// </summary>
    public static ImportedScene ReadGeometryOnly(
        string path,
        CancellationToken cancellationToken = default) =>
        ReadCore(path, ignoreSkinning: true, cancellationToken);

    private static ImportedScene ReadCore(
        string path,
        bool ignoreSkinning,
        CancellationToken cancellationToken)
    {
        string fullPath = Path.GetFullPath(path);
        ImportedModelResourceLimits.ValidateInputFile(fullPath, "GLB");
        cancellationToken.ThrowIfCancellationRequested();
        byte[] file = File.ReadAllBytes(fullPath);
        if (file.Length < 20 || BinaryPrimitives.ReadUInt32LittleEndian(file) != 0x46546C67 ||
            BinaryPrimitives.ReadUInt32LittleEndian(file.AsSpan(4)) != 2)
            throw new InvalidDataException("Only binary glTF 2.0 (.glb) is supported.");
        int jsonLength = checked((int)BinaryPrimitives.ReadUInt32LittleEndian(file.AsSpan(12)));
        if (jsonLength <= 0 ||
            jsonLength > ImportedModelResourceLimits.MaximumJsonBytes ||
            jsonLength > file.Length - 20)
        {
            throw new InvalidDataException(
                $"GLB JSON chunk length {jsonLength:N0} is invalid or exceeds the " +
                $"safe limit {ImportedModelResourceLimits.MaximumJsonBytes:N0}.");
        }
        using JsonDocument document = JsonDocument.Parse(file.AsMemory(20, jsonLength));
        int binaryHeader = checked(20 + jsonLength);
        if (binaryHeader + 8 > file.Length ||
            BinaryPrimitives.ReadUInt32LittleEndian(file.AsSpan(binaryHeader + 4)) != 0x004E4942)
            throw new InvalidDataException("GLB has no binary buffer chunk.");
        int binaryLength = checked((int)BinaryPrimitives.ReadUInt32LittleEndian(file.AsSpan(binaryHeader)));
        if (binaryLength < 0 || binaryLength > file.Length - binaryHeader - 8)
            throw new InvalidDataException("GLB binary chunk crosses the file boundary.");
        ReadOnlyMemory<byte> binary = file.AsMemory(binaryHeader + 8, binaryLength);
        JsonElement root = document.RootElement;
        ValidateDocumentResourceLimits(root, binary);
        cancellationToken.ThrowIfCancellationRequested();
        JsonElement meshes = root.GetProperty("meshes");
        int[] nodeParents = root.TryGetProperty("nodes", out JsonElement nodes)
            ? ReadNodeParents(nodes)
            : [];
        string?[] meshNodeNames = ResolveMeshNodeNames(root, meshes.GetArrayLength());
        Matrix4x4[] meshTransforms = ResolveMeshTransforms(
            root, meshes.GetArrayLength(), nodeParents);
        int?[] meshSkins = ResolveMeshSkins(root, meshes.GetArrayLength());
        GlbSkinLayout[] skins = ignoreSkinning
            ? []
            : ReadSkins(root, binary, nodeParents);
        var result = new List<ImportedMesh>();
        var importWarnings = new List<string>();

        for (int meshIndex = 0; meshIndex < meshes.GetArrayLength(); meshIndex++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            JsonElement mesh = meshes[meshIndex];
            string baseName = meshNodeNames[meshIndex] ??
                (mesh.TryGetProperty("name", out JsonElement name)
                    ? name.GetString() ?? $"mesh_{meshIndex}"
                    : $"mesh_{meshIndex}");
            int primitiveIndex = 0;
            foreach (JsonElement primitive in mesh.GetProperty("primitives").EnumerateArray())
            {
                cancellationToken.ThrowIfCancellationRequested();
                int mode = primitive.TryGetProperty("mode", out JsonElement modeElement)
                    ? modeElement.GetInt32() : 4;
                if (mode != 4) throw new InvalidDataException("Only glTF TRIANGLES primitives are supported.");
                JsonElement attributes = primitive.GetProperty("attributes");
                Vector3[] positions = ReadVector3(root, binary, attributes.GetProperty("POSITION").GetInt32());
                Vector3[] normals = attributes.TryGetProperty("NORMAL", out JsonElement normal)
                    ? ReadVector3(root, binary, normal.GetInt32()) : [];
                Vector2[] uvs = attributes.TryGetProperty("TEXCOORD_0", out JsonElement uv)
                    ? ReadVector2(root, binary, uv.GetInt32()) : [];
                uint[] colors = attributes.TryGetProperty("COLOR_0", out JsonElement color)
                    ? ReadColorsArgb(root, binary, color.GetInt32()) : [];
                uint[] indices = primitive.TryGetProperty("indices", out JsonElement index)
                    ? ReadIndices(root, binary, index.GetInt32())
                    : Enumerable.Range(0, positions.Length).Select(value => (uint)value).ToArray();
                Matrix4x4 transform = meshTransforms[meshIndex];
                ImportedSkinning? skinning = null;
                bool hasJoints = attributes.TryGetProperty("JOINTS_0", out JsonElement joints);
                bool hasWeights = attributes.TryGetProperty("WEIGHTS_0", out JsonElement weights);
                if (!ignoreSkinning && hasJoints != hasWeights)
                    throw new InvalidDataException(
                        $"Mesh {meshIndex} primitive {primitiveIndex} must contain both JOINTS_0 and WEIGHTS_0.");
                if (!ignoreSkinning && hasJoints)
                {
                    int skinIndex = meshSkins[meshIndex] ?? throw new InvalidDataException(
                        $"Mesh {meshIndex} contains skin attributes but its node has no skin.");
                    if ((uint)skinIndex >= (uint)skins.Length)
                        throw new InvalidDataException($"Mesh {meshIndex} references invalid skin {skinIndex}.");
                    GlbSkinLayout skin = skins[skinIndex];
                    ImportedJointIndices[] jointValues = RemapJointIndices(
                        ReadJointIndices(root, binary, joints.GetInt32()),
                        skin.SourceToCanonicalJoint,
                        skinIndex);
                    Vector4[] weightValues = ReadWeights(
                        root, binary, weights.GetInt32());
                    if (jointValues.Length != positions.Length || weightValues.Length != positions.Length)
                        throw new InvalidDataException("Skin attribute count differs from POSITION count.");
                    skinning = new ImportedSkinning(skin.Skeleton, jointValues, weightValues);
                }
                if (skinning is null)
                {
                    positions = positions.Select(value => Vector3.Transform(value, transform)).ToArray();
                    if (normals.Length == positions.Length)
                    {
                        Matrix4x4 normalTransform = transform;
                        if (Matrix4x4.Invert(transform, out Matrix4x4 inverse))
                            normalTransform = Matrix4x4.Transpose(inverse);
                        normals = normals.Select(value => NormalizeOrZero(
                            Vector3.TransformNormal(value, normalTransform))).ToArray();
                    }
                }
                if (normals.Length == positions.Length)
                {
                    normals = RepairInvalidNormals(
                        positions,
                        normals,
                        indices,
                        out int reconstructedNormals,
                        out int fallbackNormals);
                    int repairedNormals = reconstructedNormals + fallbackNormals;
                    if (repairedNormals > 0)
                    {
                        string resultName = primitiveIndex == 0
                            ? baseName
                            : $"{baseName}_{primitiveIndex}";
                        importWarnings.Add(
                            $"Нормали donor mesh [{result.Count}] '{resultName}': " +
                            $"автоматически исправлено {repairedNormals} нулевых или " +
                            $"нечисловых значений; {reconstructedNormals} пересчитано " +
                            "по треугольникам" +
                            (fallbackNormals == 0
                                ? "."
                                : $", {fallbackNormals} неиспользуемых/вырожденных " +
                                  "вершин получили безопасную резервную нормаль."));
                    }
                }
                result.Add(new ImportedMesh(
                    primitiveIndex == 0 ? baseName : $"{baseName}_{primitiveIndex}",
                    positions, normals, uvs, indices, colors,
                    primitive.TryGetProperty("material", out JsonElement material)
                        ? material.GetInt32() : -1,
                    skinning));
                primitiveIndex++;
            }
        }
        GlbTextureCatalog textureCatalog = ReadEmbeddedBaseColorTextures(root, binary);
        return new ImportedScene(
            result,
            textureCatalog.Textures,
            ReadMaterials(root, textureCatalog.SceneTextureIndexByImage))
        {
            ImportWarnings = importWarnings.AsReadOnly()
        };
    }

    private static void ValidateDocumentResourceLimits(
        JsonElement root,
        ReadOnlyMemory<byte> binary)
    {
        if (!root.TryGetProperty("meshes", out JsonElement meshes) ||
            meshes.ValueKind != JsonValueKind.Array)
            throw new InvalidDataException("GLB contains no mesh array.");

        ImportedModelResourceLimits.ValidateCount(
            meshes.GetArrayLength(), ImportedModelResourceLimits.MaximumMeshes,
            "GLB mesh");
        ValidateOptionalArrayCount(
            root, "nodes", ImportedModelResourceLimits.MaximumNodes);
        ValidateOptionalArrayCount(
            root, "skins", ImportedModelResourceLimits.MaximumSkins);
        ValidateOptionalArrayCount(
            root, "materials", ImportedModelResourceLimits.MaximumMaterials);
        ValidateOptionalArrayCount(
            root, "textures", ImportedModelResourceLimits.MaximumTextures);
        ValidateOptionalArrayCount(
            root, "images", ImportedModelResourceLimits.MaximumTextures);
        ValidateOptionalArrayCount(
            root, "bufferViews", ImportedModelResourceLimits.MaximumBufferViews);
        ValidateOptionalArrayCount(
            root, "accessors", ImportedModelResourceLimits.MaximumAccessors);

        JsonElement views = root.TryGetProperty("bufferViews", out JsonElement foundViews)
            ? foundViews
            : default;
        if (views.ValueKind == JsonValueKind.Array)
        {
            for (int viewIndex = 0; viewIndex < views.GetArrayLength(); viewIndex++)
            {
                JsonElement view = views[viewIndex];
                long offset = view.TryGetProperty("byteOffset", out JsonElement offsetElement)
                    ? offsetElement.GetInt64()
                    : 0;
                long length = view.GetProperty("byteLength").GetInt64();
                if (offset < 0 || length < 0 || offset > binary.Length - length)
                {
                    throw new InvalidDataException(
                        $"GLB bufferView {viewIndex} crosses the binary chunk boundary.");
                }
            }
        }

        if (!root.TryGetProperty("accessors", out JsonElement accessors) ||
            accessors.ValueKind != JsonValueKind.Array)
            throw new InvalidDataException("GLB contains no accessor array.");

        long decodedAccessorBytes = 0;
        for (int accessorIndex = 0;
             accessorIndex < accessors.GetArrayLength();
             accessorIndex++)
        {
            JsonElement accessor = accessors[accessorIndex];
            int count = accessor.GetProperty("count").GetInt32();
            ImportedModelResourceLimits.ValidateCount(
                count,
                ImportedModelResourceLimits.MaximumIndicesPerPrimitive,
                $"GLB accessor {accessorIndex}");
            int width = AccessorWidth(
                accessor.GetProperty("type").GetString(), accessorIndex);
            int componentSize = ComponentSize(
                accessor.GetProperty("componentType").GetInt32(), accessorIndex);
            decodedAccessorBytes = checked(
                decodedAccessorBytes + (long)count * width * sizeof(float));
            if (decodedAccessorBytes >
                ImportedModelResourceLimits.MaximumDecodedAccessorBytes)
            {
                throw new InvalidDataException(
                    "GLB accessors exceed the safe decoded-memory budget of " +
                    $"{ImportedModelResourceLimits.MaximumDecodedAccessorBytes / (1024 * 1024):N0} MiB.");
            }

            if (!accessor.TryGetProperty("bufferView", out JsonElement viewElement))
            {
                throw new InvalidDataException(
                    $"GLB accessor {accessorIndex} has no bufferView; sparse-only " +
                    "accessors are not supported safely.");
            }
            int viewIndex = viewElement.GetInt32();
            if (views.ValueKind != JsonValueKind.Array ||
                (uint)viewIndex >= (uint)views.GetArrayLength())
                throw new InvalidDataException(
                    $"GLB accessor {accessorIndex} references invalid bufferView {viewIndex}.");
            JsonElement view = views[viewIndex];
            long viewOffset = view.TryGetProperty("byteOffset", out JsonElement viewOffsetElement)
                ? viewOffsetElement.GetInt64()
                : 0;
            long viewLength = view.GetProperty("byteLength").GetInt64();
            long accessorOffset = accessor.TryGetProperty(
                "byteOffset", out JsonElement accessorOffsetElement)
                ? accessorOffsetElement.GetInt64()
                : 0;
            long elementSize = checked((long)componentSize * width);
            long stride = view.TryGetProperty("byteStride", out JsonElement strideElement)
                ? strideElement.GetInt64()
                : elementSize;
            if (accessorOffset < 0 || stride < elementSize)
                throw new InvalidDataException(
                    $"GLB accessor {accessorIndex} has an invalid offset or stride.");
            long relativeEnd = count == 0
                ? accessorOffset
                : checked(accessorOffset + (long)(count - 1) * stride + elementSize);
            if (relativeEnd > viewLength ||
                viewOffset > binary.Length - relativeEnd)
            {
                throw new InvalidDataException(
                    $"GLB accessor {accessorIndex} crosses its bufferView boundary.");
            }
        }

        long totalVertices = 0;
        long totalIndices = 0;
        int primitiveCount = 0;
        foreach (JsonElement mesh in meshes.EnumerateArray())
        {
            if (!mesh.TryGetProperty("primitives", out JsonElement primitives) ||
                primitives.ValueKind != JsonValueKind.Array)
                throw new InvalidDataException("GLB mesh has no primitive array.");
            primitiveCount = checked(primitiveCount + primitives.GetArrayLength());
            ImportedModelResourceLimits.ValidateCount(
                primitiveCount,
                ImportedModelResourceLimits.MaximumPrimitives,
                "GLB primitive");
            foreach (JsonElement primitive in primitives.EnumerateArray())
            {
                JsonElement attributes = primitive.GetProperty("attributes");
                int positionAccessor = attributes.GetProperty("POSITION").GetInt32();
                int vertexCount = GetAccessorCount(accessors, positionAccessor, "POSITION");
                ImportedModelResourceLimits.ValidateCount(
                    vertexCount,
                    ImportedModelResourceLimits.MaximumVerticesPerPrimitive,
                    "GLB primitive vertex");
                totalVertices = checked(totalVertices + vertexCount);

                int indexCount = primitive.TryGetProperty(
                    "indices", out JsonElement indicesElement)
                    ? GetAccessorCount(accessors, indicesElement.GetInt32(), "indices")
                    : vertexCount;
                ImportedModelResourceLimits.ValidateCount(
                    indexCount,
                    ImportedModelResourceLimits.MaximumIndicesPerPrimitive,
                    "GLB primitive index");
                totalIndices = checked(totalIndices + indexCount);
            }
        }
        ImportedModelResourceLimits.ValidateCount(
            totalVertices,
            ImportedModelResourceLimits.MaximumTotalVertices,
            "GLB total vertex");
        ImportedModelResourceLimits.ValidateCount(
            totalIndices,
            ImportedModelResourceLimits.MaximumTotalIndices,
            "GLB total index");

        if (root.TryGetProperty("skins", out JsonElement skins))
        {
            for (int skinIndex = 0; skinIndex < skins.GetArrayLength(); skinIndex++)
            {
                if (!skins[skinIndex].TryGetProperty("joints", out JsonElement joints) ||
                    joints.ValueKind != JsonValueKind.Array)
                    throw new InvalidDataException($"GLB skin {skinIndex} has no joints.");
                ImportedModelResourceLimits.ValidateCount(
                    joints.GetArrayLength(),
                    ImportedModelResourceLimits.MaximumJointsPerSkin,
                    $"GLB skin {skinIndex} joint");
            }
        }
    }

    private static void ValidateOptionalArrayCount(
        JsonElement root,
        string property,
        int maximum)
    {
        if (!root.TryGetProperty(property, out JsonElement array))
            return;
        if (array.ValueKind != JsonValueKind.Array)
            throw new InvalidDataException($"GLB {property} must be an array.");
        ImportedModelResourceLimits.ValidateCount(
            array.GetArrayLength(), maximum, $"GLB {property}");
    }

    private static int GetAccessorCount(
        JsonElement accessors,
        int accessorIndex,
        string owner)
    {
        if ((uint)accessorIndex >= (uint)accessors.GetArrayLength())
            throw new InvalidDataException(
                $"GLB {owner} references invalid accessor {accessorIndex}.");
        return accessors[accessorIndex].GetProperty("count").GetInt32();
    }

    private static int AccessorWidth(string? type, int accessorIndex) => type switch
    {
        "SCALAR" => 1,
        "VEC2" => 2,
        "VEC3" => 3,
        "VEC4" => 4,
        "MAT2" => 4,
        "MAT3" => 9,
        "MAT4" => 16,
        _ => throw new InvalidDataException(
            $"GLB accessor {accessorIndex} has unsupported type '{type}'.")
    };

    private static int ComponentSize(int componentType, int accessorIndex) =>
        componentType switch
        {
            5120 or 5121 => 1,
            5122 or 5123 => 2,
            5125 or 5126 => 4,
            _ => throw new InvalidDataException(
                $"GLB accessor {accessorIndex} has unsupported component type " +
                $"{componentType}.")
        };

    private static Vector3 NormalizeOrZero(Vector3 value)
    {
        float lengthSquared = value.LengthSquared();
        return IsFinite(value) && float.IsFinite(lengthSquared) &&
               lengthSquared > 0.000000000001f
            ? Vector3.Normalize(value)
            : Vector3.Zero;
    }

    /// <summary>
    /// Reconstructs only unusable vertex normals. Valid source normals remain
    /// untouched; triangle cross products are accumulated area-weighted. An
    /// invalid vertex without any usable incident face is unreferenced or fully
    /// degenerate, so a finite fallback keeps downstream data deterministic.
    /// </summary>
    internal static Vector3[] RepairInvalidNormals(
        IReadOnlyList<Vector3> positions,
        IReadOnlyList<Vector3> normals,
        IReadOnlyList<uint> triangleIndices,
        out int reconstructedCount,
        out int fallbackCount)
    {
        reconstructedCount = 0;
        fallbackCount = 0;
        if (normals.Count != positions.Count)
            return normals.ToArray();

        var repaired = normals.ToArray();
        var invalid = new bool[repaired.Length];
        var accumulated = new Vector3[repaired.Length];
        int invalidCount = 0;
        for (int vertex = 0; vertex < repaired.Length; vertex++)
        {
            float lengthSquared = repaired[vertex].LengthSquared();
            invalid[vertex] = !IsFinite(repaired[vertex]) ||
                              !float.IsFinite(lengthSquared) ||
                              lengthSquared <= 0.000000000001f;
            if (invalid[vertex])
                invalidCount++;
        }
        if (invalidCount == 0)
            return repaired;

        for (int index = 0; index + 2 < triangleIndices.Count; index += 3)
        {
            uint a = triangleIndices[index];
            uint b = triangleIndices[index + 1];
            uint c = triangleIndices[index + 2];
            if (a >= (uint)positions.Count ||
                b >= (uint)positions.Count ||
                c >= (uint)positions.Count)
                continue;
            Vector3 face = Vector3.Cross(
                positions[(int)b] - positions[(int)a],
                positions[(int)c] - positions[(int)a]);
            float faceLengthSquared = face.LengthSquared();
            if (!IsFinite(face) || !float.IsFinite(faceLengthSquared) ||
                faceLengthSquared <= 0.000000000001f)
            {
                continue;
            }
            if (invalid[(int)a]) accumulated[(int)a] += face;
            if (invalid[(int)b]) accumulated[(int)b] += face;
            if (invalid[(int)c]) accumulated[(int)c] += face;
        }

        for (int vertex = 0; vertex < repaired.Length; vertex++)
        {
            if (!invalid[vertex])
                continue;
            Vector3 candidate = accumulated[vertex];
            float lengthSquared = candidate.LengthSquared();
            if (IsFinite(candidate) && float.IsFinite(lengthSquared) &&
                lengthSquared > 0.000000000001f)
            {
                repaired[vertex] = Vector3.Normalize(candidate);
                reconstructedCount++;
            }
            else
            {
                repaired[vertex] = Vector3.UnitY;
                fallbackCount++;
            }
        }
        return repaired;
    }

    private static bool IsFinite(Vector3 value) =>
        float.IsFinite(value.X) &&
        float.IsFinite(value.Y) &&
        float.IsFinite(value.Z);

    private static IReadOnlyList<ImportedMaterial> ReadMaterials(
        JsonElement root,
        IReadOnlyDictionary<int, int> sceneTextureIndexByImage)
    {
        if (!root.TryGetProperty("materials", out JsonElement materials))
            return [];
        bool hasTextures = root.TryGetProperty("textures", out JsonElement textures);
        bool hasImages = root.TryGetProperty("images", out JsonElement images);
        var result = new ImportedMaterial[materials.GetArrayLength()];
        for (int materialIndex = 0; materialIndex < result.Length; materialIndex++)
        {
            JsonElement material = materials[materialIndex];
            string name = material.TryGetProperty("name", out JsonElement materialName) &&
                          !string.IsNullOrWhiteSpace(materialName.GetString())
                ? materialName.GetString()!
                : $"material_{materialIndex}";
            string? textureName = null;
            int sceneTextureIndex = -1;
            if (hasTextures && hasImages &&
                TryGetColorTextureIndex(material, out int texture))
            {
                if ((uint)texture < (uint)textures.GetArrayLength() &&
                    textures[texture].TryGetProperty("source", out JsonElement source))
                {
                    int image = source.GetInt32();
                    if ((uint)image < (uint)images.GetArrayLength())
                    {
                        JsonElement imageElement = images[image];
                        textureName = imageElement.TryGetProperty("uri", out JsonElement uri) &&
                                      !string.IsNullOrWhiteSpace(uri.GetString())
                            ? Path.GetFileName(uri.GetString())
                            : imageElement.TryGetProperty("name", out JsonElement imageName) &&
                              !string.IsNullOrWhiteSpace(imageName.GetString())
                                ? imageName.GetString()
                                : null;
                        if (!sceneTextureIndexByImage.TryGetValue(
                                image, out sceneTextureIndex))
                            sceneTextureIndex = -1;
                    }
                }
            }
            result[materialIndex] = new ImportedMaterial(
                name, textureName, sceneTextureIndex);
        }
        return result;
    }

    private static GlbSkinLayout[] ReadSkins(
        JsonElement root,
        ReadOnlyMemory<byte> binary,
        IReadOnlyList<int> nodeParents)
    {
        if (!root.TryGetProperty("skins", out JsonElement skins))
            return [];
        if (!root.TryGetProperty("nodes", out JsonElement nodes))
            throw new InvalidDataException("GLB skins require nodes.");
        var result = new GlbSkinLayout[skins.GetArrayLength()];
        for (int skinIndex = 0; skinIndex < result.Length; skinIndex++)
        {
            JsonElement skin = skins[skinIndex];
            if (!skin.TryGetProperty("joints", out JsonElement joints) ||
                !skin.TryGetProperty("inverseBindMatrices", out JsonElement inverseBind))
                throw new InvalidDataException(
                    $"Skin {skinIndex} must contain joints and inverseBindMatrices.");
            int[] nodeIndices = joints.EnumerateArray().Select(value =>
            {
                int nodeIndex = value.GetInt32();
                if ((uint)nodeIndex >= (uint)nodes.GetArrayLength())
                    throw new InvalidDataException($"Skin {skinIndex} references invalid node {nodeIndex}.");
                return nodeIndex;
            }).ToArray();
            string[] names = nodeIndices.Select(nodeIndex =>
            {
                string? name = nodes[nodeIndex].TryGetProperty("name", out JsonElement nodeName)
                    ? nodeName.GetString() : null;
                return string.IsNullOrWhiteSpace(name)
                    ? throw new InvalidDataException(
                        $"Skin {skinIndex} joint node {nodeIndex} has no name.")
                    : name;
            }).ToArray();
            Matrix4x4[] matrices = ReadMatrix4(root, binary, inverseBind.GetInt32());
            if (matrices.Length != names.Length)
                throw new InvalidDataException(
                    $"Skin {skinIndex} has {names.Length} joints but {matrices.Length} inverse bind matrices.");
            string name = skin.TryGetProperty("name", out JsonElement skinName)
                ? skinName.GetString() ?? $"skin_{skinIndex}"
                : $"skin_{skinIndex}";
            result[skinIndex] = CanonicalizeSkin(
                skinIndex, name, nodeIndices, names, matrices, nodeParents);
        }
        return result;
    }

    private static GlbSkinLayout CanonicalizeSkin(
        int skinIndex,
        string name,
        IReadOnlyList<int> sourceNodes,
        IReadOnlyList<string> sourceNames,
        IReadOnlyList<Matrix4x4> sourceMatrices,
        IReadOnlyList<int> nodeParents)
    {
        var canonicalSlotByNode = new Dictionary<int, int>();
        var canonicalNodes = new List<int>();
        var canonicalNames = new List<string>();
        var canonicalMatrices = new List<Matrix4x4>();
        var sourceToCanonical = new int[sourceNodes.Count];
        for (int sourceSlot = 0; sourceSlot < sourceNodes.Count; sourceSlot++)
        {
            int node = sourceNodes[sourceSlot];
            Matrix4x4 matrix = sourceMatrices[sourceSlot];
            if (canonicalSlotByNode.TryGetValue(node, out int canonicalSlot))
            {
                if (!canonicalMatrices[canonicalSlot].Equals(matrix))
                {
                    throw new InvalidDataException(
                        $"Skin {skinIndex} repeats joint node {node} with different " +
                        "inverse bind matrices.");
                }
                sourceToCanonical[sourceSlot] = canonicalSlot;
                continue;
            }

            canonicalSlot = canonicalNames.Count;
            canonicalSlotByNode.Add(node, canonicalSlot);
            sourceToCanonical[sourceSlot] = canonicalSlot;
            canonicalNodes.Add(node);
            canonicalNames.Add(sourceNames[sourceSlot]);
            canonicalMatrices.Add(matrix);
        }

        if (canonicalNames.Distinct(StringComparer.Ordinal).Count() != canonicalNames.Count)
        {
            throw new InvalidDataException(
                $"Skin {skinIndex} contains duplicate joint names on distinct nodes.");
        }

        var parentJointIndices = new int[canonicalNames.Count];
        Array.Fill(parentJointIndices, -1);
        for (int joint = 0; joint < parentJointIndices.Length; joint++)
        {
            int ancestorNode = nodeParents[canonicalNodes[joint]];
            while (ancestorNode >= 0)
            {
                if (canonicalSlotByNode.TryGetValue(ancestorNode, out int parentJoint))
                {
                    parentJointIndices[joint] = parentJoint;
                    break;
                }
                ancestorNode = nodeParents[ancestorNode];
            }
        }

        Matrix4x4[]? bindWorldMatrices = TryInvertBindMatrices(
            canonicalMatrices, out Matrix4x4[] invertedBindMatrices)
            ? invertedBindMatrices
            : null;
        Matrix4x4[]? bindLocalMatrices = null;
        if (bindWorldMatrices is not null)
        {
            bindLocalMatrices = new Matrix4x4[bindWorldMatrices.Length];
            for (int joint = 0; joint < bindLocalMatrices.Length; joint++)
            {
                int parent = parentJointIndices[joint];
                bindLocalMatrices[joint] = parent < 0
                    ? bindWorldMatrices[joint]
                    : bindWorldMatrices[joint] * canonicalMatrices[parent];
            }
        }
        return new GlbSkinLayout(
            new ImportedSkeleton(name, canonicalNames, canonicalMatrices)
            {
                ParentJointIndices = parentJointIndices,
                BindWorldMatrices = bindWorldMatrices,
                BindLocalMatrices = bindLocalMatrices
            },
            sourceToCanonical);
    }

    private static bool TryInvertBindMatrices(
        IReadOnlyList<Matrix4x4> inverseBindMatrices,
        out Matrix4x4[] bindWorldMatrices)
    {
        bindWorldMatrices = new Matrix4x4[inverseBindMatrices.Count];
        for (int joint = 0; joint < bindWorldMatrices.Length; joint++)
        {
            if (!Matrix4x4.Invert(
                    inverseBindMatrices[joint], out bindWorldMatrices[joint]))
            {
                bindWorldMatrices = [];
                return false;
            }
        }
        return true;
    }

    private static int[] ReadNodeParents(JsonElement nodes)
    {
        int[] parents = new int[nodes.GetArrayLength()];
        Array.Fill(parents, -1);
        for (int parent = 0; parent < parents.Length; parent++)
        {
            if (!nodes[parent].TryGetProperty("children", out JsonElement children))
                continue;
            foreach (JsonElement childElement in children.EnumerateArray())
            {
                int child = childElement.GetInt32();
                if ((uint)child >= (uint)parents.Length)
                    throw new InvalidDataException(
                        $"Node {parent} references invalid child node {child}.");
                if (parents[child] >= 0 && parents[child] != parent)
                    throw new InvalidDataException(
                        $"Node {child} has multiple parents ({parents[child]} and {parent}).");
                parents[child] = parent;
            }
        }

        for (int node = 0; node < parents.Length; node++)
        {
            var ancestors = new HashSet<int>();
            int cursor = node;
            while (cursor >= 0)
            {
                if (!ancestors.Add(cursor))
                    throw new InvalidDataException(
                        $"glTF node hierarchy contains a cycle through node {cursor}.");
                cursor = parents[cursor];
            }
        }
        return parents;
    }

    private static ImportedJointIndices[] RemapJointIndices(
        IReadOnlyList<ImportedJointIndices> source,
        IReadOnlyList<int> sourceToCanonical,
        int skinIndex)
    {
        ushort Remap(ushort sourceSlot)
        {
            if (sourceSlot >= sourceToCanonical.Count)
            {
                throw new InvalidDataException(
                    $"Skin {skinIndex} vertex references joint slot {sourceSlot}, " +
                    $"but the skin contains only {sourceToCanonical.Count} joints.");
            }
            return checked((ushort)sourceToCanonical[sourceSlot]);
        }

        return source.Select(value => new ImportedJointIndices(
            Remap(value.X), Remap(value.Y), Remap(value.Z), Remap(value.W))).ToArray();
    }

    private sealed record GlbSkinLayout(
        ImportedSkeleton Skeleton,
        IReadOnlyList<int> SourceToCanonicalJoint);

    private static int?[] ResolveMeshSkins(JsonElement root, int meshCount)
    {
        var result = new int?[meshCount];
        if (!root.TryGetProperty("nodes", out JsonElement nodes))
            return result;
        foreach (JsonElement node in nodes.EnumerateArray())
        {
            if (!node.TryGetProperty("mesh", out JsonElement mesh) ||
                !node.TryGetProperty("skin", out JsonElement skin))
                continue;
            int meshIndex = mesh.GetInt32();
            if ((uint)meshIndex >= (uint)meshCount)
                throw new InvalidDataException($"Node references invalid mesh {meshIndex}.");
            int skinIndex = skin.GetInt32();
            if (result[meshIndex] is int existing && existing != skinIndex)
                throw new InvalidDataException(
                    $"Mesh {meshIndex} is instanced with different skins ({existing}, {skinIndex}).");
            result[meshIndex] = skinIndex;
        }
        return result;
    }

    private static string?[] ResolveMeshNodeNames(JsonElement root, int meshCount)
    {
        var result = new string?[meshCount];
        var owners = new int?[meshCount];
        if (!root.TryGetProperty("nodes", out JsonElement nodes))
            return result;

        int nodeIndex = 0;
        foreach (JsonElement node in nodes.EnumerateArray())
        {
            if (!node.TryGetProperty("mesh", out JsonElement mesh))
            {
                nodeIndex++;
                continue;
            }

            int meshIndex = mesh.GetInt32();
            if ((uint)meshIndex >= (uint)meshCount)
                throw new InvalidDataException(
                    $"Node {nodeIndex} references invalid mesh {meshIndex}.");
            if (owners[meshIndex] is int existingOwner)
                throw new InvalidDataException(
                    $"Mesh {meshIndex} is instanced by nodes {existingOwner} and {nodeIndex}; " +
                    "a single ImportedMesh cannot preserve both node names and transforms.");

            owners[meshIndex] = nodeIndex;
            if (node.TryGetProperty("name", out JsonElement name) &&
                !string.IsNullOrWhiteSpace(name.GetString()))
                result[meshIndex] = name.GetString();
            nodeIndex++;
        }

        return result;
    }

    private static GlbTextureCatalog ReadEmbeddedBaseColorTextures(
        JsonElement root, ReadOnlyMemory<byte> binary)
    {
        var sceneTextureIndexByImage = new Dictionary<int, int>();
        if (!root.TryGetProperty("materials", out JsonElement materials) ||
            !root.TryGetProperty("textures", out JsonElement textures) ||
            !root.TryGetProperty("images", out JsonElement images) ||
            !root.TryGetProperty("bufferViews", out JsonElement views))
            return new GlbTextureCatalog([], sceneTextureIndexByImage);

        var imageIndices = new List<int>();
        foreach (JsonElement material in materials.EnumerateArray())
        {
            if (!TryGetColorTextureIndex(material, out int texture))
                continue;
            if ((uint)texture >= (uint)textures.GetArrayLength() ||
                !textures[texture].TryGetProperty("source", out JsonElement source))
                continue;
            int image = source.GetInt32();
            if ((uint)image < (uint)images.GetArrayLength() && !imageIndices.Contains(image))
                imageIndices.Add(image);
        }

        var result = new List<ImportedTexture>(imageIndices.Count);
        long decodedTexturePixels = 0;
        foreach (int imageIndex in imageIndices)
        {
            JsonElement image = images[imageIndex];
            if (!image.TryGetProperty("bufferView", out JsonElement viewIndex))
                continue;
            int viewNumber = viewIndex.GetInt32();
            if ((uint)viewNumber >= (uint)views.GetArrayLength())
                throw new InvalidDataException($"GLB image {imageIndex} references an invalid bufferView.");
            JsonElement view = views[viewNumber];
            int offset = view.TryGetProperty("byteOffset", out JsonElement byteOffset)
                ? byteOffset.GetInt32() : 0;
            int length = view.GetProperty("byteLength").GetInt32();
            if (offset < 0 || length <= 0 || offset > binary.Length - length)
                throw new InvalidDataException($"GLB image {imageIndex} crosses the binary buffer boundary.");
            if (length > ImportedModelResourceLimits.MaximumEncodedTextureBytes)
            {
                throw new InvalidDataException(
                    $"GLB image {imageIndex} is {length / (1024d * 1024d):N1} MiB, " +
                    "above the safe encoded-texture limit.");
            }
            string? sourceName = image.TryGetProperty("name", out JsonElement imageName)
                ? imageName.GetString() : null;
            string name = string.IsNullOrWhiteSpace(sourceName)
                ? $"image_{imageIndex}"
                : sourceName;
            string mime = image.TryGetProperty("mimeType", out JsonElement mimeType)
                ? mimeType.GetString() ?? "application/octet-stream" : "application/octet-stream";
            byte[] imageBytes = binary.Slice(offset, length).ToArray();
            ImageInfo info = Image.Identify(imageBytes) ?? throw new InvalidDataException(
                $"GLB image {imageIndex} has an unsupported or invalid image payload.");
            decodedTexturePixels = ImportedModelResourceLimits.AddTexturePixels(
                decodedTexturePixels,
                info.Width,
                info.Height,
                $"GLB image {imageIndex} '{name}'");
            sceneTextureIndexByImage.Add(imageIndex, result.Count);
            result.Add(new ImportedTexture(
                name, mime, info.Width, info.Height, imageBytes));
        }
        return new GlbTextureCatalog(
            result.AsReadOnly(), sceneTextureIndexByImage);
    }

    private sealed record GlbTextureCatalog(
        IReadOnlyList<ImportedTexture> Textures,
        IReadOnlyDictionary<int, int> SceneTextureIndexByImage);

    private static bool TryGetColorTextureIndex(
        JsonElement material,
        out int textureIndex)
    {
        // Character rips frequently store the visible atlas in emissiveTexture
        // instead of the glTF PBR base-color slot.  Prefer the proper PBR slot,
        // then accept emissive as the deterministic fallback.
        if (material.TryGetProperty("pbrMetallicRoughness", out JsonElement pbr) &&
            pbr.TryGetProperty("baseColorTexture", out JsonElement baseColor) &&
            baseColor.TryGetProperty("index", out JsonElement baseColorIndex))
        {
            textureIndex = baseColorIndex.GetInt32();
            return true;
        }
        if (material.TryGetProperty("emissiveTexture", out JsonElement emissive) &&
            emissive.TryGetProperty("index", out JsonElement emissiveIndex))
        {
            textureIndex = emissiveIndex.GetInt32();
            return true;
        }
        textureIndex = -1;
        return false;
    }

    private static Matrix4x4[] ResolveMeshTransforms(
        JsonElement root,
        int meshCount,
        IReadOnlyList<int> nodeParents)
    {
        Matrix4x4[] result = Enumerable.Repeat(Matrix4x4.Identity, meshCount).ToArray();
        if (!root.TryGetProperty("nodes", out JsonElement nodes)) return result;
        Matrix4x4[] local = nodes.EnumerateArray().Select(ReadNodeMatrix).ToArray();
        if (nodeParents.Count != local.Length)
            throw new InvalidDataException("GLB node-parent validation was not completed.");
        for (int nodeIndex = 0; nodeIndex < local.Length; nodeIndex++)
        {
            if (!nodes[nodeIndex].TryGetProperty("mesh", out JsonElement mesh)) continue;
            Matrix4x4 world = local[nodeIndex];
            int cursor = nodeParents[nodeIndex];
            while (cursor >= 0)
            {
                world *= local[cursor];
                cursor = nodeParents[cursor];
            }
            int meshIndex = mesh.GetInt32();
            if ((uint)meshIndex < (uint)result.Length) result[meshIndex] = world;
        }
        return result;
    }

    private static Matrix4x4 ReadNodeMatrix(JsonElement node)
    {
        if (node.TryGetProperty("matrix", out JsonElement matrix))
        {
            float[] v = matrix.EnumerateArray().Select(item => item.GetSingle()).ToArray();
            return new Matrix4x4(v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],v[11],v[12],v[13],v[14],v[15]);
        }
        Vector3 scale = ReadVector(node, "scale", Vector3.One);
        Vector3 translation = ReadVector(node, "translation", Vector3.Zero);
        Quaternion rotation = Quaternion.Identity;
        if (node.TryGetProperty("rotation", out JsonElement r))
        {
            float[] q = r.EnumerateArray().Select(item => item.GetSingle()).ToArray();
            rotation = new Quaternion(q[0], q[1], q[2], q[3]);
        }
        return Matrix4x4.CreateScale(scale) * Matrix4x4.CreateFromQuaternion(rotation) * Matrix4x4.CreateTranslation(translation);
    }

    private static Vector3 ReadVector(JsonElement node, string name, Vector3 fallback)
    {
        if (!node.TryGetProperty(name, out JsonElement element)) return fallback;
        float[] v = element.EnumerateArray().Select(item => item.GetSingle()).ToArray();
        return new Vector3(v[0], v[1], v[2]);
    }

    private static Vector3[] ReadVector3(
        JsonElement root,
        ReadOnlyMemory<byte> binary,
        int accessor)
    {
        FloatAccessorLayout layout = GetFloatAccessorLayout(
            root, accessor, 3, "VEC3");
        ReadOnlySpan<byte> data = binary.Span;
        var result = new Vector3[layout.Count];
        for (int index = 0; index < result.Length; index++)
        {
            int start = checked(layout.Offset + index * layout.Stride);
            result[index] = new Vector3(
                ReadFloatComponent(data, start),
                ReadFloatComponent(data, start + 4),
                ReadFloatComponent(data, start + 8));
        }
        return result;
    }

    private static Vector2[] ReadVector2(
        JsonElement root,
        ReadOnlyMemory<byte> binary,
        int accessor)
    {
        FloatAccessorLayout layout = GetFloatAccessorLayout(
            root, accessor, 2, "VEC2");
        ReadOnlySpan<byte> data = binary.Span;
        var result = new Vector2[layout.Count];
        for (int index = 0; index < result.Length; index++)
        {
            int start = checked(layout.Offset + index * layout.Stride);
            result[index] = new Vector2(
                ReadFloatComponent(data, start),
                ReadFloatComponent(data, start + 4));
        }
        return result;
    }

    // glTF permits floating-point weights and normalized unsigned integer
    // storage. Decode the latter to [0, 1] without renormalizing the VEC4 sum;
    // the transfer planner remains responsible for validating/normalizing it.
    private static Vector4[] ReadWeights(
        JsonElement root,
        ReadOnlyMemory<byte> binary,
        int accessorIndex)
    {
        JsonElement accessor = root.GetProperty("accessors")[accessorIndex];
        if (accessor.GetProperty("type").GetString() != "VEC4")
            throw new InvalidDataException("WEIGHTS_0 must be VEC4.");

        int componentType = accessor.GetProperty("componentType").GetInt32();
        int componentSize = componentType switch
        {
            5121 => 1,
            5123 => 2,
            5126 => 4,
            _ => throw new InvalidDataException(
                "WEIGHTS_0 must use FLOAT or normalized UNSIGNED_BYTE/UNSIGNED_SHORT.")
        };
        bool normalized = accessor.TryGetProperty(
            "normalized", out JsonElement normalizedElement) &&
            normalizedElement.GetBoolean();
        if (componentType is 5121 or 5123 && !normalized)
        {
            throw new InvalidDataException(
                "Integer WEIGHTS_0 must set normalized to true.");
        }
        if (componentType == 5126 && normalized)
            throw new InvalidDataException("FLOAT WEIGHTS_0 cannot be normalized.");
        if (!accessor.TryGetProperty("bufferView", out JsonElement viewIndex))
        {
            throw new InvalidDataException(
                "Sparse WEIGHTS_0 accessors without a bufferView are not supported.");
        }

        JsonElement view = root.GetProperty("bufferViews")[viewIndex.GetInt32()];
        int count = accessor.GetProperty("count").GetInt32();
        int viewOffset = view.TryGetProperty("byteOffset", out JsonElement viewOffsetElement)
            ? viewOffsetElement.GetInt32() : 0;
        int accessorOffset = accessor.TryGetProperty(
            "byteOffset", out JsonElement accessorOffsetElement)
            ? accessorOffsetElement.GetInt32() : 0;
        int elementSize = checked(componentSize * 4);
        int stride = view.TryGetProperty("byteStride", out JsonElement strideElement)
            ? strideElement.GetInt32() : elementSize;
        int viewLength = view.GetProperty("byteLength").GetInt32();
        if (count < 0 || viewOffset < 0 || accessorOffset < 0 ||
            stride < elementSize || viewLength < 0)
        {
            throw new InvalidDataException("WEIGHTS_0 has an invalid buffer layout.");
        }

        long relativeEnd = count == 0
            ? accessorOffset
            : (long)accessorOffset + (long)(count - 1) * stride + elementSize;
        long absoluteEnd = (long)viewOffset + relativeEnd;
        if (relativeEnd > viewLength || absoluteEnd > binary.Length)
            throw new InvalidDataException("WEIGHTS_0 crosses the binary buffer boundary.");

        ReadOnlySpan<byte> data = binary.Span;
        var result = new Vector4[count];
        for (int index = 0; index < count; index++)
        {
            int start = checked(viewOffset + accessorOffset + index * stride);
            Vector4 value = new(
                ReadWeightComponent(data, start, 0, componentType, componentSize),
                ReadWeightComponent(data, start, 1, componentType, componentSize),
                ReadWeightComponent(data, start, 2, componentType, componentSize),
                ReadWeightComponent(data, start, 3, componentType, componentSize));
            if (!float.IsFinite(value.X) || value.X < 0 ||
                !float.IsFinite(value.Y) || value.Y < 0 ||
                !float.IsFinite(value.Z) || value.Z < 0 ||
                !float.IsFinite(value.W) || value.W < 0)
            {
                throw new InvalidDataException(
                    $"WEIGHTS_0 contains an invalid value at element {index}.");
            }
            result[index] = value;
        }
        return result;
    }

    private static float ReadWeightComponent(
        ReadOnlySpan<byte> data,
        int start,
        int component,
        int componentType,
        int componentSize) => componentType switch
        {
            5121 => data[start + component] / 255f,
            5123 => BinaryPrimitives.ReadUInt16LittleEndian(
                data.Slice(start + component * componentSize, componentSize)) / 65535f,
            _ => BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(
                data.Slice(start + component * componentSize, componentSize)))
        };

    private static Matrix4x4[] ReadMatrix4(
        JsonElement root,
        ReadOnlyMemory<byte> binary,
        int accessor)
    {
        FloatAccessorLayout layout = GetFloatAccessorLayout(
            root, accessor, 16, "MAT4");
        ReadOnlySpan<byte> data = binary.Span;
        var result = new Matrix4x4[layout.Count];
        for (int index = 0; index < result.Length; index++)
        {
            int start = checked(layout.Offset + index * layout.Stride);
            result[index] = new Matrix4x4(
                ReadFloatComponent(data, start),
                ReadFloatComponent(data, start + 4),
                ReadFloatComponent(data, start + 8),
                ReadFloatComponent(data, start + 12),
                ReadFloatComponent(data, start + 16),
                ReadFloatComponent(data, start + 20),
                ReadFloatComponent(data, start + 24),
                ReadFloatComponent(data, start + 28),
                ReadFloatComponent(data, start + 32),
                ReadFloatComponent(data, start + 36),
                ReadFloatComponent(data, start + 40),
                ReadFloatComponent(data, start + 44),
                ReadFloatComponent(data, start + 48),
                ReadFloatComponent(data, start + 52),
                ReadFloatComponent(data, start + 56),
                ReadFloatComponent(data, start + 60));
        }
        return result;
    }

    private static ImportedJointIndices[] ReadJointIndices(
        JsonElement root,
        ReadOnlyMemory<byte> binary,
        int accessorIndex)
    {
        JsonElement accessor = root.GetProperty("accessors")[accessorIndex];
        if (accessor.GetProperty("type").GetString() != "VEC4")
            throw new InvalidDataException("JOINTS_0 must be VEC4.");
        JsonElement view = root.GetProperty("bufferViews")[accessor.GetProperty("bufferView").GetInt32()];
        int componentType = accessor.GetProperty("componentType").GetInt32();
        int componentSize = componentType switch
        {
            5121 => 1,
            5123 => 2,
            _ => throw new InvalidDataException("JOINTS_0 must use UNSIGNED_BYTE or UNSIGNED_SHORT.")
        };
        int count = accessor.GetProperty("count").GetInt32();
        int offset = (view.TryGetProperty("byteOffset", out JsonElement vo) ? vo.GetInt32() : 0) +
            (accessor.TryGetProperty("byteOffset", out JsonElement ao) ? ao.GetInt32() : 0);
        int stride = view.TryGetProperty("byteStride", out JsonElement strideElement)
            ? strideElement.GetInt32() : componentSize * 4;
        ReadOnlySpan<byte> span = binary.Span;
        var result = new ImportedJointIndices[count];
        for (int index = 0; index < count; index++)
        {
            int start = offset + index * stride;
            ushort x = ReadJointComponent(span, start, 0, componentType, componentSize);
            ushort y = ReadJointComponent(span, start, 1, componentType, componentSize);
            ushort z = ReadJointComponent(span, start, 2, componentType, componentSize);
            ushort w = ReadJointComponent(span, start, 3, componentType, componentSize);
            result[index] = new ImportedJointIndices(x, y, z, w);
        }
        return result;
    }

    private static ushort ReadJointComponent(
        ReadOnlySpan<byte> data,
        int start,
        int component,
        int componentType,
        int componentSize) => componentType == 5121
            ? data[start + component]
            : BinaryPrimitives.ReadUInt16LittleEndian(
                data.Slice(start + component * componentSize, componentSize));

    private static uint[] ReadColorsArgb(
        JsonElement root,
        ReadOnlyMemory<byte> binary,
        int accessorIndex)
    {
        JsonElement accessor = root.GetProperty("accessors")[accessorIndex];
        string type = accessor.GetProperty("type").GetString() ?? string.Empty;
        int width = type switch
        {
            "VEC3" => 3,
            "VEC4" => 4,
            _ => throw new InvalidDataException("COLOR_0 must be VEC3 or VEC4.")
        };
        int componentType = accessor.GetProperty("componentType").GetInt32();
        int componentSize = componentType switch
        {
            5121 => 1,
            5123 => 2,
            5126 => 4,
            _ => throw new InvalidDataException("Unsupported COLOR_0 component type.")
        };
        if (componentType != 5126 &&
            (!accessor.TryGetProperty("normalized", out JsonElement normalized) ||
             !normalized.GetBoolean()))
            throw new InvalidDataException("Integer COLOR_0 must be normalized.");
        JsonElement view = root.GetProperty("bufferViews")[accessor.GetProperty("bufferView").GetInt32()];
        int count = accessor.GetProperty("count").GetInt32();
        int offset = (view.TryGetProperty("byteOffset", out JsonElement vo) ? vo.GetInt32() : 0) +
            (accessor.TryGetProperty("byteOffset", out JsonElement ao) ? ao.GetInt32() : 0);
        int stride = view.TryGetProperty("byteStride", out JsonElement strideElement)
            ? strideElement.GetInt32() : componentSize * width;
        ReadOnlySpan<byte> data = binary.Span;
        var result = new uint[count];
        for (int index = 0; index < count; index++)
        {
            int start = offset + index * stride;
            byte r = ToColorByte(ReadColorComponent(
                data, start, 0, componentType, componentSize));
            byte g = ToColorByte(ReadColorComponent(
                data, start, 1, componentType, componentSize));
            byte b = ToColorByte(ReadColorComponent(
                data, start, 2, componentType, componentSize));
            byte a = width == 4
                ? ToColorByte(ReadColorComponent(
                    data, start, 3, componentType, componentSize))
                : byte.MaxValue;
            result[index] = (uint)(a << 24 | r << 16 | g << 8 | b);
        }
        return result;
    }

    private static float ReadColorComponent(
        ReadOnlySpan<byte> data,
        int start,
        int component,
        int componentType,
        int componentSize) => componentType switch
        {
            5121 => data[start + component] / 255f,
            5123 => BinaryPrimitives.ReadUInt16LittleEndian(
                data.Slice(start + component * componentSize, componentSize)) / 65535f,
            _ => BitConverter.Int32BitsToSingle(BinaryPrimitives.ReadInt32LittleEndian(
                data.Slice(start + component * componentSize, componentSize)))
        };

    private static byte ToColorByte(float value) => checked((byte)Math.Clamp(
        (int)MathF.Round(value * byte.MaxValue), 0, byte.MaxValue));

    private static FloatAccessorLayout GetFloatAccessorLayout(
        JsonElement root,
        int accessorIndex,
        int width,
        string expectedType)
    {
        JsonElement accessor = root.GetProperty("accessors")[accessorIndex];
        if (accessor.GetProperty("componentType").GetInt32() != 5126)
            throw new InvalidDataException("Only FLOAT vertex attributes are supported.");
        if (!string.Equals(
                accessor.GetProperty("type").GetString(),
                expectedType,
                StringComparison.Ordinal))
            throw new InvalidDataException(
                $"Accessor {accessorIndex} must be {expectedType}.");
        JsonElement view = root.GetProperty("bufferViews")[accessor.GetProperty("bufferView").GetInt32()];
        int count = accessor.GetProperty("count").GetInt32();
        int offset = (view.TryGetProperty("byteOffset", out JsonElement vo) ? vo.GetInt32() : 0) +
            (accessor.TryGetProperty("byteOffset", out JsonElement ao) ? ao.GetInt32() : 0);
        int stride = view.TryGetProperty("byteStride", out JsonElement strideElement)
            ? strideElement.GetInt32() : width * 4;
        return new FloatAccessorLayout(count, offset, stride);
    }

    private static float ReadFloatComponent(ReadOnlySpan<byte> data, int offset) =>
        BitConverter.Int32BitsToSingle(
            BinaryPrimitives.ReadInt32LittleEndian(data.Slice(offset, 4)));

    private readonly record struct FloatAccessorLayout(
        int Count,
        int Offset,
        int Stride);

    private static uint[] ReadIndices(JsonElement root, ReadOnlyMemory<byte> binary, int accessorIndex)
    {
        JsonElement accessor = root.GetProperty("accessors")[accessorIndex];
        JsonElement view = root.GetProperty("bufferViews")[accessor.GetProperty("bufferView").GetInt32()];
        int type = accessor.GetProperty("componentType").GetInt32();
        int size = type switch { 5121 => 1, 5123 => 2, 5125 => 4, _ => throw new InvalidDataException("Unsupported glTF index type.") };
        int count = accessor.GetProperty("count").GetInt32();
        int offset = (view.TryGetProperty("byteOffset", out JsonElement vo) ? vo.GetInt32() : 0) +
            (accessor.TryGetProperty("byteOffset", out JsonElement ao) ? ao.GetInt32() : 0);
        ReadOnlySpan<byte> span = binary.Span;
        var result = new uint[count];
        for (int i = 0; i < count; i++) result[i] = type switch
        {
            5121 => span[offset + i],
            5123 => BinaryPrimitives.ReadUInt16LittleEndian(span.Slice(offset + i * size, size)),
            _ => BinaryPrimitives.ReadUInt32LittleEndian(span.Slice(offset + i * size, size))
        };
        return result;
    }

}

public static class ImportedModelReader
{
    public static ImportedScene Read(
        string path,
        string? nativeFbxBridgePath = null,
        CancellationToken cancellationToken = default) => Path.GetExtension(path).ToLowerInvariant() switch
    {
        ".glb" => GlbModelReader.Read(path, cancellationToken),
        ".fbx" => FbxModelReader.Read(path, nativeFbxBridgePath, cancellationToken),
        ".obj" => ObjModelReader.Read(path, cancellationToken),
        _ => throw new NotSupportedException(
            "Supported replacement formats: .fbx, .glb and .obj.")
    };

    public static ImportedScene ReadGeometryOnly(
        string path,
        string? nativeFbxBridgePath = null,
        CancellationToken cancellationToken = default) => Path.GetExtension(path).ToLowerInvariant() switch
    {
        ".glb" => GlbModelReader.ReadGeometryOnly(path, cancellationToken),
        ".fbx" => FbxModelReader.ReadGeometryOnly(
            path, nativeFbxBridgePath, cancellationToken),
        ".obj" => ObjModelReader.Read(path, cancellationToken),
        _ => throw new NotSupportedException(
            "Geometry-only fallback supports .fbx, .glb and .obj.")
    };
}
