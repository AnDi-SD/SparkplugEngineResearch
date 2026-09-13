using System.Collections.ObjectModel;
using System.Numerics;
using SmoViewer.Core;

namespace SmoViewer.Scene;

/// <summary>
/// A renderable SMO occurrence prepared once for every viewer/editor host.
/// It preserves scene identity separately from the shared physical mesh.
/// </summary>
public sealed record SmoSceneMesh(
    SmoMesh Mesh,
    int SceneObjectIndex,
    SmoSharedMeshInstanceInfo? SharedInstance,
    SmoTexture? Texture,
    IReadOnlyList<SmoTexture>? AnimationFrames,
    TimeSpan? AnimationFrameDuration,
    SmoTexture? BaseTexture,
    uint? MaterialColorArgb,
    bool UsesAlphaBlend,
    SmoMaterialRenderStateInfo? MaterialRenderState,
    SmoAlphaDecalDepthInfo? AlphaDecalDepthInfo,
    SmoLargeAlphaNoDepthInfo? LargeAlphaNoDepthInfo,
    Matrix4x4 WorldTransform,
    int? SkinObjectIndex,
    IReadOnlyList<Matrix4x4>? InitialSkinMatrices,
    int? RigidNodeObjectIndex,
    IReadOnlyDictionary<int, float> BoneInfluences)
{
    public SmoRenderOccurrenceKey? OccurrenceKey { get; init; }
    public int? RenderableObjectIndex { get; init; }
    public SmoLoadedMaterial? LoadedMaterial { get; init; }
    public SmoMaterialDraw? MaterialDraw { get; init; }
    public SmoFogDraw? FogDraw { get; init; }
    public bool? SourceAlphaSort { get; init; }
    public uint SourcePriority { get; init; }
    public SmoAlphaSortData? AlphaSortData { get; init; }
    public SmoSkyBoxPose? SkyPose {get;init;}
    public Matrix4x4? AlphaSupportWorld { get; init; }
    public SmoRenderContainerKind ContainerKind { get; init; }
    public bool HasOriginalAlphaSphere {get;init;}
    public bool RequiresTransparentOrdering =>
        AlphaSortData?.RequiresQueue ?? SourceAlphaSort ?? ((MaterialRenderState?.RequiresTransparentOrdering ?? UsesAlphaBlend) ||
        SmoVertexColorUsage.HasAuthoredAlphaGradient(Mesh, MaterialRenderState) ||
        (!Mesh.HasSkinningData &&
         SmoVertexColorUsage.HasUniformPartialAlpha(Mesh) &&
         SmoVertexColorUsage.ShouldUseVertexAlphaInPreview(
             Mesh, MaterialRenderState)));
}

/// <summary>Shared decoded scene consumed by Viewer and level-editor hosts.</summary>
public sealed record SmoPreparedScene(
    SmoDocument Document,
    int TotalMeshCount,
    IReadOnlyList<SmoSceneMesh> Meshes,
    IReadOnlyList<string> DecodeErrors,
    IReadOnlyList<string> TextureIssues,
    SmoImportedFaceDiffuseInfo ImportedFaceDiffuseInfo,
    IReadOnlyDictionary<int, SmoSkin> Skins,
    SmoNodeHierarchy NodeHierarchy,
    IReadOnlyDictionary<int, Matrix4x4> BindWorldMatrices,
    IReadOnlyList<SmoSharedMeshInstanceInfo> SharedMeshInstances)
{
    public IReadOnlyList<SmoSceneText> Texts {get;init;}=[];
}

public sealed record SmoSceneText(SmoLoadedText Text,SmoRenderOccurrenceKey OccurrenceKey,
    Matrix4x4 WorldTransform,int? RigidNodeObjectIndex,SmoRenderContainerKind ContainerKind)
{
    public SmoSkyBoxPose? SkyPose {get;init;}
}

/// <summary>
/// Projects actual loaded render-support references into an asset inspection
/// scene shared by tool hosts. Membership is not native visibility or frame
/// draw order; unsupported renderables remain explicit diagnostics.
/// </summary>
public static class SmoSceneBuilder
{
    public static SmoPreparedScene Build(SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);

