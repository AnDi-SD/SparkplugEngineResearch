using System.Numerics;
using System.Security.Cryptography;
using SmoViewer.Core;

namespace SmoImporter.Core;

public enum SmoSkeletonCompatibility
{
    Exact,
    CompatibleWithWarnings,
    Incompatible
}

public sealed record SmoHierarchyAdaptation(
    string BoneName,
    string TargetPath,
    string DonorPath);

public sealed record SmoToSmoReplacementPlan(
    SmoSkeletonCompatibility Compatibility,
    int TargetMeshCount,
    int DonorMeshCount,
    int TargetTextureCount,
    int DonorTextureCount,
    int TargetBoneCount,
    int DonorBoneCount,
    int DifferentBindPoseBoneCount,
    IReadOnlyList<string> MatchedBoneNames,
    IReadOnlyList<SmoIgnoredDonorBone> IgnoredDonorBones,
    IReadOnlyList<string> UnboundTargetBones,
    IReadOnlyList<SmoHierarchyAdaptation> HierarchyAdaptations,
    string TargetSha256,
    string DonorSha256,
    IReadOnlyList<string> Messages)
{
    public bool CanReplace => Compatibility != SmoSkeletonCompatibility.Incompatible;
}

public sealed record SmoToSmoReplacementResult(
    string OutputPath,
    int MeshCount,
    int TextureCount,
    int DonorMeshCount,
    int DonorTextureCount,
    int TriangleCount,
    int BoneCount,
    long FileSize,
    string Sha256,
    SmoSkeletonCompatibility Compatibility);

/// <summary>
/// Creates a game-resource replacement that keeps the complete target object
/// graph and transplants the donor's mesh/texture visual payload into it.
/// </summary>
public static class SmoToSmoReplacer
{
    private const float BindPoseTolerance = 0.01f;

    public static SmoToSmoReplacementPlan Analyze(
        SmoDocument target,
        SmoDocument donor)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(donor);

        var blocking = new List<string>();
        var warnings = new List<string>();
        if (target.SourcePath is not null && donor.SourcePath is not null &&
            Path.GetFullPath(target.SourcePath).Equals(
                Path.GetFullPath(donor.SourcePath), StringComparison.OrdinalIgnoreCase))
        {
            blocking.Add("Целевой SMO и SMO-донор указывают на один и тот же файл.");
        }
        if (target.HasErrors)
            blocking.Add("Целевой SMO содержит ошибки структуры FFPS.");
        if (donor.HasErrors)
            blocking.Add("SMO-донор содержит ошибки структуры FFPS.");
        if (target.Header.Unknown04 != donor.Header.Unknown04)
            blocking.Add(
                $"Версия serializer различается: target=0x{target.Header.Unknown04:X}, " +
                $"donor=0x{donor.Header.Unknown04:X}.");
        if (target.Header.Version != donor.Header.Version)
            blocking.Add(
                $"Платформенный профиль различается: target=0x{target.Header.Version:X}, " +
                $"donor=0x{donor.Header.Version:X}.");

        int targetMeshes = CountObjects(target, SmoClassIds.MeshData);
        int donorMeshes = CountObjects(donor, SmoClassIds.MeshData);
        int targetTextures = CountObjects(target, SmoClassIds.TextureData);
        int donorTextures = CountObjects(donor, SmoClassIds.TextureData);
        if (targetMeshes == 0)
            blocking.Add("Целевой SMO не содержит mesh-объектов.");
        if (donorMeshes == 0)
            blocking.Add("SMO-донор не содержит mesh-объектов.");

        SkeletonProfile targetSkeleton = BuildSkeletonProfile(target);
        SkeletonProfile donorSkeleton = BuildSkeletonProfile(donor);
        SmoBoneRemapPlan boneMapping = SmoBoneMappingPlanner.Build(target, donor);
        blocking.AddRange(targetSkeleton.Errors.Select(message => "Target: " + message));
        blocking.AddRange(donorSkeleton.Errors.Select(message => "Donor: " + message));
        blocking.AddRange(boneMapping.Errors.Select(message => "Bone mapping: " + message));

