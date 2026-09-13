using System.Collections.ObjectModel;

namespace SmoViewer.Core;

/// <summary>
/// Confirmed first-pass <c>FinalBlendOp</c> families used by the shipped PC
/// assets. The numeric operation remains available on
/// <see cref="SmoMaterialRenderStateInfo"/>; this enum describes the rendering
/// policy that the viewer can apply without treating every blended effect as
/// ordinary source alpha.
/// </summary>
public enum SmoMaterialBlendMode
{
    Unknown = 0,
    OpaqueFinalBlend0,
    OpaqueFinalBlend2,
    RigidTextureAlphaSurfaceFinalBlend2,
    PrincessTransparentSurfaceFinalBlend2,
    SkinnedTransparentSurfaceFinalBlend2,
    UnconfirmedTransparentSurfaceFinalBlend2Hybrid,
    EffectFinalBlend4,
    EffectFinalBlend5,
    FinalBlend6Companion2,
    FinalBlend6Companion4,
    NonStandardFinalBlend6
}

public enum SmoMaterialConsumerKind
{
    Unknown = 0,
    RigidOrEffect,
    SkinnedSurface,
    SkinnedEffect
}

public enum SmoVertexDiffuseProfile
{
    Unknown = 0,
    Missing,
    UniformOpaqueBlack,
    UniformOpaqueWhite,
    UniformOther,
    Mixed
}

