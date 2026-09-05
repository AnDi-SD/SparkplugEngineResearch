using SmoLVLcreator.Core;
using SmoImporter.Core;
using SmoViewer.Core;
using SmoViewer.Scene;
using System.Buffers.Binary;
using System.Globalization;
using System.Numerics;

namespace SmoLVLcreator.ProjectTool;

internal static class Program
{
    public static int Main(string[] args)
    {
        try
        {
            if (args.Length == 3 && Is(args[0], "import"))
            {
                Import(args[1], args[2]);
                return 0;
            }
            if (args.Length == 2 && Is(args[0], "model-info"))
            {
                ShowModelInfo(args[1]);
                return 0;
            }
            if (args.Length == 3 && Is(args[0], "scene-mesh-info"))
            {
                ShowSceneMeshInfo(args[1], args[2]);
                return 0;
            }
            if (args.Length == 3 && Is(args[0], "build"))
            {
                Build(args[1], args[2]);
                return 0;
            }
            if (args.Length == 4 && Is(args[0], "roundtrip"))
            {
                RoundTrip(args[1], args[2], args[3]);
                return 0;
            }
            if (args.Length == 2 && Is(args[0], "placements"))
            {
                ListPlacements(args[1]);
                return 0;
            }
            if (args.Length == 3 && Is(args[0], "placement-transform"))
            {
                ShowPlacementTransform(args[1], ParseObjectIndex(args[2]));
                return 0;
            }
            if (args.Length == 2 && Is(args[0], "missing-transforms"))
            {
                ListMissingTransforms(args[1]);
                return 0;
            }
            if (args.Length == 2 && Is(args[0], "transform-objects"))
            {
                ListTransformObjects(args[1]);
                return 0;
            }
            if (args.Length == 2 && Is(args[0], "reference-templates"))
            {
                ListReferenceTemplates(args[1]);
                return 0;
            }
            if (args.Length == 2 && Is(args[0], "node-entities"))
            {
                ListNodeEntities(args[1]);
                return 0;
            }
            if (args.Length == 8 && Is(args[0], "clone-reference"))
            {
                CloneReferencePlacement(
                    args[1],
                    ParseObjectIndex(args[2]),
                    new Vector3(
                        ParseSingle(args[3]),
                        ParseSingle(args[4]),
                        ParseSingle(args[5])),
                    args[6],
                    args[7]);
                return 0;
            }
            if (args.Length == 4 && Is(args[0], "remove-branch"))
            {
                RemoveBranch(
                    args[1],
                    ParseObjectIndex(args[2]),
                    args[3]);
                return 0;
            }
            if (args.Length == 4 && Is(args[0], "remove-branch-preserve"))
            {
                RemoveBranchPreservingSharedResources(
                    args[1],
                    ParseObjectIndex(args[2]),
                    args[3]);
                return 0;
            }
            if (args.Length == 4 && Is(args[0], "remove-placement"))
            {
                RemovePlacement(args[1], ParseObjectId(args[2]), args[3]);
                return 0;
            }
            if (args.Length == 7 && Is(args[0], "translate"))
            {
                Translate(
                    args[1],
                    ParseObjectIndex(args[2]),
                    new Vector3(
                        ParseSingle(args[3]),
                        ParseSingle(args[4]),
                        ParseSingle(args[5])),
                    args[6]);
                return 0;
            }
            if (args.Length == 13 && Is(args[0], "set-placement-trs"))
            {
                SetPlacementTrs(
                    args[1],
                    ParseObjectIndex(args[2]),
                    new Vector3(
                        ParseSingle(args[3]),
                        ParseSingle(args[4]),
                        ParseSingle(args[5])),
                    new Vector3(
                        ParseSingle(args[6]),
                        ParseSingle(args[7]),
                        ParseSingle(args[8])),
                    new Vector3(
                        ParseSingle(args[9]),
                        ParseSingle(args[10]),
                        ParseSingle(args[11])),
                    args[12]);
                return 0;
            }
            if (args.Length == 13 && Is(args[0], "set-entity-trs"))
            {
                SetEntityTrs(
                    args[1],
                    args[2],
                    new Vector3(
                        ParseSingle(args[3]),
                        ParseSingle(args[4]),
                        ParseSingle(args[5])),
                    new Vector3(
                        ParseSingle(args[6]),
                        ParseSingle(args[7]),
                        ParseSingle(args[8])),
                    new Vector3(
                        ParseSingle(args[9]),
                        ParseSingle(args[10]),
                        ParseSingle(args[11])),
                    args[12]);
                return 0;
            }
            if (args.Length == 8 && Is(args[0], "set-vector"))
            {
                SetVector(
                    args[1],
                    ParseObjectIndex(args[2]),
                    args[3],
                    new Vector3(
                        ParseSingle(args[4]),
                        ParseSingle(args[5]),
                        ParseSingle(args[6])),
                    args[7]);
                return 0;
            }
            if (args.Length == 9 && Is(args[0], "set-quaternion"))
            {
                SetQuaternion(
                    args[1],
                    ParseObjectIndex(args[2]),
                    args[3],
                    new Quaternion(
                        ParseSingle(args[4]),
                        ParseSingle(args[5]),
                        ParseSingle(args[6]),
                        ParseSingle(args[7])),
                    args[8]);
                return 0;
            }

            PrintUsage();
            return 64;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine($"ОШИБКА: {exception.Message}");
            Console.Error.WriteLine(exception);
            return 1;
        }
    }