        var loaded = SmoLoadedResources.Get(document);
        var catalog = SmoRenderableCatalog.Get(document);
        var bindings = SmoTextureBindingResolver.ResolveByRenderable(document);
        var nodeHierarchy = SmoNodeHierarchy.Decode(document);
        var skins = catalog.ByObjectIndex.Values.Where(value => value.Skin is not null)
            .ToDictionary(value => value.ObjectIndex, value => value.Skin!);
        var decodeErrors = catalog.Issues.ToList();
        var textureIssues = new List<string>();
        textureIssues.AddRange(loaded.PreviewMaterialIssues);
        if (loaded.LoadIssue is not null) decodeErrors.Add(loaded.LoadIssue);
        if (loaded.SceneIssue is not null) decodeErrors.Add(loaded.SceneIssue);
        textureIssues.AddRange(loaded.CompatibilityIssues);
        decodeErrors.AddRange(loaded.Models.Values.Select(value => value.SkinIssue).OfType<string>());

        // Decode each actual referenced mesh once. A storage ancestor neither
        // selects its consumers nor supplies their material or Skin palette.
        var meshes = new Dictionary<int, SmoMesh>();
        var meshIds = loaded.Models.Values.Select(value => value.MeshObjectIndex)
            .OfType<int>().Distinct().ToArray();
        foreach (int index in meshIds)
        {
            if (SmoMeshDecoder.TryDecode(document, document.Objects[index], out var mesh, out string error)
                && mesh is not null) meshes.Add(index, mesh);
            else decodeErrors.Add(error);
        }
        var renderMeshes = new List<SmoSceneMesh>();
        var renderTexts = new List<SmoSceneText>();
        var sharedInstances = new List<SmoSharedMeshInstanceInfo>();
        var influences = new Dictionary<(int Mesh, int Skin), IReadOnlyDictionary<int, float>>();
        foreach (var occurrence in loaded.RenderOccurrences)
        {
            if (occurrence.Issue is not null)
            {
                decodeErrors.Add(occurrence.Issue);
                continue;
            }
            int modelIndex = occurrence.RenderableObjectIndex;
            if(loaded.Texts.TryGetValue(modelIndex,out var text))
            {
                if(text.GeometryIssue is not null)decodeErrors.Add(text.GeometryIssue);
                else if(text.Geometry is not null)renderTexts.Add(new(text,occurrence.Key,occurrence.InputWorld!.Value,
                    occurrence.RigidNodeObjectIndex,loaded.RenderContainersByObjectIndex[occurrence.Key.ContainerObjectIndex].Kind)
                    {SkyPose=loaded.RenderContainersByObjectIndex[occurrence.Key.ContainerObjectIndex].SkyPose});
                continue;
            }
            var model = loaded.Models[modelIndex];
            var modelEntry = document.Objects[modelIndex];
            if (model.MeshObjectIndex is not int meshIndex)
            {
                decodeErrors.Add($"MODEL_WITHOUT_MESH: [{modelIndex}] {modelEntry.Name}.");
                continue;
            }
            if (!meshes.TryGetValue(meshIndex, out var mesh)) continue;
            skins.TryGetValue(modelIndex, out var skin);
            if (skin is not null && model.SkinIssue is not null) continue;
            bindings.TryGetValue(modelIndex, out var binding);
            bool commonDraw = model.Material is not null && loaded.PreviewMaterialDraws.ContainsKey(model.Material.ObjectIndex);
            if (binding is not null) textureIssues.AddRange(binding.Diagnostics.Where(issue =>
                !commonDraw || !issue.StartsWith("MATERIAL_UV_FRONTEND_PENDING:", StringComparison.Ordinal)));
            if (binding?.Issue is string bindingIssue &&
                (!commonDraw || !bindingIssue.StartsWith("MATERIAL_FRONTEND_SHAPE:", StringComparison.Ordinal))) textureIssues.Add(bindingIssue);
            var texture = binding?.Issue is null && mesh.HasTextureCoordinates ? binding?.Texture : null;
            if (binding?.Texture is not null && !mesh.HasTextureCoordinates)
                textureIssues.Add($"UNSUPPORTED_VERTEX_UV_LAYOUT: Mesh [{meshIndex}] \"{mesh.Name}\" uses format 0x{mesh.VertexFormat:X} with serialized stride {mesh.Stride}.");
            var state = binding?.MaterialRenderState;
            if (state is not null && !commonDraw)
            {
                state = SmoMaterialRenderState.BindToRenderable(state, mesh, skin, texture);
                if (state.LoadIssueDiagnostic is string diagnostic)
                    textureIssues.Add($"MATERIAL_RENDER_STATE: Model [{modelIndex}]: {diagnostic} {state.Summary}");
                var granularity = SmoAlphaRunGranularityAnalyzer.Analyze(mesh, state, skin);
                if (granularity.Diagnostic is string alphaDiagnostic)
                    textureIssues.Add($"{alphaDiagnostic} Model [{modelIndex}], mesh [{meshIndex}].");
            }
            IReadOnlyDictionary<int, float> boneInfluences = new ReadOnlyDictionary<int, float>(new Dictionary<int, float>());
            if (skin is not null)
            {
                var key = (meshIndex, modelIndex);
                if (!influences.TryGetValue(key, out boneInfluences!))
                {
                    boneInfluences = ResolveBoneInfluences(mesh, skin);
                    influences.Add(key, boneInfluences);
                }
            }
            // Preserve the old, narrowly named StaticRenderObject metadata view
            // for callers which need it. It no longer selects scene occurrences.
            SmoSharedMeshInstanceInfo? shared = null;
            var container = loaded.RenderContainersByObjectIndex[occurrence.Key.ContainerObjectIndex];
            if (skin is null && container.Kind == SmoRenderContainerKind.StaticRenderObject
                && catalog.ByObjectIndex.TryGetValue(modelIndex, out var resource)
                && resource.Mesh.Encoding != SmoNodeRelationshipEncoding.InlineObject)
            {
                var owner = document.Objects[container.ObjectIndex];
                var meshEntry = document.Objects[meshIndex];
                shared = new(owner.Index, owner.Name, modelIndex, modelEntry.Name,
                    meshIndex, meshEntry.Id, meshEntry.Name, model.Material?.ObjectIndex, occurrence.InputWorld!.Value);
                sharedInstances.Add(shared);
            }
            renderMeshes.Add(new SmoSceneMesh(mesh, modelIndex, shared, texture,
                binding?.AnimationFrames, binding?.FrameDuration, binding?.BaseTexture,
                binding?.DiffuseArgb, state?.UsesAlphaBlend ?? binding?.UsesAlphaBlend == true,
                state, null, null, occurrence.InputWorld!.Value, skin?.ObjectIndex,
                model.InitialSkinPalette, occurrence.RigidNodeObjectIndex, boneInfluences)
            {
                OccurrenceKey = occurrence.Key,
                RenderableObjectIndex = modelIndex,
                LoadedMaterial = model.Material,
                FogDraw = model.FogDraw,
                MaterialDraw = model.Material is null ? null : loaded.PreviewMaterialDraws.GetValueOrDefault(model.Material.ObjectIndex),
                SourceAlphaSort = model.AlphaSort,
                SourcePriority = model.Priority,
                AlphaSortData = model.AlphaSortData,
                AlphaSupportWorld = container.World,
                ContainerKind = container.Kind,
                SkyPose=container.SkyPose
            });
        }
        foreach (int index in loaded.Models.Keys.Except(loaded.RenderOccurrences.Select(value => value.RenderableObjectIndex)))
            decodeErrors.Add($"UNATTACHED_RENDERABLE: [{index}] is loaded but has no render support membership.");

