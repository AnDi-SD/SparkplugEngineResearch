using SmoImporter.Core;
using SmoViewer.Core;
using System.Numerics;

internal static class MaterialGroupMatchingRegression
{
    public static void RunNativeBranchIntegration(
        string targetPath,
        string donorPath,
        string outputPath)
    {
        targetPath = Path.GetFullPath(targetPath);
        donorPath = Path.GetFullPath(donorPath);
        outputPath = Path.GetFullPath(outputPath);
        if (string.Equals(targetPath, outputPath, StringComparison.OrdinalIgnoreCase) ||
            string.Equals(donorPath, outputPath, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(
                "Material matching integration output must be separate from both inputs.");
        }

        byte[] targetBefore = File.ReadAllBytes(targetPath);
        byte[] donorBefore = File.ReadAllBytes(donorPath);
        SmoDocument target = SmoDocument.Load(targetPath);
        ImportedScene source = ImportedModelReader.ReadGeometryOnly(donorPath);
        ImportedTexture[] adjacentTextures = source.Materials
            .Select(material => material.BaseColorTextureName)
            .Where(name => !string.IsNullOrWhiteSpace(name))
            .Select(name => Path.Combine(
                Path.GetDirectoryName(donorPath)!, Path.GetFileName(name!)))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .Where(File.Exists)
            .Select(ImportedTextureFileReader.Read)
            .ToArray();
        if (adjacentTextures.Length > 0)
        {
            source = ImportedTextureCatalog.ResolveExternalOverrides(
                source, Array.AsReadOnly(adjacentTextures)).EffectiveScene;
        }
        Require(source.Meshes.Count > 0 && source.Textures.Count == 2,
            "the integration donor must expose exactly two resolved texture groups");

        ImportedScene donor = BindEveryVertexToPelvis(target, source);
        Vector2[][] uvBefore = donor.Meshes
            .Select(mesh => mesh.TextureCoordinates.ToArray())
            .ToArray();
        byte[][] texturesBefore = donor.Textures
            .Select(texture => texture.Data.ToArray())
            .ToArray();

        VerifyCompatibleTwoToTwoRemainsSeparate(target, donor, outputPath);

        GlbSkinTransferPlan plan = SmoSkinnedGlbReplacer.Analyze(target, donor);
        Require(plan.CanReplace,
            "the equal-count integration donor must become writable through native branches: " +
            string.Join(" | ", plan.Messages));
        Require(plan.Messages.Count(message => message.Contains(
                    "independent native TextureData", StringComparison.Ordinal)) == 1,
            "analysis must select independent native texture branches");
        Require(plan.MaterialGroupCount == 2,
            "the final writer plan must retain both donor texture groups");

        GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
            target,
            donor,
            ReplacementTransform.Identity,
            outputPath,
            SkinnedGeometryTransferMode.PreservePreparedGeometry);
        SmoDocument output = SmoDocument.Load(outputPath);
        Require(!output.HasErrors,
            "the native-branch fallback output must pass the strict SMO parser");
        Require(result.TriangleCount ==
                source.Meshes.Sum(mesh => mesh.TriangleIndices.Length / 3),
            "the native-branch writer must preserve every donor triangle");
        int addedTextures = output.Objects.Count(entry =>
            entry.TypeHash == SmoClassIds.TextureData &&
            target.Objects.All(sourceEntry => sourceEntry.Id != entry.Id));
        Require(addedTextures == 2,
            "the fallback must append two independent TextureData objects");
        VerifyRigidPalettesUnchanged(target, output);
        Require(donor.Meshes.Select((mesh, index) =>
                    mesh.TextureCoordinates.SequenceEqual(uvBefore[index])).All(value => value),
            "analysis/writer must not mutate caller-owned donor UV arrays");
        Require(donor.Textures.Select((texture, index) =>
                    texture.Data.SequenceEqual(texturesBefore[index])).All(value => value),
            "analysis/writer must not mutate caller-owned donor textures");
        Require(File.ReadAllBytes(targetPath).SequenceEqual(targetBefore) &&
                File.ReadAllBytes(donorPath).SequenceEqual(donorBefore),
            "the integration run must leave both source files byte-identical");
    }

    private static void VerifyCompatibleTwoToTwoRemainsSeparate(
        SmoDocument target,
        ImportedScene pelvisDonor,
        string requestedOutputPath)
    {
        ImportedScene compatibleDonor = BindTextureGroupToJoint(
            pelvisDonor, sourceTextureIndex: 1, jointName: "Head");
        GlbSkinTransferPlan direct = SmoSkinnedGlbReplacer.Analyze(
            target, compatibleDonor);
        Require(direct.CanReplace,
            "the compatible 2-to-2 donor must have a complete direct assignment: " +
            string.Join(" | ", direct.Messages));
        Require(direct.MaterialGroupCount == 2 &&
                !direct.Messages.Any(message => message.Contains(
                    "Packed 2 donor texture groups", StringComparison.Ordinal)),
            "a compatible 2-to-2 assignment must retain both textures separately");

        string directory = Path.GetDirectoryName(requestedOutputPath) ??
            throw new InvalidOperationException("Integration output directory is unavailable.");
        string directOutput = Path.Combine(
            directory,
            $".{Path.GetFileName(requestedOutputPath)}.direct-{Guid.NewGuid():N}.smo");
        try
        {
            GlbSkinTransferResult result = SmoSkinnedGlbReplacer.Replace(
                target,
                compatibleDonor,
                ReplacementTransform.Identity,
                directOutput,
                SkinnedGeometryTransferMode.PreservePreparedGeometry);
            SmoDocument output = SmoDocument.Load(directOutput);
            Require(!output.HasErrors &&
                    result.TriangleCount == compatibleDonor.Meshes.Sum(mesh =>
                        mesh.TriangleIndices.Length / 3),
                "the compatible direct assignment must pass strict writer verification");
            VerifyRigidPalettesUnchanged(target, output);
        }
        finally
        {
            if (File.Exists(directOutput))
                File.Delete(directOutput);
        }
    }