public sealed record SmoMaterialRenderStateInfo(
    uint FinalBlendOperation,
    IReadOnlyList<uint> MaterialRenderStates,
    SmoMaterialBlendMode BlendMode,
    string? FormatDiagnostic,
    SmoMaterialConsumerKind ConsumerKind = SmoMaterialConsumerKind.Unknown,
    uint? AlphaSortEnable = null,
    uint? RenderPriority = null,
    SmoVertexDiffuseProfile VertexDiffuseProfile = SmoVertexDiffuseProfile.Unknown,
    SmoTextureUvAlphaCoverage TextureUvAlphaCoverage = default)
{
    private static readonly uint[] ConfirmedPrincessTransparentSurfaceTuple =
        [0, 0, 1, 0, 1, 0, 3, 0, 4, 0, 6];
    private static readonly uint[] ConfirmedSkinnedTransparentSurfaceTuple =
        [0, 0, 1, 0, 1, 1, 3, 0, 4, 0, 6];
    private static readonly uint[] ConfirmedSkinnedSurfaceTuple =
        [0, 0, 1, 2, 1, 1, 3, 0, 4, 0, 6];
    private static readonly uint[] ConfirmedRigidEffectTuple =
        [0, 0, 1, 0, 1, 0, 3, 0, 2, 0, 6];
    private static readonly uint[] ConfirmedSkinnedEffectTuple =
        [0, 0, 1, 2, 1, 1, 3, 0, 2, 0, 6];

    public uint? CompanionBlendState =>
        MaterialRenderStates.Count > 8 ? MaterialRenderStates[8] : null;

    public bool UsesAlphaBlend =>
        FinalBlendOperation is 0x4 or 0x5 or 0x6 ||
        BlendMode is
            SmoMaterialBlendMode.RigidTextureAlphaSurfaceFinalBlend2 or
            SmoMaterialBlendMode.PrincessTransparentSurfaceFinalBlend2 or
            SmoMaterialBlendMode.SkinnedTransparentSurfaceFinalBlend2 or
            SmoMaterialBlendMode.UnconfirmedTransparentSurfaceFinalBlend2Hybrid;

    public bool RequiresTransparentOrdering => UsesAlphaBlend;

    /// <summary>
    /// Exact consumer/material context observed on the tiara surface in the
    /// shipped <c>bloom_princess.smo</c>. FinalBlendOp 2 or texture alpha by
    /// itself is deliberately insufficient.
    /// </summary>
    public bool HasPrincessTransparentSurfaceContext =>
        FinalBlendOperation == 0x2 &&
        ConsumerKind == SmoMaterialConsumerKind.SkinnedSurface &&
        MaterialRenderStates.SequenceEqual(
            ConfirmedPrincessTransparentSurfaceTuple) &&
        AlphaSortEnable == 0 &&
        RenderPriority == 1 &&
        VertexDiffuseProfile ==
            SmoVertexDiffuseProfile.UniformOpaqueBlack;

    public bool HasRigidTextureAlphaSurfaceContext =>
        FinalBlendOperation == 0x2 &&
        ConsumerKind == SmoMaterialConsumerKind.RigidOrEffect &&
        CompanionBlendState == 2;

    public bool HasConfirmedRigidTextureAlphaSurfaceState =>
        HasRigidTextureAlphaSurfaceContext &&
        TextureUvAlphaCoverage.HasPartialAlpha;

    /// <summary>
    /// The exact princess surface context plus decoded texture alpha sampled
    /// through triangles in UV0. Both fully transparent and partial covered
    /// texels are required because the canonical tiara is a graduated
    /// transparent surface. This also prevents opaque sibling chunks which only
    /// touch antialiased atlas texels from inheriting the tiara blend mode.
    /// </summary>
    public bool HasConfirmedPrincessTransparentSurfaceState =>
        HasPrincessTransparentSurfaceContext &&
        TextureUvAlphaCoverage.HasTransparentAndPartialAlpha;

    /// <summary>
    /// Exact large skinned partial-alpha surface contract observed on shipped
    /// PC assets such as Minautor mesh 13. It deliberately remains separate
    /// from the princess tiara contract because both the material tuple and
    /// consuming spSkin AlphaSortEnable value differ.
    /// </summary>
    public bool HasSkinnedTransparentSurfaceContext =>
        FinalBlendOperation == 0x2 &&
        ConsumerKind == SmoMaterialConsumerKind.SkinnedSurface &&
        MaterialRenderStates.SequenceEqual(
            ConfirmedSkinnedTransparentSurfaceTuple) &&
        AlphaSortEnable == 1 &&
        RenderPriority == 1 &&
        VertexDiffuseProfile ==
            SmoVertexDiffuseProfile.UniformOpaqueBlack;

    public bool HasConfirmedSkinnedTransparentSurfaceState =>
        HasSkinnedTransparentSurfaceContext &&
        TextureUvAlphaCoverage.HasPartialAlpha;

    /// <summary>
    /// Generated files have existed with the princess RS[5]=0 tuple but the
    /// skinned-surface AlphaSortEnable=1 consumer. No pristine bound fixture
    /// confirms that hybrid. It still needs authored-alpha preview and
    /// transparent ordering, so the Viewer labels it instead of silently
    /// treating it as opaque.
    /// </summary>
    public bool HasUnconfirmedTransparentSurfaceHybridContext =>
        FinalBlendOperation == 0x2 &&
        ConsumerKind == SmoMaterialConsumerKind.SkinnedSurface &&
        MaterialRenderStates.SequenceEqual(
            ConfirmedPrincessTransparentSurfaceTuple) &&
        AlphaSortEnable == 1 &&
        RenderPriority == 1 &&
        VertexDiffuseProfile ==
            SmoVertexDiffuseProfile.UniformOpaqueBlack;

    public bool HasUnconfirmedTransparentSurfaceHybridState =>
        HasUnconfirmedTransparentSurfaceHybridContext &&
        TextureUvAlphaCoverage.HasPartialAlpha;

    public bool RequiresFinalBlend2UvAlphaAnalysis =>
        HasRigidTextureAlphaSurfaceContext ||
        HasPrincessTransparentSurfaceContext ||
        HasSkinnedTransparentSurfaceContext ||
        HasUnconfirmedTransparentSurfaceHybridContext;

    /// <summary>
    /// The observed FinalBlendOp 2 transparent profile with RS[5]=0. The
    /// Viewer uses this only to identify possible large-surface ordering risk;
    /// it does not claim that RS[5] has been semantically decoded as a native
    /// depth-write switch.
    /// </summary>
    public bool HasRs5ZeroTransparentSurfaceProfile =>
        HasConfirmedPrincessTransparentSurfaceState ||
        HasUnconfirmedTransparentSurfaceHybridState;

    /// <summary>
    /// The two confirmed FinalBlendOp 6 tuples are consumer-specific and are
    /// compared in full. RS[8] alone is insufficient: the shipped skinned
    /// surface fixtures also use RS[3]=2. This does not claim that the OpenGL
    /// preview reproduces the native renderer's exact blend equation.
    /// </summary>
    public bool HasConfirmedConsumerTuple =>
        FinalBlendOperation == 0x6 &&
        ConsumerKind switch
        {
            SmoMaterialConsumerKind.SkinnedSurface =>
                MaterialRenderStates.SequenceEqual(ConfirmedSkinnedSurfaceTuple),
            SmoMaterialConsumerKind.RigidOrEffect =>
                // Both tuples occur on confirmed rigid level effects. The
                // second form is used by the Alfea03 book glow/spark meshes;
                // RS[3]/RS[5] therefore do not identify skinning by themselves.
                MaterialRenderStates.SequenceEqual(ConfirmedRigidEffectTuple) ||
                MaterialRenderStates.SequenceEqual(ConfirmedSkinnedEffectTuple),
            SmoMaterialConsumerKind.SkinnedEffect =>
                MatchesConfirmedSkinnedEffectTuple(MaterialRenderStates),
            _ => false
        };

    public bool HasMaterialTupleDivergence =>
        FinalBlendOperation == 0x6 &&
        ConsumerKind != SmoMaterialConsumerKind.Unknown &&
        !HasConfirmedConsumerTuple;

    public bool HasAlphaSortStateDivergence =>
        FinalBlendOperation == 0x6 &&
        ConsumerKind == SmoMaterialConsumerKind.SkinnedSurface &&
        AlphaSortEnable.HasValue &&
        AlphaSortEnable.Value != 1;

    public bool IsAlphaSortStateMissing =>
        FinalBlendOperation == 0x6 &&
        ConsumerKind == SmoMaterialConsumerKind.SkinnedSurface &&
        !AlphaSortEnable.HasValue;

    public bool HasPriorityDivergenceUnconfirmed =>
        FinalBlendOperation == 0x6 &&
        ConsumerKind == SmoMaterialConsumerKind.SkinnedSurface &&
        RenderPriority.HasValue &&
        RenderPriority.Value != 1;

    public bool HasVertexDiffuseDivergenceUnconfirmed =>
        FinalBlendOperation == 0x6 &&
        ConsumerKind == SmoMaterialConsumerKind.SkinnedSurface &&
        VertexDiffuseProfile == SmoVertexDiffuseProfile.UniformOpaqueWhite;

    public bool HasConsumerStateMismatch =>
        HasMaterialTupleDivergence || HasAlphaSortStateDivergence;

    public bool HasConfirmedRenderableState =>
        HasConfirmedPrincessTransparentSurfaceState ||
        HasConfirmedSkinnedTransparentSurfaceState ||
        (HasConfirmedConsumerTuple &&
         (ConsumerKind != SmoMaterialConsumerKind.SkinnedSurface ||
          AlphaSortEnable == 1));

    public bool UsesEmissiveApproximation =>
        (BlendMode is SmoMaterialBlendMode.EffectFinalBlend4 or
            SmoMaterialBlendMode.EffectFinalBlend5) ||
        (ConsumerKind == SmoMaterialConsumerKind.RigidOrEffect &&
         BlendMode == SmoMaterialBlendMode.FinalBlend6Companion2) ||
        HasConsumerStateMismatch;

    public bool UsesLuminanceCoverageApproximation =>
        BlendMode == SmoMaterialBlendMode.EffectFinalBlend4 ||
        HasConsumerStateMismatch;

    /// <summary>
    /// A known blend approximation is useful in material details, but is not a
    /// malformed-resource problem and therefore must not populate the Viewer's
    /// load-issue log by itself.
    /// </summary>
    public string? LoadIssueDiagnostic =>
        BlendMode is SmoMaterialBlendMode.EffectFinalBlend4 or
            SmoMaterialBlendMode.EffectFinalBlend5 &&
        !HasConsumerStateMismatch
            ? null
            : Diagnostic;

    public string? Diagnostic
    {
        get
        {
            List<string> diagnostics = [];
            if (FormatDiagnostic is not null)
                diagnostics.Add(FormatDiagnostic);
            if (HasMaterialTupleDivergence)
            {
                uint[] expected = ExpectedConsumerTuple!;
                diagnostics.Add(
                    "MATERIAL_CONSUMER_TUPLE_DIVERGENCE: FinalBlendOp 0x6 " +
                    $"for {ConsumerLabel} expects the exact observed corpus tuple " +
                    $"[{string.Join(", ", expected)}], but found " +
                    $"[{string.Join(", ", MaterialRenderStates)}]. " +
                    $"Differences: {DescribeTupleDifferences(expected)}. The OpenGL " +
                    "preview uses an effect-like warning approximation, not a " +
                    "native frame.");
            }
            if (HasUnconfirmedTransparentSurfaceHybridState)
            {
                diagnostics.Add(
                    "UNCONFIRMED_FINAL_BLEND_2_HYBRID: this skinned partial-alpha " +
                    "surface combines the princess RS[5]=0 material tuple with " +
                    "AlphaSortEnable=1. No pristine bound fixture confirms that " +
                    "combination. The OpenGL preview keeps authored alpha and " +
                    "transparent draw ordering so the surface is visible, but " +
                    "this is an approximation and an isolated Viewer frame " +
                    "cannot prove ordering against level geometry.");
            }
            if (HasAlphaSortStateDivergence)
            {
                diagnostics.Add(
                    "ALPHA_SORT_STATE_DIVERGENCE: the consuming spSkin has " +
                    $"AlphaSortEnable={AlphaSortEnable}; the pristine IceWorm and " +
                    "Yeti skinned-alpha consumers use 1. The OpenGL preview marks " +
                    "this as non-confirmed; native ordering is not reproduced.");
            }
            else if (IsAlphaSortStateMissing)
            {
                diagnostics.Add(
                    "ALPHA_SORT_STATE_UNAVAILABLE: the consuming spSkin did not " +
                    "yield a direct type-2 AlphaSortEnable DWORD, so this " +
                    "skinned-alpha renderable cannot be confirmed.");
            }
            if (HasPriorityDivergenceUnconfirmed)
            {
                diagnostics.Add(
                    "RENDER_PRIORITY_DIVERGENCE_UNCONFIRMED: the consuming spSkin " +
                    $"has Priority={RenderPriority}; compared pristine alpha " +
                    "fixtures use 1. The native meaning and visual impact of " +
                    "this difference are not established.");
            }
            if (HasVertexDiffuseDivergenceUnconfirmed)
            {
                diagnostics.Add(
                    "VERTEX_DIFFUSE_PROFILE_DIVERGENCE_UNCONFIRMED: this skinned " +
                    "FinalBlendOp 0x6 surface has uniform opaque-white vertex " +
                    "diffuse, while the compared pristine IceWorm/Yeti alpha " +
                    "surfaces and Bloom character body fixtures use uniform " +
                    "opaque black. This is an observed divergence only; it is " +
                    "not established as a cause of native glow or transparency.");
            }
            return diagnostics.Count == 0
                ? null
                : string.Join(" ", diagnostics);
        }
    }

    public SmoMaterialRenderStateInfo ForConsumer(
        SmoMaterialConsumerKind consumerKind,
        uint? alphaSortEnable = null,
        uint? renderPriority = null,
        SmoVertexDiffuseProfile vertexDiffuseProfile =
            SmoVertexDiffuseProfile.Unknown,
        SmoTextureUvAlphaCoverage textureUvAlphaCoverage = default)
    {
        SmoMaterialBlendMode rawMode = FinalBlendOperation == 0x2
            ? SmoMaterialBlendMode.OpaqueFinalBlend2
            : BlendMode;
        SmoMaterialRenderStateInfo bound = this with
        {
            BlendMode = rawMode,
            ConsumerKind = consumerKind,
            AlphaSortEnable = alphaSortEnable,
            RenderPriority = renderPriority,
            VertexDiffuseProfile = vertexDiffuseProfile,
            TextureUvAlphaCoverage = textureUvAlphaCoverage
        };
        if (bound.HasConfirmedPrincessTransparentSurfaceState)
        {
            return bound with
            {
                BlendMode =
                    SmoMaterialBlendMode.PrincessTransparentSurfaceFinalBlend2
            };
        }
        if (bound.HasConfirmedRigidTextureAlphaSurfaceState)
        {
            return bound with
            {
                BlendMode =
                    SmoMaterialBlendMode.RigidTextureAlphaSurfaceFinalBlend2
            };
        }
        if (bound.HasConfirmedSkinnedTransparentSurfaceState)
        {
            return bound with
            {
                BlendMode =
                    SmoMaterialBlendMode.SkinnedTransparentSurfaceFinalBlend2
            };
        }
        return bound.HasUnconfirmedTransparentSurfaceHybridState
            ? bound with
            {
                BlendMode = SmoMaterialBlendMode
                    .UnconfirmedTransparentSurfaceFinalBlend2Hybrid
            }
            : bound;
    }

    public string Summary =>
        $"FinalBlendOp=0x{FinalBlendOperation:X}; mode={BlendMode}; " +
        $"consumer={ConsumerKind}; AlphaSortEnable={Format(AlphaSortEnable)}; " +
        $"Priority={Format(RenderPriority)}; vertexDiffuse={VertexDiffuseProfile}; " +
        $"uvTextureAlpha={TextureUvAlphaCoverage}; " +
        $"MaterialRenderStates=[{string.Join(", ", MaterialRenderStates)}]";

    private uint[]? ExpectedConsumerTuple => ConsumerKind switch
    {
        SmoMaterialConsumerKind.SkinnedSurface => ConfirmedSkinnedSurfaceTuple,
        SmoMaterialConsumerKind.RigidOrEffect =>
            MaterialRenderStates.SequenceEqual(ConfirmedSkinnedEffectTuple)
                ? ConfirmedSkinnedEffectTuple
                : ConfirmedRigidEffectTuple,
        SmoMaterialConsumerKind.SkinnedEffect => ConfirmedSkinnedEffectTuple,
        _ => null
    };

    private string ConsumerLabel => ConsumerKind switch
    {
        SmoMaterialConsumerKind.SkinnedSurface =>
            "a skinned surface mesh with normals",
        SmoMaterialConsumerKind.RigidOrEffect => "a rigid/effect consumer",
        SmoMaterialConsumerKind.SkinnedEffect => "a confirmed skinned effect",
        _ => "an unknown consumer"
    };

    public static bool MatchesConfirmedSkinnedEffectTuple(
        IReadOnlyList<uint> materialRenderStates)
    {
        ArgumentNullException.ThrowIfNull(materialRenderStates);
        return materialRenderStates.SequenceEqual(ConfirmedRigidEffectTuple) ||
               materialRenderStates.SequenceEqual(ConfirmedSkinnedEffectTuple);
    }

    private string DescribeTupleDifferences(IReadOnlyList<uint> expected)
    {
        List<string> differences = [];
        int count = Math.Max(expected.Count, MaterialRenderStates.Count);
        for (int index = 0; index < count; index++)
        {
            string observed = index < MaterialRenderStates.Count
                ? MaterialRenderStates[index].ToString()
                : "<missing>";
            string wanted = index < expected.Count
                ? expected[index].ToString()
                : "<none>";
            if (!string.Equals(observed, wanted, StringComparison.Ordinal))
                differences.Add($"RS[{index}]={observed} (expected {wanted})");
        }
        return differences.Count == 0
            ? "none"
            : string.Join(", ", differences);
    }

    private static string Format(uint? value) =>
        value?.ToString() ?? "<unavailable>";
}