    private static void Import(string sourceArgument, string projectArgument)
    {
        string sourcePath = ExistingFile(sourceArgument);
        string projectPath = OutputFile(projectArgument, ".smolvlproj");
        SmoDocument source = SmoDocument.Load(sourcePath);
        SmoProject project = SmoProject.Import(source);
        SmoProjectArchive.Save(project, projectPath);
        Console.WriteLine(
            $"PROJECT OK: {projectPath}\n" +
            $"source={source.Data.Length:N0} bytes; " +
            $"data={project.DataSection.Length:N0} bytes; " +
            $"objects={project.Objects.Count:N0}; " +
            $"sha256={project.Manifest.SourceSha256}");
    }

    private static void ShowModelInfo(string modelArgument)
    {
        string modelPath = ExistingFile(modelArgument);
        ImportedScene scene = ImportedModelReader.Read(modelPath);
        long vertices = scene.Meshes.Sum(mesh => (long)mesh.Positions.Length);
        long indices = scene.Meshes.Sum(mesh => (long)mesh.TriangleIndices.Length);
        long vertexAlpha = scene.Meshes.Sum(mesh =>
            (long)mesh.DiffuseColors.Count(color => (color >> 24) != byte.MaxValue));
        uint maximumIndex = scene.Meshes
            .SelectMany(mesh => mesh.TriangleIndices)
            .DefaultIfEmpty()
            .Max();
        Vector3 minimum = new(float.PositiveInfinity);
        Vector3 maximum = new(float.NegativeInfinity);
        foreach (Vector3 position in scene.Meshes.SelectMany(mesh => mesh.Positions))
        {
            minimum = Vector3.Min(minimum, position);
            maximum = Vector3.Max(maximum, position);
        }
        Console.WriteLine(
            $"MODEL OK: {modelPath}\n" +
            $"format={Path.GetExtension(modelPath).TrimStart('.').ToUpperInvariant()}; " +
            $"meshes={scene.Meshes.Count:N0}; vertices={vertices:N0}; " +
            $"triangles={indices / 3:N0}; max-index={maximumIndex:N0}; " +
            $"requires-u32={maximumIndex > ushort.MaxValue}\n" +
            $"normals={scene.Meshes.Count(mesh => mesh.Normals.Length == mesh.Positions.Length):N0}/" +
            $"{scene.Meshes.Count:N0}; uv0={scene.Meshes.Count(mesh => mesh.TextureCoordinates.Length == mesh.Positions.Length):N0}/" +
            $"{scene.Meshes.Count:N0}; uv1={scene.Meshes.Count(mesh => mesh.SecondaryTextureCoordinates.Length == mesh.Positions.Length):N0}/" +
            $"{scene.Meshes.Count:N0}; colors={scene.Meshes.Count(mesh => mesh.DiffuseColors.Length == mesh.Positions.Length):N0}/" +
            $"{scene.Meshes.Count:N0}; vertex-alpha={vertexAlpha:N0}\n" +
            $"skinned={scene.Meshes.Count(mesh => mesh.Skinning is not null):N0}; " +
            $"materials={scene.Materials.Count:N0}; textures={scene.Textures.Count:N0}; " +
            $"texture-bytes={scene.Textures.Sum(texture => (long)texture.Data.Length):N0}\n" +
            $"bounds-min={FormatVector(minimum)}; bounds-max={FormatVector(maximum)}; " +
            $"size={FormatVector(maximum - minimum)}");
        foreach ((ImportedMaterial material, int index) in scene.Materials.Select(
                     (material, index) => (material, index)))
        {
            Console.WriteLine(
                $"MATERIAL {index}: {material.Name}; alpha={material.AlphaMode}; " +
                $"cutoff={FormatSingle(material.AlphaCutoff)}; " +
                $"texture={material.BaseColorTextureIndex}:{material.BaseColorTextureName ?? "-"}");
        }
        foreach ((ImportedTexture texture, int index) in scene.Textures.Select(
                     (texture, index) => (texture, index)))
        {
            Console.WriteLine(
                $"TEXTURE {index}: {texture.Name}; {texture.MimeType}; " +
                $"{texture.Width}x{texture.Height}; bytes={texture.Data.Length:N0}");
        }
        foreach ((ImportedMesh mesh, int index) in scene.Meshes.Select(
                     (mesh, index) => (mesh, index)))
        {
            Console.WriteLine(
                $"MESH {index}: {mesh.Name}; vertices={mesh.Positions.Length:N0}; " +
                $"triangles={mesh.TriangleIndices.Length / 3:N0}; " +
                $"material={mesh.MaterialIndex}; skin={mesh.Skinning?.Skeleton.Name ?? "-"}");
        }
        foreach (string warning in scene.ImportWarnings)
            Console.WriteLine($"WARNING: {warning}");
    }