        var faceDiffuse = SmoImportedFaceDiffuseAnalyzer.Analyze(meshes.Values.ToArray());
        if (faceDiffuse.Diagnostic is string faceDiagnostic) textureIssues.Add(faceDiagnostic);
        ApplyAlphaOrderingDiagnostics(renderMeshes, textureIssues);
        return new SmoPreparedScene(document, meshIds.Length,
            renderMeshes.AsReadOnly(), decodeErrors.AsReadOnly(),
            Array.AsReadOnly(textureIssues.Distinct().ToArray()), faceDiffuse,
            new ReadOnlyDictionary<int, SmoSkin>(skins), nodeHierarchy,
            loaded.NodeWorlds, sharedInstances.AsReadOnly()){Texts=renderTexts.AsReadOnly()};
    }

    private static void ApplyAlphaOrderingDiagnostics(
        List<SmoSceneMesh> renderMeshes,
        ICollection<string> textureIssues)
    {
        SmoAlphaDecalOpaqueSurface[] opaqueSurfaces = renderMeshes
            .Where(renderMesh =>
                !renderMesh.UsesAlphaBlend &&
                renderMesh.Mesh.HasSkinningData &&
                renderMesh.Mesh.HasNormals &&
                renderMesh.Mesh.TriangleCount > 0)
            .Select(renderMesh => new SmoAlphaDecalOpaqueSurface(
                renderMesh.Mesh, renderMesh.WorldTransform))
            .ToArray();

        for (int index = 0; index < renderMeshes.Count; index++)
        {
            SmoSceneMesh renderMesh = renderMeshes[index];
            if (renderMesh.MaterialDraw is not null) continue;
            if (renderMesh.MaterialRenderState is not
                SmoMaterialRenderStateInfo renderState)
            {
                continue;
            }

            SmoAlphaDecalDepthInfo depthInfo = SmoAlphaDecalDepthAnalyzer.Analyze(
                renderMesh.Mesh,
                renderState,
                renderMesh.WorldTransform,
                opaqueSurfaces);
            SmoLargeAlphaNoDepthInfo largeAlphaInfo =
                SmoLargeAlphaNoDepthAnalyzer.Analyze(
                    renderMesh.Mesh,
                    renderState,
                    renderMesh.WorldTransform,
                    opaqueSurfaces);
            if (depthInfo.Diagnostic is string depthDiagnostic)
            {
                textureIssues.Add(
                    $"{depthDiagnostic} Mesh [{renderMesh.Mesh.ObjectIndex}] " +
                    $"\"{renderMesh.Mesh.Name}\".");
            }
            if (largeAlphaInfo.Diagnostic is string largeAlphaDiagnostic)
            {
                textureIssues.Add(
                    $"{largeAlphaDiagnostic} Mesh " +
                    $"[{renderMesh.Mesh.ObjectIndex}] " +
                    $"\"{renderMesh.Mesh.Name}\".");
            }
            if (depthInfo.HasNearCoplanarDepthRisk ||
                largeAlphaInfo.HasLargeSurfaceOrderingRisk)
            {
                renderMeshes[index] = renderMesh with
                {
                    AlphaDecalDepthInfo = depthInfo,
                    LargeAlphaNoDepthInfo = largeAlphaInfo
                };
            }
        }
    }

    private static IReadOnlyDictionary<int, float> ResolveBoneInfluences(
        SmoMesh mesh,
        SmoSkin skin)
    {
        var result = new Dictionary<int, float>();
        if (!mesh.HasSkinningData)
            return result;

        for (int vertex = 0; vertex < mesh.VertexCount; vertex++)
        {
            Vector4 weights = mesh.BlendWeights[vertex];
            SmoBlendIndices indices = mesh.BlendIndices[vertex];
            Add(indices.X, weights.X);
            Add(indices.Y, weights.Y);
            Add(indices.Z, weights.Z);
            Add(indices.W, weights.W);
        }
        return result;

        void Add(int paletteIndex, float weight)
        {
            if (weight <= 0.000001f ||
                (uint)paletteIndex >= (uint)skin.Bones.Count)
            {
                return;
            }
            int nodeIndex = skin.Bones[paletteIndex].NodeObjectIndex;
            result[nodeIndex] = Math.Max(
                result.GetValueOrDefault(nodeIndex), weight);
        }
    }

}