        if (targetSkeleton.Bones.Count == 0 || donorSkeleton.Bones.Count == 0)
        {
            blocking.Add(
                "Текущий режим визуальной подмены требует skin palettes в обеих моделях.");
        }
        if (boneMapping.UnboundTargetBones.Count > 0)
            warnings.Add(
                $"Target-кости без donor weights ({boneMapping.UnboundTargetBones.Count}): " +
                FormatNames(boneMapping.UnboundTargetBones) +
                ". Они останутся в target graph без новой vertex-привязки; модель может " +
                "отображаться неверно.");
        if (boneMapping.IgnoredDonorBones.Count > 0)
            warnings.Add(
                $"Дополнительные donor-кости ({boneMapping.IgnoredDonorBones.Count}) " +
                "будут проигнорированы, а их веса перенесены на ближайшие shared bones: " +
                FormatNames(boneMapping.IgnoredDonorBones.Select(item =>
                    $"{item.DonorBoneName}→{item.TargetBoneName}").ToArray()) + ".");
        int differentBindPoseBones = 0;
        var hierarchyAdaptations = new List<SmoHierarchyAdaptation>();
        foreach (string name in targetSkeleton.Bones.Keys.Intersect(
                     donorSkeleton.Bones.Keys, StringComparer.Ordinal))
        {
            SkeletonBone targetBone = targetSkeleton.Bones[name];
            SkeletonBone donorBone = donorSkeleton.Bones[name];
            if (!string.Equals(
                    targetBone.ParentName,
                    donorBone.ParentName,
                    StringComparison.Ordinal))
            {
                blocking.Add(
                    $"Иерархия кости \"{name}\" различается: target parent " +
                    $"\"{targetBone.ParentName}\", donor parent \"{donorBone.ParentName}\".");
            }
            else if (!targetBone.ParentPath.Equals(
                         donorBone.ParentPath, StringComparison.Ordinal))
            {
                hierarchyAdaptations.Add(new SmoHierarchyAdaptation(
                    name, targetBone.ParentPath, donorBone.ParentPath));
            }
            if (!ApproximatelyEqual(targetBone.BindWorld, donorBone.BindWorld))
                differentBindPoseBones++;
        }
        if (differentBindPoseBones > 0)
        {
            warnings.Add(
                $"У {differentBindPoseBones} костей различается bind pose. Имена и иерархия " +
                "совместимы; inverse bind matrices будут взяты из донора, а node graph и " +
                "анимационные узлы останутся от target. Нужна проверка результата в игре.");
        }
        if (hierarchyAdaptations.Count > 0)
        {
            warnings.Add(
                $"У {hierarchyAdaptations.Count} общих костей различаются только " +
                "промежуточные helper/control nodes. После их пропуска weighted hierarchy " +
                "совпадает; подробности показаны в дереве сопоставления.");
        }

        if (targetTextures == 0 || donorTextures == 0)
            blocking.Add("Обе модели должны содержать texture-объекты с подтверждёнными mesh bindings.");
        if (donorTextures > targetTextures)
            blocking.Add(
                $"Число texture slots различается: target={targetTextures}, donor={donorTextures}. " +
                "Безопасный режим сохраняет весь object graph целевого SMO и пока не умеет " +
                "добавлять новые material/texture branches. Подмена остановлена без записи файла.");
        else if (targetTextures != donorTextures)
            warnings.Add(
                $"Число texture slots различается: target={targetTextures}, donor={donorTextures}. " +
                "Лишние target texture slots сохранят IDs/ссылки и получат копию ближайшей " +
                "donor texture; object graph target не меняется.");
        if (targetMeshes != donorMeshes)
            warnings.Add(
                $"Число mesh slots различается: target={targetMeshes}, donor={donorMeshes}. " +
                "Геометрия донора будет перераспределена, лишние target slots станут невидимыми.");

        // Run the complete in-memory planner here so the GUI blocks unsupported
        // texture layouts or palette capacity before the user chooses an output.
        if (blocking.Count == 0)
        {
            try
            {
                _ = SmoVisualTransplanter.Transplant(target, donor);
            }
            catch (Exception exception) when (exception is InvalidDataException or
                                              InvalidOperationException or
                                              OverflowException)
            {
                blocking.Add("Visual transplant не может быть построен: " + exception.Message);
            }
        }