    private static void ShowSceneMeshInfo(string smoArgument, string nameFilter)
    {
        string path = ExistingFile(smoArgument);
        SmoDocument document = SmoDocument.Load(path);
        SmoPreparedScene scene = SmoViewer.Scene.SmoSceneBuilder.Build(document);
        bool includeAll = nameFilter == "*";
        SmoSceneMesh[] matches = scene.Meshes
            .Where(mesh => mesh.SharedInstance is null &&
                (includeAll ||
                 document.Objects[mesh.Mesh.ObjectIndex].Name.Contains(
                     nameFilter,
                     StringComparison.OrdinalIgnoreCase)))
            .OrderBy(mesh => mesh.Mesh.ObjectIndex)
            .ToArray();
        Console.WriteLine(
            $"SCENE MESH INFO: {path}; filter={nameFilter}; matches={matches.Length}");
        foreach (SmoSceneMesh mesh in matches)
        {
            SmoObjectEntry entry = document.Objects[mesh.Mesh.ObjectIndex];
            SmoObjectEntry? model = entry.ParentIndex is int parentIndex
                ? document.Objects[parentIndex]
                : null;
            SmoObjectEntry[] materials = model is null
                ? []
                : document.Objects.Where(candidate =>
                    candidate.ParentIndex == model.Index &&
                    candidate.TypeHash == SmoClassIds.MaterialData).ToArray();
            Console.WriteLine(
                $"MESH index={entry.Index}; id={entry.Id}; name={entry.Name}; " +
                $"model={model?.Index}:{model?.Name}; texture={mesh.Texture?.ObjectIndex}; " +
                $"position=({mesh.WorldTransform.M41:G9}," +
                $"{mesh.WorldTransform.M42:G9},{mesh.WorldTransform.M43:G9}); " +
                $"usesAlpha={mesh.UsesAlphaBlend}; order={mesh.RequiresTransparentOrdering}; " +
                $"mode={mesh.MaterialRenderState?.BlendMode}; " +
                $"final={mesh.MaterialRenderState?.FinalBlendOperation}");
            foreach (SmoObjectEntry material in materials)
            {
                bool decoded = SmoMaterialRenderState.TryDecode(
                    document,
                    material,
                    out SmoMaterialRenderStateInfo? state);
                Console.WriteLine(
                    $"  MATERIAL index={material.Index}; id={material.Id}; " +
                    $"name={material.Name}; decoded={decoded}; " +
                    $"mode={state?.BlendMode}; final={state?.FinalBlendOperation}; " +
                    $"states={string.Join(',', state?.MaterialRenderStates ?? [])}");
            }
        }
    }

    private static void Build(string projectArgument, string outputArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        string outputPath = OutputFile(outputArgument, ".smo");
        SmoProject project = SmoProjectArchive.Load(projectPath);
        SmoProjectBuildResult result =
            SmoProjectSerializer.Build(project, outputPath);
        Console.WriteLine(FormatResult(result));
    }