/// <summary>
/// Decodes the first material pass and its companion 11-value render-state
/// array from <c>spMaterialData</c>.
/// </summary>
public static class SmoMaterialRenderState
{
    private const int MaterialRenderStateCount = 11;
    private const int CompanionBlendStateIndex = 8;

    public static IReadOnlyDictionary<int, uint> ResolveAll(SmoDocument document)
    {
        IReadOnlyDictionary<int, SmoMaterialRenderStateInfo> detailed =
            ResolveDetailed(document);
        return new ReadOnlyDictionary<int, uint>(
            detailed.ToDictionary(
                item => item.Key,
                item => item.Value.FinalBlendOperation));
    }

    public static IReadOnlyDictionary<int, SmoMaterialRenderStateInfo> ResolveDetailed(
        SmoDocument document)
    {
        ArgumentNullException.ThrowIfNull(document);
        var catalog = SmoRenderableCatalog.Get(document);
        var result = new Dictionary<int, SmoMaterialRenderStateInfo>();
        foreach (var mesh in document.Objects.Where(entry => entry.TypeHash == SmoClassIds.MeshData))
        {
            if (catalog.TryGetStoredMeshOwner(mesh, out var owner) &&
                owner.Renderable.Material?.TargetObjectIndex is int material &&
                TryDecode(document, document.Objects[material], out var state) && state is not null)
                result[mesh.Index] = state;
        }

        return new ReadOnlyDictionary<int, SmoMaterialRenderStateInfo>(result);
    }