    private static ImportedScene BindEveryVertexToPelvis(
        SmoDocument target,
        ImportedScene source)
    {
        TargetRigDefinition rig = TargetRigDefinition.FromSmoDocument(target);
        int pelvisIndex = rig.GetJointIndex("Pelvis");
        Matrix4x4[] inverseBind = rig.Joints.Select(joint =>
        {
            if (!Matrix4x4.Invert(joint.BindWorldMatrix, out Matrix4x4 inverse))
            {
                throw new InvalidOperationException(
                    $"Integration target joint {joint.Name} has no invertible bind matrix.");
            }
            return inverse;
        }).ToArray();
        var skeleton = new ImportedSkeleton(
            "material_matching_target_rig",
            Array.AsReadOnly(rig.Joints.Select(joint => joint.Name).ToArray()),
            Array.AsReadOnly(inverseBind))
        {
            ParentJointIndices = Array.AsReadOnly(
                rig.Joints.Select(joint => joint.ParentJointIndex).ToArray()),
            BindWorldMatrices = Array.AsReadOnly(
                rig.Joints.Select(joint => joint.BindWorldMatrix).ToArray()),
            BindLocalMatrices = Array.AsReadOnly(
                rig.Joints.Select(joint => joint.BindLocalMatrix).ToArray())
        };
        ImportedMesh[] meshes = source.Meshes.Select(mesh => mesh with
        {
            Skinning = new ImportedSkinning(
                skeleton,
                Enumerable.Repeat(
                    new ImportedJointIndices(checked((ushort)pelvisIndex), 0, 0, 0),
                    mesh.Positions.Length).ToArray(),
                Enumerable.Repeat(Vector4.UnitX, mesh.Positions.Length).ToArray())
        }).ToArray();
        return source with { Meshes = Array.AsReadOnly(meshes) };
    }

    private static ImportedScene BindTextureGroupToJoint(
        ImportedScene source,
        int sourceTextureIndex,
        string jointName)
    {
        ImportedSkeleton skeleton = source.Meshes
            .Select(mesh => mesh.Skinning?.Skeleton)
            .First(value => value is not null)!;
        int jointIndex = skeleton.JointNames
            .Select((name, index) => (name, index))
            .Single(item => string.Equals(
                item.name, jointName, StringComparison.Ordinal)).index;
        bool selectedAny = false;
        ImportedMesh[] meshes = source.Meshes.Select(mesh =>
        {
            bool selected = (uint)mesh.MaterialIndex < (uint)source.Materials.Count &&
                source.Materials[mesh.MaterialIndex].BaseColorTextureIndex ==
                sourceTextureIndex;
            if (!selected)
                return mesh;
            selectedAny = true;
            return mesh with
            {
                Skinning = new ImportedSkinning(
                    skeleton,
                    Enumerable.Repeat(
                        new ImportedJointIndices(
                            checked((ushort)jointIndex), 0, 0, 0),
                        mesh.Positions.Length).ToArray(),
                    Enumerable.Repeat(Vector4.UnitX, mesh.Positions.Length).ToArray())
            };
        }).ToArray();
        Require(selectedAny,
            $"source texture group {sourceTextureIndex} must select at least one mesh");
        return source with { Meshes = Array.AsReadOnly(meshes) };
    }

    private static void VerifyRigidPalettesUnchanged(
        SmoDocument target,
        SmoDocument output)
    {
        foreach (SmoObjectEntry targetSkinEntry in target.Objects.Where(entry =>
                     entry.TypeHash == SmoClassIds.Skin))
        {
            if (!SmoSkinDecoder.TryDecode(
                    target, targetSkinEntry, out SmoSkin? before, out _) ||
                before is null)
            {
                continue;
            }
            string[] names = before.Bones.Select(bone =>
                target.Objects[bone.NodeObjectIndex].Name).ToArray();
            if (names.Distinct(StringComparer.Ordinal).Take(2).Count() != 1)
                continue;
            SmoObjectEntry outputSkinEntry = output.Objects.Single(entry =>
                entry.Id == targetSkinEntry.Id);
            Require(SmoSkinDecoder.TryDecode(
                    output, outputSkinEntry, out SmoSkin? after, out string error) &&
                after is not null,
                $"rigid target skin [{targetSkinEntry.Index}] must remain decodable: {error}");
            string[] outputNames = after!.Bones.Select(bone =>
                output.Objects[bone.NodeObjectIndex].Name).ToArray();
            Require(names.SequenceEqual(outputNames, StringComparer.Ordinal),
                $"rigid target skin [{targetSkinEntry.Index}] palette must remain unchanged");
        }
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException("Material matching regression: " + message + ".");
    }
}