    private static void RoundTrip(
        string sourceArgument,
        string projectArgument,
        string outputArgument)
    {
        string sourcePath = ExistingFile(sourceArgument);
        string projectPath = OutputFile(projectArgument, ".smolvlproj");
        string outputPath = OutputFile(outputArgument, ".smo");
        if (string.Equals(sourcePath, outputPath, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(
                "Project round-trip refuses to overwrite its source SMO.");
        }

        SmoDocument source = SmoDocument.Load(sourcePath);
        SmoProjectArchive.Save(
            SmoProject.Import(source),
            projectPath);
        SmoProject reopened =
            SmoProjectArchive.Load(projectPath);
        SmoProjectBuildResult result =
            SmoProjectSerializer.Build(reopened, outputPath);
        Console.WriteLine(
            $"ROUNDTRIP OK: {sourcePath}\n" +
            $"project={projectPath}\n" +
            FormatResult(result));
        if (!result.IsByteIdenticalToImportedSource)
        {
            throw new InvalidDataException(
                "The zero-edit project round-trip is structurally valid but not " +
                "byte-identical to its imported source.");
        }
    }

    private static void ListPlacements(string projectArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        SmoProjectObject[] placements = project.Objects
            .Where(item => item.TypeHash == SmoClassIds.StaticRenderObject)
            .ToArray();
        foreach (SmoProjectObject placement in placements)
        {
            Console.WriteLine(
                $"{placement.Index}\t0x{placement.Id:X8}\t{placement.DisplayName}");
        }
        Console.WriteLine($"PLACEMENTS: {placements.Length:N0}");
    }

    private static void ShowPlacementTransform(
        string projectArgument,
        int objectIndex)
    {
        string projectPath = ExistingFile(projectArgument);
        SmoProject project = SmoProjectArchive.Load(projectPath);
        SmoProjectObject placement = project.Objects[objectIndex];
        if (placement.TypeHash != SmoClassIds.StaticRenderObject)
            throw new ArgumentException($"Project object {objectIndex} is not a static placement.");
        Matrix4x4 world = DecodeMatrix(project.GetPropertyBytes(
            objectIndex,
            SmoPropertyKeys.WorldMatrix,
            SmoPropertyValueKind.Matrix4x4));
        if (!Matrix4x4.Decompose(
                world,
                out Vector3 scale,
                out Quaternion rotation,
                out Vector3 position))
        {
            throw new InvalidDataException(
                $"Placement {objectIndex} has a non-decomposable world matrix.");
        }
        Vector3 rotationDegrees = SmoEulerAngles.ToDegrees(rotation);
        Console.WriteLine(
            $"PLACEMENT TRANSFORM: object={objectIndex}; id=0x{placement.Id:X8}; " +
            $"name={placement.DisplayName}\n" +
            $"position=({FormatSingle(position.X)}, {FormatSingle(position.Y)}, " +
            $"{FormatSingle(position.Z)})\n" +
            $"rotation-degrees=({FormatSingle(rotationDegrees.X)}, " +
            $"{FormatSingle(rotationDegrees.Y)}, {FormatSingle(rotationDegrees.Z)})\n" +
            $"scale=({FormatSingle(scale.X)}, {FormatSingle(scale.Y)}, " +
            $"{FormatSingle(scale.Z)})");
    }

    private static void ListMissingTransforms(string projectArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        SmoProjectObject[] candidates = project.Objects
            .Where(item =>
                item.TypeHash is SmoClassIds.Node or
                    SmoClassIds.RenderNode or SmoClassIds.Model &&
                item.Fields.Any(field =>
                    field.FieldType == 0 && field.Occurrence == 0 &&
                    field.PayloadSize == 12) &&
                (!item.Fields.Any(field =>
                     field.FieldType == 1 && field.Occurrence == 0 &&
                     field.PayloadSize == 16) ||
                 !item.Fields.Any(field =>
                     field.FieldType == 2 && field.Occurrence == 0 &&
                     field.PayloadSize == 12)))
            .ToArray();
        foreach (SmoProjectObject item in candidates)
        {
            bool rotation = item.Fields.Any(field =>
                field.FieldType == 1 && field.Occurrence == 0 &&
                field.PayloadSize == 16);
            bool scale = item.Fields.Any(field =>
                field.FieldType == 2 && field.Occurrence == 0 &&
                field.PayloadSize == 12);
            Console.WriteLine(
                $"{item.Index}\t0x{item.Id:X8}\t{item.DisplayName}\t" +
                $"class={FormatClassName(item.TypeHash)};" +
                $"rotation={(rotation ? "present" : "missing")};" +
                $"scale={(scale ? "present" : "missing")}");
        }
        Console.WriteLine($"MISSING TRANSFORMS: {candidates.Length:N0}");
    }

    private static void ListTransformObjects(string projectArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        SmoProject project = SmoProjectArchive.Load(projectPath);
        SmoProjectObject[] candidates = project.Objects
            .Where(item =>
                item.TypeHash is SmoClassIds.Node or
                    SmoClassIds.RenderNode or SmoClassIds.Model &&
                item.Fields.Any(field =>
                    field.FieldType == 0 && field.Occurrence == 0 &&
                    field.PayloadSize == 12))
            .ToArray();
        foreach (SmoProjectObject item in candidates)
        {
            Console.WriteLine(
                $"{item.Index}\t0x{item.Id:X8}\t{item.DisplayName}\t" +
                $"class={FormatClassName(item.TypeHash)}");
        }
        Console.WriteLine($"TRANSFORM OBJECTS: {candidates.Length:N0}");
    }

    private static void ListReferenceTemplates(string projectArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        SmoProjectObject[] templates = project.Objects
            .Where(item => project.CanAddReferencePlacement(item.Index, out _))
            .ToArray();
        foreach (SmoProjectObject template in templates)
        {
            Console.WriteLine(
                $"{template.Index}\t0x{template.Id:X8}\t{template.DisplayName}");
        }
        Console.WriteLine($"REFERENCE TEMPLATES: {templates.Length:N0}");
    }

    private static void ListNodeEntities(string sourceArgument)
    {
        string sourcePath = ExistingFile(sourceArgument);
        var level = new SmoLevelDocument(SmoLevelWorkspace.Load(sourcePath));
        int count = 0;
        foreach (SmoLevelEntity entity in level.Entities
                     .Where(item => item.Kind == SmoLevelEntityKind.Visual)
                     .OrderBy(item => item.Id.SceneObjectIndex))
        {
            int? staticOwner = SmoPlacementTransformWriter.FindStaticPlacementOwnerIndex(
                level.Workspace.Document,
                entity.Id.SceneObjectIndex);
            int? nodeOwner = SmoPlacementTransformWriter.FindNodeTransformOwnerIndex(
                level.Workspace.Document,
                entity.Id.SceneObjectIndex);
            if (staticOwner is not null || nodeOwner is not int ownerIndex)
                continue;
            SmoObjectEntry owner = level.Workspace.Document.Objects[ownerIndex];
            string className = FormatClassName(owner.TypeHash);
            Matrix4x4.Decompose(
                entity.WorldTransform,
                out Vector3 scale,
                out Quaternion rotation,
                out Vector3 position);
            Vector3 rotationDegrees = SmoEulerAngles.ToDegrees(rotation);
            Console.WriteLine(
                $"{entity.Id.SceneObjectIndex}\t{entity.Name}\towner={ownerIndex}\t" +
                $"class={className}\tposition=({FormatVector(position)})\t" +
                $"rotation=({FormatVector(rotationDegrees)})\t" +
                $"scale=({FormatVector(scale)})");
            count++;
        }
        Console.WriteLine($"NODE ENTITIES: {count:N0}");
    }

    private static void CloneReferencePlacement(
        string projectArgument,
        int templateObjectIndex,
        Vector3 delta,
        string displayName,
        string outputArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        string outputPath = OutputFile(outputArgument, ".smolvlproj");
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        uint newId = project.AddReferencePlacementTranslated(
            templateObjectIndex,
            delta,
            displayName);
        SmoProjectArchive.Save(project, outputPath);
        Console.WriteLine(
            $"PROJECT REFERENCE PLACEMENT OK: {outputPath}\n" +
            $"template={templateObjectIndex}; new-root=0x{newId:X8}; " +
            $"delta=({FormatSingle(delta.X)}, {FormatSingle(delta.Y)}, " +
            $"{FormatSingle(delta.Z)}); " +
            $"reference-placements={project.Manifest.ReferencePlacements.Count:N0}; " +
            $"immutable-data={project.DataSection.Length:N0} bytes");
    }

    private static void Translate(
        string projectArgument,
        int objectIndex,
        Vector3 delta,
        string outputArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        string outputPath = OutputFile(outputArgument, ".smolvlproj");
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        project.TranslateStaticPlacement(objectIndex, delta);
        SmoProjectArchive.Save(project, outputPath);
        Console.WriteLine(
            $"PROJECT EDIT OK: {outputPath}\n" +
            $"object={objectIndex}; delta=({FormatSingle(delta.X)}, " +
            $"{FormatSingle(delta.Y)}, {FormatSingle(delta.Z)}); " +
            $"property-edits={project.Manifest.PropertyEdits.Count:N0}");
    }

    private static void SetPlacementTrs(
        string projectArgument,
        int objectIndex,
        Vector3 position,
        Vector3 rotationDegrees,
        Vector3 scale,
        string outputArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        string outputPath = OutputFile(outputArgument, ".smolvlproj");
        SmoProject project = SmoProjectArchive.Load(projectPath);
        SmoProjectObject placement = project.Objects[objectIndex];
        Matrix4x4 world =
            Matrix4x4.CreateScale(scale) *
            Matrix4x4.CreateFromQuaternion(
                SmoEulerAngles.FromDegrees(rotationDegrees)) *
            Matrix4x4.CreateTranslation(position);
        project.SetPlacementTransform(placement.Id, world);
        SmoProjectArchive.Save(project, outputPath);
        Console.WriteLine(
            $"PROJECT PLACEMENT TRS OK: {outputPath}\n" +
            $"object={objectIndex}; id=0x{placement.Id:X8}; " +
            $"position=({FormatSingle(position.X)}, {FormatSingle(position.Y)}, " +
            $"{FormatSingle(position.Z)}); rotation-degrees=(" +
            $"{FormatSingle(rotationDegrees.X)}, {FormatSingle(rotationDegrees.Y)}, " +
            $"{FormatSingle(rotationDegrees.Z)}); scale=({FormatSingle(scale.X)}, " +
            $"{FormatSingle(scale.Y)}, {FormatSingle(scale.Z)}); " +
            $"property-edits={project.Manifest.PropertyEdits.Count:N0}");
    }

    private static void SetEntityTrs(
        string sourceArgument,
        string entityName,
        Vector3 position,
        Vector3 rotationDegrees,
        Vector3 scale,
        string outputArgument)
    {
        string sourcePath = ExistingFile(sourceArgument);
        string outputPath = OutputFile(outputArgument, ".smo");
        var level = new SmoLevelDocument(SmoLevelWorkspace.Load(sourcePath));
        SmoLevelEntity[] matches = int.TryParse(
                entityName,
                NumberStyles.None,
                CultureInfo.InvariantCulture,
                out int entityIndex)
            ? level.Entities
                .Where(item => item.Id.SceneObjectIndex == entityIndex)
                .ToArray()
            : level.Entities
                .Where(item => item.Name.Equals(
                    entityName,
                    StringComparison.OrdinalIgnoreCase))
                .ToArray();
        if (matches.Length != 1)
        {
            throw new ArgumentException(
                $"Entity name '{entityName}' resolved to {matches.Length} objects; " +
                "an exact unique name is required.");
        }
        SmoLevelEntity entity = matches[0];
        Matrix4x4 world =
            Matrix4x4.CreateScale(scale) *
            Matrix4x4.CreateFromQuaternion(
                SmoEulerAngles.FromDegrees(rotationDegrees)) *
            Matrix4x4.CreateTranslation(position);
        if (!level.SetEntityTransform(entity.Id, world, "ProjectTool entity TRS"))
            throw new InvalidOperationException("The requested entity transform is unchanged.");
        SmoLevelSaveResult result = SmoLevelSaveService.Save(level, outputPath);
        var reopened = new SmoLevelDocument(SmoLevelWorkspace.Load(outputPath));
        SmoLevelEntity verified = reopened.Entities.Single(item =>
            item.Id.SceneObjectIndex == entity.Id.SceneObjectIndex);
        Console.WriteLine(
            $"ENTITY TRS OK: {outputPath}\n" +
            $"entity={entity.Id.SceneObjectIndex}; name={entity.Name}; " +
            $"position=({FormatVector(position)}); " +
            $"rotation-degrees=({FormatVector(rotationDegrees)}); " +
            $"scale=({FormatVector(scale)}); " +
            $"saved-position=({FormatVector(verified.WorldTransform.Translation)}); " +
            $"patched-transforms={result.PatchedTransformCount:N0}; " +
            $"bytes={result.FileSize:N0}");
    }

    private static void RemoveBranch(
        string projectArgument,
        int objectIndex,
        string outputArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        string outputPath = OutputFile(outputArgument, ".smolvlproj");
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        project.RemoveInlineBranch(objectIndex);
        SmoProjectArchive.Save(project, outputPath);
        Console.WriteLine(
            $"PROJECT STRUCTURE EDIT OK: {outputPath}\n" +
            $"removed-root={objectIndex}; " +
            $"branch-removals={project.Manifest.BranchRemovals.Count:N0}; " +
            $"immutable-data={project.DataSection.Length:N0} bytes");
    }

    private static void RemoveBranchPreservingSharedResources(
        string projectArgument,
        int objectIndex,
        string outputArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        string outputPath = OutputFile(outputArgument, ".smolvlproj");
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        IReadOnlyList<uint> relocated =
            project.RelocateSharedLeavesForBranch(objectIndex);
        project.RemoveInlineBranch(objectIndex);
        SmoProjectArchive.Save(project, outputPath);
        Console.WriteLine(
            $"PROJECT STRUCTURE EDIT OK: {outputPath}\n" +
            $"removed-root={objectIndex}; relocated-resources=" +
            $"{string.Join(',', relocated)}; " +
            $"branch-removals={project.Manifest.BranchRemovals.Count:N0}; " +
            $"resource-relocations={project.Manifest.ResourceRelocations.Count:N0}; " +
            $"immutable-data={project.DataSection.Length:N0} bytes");
    }

    private static void RemovePlacement(
        string projectArgument,
        uint objectId,
        string outputArgument)
    {
        string projectPath = ExistingFile(projectArgument);
        string outputPath = OutputFile(outputArgument, ".smolvlproj");
        SmoProject project = SmoProjectArchive.Load(projectPath);
        project.RemovePlacement(objectId);
        SmoProjectArchive.Save(project, outputPath);
        Console.WriteLine(
            $"PROJECT PLACEMENT REMOVAL OK: {outputPath}\n" +
            $"removed-root=0x{objectId:X8}; " +
            $"reference-placements={project.Manifest.ReferencePlacements.Count:N0}; " +
            $"branch-removals={project.Manifest.BranchRemovals.Count:N0}; " +
            $"immutable-data={project.DataSection.Length:N0} bytes");
    }

    private static void SetVector(
        string projectArgument,
        int objectIndex,
        string propertyKey,
        Vector3 value,
        string outputArgument)
    {
        SavePropertyEdit(
            projectArgument,
            outputArgument,
            objectIndex,
            propertyKey,
            project => project.SetProperty(objectIndex, propertyKey, value));
    }

    private static void SetQuaternion(
        string projectArgument,
        int objectIndex,
        string propertyKey,
        Quaternion value,
        string outputArgument)
    {
        SavePropertyEdit(
            projectArgument,
            outputArgument,
            objectIndex,
            propertyKey,
            project => project.SetProperty(objectIndex, propertyKey, value));
    }

    private static void SavePropertyEdit(
        string projectArgument,
        string outputArgument,
        int objectIndex,
        string propertyKey,
        Action<SmoProject> apply)
    {
        string projectPath = ExistingFile(projectArgument);
        string outputPath = OutputFile(outputArgument, ".smolvlproj");
        SmoProject project =
            SmoProjectArchive.Load(projectPath);
        apply(project);
        SmoProjectArchive.Save(project, outputPath);
        Console.WriteLine(
            $"PROJECT EDIT OK: {outputPath}\n" +
            $"object={objectIndex}; property={propertyKey}; " +
            $"property-edits={project.Manifest.PropertyEdits.Count:N0}");
    }

    private static string FormatResult(SmoProjectBuildResult result) =>
        $"output={result.OutputPath}\n" +
        $"bytes={result.FileSize:N0}; objects={result.ObjectCount:N0}; " +
        $"sha256={result.Sha256}; " +
        $"byte-identical={result.IsByteIdenticalToImportedSource}";

    private static string ExistingFile(string argument)
    {
        string path = Path.GetFullPath(argument);
        if (!File.Exists(path))
            throw new FileNotFoundException("Input file does not exist.", path);
        return path;
    }

    private static string OutputFile(string argument, string expectedExtension)
    {
        string path = Path.GetFullPath(argument);
        if (!Path.GetExtension(path).Equals(
                expectedExtension,
                StringComparison.OrdinalIgnoreCase))
        {
            throw new ArgumentException(
                $"Output file must use the {expectedExtension} extension.");
        }
        string? directory = Path.GetDirectoryName(path);
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
            throw new DirectoryNotFoundException(directory);
        return path;
    }

    private static bool Is(string value, string expected) =>
        value.Equals(expected, StringComparison.OrdinalIgnoreCase);

    private static int ParseObjectIndex(string value)
    {
        if (!int.TryParse(
                value,
                NumberStyles.None,
                CultureInfo.InvariantCulture,
                out int result) ||
            result < 0)
        {
            throw new ArgumentException($"Invalid non-negative object index: {value}");
        }
        return result;
    }

    private static uint ParseObjectId(string value)
    {
        string digits = value.StartsWith("0x", StringComparison.OrdinalIgnoreCase)
            ? value[2..]
            : value;
        NumberStyles style = value.StartsWith("0x", StringComparison.OrdinalIgnoreCase)
            ? NumberStyles.AllowHexSpecifier
            : NumberStyles.None;
        if (!uint.TryParse(digits, style, CultureInfo.InvariantCulture, out uint result))
            throw new ArgumentException($"Invalid object ID: {value}");
        return result;
    }

    private static float ParseSingle(string value)
    {
        if (!float.TryParse(
                value,
                NumberStyles.Float,
                CultureInfo.InvariantCulture,
                out float result) ||
            !float.IsFinite(result))
        {
            throw new ArgumentException($"Invalid finite number: {value}");
        }
        return result;
    }

    private static string FormatSingle(float value) =>
        value.ToString("R", CultureInfo.InvariantCulture);

    private static string FormatVector(Vector3 value) =>
        $"{FormatSingle(value.X)}, {FormatSingle(value.Y)}, {FormatSingle(value.Z)}";

    private static string FormatClassName(uint typeHash) =>
        SmoClassRegistry.TryGetName(typeHash, out string? className)
            ? className!
            : $"0x{typeHash:X8}";

    private static Matrix4x4 DecodeMatrix(ReadOnlySpan<byte> data)
    {
        if (data.Length != 64)
            throw new InvalidDataException("A placement matrix must contain 64 bytes.");
        Span<float> cells = stackalloc float[16];
        for (int index = 0; index < cells.Length; index++)
        {
            cells[index] = BitConverter.Int32BitsToSingle(
                BinaryPrimitives.ReadInt32LittleEndian(data[(index * 4)..]));
        }
        return new Matrix4x4(
            cells[0], cells[1], cells[2], cells[3],
            cells[4], cells[5], cells[6], cells[7],
            cells[8], cells[9], cells[10], cells[11],
            cells[12], cells[13], cells[14], cells[15]);
    }

    private static void PrintUsage()
    {
        Console.WriteLine(
            "SmoLVLcreator.ProjectTool\n\n" +
            "  model-info <model.obj|model.glb|model.fbx>\n" +
            "  scene-mesh-info <file.smo> <name-substring|*>\n" +
            "  import    <source.smo> <project.smolvlproj>\n" +
            "  build     <project.smolvlproj> <output.smo>\n" +
            "  roundtrip <source.smo> <project.smolvlproj> <output.smo>\n" +
            "  placements <project.smolvlproj>\n" +
            "  placement-transform <project.smolvlproj> <object-index>\n" +
            "  missing-transforms <project.smolvlproj>\n" +
            "  transform-objects <project.smolvlproj>\n" +
            "  reference-templates <project.smolvlproj>\n" +
            "  node-entities <source.smo>\n" +
            "  clone-reference <project.smolvlproj> <template-index> <dx> <dy> <dz> " +
            "<name> <output.smolvlproj>\n" +
            "  remove-branch <project.smolvlproj> <object-index> <output.smolvlproj>\n" +
            "  remove-branch-preserve <project.smolvlproj> <object-index> " +
            "<output.smolvlproj>\n" +
            "  remove-placement <project.smolvlproj> <object-id> " +
            "<output.smolvlproj>\n" +
            "  translate <project.smolvlproj> <object-index> <dx> <dy> <dz> " +
            "<output.smolvlproj>\n" +
            "  set-placement-trs <project> <object-index> <px> <py> <pz> " +
            "<rx-deg> <ry-deg> <rz-deg> <sx> <sy> <sz> <output.smolvlproj>\n" +
            "  set-entity-trs <source.smo> <entity-name-or-index> <px> <py> <pz> " +
            "<rx-deg> <ry-deg> <rz-deg> <sx> <sy> <sz> <output.smo>\n" +
            "  set-vector <project> <object-index> <property> <x> <y> <z> " +
            "<output.smolvlproj>\n" +
            "  set-quaternion <project> <object-index> <property> <x> <y> <z> <w> " +
            "<output.smolvlproj>\n\n" +
            "Рабочий saver SmoLVLcreator этими командами не изменяется.");
    }
}