    public static bool TryDecodeFlags(
        SmoDocument document,
        SmoObjectEntry material,
        out uint flags)
    {
        if (TryDecode(document, material, out SmoMaterialRenderStateInfo? state) &&
            state is not null)
        {
            flags = state.FinalBlendOperation;
            return true;
        }

        flags = 0;
        return false;
    }

    public static unsafe bool TryDecode(SmoDocument document, SmoObjectEntry material,
        out SmoMaterialRenderStateInfo? state)
    {
        state = null;
        if (!SmoMaterialInspection.TryRead(document, material, out var snapshot, out _) || snapshot!.Passes.Length == 0)
            return false;
        var info = snapshot.Info;
        state = Classify(snapshot.Passes[0].Blend, new ReadOnlySpan<uint>(info.States, MaterialRenderStateCount).ToArray());
        return true;
    }

    public static SmoMaterialRenderStateInfo Classify(
        uint finalBlendOperation,
        IReadOnlyList<uint> materialRenderStates)
    {
        ArgumentNullException.ThrowIfNull(materialRenderStates);
        uint? companion = materialRenderStates.Count > CompanionBlendStateIndex
            ? materialRenderStates[CompanionBlendStateIndex]
            : null;

        SmoMaterialBlendMode mode = finalBlendOperation switch
        {
            0x0 => SmoMaterialBlendMode.OpaqueFinalBlend0,
            0x2 => SmoMaterialBlendMode.OpaqueFinalBlend2,
            0x4 => SmoMaterialBlendMode.EffectFinalBlend4,
            0x5 => SmoMaterialBlendMode.EffectFinalBlend5,
            0x6 when companion == 2 =>
                SmoMaterialBlendMode.FinalBlend6Companion2,
            0x6 when companion == 4 =>
                SmoMaterialBlendMode.FinalBlend6Companion4,
            0x6 => SmoMaterialBlendMode.NonStandardFinalBlend6,
            _ => SmoMaterialBlendMode.Unknown
        };

        string? diagnostic = mode switch
        {
            SmoMaterialBlendMode.EffectFinalBlend4 =>
                "MATERIAL_EFFECT_BLEND_4: native effect/additive blend; " +
                $"companion MaterialRenderStates[8]={Format(companion)}; the OpenGL " +
                "preview uses additive luminance coverage, " +
                "not a native frame.",
            SmoMaterialBlendMode.EffectFinalBlend5 =>
                "MATERIAL_EFFECT_BLEND_5: distinct native projectile/effect blend; " +
                $"companion MaterialRenderStates[8]={Format(companion)}; the OpenGL " +
                "preview uses an additive approximation, not a native frame.",
            SmoMaterialBlendMode.NonStandardFinalBlend6 =>
                $"UNCONFIRMED_FINAL_BLEND_6_TUPLE: FinalBlendOp 0x6 has " +
                $"MaterialRenderStates[8]={Format(companion)}; confirmed corpus " +
                "tuples use companion value 4 for skinned surface meshes with " +
                "normals and 2 for rigid/effect consumers.",
            SmoMaterialBlendMode.Unknown =>
                $"UNKNOWN_FINAL_BLEND: unsupported FinalBlendOp " +
                $"0x{finalBlendOperation:X} with MaterialRenderStates[8]=" +
                $"{Format(companion)}.",
            _ when materialRenderStates.Count != MaterialRenderStateCount =>
                $"MATERIAL_RENDER_STATES_MISSING: decoded " +
                $"{materialRenderStates.Count} values; expected " +
                $"{MaterialRenderStateCount}.",
            _ => null
        };

        return new SmoMaterialRenderStateInfo(
            finalBlendOperation,
            materialRenderStates.ToArray(),
            mode,
            diagnostic);
    }