        SmoSkeletonCompatibility compatibility = blocking.Count > 0
            ? SmoSkeletonCompatibility.Incompatible
            : warnings.Count > 0
                ? SmoSkeletonCompatibility.CompatibleWithWarnings
                : SmoSkeletonCompatibility.Exact;
        string[] messages = blocking.Concat(warnings).ToArray();
        return new SmoToSmoReplacementPlan(
            compatibility,
            targetMeshes,
            donorMeshes,
            targetTextures,
            donorTextures,
            targetSkeleton.Bones.Count,
            donorSkeleton.Bones.Count,
            differentBindPoseBones,
            boneMapping.MatchedBones,
            boneMapping.IgnoredDonorBones,
            boneMapping.UnboundTargetBones,
            hierarchyAdaptations,
            ComputeSha256(target.Data.Span),
            ComputeSha256(donor.Data.Span),
            messages);
    }

    public static SmoToSmoReplacementResult Replace(
        SmoDocument target,
        SmoDocument donor,
        string outputPath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(outputPath);
        SmoToSmoReplacementPlan plan = Analyze(target, donor);
        if (!plan.CanReplace)
            throw new InvalidOperationException(
                "SMO-донор несовместим с целевым скелетом:\n" +
                string.Join("\n", plan.Messages.Select(message => "• " + message)));

        string fullOutput = Path.GetFullPath(outputPath);
        EnsureSeparateOutput(fullOutput, target.SourcePath, "целевой SMO");
        EnsureSeparateOutput(fullOutput, donor.SourcePath, "SMO-донор");
        string directory = Path.GetDirectoryName(fullOutput) ??
            throw new InvalidOperationException("Не удалось определить папку результата.");
        Directory.CreateDirectory(directory);
        string temporaryPath = Path.Combine(
            directory, $".{Path.GetFileName(fullOutput)}.{Guid.NewGuid():N}.tmp");

        try
        {
            SmoVisualTransplantResult transplant =
                SmoVisualTransplanter.Transplant(target, donor);
            File.WriteAllBytes(temporaryPath, transplant.Data);
            SmoDocument verified = SmoDocument.Load(temporaryPath);
            string verifiedHash = ComputeSha256(verified.Data.Span);
            int verifiedTriangles = verified.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
                .Select(entry => SmoMeshDecoder.Decode(verified, entry))
                .Sum(CountNonDegenerateTriangles);
            var verificationErrors = new List<string>();
            if (verified.HasErrors)
                verificationErrors.Add("strict parser reported errors");
            if (verified.Header.Unknown04 != target.Header.Unknown04 ||
                verified.Header.Version != target.Header.Version)
                verificationErrors.Add("target header profile changed");
            if (verified.Objects.Count != target.Objects.Count)
                verificationErrors.Add(
                    $"object count {verified.Objects.Count} != target {target.Objects.Count}");
            if (!verified.Objects.Zip(target.Objects).All(pair =>
                    pair.First.Id == pair.Second.Id &&
                    pair.First.Name == pair.Second.Name &&
                    pair.First.TypeHash == pair.Second.TypeHash))
                verificationErrors.Add("target object identities changed");
            int verifiedMeshes = verified.Objects.Count(entry =>
                entry.TypeHash == SmoClassIds.MeshData);
            if (verifiedMeshes != transplant.MeshCount)
                verificationErrors.Add(
                    $"mesh slots {verifiedMeshes} != expected {transplant.MeshCount}");
            int verifiedTextures = verified.Objects.Count(entry =>
                entry.TypeHash == SmoClassIds.TextureData);
            if (verifiedTextures != transplant.TextureCount)
                verificationErrors.Add(
                    $"texture slots {verifiedTextures} != expected {transplant.TextureCount}");
            if (verifiedTriangles != transplant.TriangleCount)
                verificationErrors.Add(
                    $"triangles {verifiedTriangles} != donor {transplant.TriangleCount}");
            foreach (SmoObjectEntry skinEntry in verified.Objects.Where(entry =>
                         entry.TypeHash == SmoClassIds.Skin))
            {
                if (!SmoSkinDecoder.TryDecode(
                        verified, skinEntry, out SmoSkin? skin, out string skinError) ||
                    skin is null)
                {
                    verificationErrors.Add(
                        $"skin [{skinEntry.Index}] is invalid: {skinError}");
                }
            }
            if (verificationErrors.Count > 0)
            {
                throw new InvalidDataException(
                    "Результат SMO→SMO не сохранил target object graph или не прошёл " +
                    "структурную проверку visual payload: " +
                    string.Join("; ", verificationErrors) + ".");
            }

            File.Move(temporaryPath, fullOutput, true);
            return new SmoToSmoReplacementResult(
                fullOutput,
                transplant.MeshCount,
                transplant.TextureCount,
                plan.DonorMeshCount,
                transplant.DonorTextureCount,
                transplant.TriangleCount,
                plan.TargetBoneCount,
                verified.Data.Length,
                verifiedHash,
                plan.Compatibility);
        }
        finally
        {
            if (File.Exists(temporaryPath))
                File.Delete(temporaryPath);
        }
    }

    private static SkeletonProfile BuildSkeletonProfile(SmoDocument document)
    {
        var errors = new List<string>();
        var occurrences = new Dictionary<string, List<SkeletonBoneOccurrence>>(
            StringComparer.Ordinal);
        SmoNodeHierarchy hierarchy = SmoNodeHierarchy.Decode(document);
        IReadOnlyDictionary<int, SmoObjectEntry> entries = document.Objects
            .ToDictionary(entry => entry.Index);

        foreach (SmoObjectEntry skinEntry in document.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Skin))
        {
            if (!SmoSkinDecoder.TryDecode(
                    document, skinEntry, out SmoSkin? skin, out string error) ||
                skin is null)
            {
                errors.Add($"skin [{skinEntry.Index}] не декодирован: {error}");
                continue;
            }

            foreach (SmoSkinBone bone in skin.Bones)
            {
                SmoObjectEntry node = entries[bone.NodeObjectIndex];
                if (string.IsNullOrWhiteSpace(node.Name))
                {
                    errors.Add(
                        $"skin [{skinEntry.Index}] содержит безымянную кость " +
                        $"в palette slot {bone.PaletteIndex}.");
                    continue;
                }
                if (!Matrix4x4.Invert(bone.InverseBindMatrix, out Matrix4x4 bindWorld))
                {
                    errors.Add($"У кости \"{node.Name}\" необратимая inverse bind matrix.");
                    continue;
                }

                if (!occurrences.TryGetValue(
                        node.Name, out List<SkeletonBoneOccurrence>? values))
                {
                    values = [];
                    occurrences.Add(node.Name, values);
                }
                values.Add(new SkeletonBoneOccurrence(
                    node.Index, bindWorld));
            }
        }

        var bones = new Dictionary<string, SkeletonBone>(StringComparer.Ordinal);
        HashSet<string> weightedNames = occurrences.Keys.ToHashSet(StringComparer.Ordinal);
        foreach ((string name, List<SkeletonBoneOccurrence> values) in occurrences)
        {
            int[] nodeIndices = values.Select(value => value.NodeObjectIndex)
                .Distinct().ToArray();
            if (nodeIndices.Length != 1)
            {
                errors.Add(
                    $"Имя кости \"{name}\" неоднозначно и принадлежит объектам " +
                    string.Join(", ", nodeIndices.Select(index => $"[{index}]")) + ".");
                continue;
            }
            if (values.Skip(1).Any(value =>
                    !ApproximatelyEqual(values[0].BindWorld, value.BindWorld)))
            {
                errors.Add(
                    $"Кость \"{name}\" имеет разные bind matrices в skin palettes.");
                continue;
            }
            BoneParentInfo[] parentInfos = values.Select(value =>
                    ResolveWeightedBoneParent(
                        value.NodeObjectIndex, hierarchy, entries, weightedNames))
                .ToArray();
            string[] parentNames = parentInfos.Select(value => value.ParentName)
                .Where(name => name is not null).Cast<string>()
                .Distinct(StringComparer.Ordinal).ToArray();
            if (parentNames.Length > 1)
            {
                errors.Add($"У кости \"{name}\" неоднозначная иерархия родителей.");
                continue;
            }
            string[] parentPaths = parentInfos.Select(value => value.Path)
                .Distinct(StringComparer.Ordinal).ToArray();
            if (parentPaths.Length > 1)
            {
                errors.Add($"У кости \"{name}\" неоднозначная цепочка родителей.");
                continue;
            }
            bones.Add(name, new SkeletonBone(
                name,
                parentNames.SingleOrDefault(),
                parentPaths.SingleOrDefault() ?? "(root)",
                values[0].BindWorld));
        }

        return new SkeletonProfile(bones, errors);
    }

    private static int CountNonDegenerateTriangles(SmoMesh mesh)
    {
        int count = 0;
        for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
        {
            uint first = mesh.TriangleIndices[index];
            uint second = mesh.TriangleIndices[index + 1];
            uint third = mesh.TriangleIndices[index + 2];
            if (first != second && second != third && first != third)
                count++;
        }
        return count;
    }

    private static BoneParentInfo ResolveWeightedBoneParent(
        int nodeIndex,
        SmoNodeHierarchy hierarchy,
        IReadOnlyDictionary<int, SmoObjectEntry> entries,
        IReadOnlySet<string> weightedNames)
    {
        var path = new List<string>();
        var visited = new HashSet<int> { nodeIndex };
        int cursor = nodeIndex;
        while (hierarchy.ParentsByChild.TryGetValue(
                   cursor, out IReadOnlyList<int>? parents) && parents.Count == 1 &&
               entries.TryGetValue(parents[0], out SmoObjectEntry? parent) &&
               parent.TypeHash == SmoClassIds.Node && visited.Add(parent.Index))
        {
            path.Add(parent.Name);
            if (weightedNames.Contains(parent.Name))
                return new BoneParentInfo(
                    parent.Name,
                    path.Count == 0 ? "(root)" : string.Join(" → ", path));
            cursor = parent.Index;
        }
        return new BoneParentInfo(
            null,
            path.Count == 0 ? "(root)" : string.Join(" → ", path) + " → (root)");
    }

    private static void EnsureSeparateOutput(
        string outputPath,
        string? inputPath,
        string inputDescription)
    {
        if (inputPath is not null && Path.GetFullPath(inputPath).Equals(
                outputPath, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(
                $"Результат нельзя записать поверх {inputDescription}; выберите новый файл.");
        }
    }

    private static int CountObjects(SmoDocument document, uint classId) =>
        document.Objects.Count(entry => entry.TypeHash == classId);

    private static string ComputeSha256(ReadOnlySpan<byte> data) =>
        Convert.ToHexString(SHA256.HashData(data));

    private static bool ApproximatelyEqual(Matrix4x4 left, Matrix4x4 right)
    {
        ReadOnlySpan<float> leftValues = MatrixValues(left);
        ReadOnlySpan<float> rightValues = MatrixValues(right);
        for (int index = 0; index < leftValues.Length; index++)
            if (MathF.Abs(leftValues[index] - rightValues[index]) > BindPoseTolerance)
                return false;
        return true;
    }

    private static float[] MatrixValues(Matrix4x4 value) =>
    [
        value.M11, value.M12, value.M13, value.M14,
        value.M21, value.M22, value.M23, value.M24,
        value.M31, value.M32, value.M33, value.M34,
        value.M41, value.M42, value.M43, value.M44
    ];

    private static string FormatNames(IReadOnlyList<string> names)
    {
        const int maximumShown = 12;
        string shown = string.Join(", ", names.Take(maximumShown));
        return names.Count <= maximumShown
            ? shown
            : $"{shown} … и ещё {names.Count - maximumShown}";
    }

    private sealed record SkeletonProfile(
        IReadOnlyDictionary<string, SkeletonBone> Bones,
        IReadOnlyList<string> Errors);

    private sealed record SkeletonBone(
        string Name,
        string? ParentName,
        string ParentPath,
        Matrix4x4 BindWorld);

    private sealed record SkeletonBoneOccurrence(
        int NodeObjectIndex,
        Matrix4x4 BindWorld);

    private sealed record BoneParentInfo(
        string? ParentName,
        string Path);
}