    public static bool UsesAlphaBlend(uint finalBlendOperation) =>
        finalBlendOperation is 0x4 or 0x5 or 0x6;

    /// <summary>
    /// Selects the evidence-backed consumer family used only for Viewer
    /// diagnostics. The corpus currently proves the state-4 FinalBlendOp 6
    /// tuple for skinned surface meshes that carry normals; other geometry is
    /// kept in the rigid/effect family rather than extrapolating that result.
    /// </summary>
    public static SmoMaterialConsumerKind ClassifyConsumer(
        SmoMesh mesh,
        SmoMaterialRenderStateInfo? renderState = null)
    {
        ArgumentNullException.ThrowIfNull(mesh);
        if (!mesh.HasSkinningData || !mesh.HasNormals)
            return SmoMaterialConsumerKind.RigidOrEffect;
        return renderState?.FinalBlendOperation == 0x6 &&
               SmoMaterialRenderStateInfo.MatchesConfirmedSkinnedEffectTuple(
                   renderState.MaterialRenderStates)
            ? SmoMaterialConsumerKind.SkinnedEffect
            : SmoMaterialConsumerKind.SkinnedSurface;
    }

    public static SmoVertexDiffuseProfile ClassifyVertexDiffuse(SmoMesh mesh)
    {
        ArgumentNullException.ThrowIfNull(mesh);
        if (!mesh.HasDiffuseColors)
            return SmoVertexDiffuseProfile.Missing;

        uint first = mesh.DiffuseColorsArgb[0];
        bool uniform = mesh.DiffuseColorsArgb
            .Skip(1)
            .All(color => color == first);
        if (!uniform)
            return SmoVertexDiffuseProfile.Mixed;
        return first switch
        {
            0xFF000000 => SmoVertexDiffuseProfile.UniformOpaqueBlack,
            0xFFFFFFFF => SmoVertexDiffuseProfile.UniformOpaqueWhite,
            _ => SmoVertexDiffuseProfile.UniformOther
        };
    }

    public static SmoMaterialRenderStateInfo BindToRenderable(
        SmoMaterialRenderStateInfo renderState,
        SmoMesh mesh,
        SmoSkin? consumingSkin,
        SmoTexture? texture = null)
    {
        ArgumentNullException.ThrowIfNull(renderState);
        ArgumentNullException.ThrowIfNull(mesh);
        SmoMaterialConsumerKind consumerKind =
            ClassifyConsumer(mesh, renderState);
        SmoVertexDiffuseProfile vertexDiffuse =
            ClassifyVertexDiffuse(mesh);
        SmoMaterialRenderStateInfo bound = renderState.ForConsumer(
            consumerKind,
            consumingSkin?.AlphaSortEnable,
            consumingSkin?.Priority,
            vertexDiffuse);
        if (texture is null || !bound.RequiresFinalBlend2UvAlphaAnalysis)
            return bound;

        return renderState.ForConsumer(
            consumerKind,
            consumingSkin?.AlphaSortEnable,
            consumingSkin?.Priority,
            vertexDiffuse,
            SmoTextureUvAlphaAnalyzer.Analyze(mesh, texture));
    }

    private static string Format(uint? value) =>
        value?.ToString() ?? "<missing>";
}
