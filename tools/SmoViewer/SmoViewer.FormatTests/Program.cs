using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
using System.Text;
using SmoViewer.Core;

namespace SmoViewer.FormatTests;

internal static class Program
{
    private static int _assertionCount;

    public static int Main(string[] args)
    {
        try
        {
            TestSyntheticDocument();
            TestDataBlockHeaders();
            TestGuiStateClassification();
            TestMaterialRenderStates();
            TestAlphaDecalOverlayLossSimulation();
            TestSyntheticTextures();
            TestVertexColorUsage();
            TestNativeTransparencyFixtures();

            CorpusOptions options = ParseCorpusOptions(args);
            string corpusPath = options.Path is not null
                ? Path.GetFullPath(options.Path)
                : Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "Samples"));

            if (Directory.Exists(corpusPath) || File.Exists(corpusPath))
                TestLocalCorpus(corpusPath, options.SampleCount, options.Seed);
            else
                Console.WriteLine($"Corpus not present; local sample checks skipped: {corpusPath}");

            Console.WriteLine($"PASS: {_assertionCount} assertions");
            return 0;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine($"FAIL: {exception.Message}");
            Console.Error.WriteLine(exception);
            return 1;
        }
    }

    private static void TestSyntheticDocument()
    {
        byte[] data = CreateSyntheticDocument();
        SmoDocument document = SmoDocument.Parse(data, "synthetic.smo");

        Equal("FFPS", document.Header.Signature, "synthetic signature");
        Equal((uint)data.Length, document.Header.FileSize, "synthetic declared length");
        Equal(1, document.Objects.Count, "synthetic object count");
        Equal("root", document.Objects[0].Name, "synthetic object name");
        Equal(SmoClassIds.Node, document.Objects[0].TypeHash, "synthetic class");
        True(document.Objects[0].SignatureMatches, "synthetic SBOO signature");
        True(document.Objects[0].IsWithinDataSection, "synthetic object bounds");
        Equal(0, document.Diagnostics.Count(
            item => item.Severity == SmoDiagnosticSeverity.Error),
            "synthetic errors");
    }

    private static byte[] CreateSyntheticDocument()
    {
        const int objectSize = sizeof(uint) + 4 + 1;
        byte[] body = new byte[objectSize];
        WriteUInt32(body, 0, SmoClassIds.Node);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(body, sizeof(uint));
        body[^1] = 0;
        return CreateSingleObjectDocument("root", SmoClassIds.Node, body);
    }

    private static void TestGuiStateClassification()
    {
        Equal(
            SmoGuiVisualState.Normal,
            SmoGuiSceneAnalyzer.ClassifyVisualStateName("NORMAL96"),
            "GUI NORMAL state with numeric suffix");
        Equal(
            SmoGuiVisualState.Highlighted,
            SmoGuiSceneAnalyzer.ClassifyVisualStateName("HIGHLIGHTED08"),
            "GUI HIGHLIGHTED state with numeric suffix");
        Equal(
            SmoGuiVisualState.Pushed,
            SmoGuiSceneAnalyzer.ClassifyVisualStateName("PUSHED"),
            "GUI PUSHED state");
        Equal(
            SmoGuiVisualState.Disabled,
            SmoGuiSceneAnalyzer.ClassifyVisualStateName("DISABLED163"),
            "GUI DISABLED state with numeric suffix");
        Equal(
            SmoGuiVisualState.Shadow,
            SmoGuiSceneAnalyzer.ClassifyVisualStateName("shadow153"),
            "GUI shadow layer");
        Equal(
            SmoGuiVisualState.Unclassified,
            SmoGuiSceneAnalyzer.ClassifyVisualStateName("resolution_label"),
            "GUI semantic anchor remains a common layer");
    }

    private static byte[] CreateSingleObjectDocument(
        string objectName,
        uint typeHash,
        byte[] body,
        int? serializedSize = null)
    {
        byte[] name = Encoding.ASCII.GetBytes(objectName + "\0");
        int tableLength = sizeof(uint) + sizeof(ushort) + name.Length + 3 * sizeof(uint);
        int dataStart = SmoHeader.Size + tableLength + sizeof(uint);
        int declaredObjectSize = serializedSize ?? body.Length;
        if (declaredObjectSize < 0 || declaredObjectSize > body.Length)
            throw new ArgumentOutOfRangeException(nameof(serializedSize));

        byte[] data = new byte[dataStart + body.Length];
        Encoding.ASCII.GetBytes("FFPS").CopyTo(data, 0);
        WriteUInt32(data, 0x04, 0x26);
        WriteUInt32(data, 0x08, 0);
        WriteUInt32(data, 0x0C, (uint)data.Length);
        WriteUInt32(data, 0x10, 2);
        WriteUInt32(data, 0x14, (uint)dataStart);
        WriteUInt32(data, 0x18, (uint)body.Length);
        WriteUInt32(data, 0x1C, 1);

        int offset = SmoHeader.ObjectTableOffset;
        WriteUInt32(data, offset, 1);
        offset += sizeof(uint);
        BinaryPrimitives.WriteUInt16LittleEndian(
            data.AsSpan(offset, sizeof(ushort)), (ushort)name.Length);
        offset += sizeof(ushort);
        name.CopyTo(data, offset);
        offset += name.Length;
        WriteUInt32(data, offset, typeHash);
        offset += sizeof(uint);
        WriteUInt32(data, offset, 0);
        offset += sizeof(uint);
        WriteUInt32(data, offset, (uint)declaredObjectSize);

        body.CopyTo(data, dataStart);
        return data;
    }

    private static byte[] CreateSyntheticTextureObject(ushort formatCode = 0x32E3)
    {
        const int width = 8;
        const int height = 8;
        const int pixelBytes = width * height * 4;
        bool hasPixelMarker = formatCode is 0x32E3 or 0x43E3;
        int pixelOffset = hasPixelMarker ? 0x3D : 0x34;
        uint outerTail = formatCode switch
        {
            0x32E3 => 0x32,
            0x43E3 => 0x43,
            0x29E3 => 0x29,
            _ => throw new ArgumentOutOfRangeException(nameof(formatCode))
        };
        int minimumSize = pixelOffset + pixelBytes + sizeof(uint);
        int outerBlockEnd = checked(0x0D + pixelBytes + (int)outerTail);
        byte[] body = new byte[Math.Max(minimumSize, outerBlockEnd)];

        WriteUInt32(body, 0, SmoClassIds.TextureData);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(body, sizeof(uint));
        if (hasPixelMarker)
        {
            body[0x08] = 0xE3;
            WriteUInt32(body, 0x09, pixelBytes + outerTail);
            body[0x19] = 0xE1;
            WriteUInt32(body, 0x1A, pixelBytes + 0x20u);
            body[0x1E] = 0xE0;
            WriteUInt32(body, 0x1F, pixelBytes + 0x1Au);
            WriteUInt32(body, 0x24, width);
            WriteUInt32(body, 0x28, height);
            body[0x3C] = 0x00;
        }
        else
        {
            body[0x08] = 0xE3;
            WriteUInt32(body, 0x09, pixelBytes + 0x29u);
            body[0x10] = 0xE1;
            WriteUInt32(body, 0x11, pixelBytes + 0x20u);
            body[0x15] = 0xE0;
            WriteUInt32(body, 0x16, pixelBytes + 0x1Au);
            WriteUInt32(body, 0x1B, width);
            WriteUInt32(body, 0x1F, height);
            WriteUInt32(body, 0x28, width);
            WriteUInt32(body, 0x2C, width * 4u);
            WriteUInt32(body, 0x30, height);
        }

        for (int offset = pixelOffset; offset < pixelOffset + pixelBytes; offset += 4)
        {
            body[offset] = 0x33;
            body[offset + 1] = 0x22;
            body[offset + 2] = 0x11;
            body[offset + 3] = 0x44;
        }

        int lastPixelOffset = pixelOffset + pixelBytes - 4;
        body[lastPixelOffset] = 0xB6;
        body[lastPixelOffset + 1] = 0xC7;
        body[lastPixelOffset + 2] = 0xD8;
        body[lastPixelOffset + 3] = 0xA5;

        return body;
    }

    private static void TestDataBlockHeaders()
    {
        byte[] fixedFour = [0x63, 1, 2, 3, 4];
        True(
            SmoDataBlockReader.TryReadHeader(fixedFour, out SmoDataBlockHeader first),
            "fixed-size data block");
        Equal(3, first.FieldType, "fixed-size field type");
        Equal((uint)4, first.PayloadSize, "fixed-size payload");
        Equal(1, first.HeaderSize, "fixed-size header");

        byte[] extended = [0xDF, 0x2A, 0x03, 0x00, 9, 8, 7];
        True(
            SmoDataBlockReader.TryReadHeader(extended, out SmoDataBlockHeader second),
            "extended data block");
        Equal(0x2A, second.FieldType, "extended field type");
        Equal((uint)3, second.PayloadSize, "extended payload");
        Equal(4, second.HeaderSize, "extended header");
    }

    private static void TestMaterialRenderStates()
    {
        uint[] opaqueStates = [0, 0, 1, 0, 1, 1, 3, 0, 4, 0, 6];
        uint[] effectStates = [0, 0, 1, 2, 1, 1, 3, 0, 4, 0, 6];
        uint[] rigidAlphaStates = [0, 0, 1, 0, 1, 0, 3, 0, 2, 0, 6];

        SmoMaterialRenderStateInfo opaque0 =
            SmoMaterialRenderState.Classify(0x0, opaqueStates);
        Equal(SmoMaterialBlendMode.OpaqueFinalBlend0, opaque0.BlendMode,
            "FinalBlendOp 0 classification");
        True(!opaque0.UsesAlphaBlend, "FinalBlendOp 0 is opaque");

        SmoMaterialRenderStateInfo opaque2 =
            SmoMaterialRenderState.Classify(0x2, opaqueStates);
        Equal(SmoMaterialBlendMode.OpaqueFinalBlend2, opaque2.BlendMode,
            "FinalBlendOp 2 classification");
        True(!opaque2.UsesAlphaBlend,
            "raw FinalBlendOp 2 is opaque without consumer and UV evidence");

        uint[] princessTransparentStates =
            [0, 0, 1, 0, 1, 0, 3, 0, 4, 0, 6];
        SmoMaterialRenderStateInfo princessRaw =
            SmoMaterialRenderState.Classify(0x2, princessTransparentStates);
        Equal(SmoMaterialBlendMode.OpaqueFinalBlend2, princessRaw.BlendMode,
            "princess-style FinalBlendOp 2 remains raw opaque");
        SmoMaterialRenderStateInfo princessWithoutTexture =
            princessRaw.ForConsumer(
                SmoMaterialConsumerKind.SkinnedSurface,
                alphaSortEnable: 0,
                renderPriority: 1,
                vertexDiffuseProfile:
                    SmoVertexDiffuseProfile.UniformOpaqueBlack);
        True(princessWithoutTexture.HasPrincessTransparentSurfaceContext,
            "exact op2 tuple and spSkin consumer context is recognized");
        True(!princessWithoutTexture.HasConfirmedPrincessTransparentSurfaceState &&
             !princessWithoutTexture.UsesAlphaBlend,
            "exact op2 context alone does not claim texture transparency");
        SmoTextureUvAlphaCoverage partialUvCoverage = new(
            true,
            SampledTexelCount: 10,
            FullyTransparentTexelCount: 1,
            PartialAlphaTexelCount: 8,
            OpaqueTexelCount: 1);
        SmoMaterialRenderStateInfo princessConfirmed =
            princessRaw.ForConsumer(
                SmoMaterialConsumerKind.SkinnedSurface,
                alphaSortEnable: 0,
                renderPriority: 1,
                vertexDiffuseProfile:
                    SmoVertexDiffuseProfile.UniformOpaqueBlack,
                textureUvAlphaCoverage: partialUvCoverage);
        Equal(
            SmoMaterialBlendMode.PrincessTransparentSurfaceFinalBlend2,
            princessConfirmed.BlendMode,
            "exact princess op2 context plus covered partial alpha classification");
        True(princessConfirmed.HasConfirmedPrincessTransparentSurfaceState &&
             princessConfirmed.HasConfirmedRenderableState &&
             princessConfirmed.UsesAlphaBlend &&
             princessConfirmed.RequiresTransparentOrdering,
            "confirmed princess op2 surface enters transparent ordering");
        SmoMaterialRenderStateInfo skinnedTransparentConfirmed =
            opaque2.ForConsumer(
                SmoMaterialConsumerKind.SkinnedSurface,
                alphaSortEnable: 1,
                renderPriority: 1,
                vertexDiffuseProfile:
                    SmoVertexDiffuseProfile.UniformOpaqueBlack,
                textureUvAlphaCoverage: partialUvCoverage);
        Equal(
            SmoMaterialBlendMode.SkinnedTransparentSurfaceFinalBlend2,
            skinnedTransparentConfirmed.BlendMode,
            "exact RS5=1/A1 skinned op2 partial-alpha classification");
        True(skinnedTransparentConfirmed
                 .HasConfirmedSkinnedTransparentSurfaceState &&
             skinnedTransparentConfirmed.HasConfirmedRenderableState &&
             skinnedTransparentConfirmed.UsesAlphaBlend &&
             skinnedTransparentConfirmed.RequiresTransparentOrdering &&
             !skinnedTransparentConfirmed.UsesEmissiveApproximation,
            "exact skinned op2 surface uses transparent non-emissive ordering");
        SmoMaterialRenderStateInfo arbitraryOp2WithAlpha = opaque2.ForConsumer(
            SmoMaterialConsumerKind.SkinnedSurface,
            alphaSortEnable: 0,
            renderPriority: 1,
            vertexDiffuseProfile:
                SmoVertexDiffuseProfile.UniformOpaqueBlack,
            textureUvAlphaCoverage: partialUvCoverage);
        Equal(SmoMaterialBlendMode.OpaqueFinalBlend2,
            arbitraryOp2WithAlpha.BlendMode,
            "arbitrary op2 texture alpha is not a princess surface");
        SmoMaterialRenderStateInfo wrongPrincessSkinState =
            princessRaw.ForConsumer(
                SmoMaterialConsumerKind.SkinnedSurface,
                alphaSortEnable: 1,
                renderPriority: 1,
                vertexDiffuseProfile:
                    SmoVertexDiffuseProfile.UniformOpaqueBlack,
                textureUvAlphaCoverage: partialUvCoverage);
        Equal(
            SmoMaterialBlendMode
                .UnconfirmedTransparentSurfaceFinalBlend2Hybrid,
            wrongPrincessSkinState.BlendMode,
            "RS5=0/A1 hybrid remains visible but separately classified");
        True(wrongPrincessSkinState
                 .HasUnconfirmedTransparentSurfaceHybridState &&
             wrongPrincessSkinState.UsesAlphaBlend &&
             wrongPrincessSkinState.RequiresTransparentOrdering &&
             !wrongPrincessSkinState.HasConfirmedRenderableState &&
             !wrongPrincessSkinState.UsesEmissiveApproximation &&
             wrongPrincessSkinState.Diagnostic?.StartsWith(
                 "UNCONFIRMED_FINAL_BLEND_2_HYBRID:",
                 StringComparison.Ordinal) == true,
            "RS5=0/A1 hybrid has conspicuous non-opaque WPF warning path");

        SmoMaterialRenderStateInfo effect4 =
            SmoMaterialRenderState.Classify(0x4, effectStates);
        Equal(SmoMaterialBlendMode.EffectFinalBlend4, effect4.BlendMode,
            "FinalBlendOp 4 effect classification");
        True(effect4.UsesEmissiveApproximation,
            "FinalBlendOp 4 uses the emissive preview path");
        True(effect4.UsesLuminanceCoverageApproximation,
            "FinalBlendOp 4 uses luminance coverage");
        True(effect4.Diagnostic?.StartsWith(
                "MATERIAL_EFFECT_BLEND_4:", StringComparison.Ordinal) == true,
            "FinalBlendOp 4 has an explicit approximation diagnostic");
        True(effect4.Diagnostic?.Contains(
                "not a native frame", StringComparison.Ordinal) == true,
            "FinalBlendOp 4 does not claim native-frame parity");

        SmoMaterialRenderStateInfo effect4WithCompanion2 =
            SmoMaterialRenderState.Classify(0x4, rigidAlphaStates);
        Equal(SmoMaterialBlendMode.EffectFinalBlend4,
            effect4WithCompanion2.BlendMode,
            "FinalBlendOp 4 stays an effect when companion state is 2");

        SmoMaterialRenderStateInfo effect5 =
            SmoMaterialRenderState.Classify(0x5, rigidAlphaStates);
        Equal(SmoMaterialBlendMode.EffectFinalBlend5, effect5.BlendMode,
            "FinalBlendOp 5 effect classification");
        True(effect5.UsesEmissiveApproximation,
            "FinalBlendOp 5 uses its separate emissive preview path");
        True(!effect5.UsesLuminanceCoverageApproximation,
            "FinalBlendOp 5 preserves authored alpha instead of op4 coverage");
        True(effect5.Diagnostic?.Contains(
                "not a native frame", StringComparison.Ordinal) == true,
            "FinalBlendOp 5 does not claim native-frame parity");

        True(SmoMaterialRenderState.UsesAlphaBlend(0x4) &&
             SmoMaterialRenderState.UsesAlphaBlend(0x5) &&
             SmoMaterialRenderState.UsesAlphaBlend(0x6),
            "known blended operations enter transparent ordering");
        True(!SmoMaterialRenderState.UsesAlphaBlend(0x7),
            "unknown operation 7 is not accepted through the former bit mask");

        SmoMaterialRenderStateInfo rigidAlpha =
            SmoMaterialRenderState.Classify(0x6, rigidAlphaStates);
        Equal(SmoMaterialBlendMode.FinalBlend6Companion2, rigidAlpha.BlendMode,
            "FinalBlendOp 6 companion 2 tuple classification");
        SmoMaterialRenderStateInfo rigidConsumer = rigidAlpha.ForConsumer(
            SmoMaterialConsumerKind.RigidOrEffect);
        True(rigidConsumer.HasConfirmedConsumerTuple,
            "FinalBlendOp 6 companion 2 is confirmed for rigid/effect geometry");
        True(rigidConsumer.UsesEmissiveApproximation,
            "confirmed rigid/effect tuple uses the level-effect preview path");
        True(rigidConsumer.Diagnostic is null,
            "confirmed rigid/effect tuple has no compatibility warning");
        SmoMaterialRenderStateInfo wrongSkinnedConsumer = rigidAlpha.ForConsumer(
            SmoMaterialConsumerKind.SkinnedSurface,
            alphaSortEnable: 1,
            renderPriority: 1,
            vertexDiffuseProfile: SmoVertexDiffuseProfile.UniformOpaqueBlack);
        True(wrongSkinnedConsumer.HasConsumerStateMismatch,
            "FinalBlendOp 6 companion 2 is rejected for skinned geometry");
        True(wrongSkinnedConsumer.UsesEmissiveApproximation &&
             wrongSkinnedConsumer.UsesLuminanceCoverageApproximation,
            "invalid skinned companion 2 becomes conspicuous in preview");
        True(wrongSkinnedConsumer.Diagnostic?.StartsWith(
                "MATERIAL_CONSUMER_TUPLE_DIVERGENCE:",
                StringComparison.Ordinal) == true,
            "invalid skinned companion 2 has a stable diagnostic");

        SmoMaterialRenderStateInfo skinnedAlpha =
            SmoMaterialRenderState.Classify(0x6, effectStates);
        Equal(SmoMaterialBlendMode.FinalBlend6Companion4, skinnedAlpha.BlendMode,
            "FinalBlendOp 6 companion 4 tuple classification");
        SmoMaterialRenderStateInfo skinnedConsumer = skinnedAlpha.ForConsumer(
            SmoMaterialConsumerKind.SkinnedSurface,
            alphaSortEnable: 1,
            renderPriority: 1,
            vertexDiffuseProfile: SmoVertexDiffuseProfile.UniformOpaqueBlack);
        True(skinnedConsumer.HasConfirmedConsumerTuple,
            "FinalBlendOp 6 companion 4 is confirmed for skinned surfaces");
        True(skinnedConsumer.HasConfirmedRenderableState,
            "exact skinned tuple plus AlphaSortEnable 1 is confirmed");
        True(!skinnedConsumer.UsesEmissiveApproximation,
            "confirmed skinned surface tuple is not previewed as op4 glow");
        True(skinnedConsumer.Diagnostic is null,
            "confirmed skinned surface tuple has no compatibility warning");
        SmoMaterialRenderStateInfo wrongRigidConsumer = skinnedAlpha.ForConsumer(
            SmoMaterialConsumerKind.RigidOrEffect);
        True(wrongRigidConsumer.HasConsumerStateMismatch,
            "FinalBlendOp 6 companion 4 is rejected for rigid geometry");

        SmoMaterialRenderStateInfo skinnedEffectA = rigidAlpha.ForConsumer(
            SmoMaterialConsumerKind.SkinnedEffect,
            alphaSortEnable: 0,
            renderPriority: 1,
            vertexDiffuseProfile: SmoVertexDiffuseProfile.Mixed);
        True(skinnedEffectA.HasConfirmedConsumerTuple &&
             !skinnedEffectA.HasConsumerStateMismatch,
            "known Droid/Golem companion-2 tuple remains a skinned effect");
        True(!skinnedEffectA.HasAlphaSortStateDivergence,
            "surface AlphaSort requirement is not generalized to skinned effects");
        uint[] skinnedEffectBStates = effectStates.ToArray();
        skinnedEffectBStates[8] = 2;
        SmoMaterialRenderStateInfo skinnedEffectB =
            SmoMaterialRenderState.Classify(0x6, skinnedEffectBStates)
                .ForConsumer(SmoMaterialConsumerKind.SkinnedEffect);
        True(skinnedEffectB.HasConfirmedConsumerTuple,
            "known Stormy/Knut companion-2 tuple remains a skinned effect");

        uint[] generatedTuple = effectStates.ToArray();
        generatedTuple[3] = 0;
        SmoMaterialRenderStateInfo generatedRs3Divergence =
            SmoMaterialRenderState.Classify(0x6, generatedTuple).ForConsumer(
                SmoMaterialConsumerKind.SkinnedSurface,
                alphaSortEnable: 1,
                renderPriority: 1,
                vertexDiffuseProfile: SmoVertexDiffuseProfile.UniformOpaqueBlack);
        Equal(SmoMaterialBlendMode.FinalBlend6Companion4,
            generatedRs3Divergence.BlendMode,
            "RS3 divergence retains the raw companion-4 family");
        True(!generatedRs3Divergence.HasConfirmedConsumerTuple &&
             generatedRs3Divergence.HasMaterialTupleDivergence,
            "RS8 alone cannot confirm a generated skinned-alpha tuple");
        True(generatedRs3Divergence.Diagnostic?.Contains(
                "RS[3]=0 (expected 2)", StringComparison.Ordinal) == true,
            "full-tuple diagnostic identifies the RS3 divergence");

        SmoMaterialRenderStateInfo generatedAlphaSortDivergence =
            skinnedAlpha.ForConsumer(
                SmoMaterialConsumerKind.SkinnedSurface,
                alphaSortEnable: 0,
                renderPriority: 1,
                vertexDiffuseProfile: SmoVertexDiffuseProfile.UniformOpaqueBlack);
        True(generatedAlphaSortDivergence.HasAlphaSortStateDivergence &&
             !generatedAlphaSortDivergence.HasConfirmedRenderableState,
            "AlphaSortEnable 0 leaves an exact material tuple non-confirmed");
        True(generatedAlphaSortDivergence.Diagnostic?.Contains(
                "ALPHA_SORT_STATE_DIVERGENCE:",
                StringComparison.Ordinal) == true,
            "AlphaSortEnable divergence has a stable diagnostic");

        SmoMaterialRenderStateInfo generatedWhiteDiffuse =
            skinnedAlpha.ForConsumer(
                SmoMaterialConsumerKind.SkinnedSurface,
                alphaSortEnable: 1,
                renderPriority: 1,
                vertexDiffuseProfile: SmoVertexDiffuseProfile.UniformOpaqueWhite);
        True(generatedWhiteDiffuse.HasVertexDiffuseDivergenceUnconfirmed,
            "uniform white skinned-alpha diffuse records the observed divergence");
        True(!generatedWhiteDiffuse.HasConsumerStateMismatch,
            "vertex diffuse observation alone does not select warning blending");
        True(generatedWhiteDiffuse.Diagnostic?.Contains(
                "not established as a cause", StringComparison.Ordinal) == true,
            "vertex diffuse diagnostic explicitly avoids a causal claim");
        True(generatedWhiteDiffuse.Summary.Contains(
                "AlphaSortEnable=1; Priority=1; " +
                "vertexDiffuse=UniformOpaqueWhite",
                StringComparison.Ordinal),
            "per-renderable skin and vertex evidence is propagated to summary");

        uint[] unknownCompanion = effectStates.ToArray();
        unknownCompanion[8] = 5;
        SmoMaterialRenderStateInfo nonstandard6 =
            SmoMaterialRenderState.Classify(0x6, unknownCompanion);
        Equal(SmoMaterialBlendMode.NonStandardFinalBlend6,
            nonstandard6.BlendMode,
            "unknown FinalBlendOp 6 companion classification");
        True(nonstandard6.Diagnostic?.StartsWith(
                "UNCONFIRMED_FINAL_BLEND_6_TUPLE:",
                StringComparison.Ordinal) == true,
            "unknown FinalBlendOp 6 companion has a stable diagnostic");

        SmoMaterialRenderStateInfo unknown =
            SmoMaterialRenderState.Classify(0x7, effectStates);
        Equal(SmoMaterialBlendMode.Unknown, unknown.BlendMode,
            "unknown FinalBlendOp classification");
        True(unknown.Diagnostic?.StartsWith(
                "UNKNOWN_FINAL_BLEND:", StringComparison.Ordinal) == true,
            "unknown FinalBlendOp has a stable diagnostic");

        SmoDocument synthetic = CreateSyntheticMaterialDocument(
            0x6, effectStates);
        True(SmoMaterialRenderState.TryDecode(
                synthetic,
                synthetic.Objects.Single(),
                out SmoMaterialRenderStateInfo? decoded),
            "synthetic material render state decodes");
        Equal((uint)0x6, decoded!.FinalBlendOperation,
            "synthetic FinalBlendOp");
        True(decoded.MaterialRenderStates.SequenceEqual(effectStates),
            "synthetic 11-value MaterialRenderStates tuple");
        Equal(SmoMaterialBlendMode.FinalBlend6Companion4, decoded.BlendMode,
            "synthetic decoded tuple family");
    }

    private static void TestAlphaDecalOverlayLossSimulation()
    {
        byte[] pixels =
        [
            10, 20, 30, 255,
            40, 50, 60, 127,
            70, 80, 90, 0
        ];
        SmoAlphaDecalDepthAnalyzer.ApplyOverlayLossSimulation(pixels);
        True(pixels.SequenceEqual(new byte[]
            {
                10, 20, 30, 0,
                40, 50, 60, 0,
                70, 80, 90, 0
            }),
            "face-overlay loss simulation zeros alpha and preserves hidden RGB");

        bool rejectedIncompletePixel = false;
        try
        {
            SmoAlphaDecalDepthAnalyzer.ApplyOverlayLossSimulation(
                new byte[3]);
        }
        catch (ArgumentException)
        {
            rejectedIncompletePixel = true;
        }
        True(rejectedIncompletePixel,
            "face-overlay loss simulation rejects incomplete BGRA32 pixels");
    }

    private static void TestNativeTransparencyFixtures()
    {
        string? workspace = FindWorkspaceRoot();
        if (workspace is null)
        {
            Console.WriteLine(
                "Workspace local-data not present; native transparency fixtures skipped.");
            return;
        }

        string minautor = Path.Combine(
            workspace,
            "local-data",
            "pc-pristine",
            "Media",
            "Characters",
            "Minautor",
            "Minautor.smo");
        if (File.Exists(minautor))
            CheckMinautorFinalBlend2Surface(minautor);

        string d90d = Path.Combine(
            workspace, "local-data", "bloom_jeans_skinned_Model.smo");
        if (File.Exists(d90d))
            CheckD90DTransparencyDiagnostics(d90d);

        string hybrid = Path.Combine(
            workspace,
            "local-data",
            "test-output",
            "bloom_jeans_layla_face_white_alpha_sorted_20260821.smo");
        if (File.Exists(hybrid))
            CheckUnconfirmedFinalBlend2Hybrid(hybrid);

        string exactCandidate = Path.Combine(
            workspace,
            "local-data",
            "test-output",
            "bloom_jeans_skinned_Model_primary_W2_20260821.smo");
        if (File.Exists(exactCandidate))
            CheckExactFinalBlend2Candidate(exactCandidate);
    }

    private static void CheckMinautorFinalBlend2Surface(string path)
    {
        SmoDocument document = SmoDocument.Load(path);
        FixtureRenderable renderable = BindFixtureRenderables(document)
            .Single(item => item.Mesh.ObjectIndex == 13);

        True(renderable.State.MaterialRenderStates.SequenceEqual(
                new uint[] { 0, 0, 1, 0, 1, 1, 3, 0, 4, 0, 6 }),
            "Minautor mesh 13 exact FinalBlendOp 2 material tuple");
        Equal((uint)1,
            renderable.Skin!.AlphaSortEnable.GetValueOrDefault(uint.MaxValue),
            "Minautor mesh 13 AlphaSortEnable");
        Equal((uint)1,
            renderable.Skin.Priority.GetValueOrDefault(uint.MaxValue),
            "Minautor mesh 13 Priority");
        Equal(SmoVertexDiffuseProfile.UniformOpaqueBlack,
            renderable.State.VertexDiffuseProfile,
            "Minautor mesh 13 black vertex diffuse");
        Equal(
            SmoMaterialBlendMode.SkinnedTransparentSurfaceFinalBlend2,
            renderable.State.BlendMode,
            "Minautor mesh 13 exact bound transparent mode");
        True(renderable.State.HasConfirmedSkinnedTransparentSurfaceState &&
             renderable.State.UsesAlphaBlend &&
             renderable.State.RequiresTransparentOrdering &&
             !renderable.State.UsesEmissiveApproximation &&
             renderable.State.Diagnostic is null,
            "Minautor mesh 13 uses confirmed non-emissive transparent ordering");
        True(renderable.State.TextureUvAlphaCoverage.HasPartialAlpha,
            "Minautor mesh 13 has triangle-covered partial texture alpha");
    }

    private static void CheckD90DTransparencyDiagnostics(string path)
    {
        string sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
        const string expectedSha256 =
            "D90D6088AA12E6ACE6F6D0CE3E0F9A25B38D129D074C3DF980BB3F2DD88024A0";
        if (!sha256.Equals(expectedSha256, StringComparison.Ordinal))
        {
            Console.WriteLine(
                $"Mutable D90D user-output fixture changed; exact historical " +
                $"checks skipped: {sha256}");
            return;
        }

        SmoDocument document = SmoDocument.Load(path);
        FixtureRenderable[] renderables = BindFixtureRenderables(document);
        FixtureRenderable[] transparent = renderables
            .Where(item => item.State.UsesAlphaBlend)
            .ToArray();
        Equal(3, transparent.Length,
            "D90D transparent FinalBlendOp 2 run count");
        True(transparent.All(item =>
                item.State.BlendMode ==
                    SmoMaterialBlendMode.PrincessTransparentSurfaceFinalBlend2 &&
                item.State.HasConfirmedPrincessTransparentSurfaceState &&
                item.Skin?.AlphaSortEnable == 0),
            "D90D remains the separate Princess RS5=0/A0 contract");

        SmoAlphaDecalOpaqueSurface[] opaque = renderables
            .Where(item => !item.State.UsesAlphaBlend)
            .Select(item => new SmoAlphaDecalOpaqueSurface(
                item.Mesh, item.WorldTransform))
            .ToArray();
        var largeRisks = transparent
            .Select(item => (
                item.Mesh,
                Info: SmoLargeAlphaNoDepthAnalyzer.Analyze(
                    item.Mesh, item.State, item.WorldTransform, opaque)))
            .Where(item => item.Info.HasLargeSurfaceOrderingRisk)
            .ToArray();
        Equal(1, largeRisks.Length,
            "D90D large RS5=0 alpha ordering warning count");
        Equal(96, largeRisks[0].Mesh.ObjectIndex,
            "D90D wing run receives the large alpha ordering warning");
        True(largeRisks[0].Info.Diagnostic?.Contains(
                "cannot prove", StringComparison.Ordinal) == true,
            "D90D large wing diagnostic disclaims isolated Viewer proof");

        SmoImportedFaceDiffuseInfo diffuse =
            SmoImportedFaceDiffuseAnalyzer.Analyze(
                renderables.Select(item => item.Mesh).ToArray());
        Equal(5, diffuse.RetainedOpaqueWhiteSurfaceCount,
            "D90D retained white skinned surface count");
        Equal(2, diffuse.ImportedOpaqueBlackFaceRunCount,
            "D90D imported black imp_o_x run count");
        True(diffuse.HasDiffuseMismatch &&
             diffuse.Diagnostic?.StartsWith(
                 "IMPORTED_FACE_VERTEX_DIFFUSE_MISMATCH:",
                 StringComparison.Ordinal) == true,
            "D90D mixed retained/imported face diffuse warning");
    }

    private static void CheckUnconfirmedFinalBlend2Hybrid(string path)
    {
        SmoDocument document = SmoDocument.Load(path);
        FixtureRenderable[] renderables = BindFixtureRenderables(document);
        FixtureRenderable[] hybrid = renderables
            .Where(item => item.State
                .HasUnconfirmedTransparentSurfaceHybridState)
            .ToArray();
        Equal(3, hybrid.Length,
            "A1 plus RS5=0 hybrid partial-alpha run count");
        True(hybrid.All(item =>
                item.State.BlendMode == SmoMaterialBlendMode
                    .UnconfirmedTransparentSurfaceFinalBlend2Hybrid &&
                item.State.UsesAlphaBlend &&
                item.State.RequiresTransparentOrdering &&
                !item.State.HasConfirmedRenderableState &&
                !item.State.UsesEmissiveApproximation &&
                item.State.Diagnostic?.StartsWith(
                    "UNCONFIRMED_FINAL_BLEND_2_HYBRID:",
                    StringComparison.Ordinal) == true),
            "A1 plus RS5=0 hybrid is conspicuous, transparent and non-emissive");

        SmoImportedFaceDiffuseInfo diffuse =
            SmoImportedFaceDiffuseAnalyzer.Analyze(
                renderables.Select(item => item.Mesh).ToArray());
        Equal(2, diffuse.ImportedOpaqueWhiteFaceRunCount,
            "face-white candidate imp_o_x count");
        True(!diffuse.HasDiffuseMismatch && diffuse.Diagnostic is null,
            "face-white candidate has zero imp_o/body diffuse warnings");
    }

    private static void CheckExactFinalBlend2Candidate(string path)
    {
        string sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
        const string expectedSha256 =
            "C750DB020797FE631EF47DE83067D39335D9E671F4039EB8446CB5659D1FF369";
        if (!sha256.Equals(expectedSha256, StringComparison.Ordinal))
        {
            Console.WriteLine(
                $"Optional C750 exact candidate changed; structural checks " +
                $"skipped: {sha256}");
            return;
        }

        SmoDocument document = SmoDocument.Load(path);
        FixtureRenderable[] renderables = BindFixtureRenderables(document);
        FixtureRenderable[] exact = renderables
            .Where(item => item.State
                .HasConfirmedSkinnedTransparentSurfaceState)
            .ToArray();
        Equal(3, exact.Length,
            "C750 exact skinned FinalBlendOp 2 run count");
        True(exact.All(item =>
                item.State.BlendMode ==
                    SmoMaterialBlendMode.SkinnedTransparentSurfaceFinalBlend2 &&
                item.State.MaterialRenderStates.SequenceEqual(
                    new uint[] { 0, 0, 1, 0, 1, 1, 3, 0, 4, 0, 6 }) &&
                item.Skin?.AlphaSortEnable == 1 &&
                item.Skin.Priority == 1 &&
                item.State.VertexDiffuseProfile ==
                    SmoVertexDiffuseProfile.UniformOpaqueBlack &&
                item.State.TextureUvAlphaCoverage.HasPartialAlpha &&
                item.State.UsesAlphaBlend &&
                item.State.RequiresTransparentOrdering &&
                !item.State.UsesEmissiveApproximation &&
                item.State.Diagnostic is null),
            "C750 three exact runs use the confirmed non-emissive contract");

        SmoAlphaDecalOpaqueSurface[] opaque = renderables
            .Where(item => !item.State.UsesAlphaBlend)
            .Select(item => new SmoAlphaDecalOpaqueSurface(
                item.Mesh, item.WorldTransform))
            .ToArray();
        Equal(0, exact.Count(item =>
                SmoLargeAlphaNoDepthAnalyzer.Analyze(
                        item.Mesh,
                        item.State,
                        item.WorldTransform,
                        opaque)
                    .HasLargeSurfaceOrderingRisk),
            "C750 RS5=1 exact surfaces have zero RS5=0 large warnings");

        SmoImportedFaceDiffuseInfo diffuse =
            SmoImportedFaceDiffuseAnalyzer.Analyze(
                renderables.Select(item => item.Mesh).ToArray());
        Equal(2, diffuse.ImportedOpaqueWhiteFaceRunCount,
            "C750 white imp_o_x face run count");
        True(!diffuse.HasDiffuseMismatch && diffuse.Diagnostic is null,
            "C750 has zero mixed face diffuse warnings");
    }

    private static FixtureRenderable[] BindFixtureRenderables(
        SmoDocument document)
    {
        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        var result = new List<FixtureRenderable>();
        foreach (SmoObjectEntry entry in document.Objects.Where(
                     item => item.TypeHash == SmoClassIds.MeshData))
        {
            SmoMesh mesh = SmoMeshDecoder.Decode(document, entry);
            if (!bindings.TryGetValue(entry.Index, out SmoTextureBinding? binding) ||
                binding.MaterialRenderState is null)
            {
                continue;
            }

            int? skinIndex = FindAncestorObjectIndex(
                document.Objects, entry, SmoClassIds.Skin);
            SmoSkin? skin = null;
            if (skinIndex.HasValue)
            {
                SmoSkinDecoder.TryDecode(
                    document, document.Objects[skinIndex.Value], out skin, out _);
            }
            SmoMaterialRenderStateInfo state =
                SmoMaterialRenderState.BindToRenderable(
                    binding.MaterialRenderState,
                    mesh,
                    skin,
                    binding.Texture);
            result.Add(new FixtureRenderable(
                mesh,
                skin,
                state,
                SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, entry)));
        }
        return result.ToArray();
    }

    private static string? FindWorkspaceRoot()
    {
        DirectoryInfo? cursor = new(AppContext.BaseDirectory);
        while (cursor is not null)
        {
            if (Directory.Exists(Path.Combine(cursor.FullName, "local-data")) &&
                Directory.Exists(Path.Combine(cursor.FullName, "tools")))
            {
                return cursor.FullName;
            }
            cursor = cursor.Parent;
        }
        return null;
    }

    private static SmoDocument CreateSyntheticMaterialDocument(
        uint finalBlendOperation,
        IReadOnlyList<uint> materialRenderStates)
    {
        if (materialRenderStates.Count != 11)
            throw new ArgumentException("Expected 11 render states.",
                nameof(materialRenderStates));

        byte[] body = new byte[8 + 2 + 11 * sizeof(uint) + 1 + sizeof(uint)];
        WriteUInt32(body, 0, SmoClassIds.MaterialData);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(body, sizeof(uint));
        body[8] = 0xA0; // field type 0, UInt8 payload size
        body[9] = 11 * sizeof(uint);
        for (int index = 0; index < materialRenderStates.Count; index++)
            WriteUInt32(body, 10 + index * sizeof(uint), materialRenderStates[index]);
        int operationHeader = 10 + 11 * sizeof(uint);
        body[operationHeader] = 0x63; // field type 3, fixed four-byte payload
        WriteUInt32(body, operationHeader + 1, finalBlendOperation);

        return SmoDocument.Parse(CreateSingleObjectDocument(
            "material", SmoClassIds.MaterialData, body));
    }

    private static void TestSyntheticTextures()
    {
        (ushort FormatCode, string Name)[] cases =
        [
            (0x32E3, "BGRA 0x32E3"),
            (0x43E3, "BGRA 0x43E3"),
            (0x29E3, "BGRA 0x29E3")
        ];

        foreach ((ushort formatCode, string name) in cases)
        {
            byte[] body = CreateSyntheticTextureObject(formatCode);
            byte[] data = CreateSingleObjectDocument(
                $"texture_{formatCode:X4}",
                SmoClassIds.TextureData,
                body);
            SmoDocument document = SmoDocument.Parse(data, $"texture_{formatCode:X4}.smo");

            True(
                SmoTextureDecoder.TryDecode(
                    document,
                    document.Objects.Single(),
                    out SmoTexture? texture,
                    out string error),
                $"synthetic {name} texture decodes: {error}");
            Equal(8, texture!.Width, $"synthetic {name} width");
            Equal(8, texture.Height, $"synthetic {name} height");
            Equal(formatCode, texture.FormatCode, $"synthetic {name} format code");
            Equal(SmoTextureLayout.Bgra, texture.SourceLayout,
                $"synthetic {name} source layout");
            Equal(32, texture.PixelStride, $"synthetic {name} pixel stride");
            Equal(8 * 8 * 4, texture.Bgra32Pixels.Length, $"synthetic {name} size");

            ReadOnlySpan<byte> pixels = texture.Bgra32Pixels.Span;
            Equal((byte)0x33, pixels[0], $"synthetic {name} first blue");
            Equal((byte)0x22, pixels[1], $"synthetic {name} first green");
            Equal((byte)0x11, pixels[2], $"synthetic {name} first red");
            Equal((byte)0x44, pixels[3], $"synthetic {name} first alpha");

            if (formatCode is 0x32E3 or 0x43E3)
            {
                Equal((byte)0x00, body[0x3C], $"synthetic {name} serializer marker");
                True(pixels[3] != body[0x3C],
                    $"synthetic {name} serializer marker is not decoded as alpha");
                int last = pixels.Length - 4;
                Equal((byte)0xB6, pixels[last], $"synthetic {name} last blue");
                Equal((byte)0xC7, pixels[last + 1], $"synthetic {name} last green");
                Equal((byte)0xD8, pixels[last + 2], $"synthetic {name} last red");
                Equal((byte)0xA5, pixels[last + 3], $"synthetic {name} last alpha");
            }
        }

        byte[] invalidPixelMarkerBody =
            CreateSyntheticTextureObject();
        invalidPixelMarkerBody[0x3C] = 0x01;
        SmoDocument invalidPixelMarkerDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "invalid_pixel_marker",
                SmoClassIds.TextureData,
                invalidPixelMarkerBody));
        True(
            !SmoTextureDecoder.TryDecode(
                invalidPixelMarkerDocument,
                invalidPixelMarkerDocument.Objects.Single(),
                out _,
                out string invalidPixelMarkerError),
            "wrong 0x32E3 pixel serializer marker is rejected");
        True(
            invalidPixelMarkerError.StartsWith("TEXTURE_PIXEL_MARKER_MISMATCH:"),
            "wrong 0x32E3 pixel serializer marker has a stable diagnostic code");

        byte[] invalidMarkerBody = CreateSyntheticTextureObject();
        invalidMarkerBody[0x19] = 0xE0;
        SmoDocument invalidMarkerDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "invalid_marker",
                SmoClassIds.TextureData,
                invalidMarkerBody));
        True(
            !SmoTextureDecoder.TryDecode(
                invalidMarkerDocument,
                invalidMarkerDocument.Objects.Single(),
                out _,
                out string invalidMarkerError),
            "wrong nested texture marker is rejected");
        True(
            invalidMarkerError.StartsWith("TEXTURE_BLOCK_SIZE_MISMATCH:"),
            "wrong nested marker has a stable diagnostic code");

        byte[] oversizedBlockBody =
            CreateSyntheticTextureObject();
        WriteUInt32(oversizedBlockBody, 0x1A, uint.MaxValue);
        SmoDocument oversizedBlockDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "oversized_block",
                SmoClassIds.TextureData,
                oversizedBlockBody));
        True(
            !SmoTextureDecoder.TryDecode(
                oversizedBlockDocument,
                oversizedBlockDocument.Objects.Single(),
                out _,
                out string oversizedBlockError),
            "nested texture payload cannot escape the object interval");
        True(
            oversizedBlockError.StartsWith("TEXTURE_BLOCK_SIZE_MISMATCH:"),
            "oversized nested block has a stable diagnostic code");

        byte[] mismatchedDimensionsBody =
            CreateSyntheticTextureObject(0x29E3);
        WriteUInt32(mismatchedDimensionsBody, 0x1B, 4);
        SmoDocument mismatchedDimensionsDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "mismatched_dimensions",
                SmoClassIds.TextureData,
                mismatchedDimensionsBody));
        True(
            !SmoTextureDecoder.TryDecode(
                mismatchedDimensionsDocument,
                mismatchedDimensionsDocument.Objects.Single(),
                out _,
                out string mismatchedDimensionsError),
            "inconsistent BGRA dimensions are rejected");
        True(
            mismatchedDimensionsError.StartsWith("TEXTURE_DIMENSION_MISMATCH:"),
            "inconsistent BGRA dimensions have a stable diagnostic code");

        byte[] mismatchedStrideBody =
            CreateSyntheticTextureObject(0x29E3);
        WriteUInt32(mismatchedStrideBody, 0x2C, 4);
        SmoDocument mismatchedStrideDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "mismatched_stride",
                SmoClassIds.TextureData,
                mismatchedStrideBody));
        True(
            !SmoTextureDecoder.TryDecode(
                mismatchedStrideDocument,
                mismatchedStrideDocument.Objects.Single(),
                out _,
                out string mismatchedStrideError),
            "inconsistent BGRA row stride is rejected");
        True(
            mismatchedStrideError.StartsWith("TEXTURE_ROW_STRIDE_MISMATCH:"),
            "inconsistent BGRA row stride has a stable diagnostic code");

        byte[] unsupportedBody = CreateSyntheticTextureObject();
        unsupportedBody[0x09] = 0xFD;
        SmoDocument unsupportedDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "unsupported",
                SmoClassIds.TextureData,
                unsupportedBody));
        True(
            !SmoTextureDecoder.TryDecode(
                unsupportedDocument,
                unsupportedDocument.Objects.Single(),
                out _,
                out string unsupportedError),
            "unknown texture format is rejected");
        True(
            unsupportedError.StartsWith("UNSUPPORTED_TEXTURE_FORMAT:"),
            "unknown texture format has a stable diagnostic code");

        byte[] truncatedBody = CreateSyntheticTextureObject();
        SmoDocument truncatedDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "truncated",
                SmoClassIds.TextureData,
                truncatedBody,
                serializedSize: 0x40));
        True(
            !SmoTextureDecoder.TryDecode(
                truncatedDocument,
                truncatedDocument.Objects.Single(),
                out _,
                out string truncatedError),
            "texture pixels cannot escape the object interval");
        True(
            truncatedError.StartsWith("TEXTURE_PIXEL_BUFFER_OUTSIDE_OBJECT:"),
            "texture boundary failure has a stable diagnostic code");

        SmoDocument nodeDocument = SmoDocument.Parse(CreateSyntheticDocument());
        True(
            !SmoTextureDecoder.TryDecode(
                nodeDocument,
                nodeDocument.Objects.Single(),
                out _,
                out string classError),
            "non-texture object is rejected by texture decoder");
        True(
            classError.StartsWith("NOT_TEXTURE_DATA:"),
            "non-texture failure has a stable diagnostic code");
    }

    private static void TestVertexColorUsage()
    {
        const uint black = 0xFF000000;
        const uint white = 0xFFFFFFFF;

        uint[] trollPlaceholder = Enumerable.Repeat(black, 425)
            .Concat(Enumerable.Repeat(white, 10))
            .ToArray();
        True(
            !SmoVertexColorUsage.ShouldModulateTexture(
                0x097E, trollPlaceholder),
            "dominant-black two-colour character diffuse is a placeholder");

        uint[] boundaryTwoColour = Enumerable.Repeat(black, 95)
            .Concat(Enumerable.Repeat(white, 5))
            .ToArray();
        True(
            SmoVertexColorUsage.ShouldModulateTexture(
                0x097E, boundaryTwoColour),
            "exactly 95-percent black character diffuse remains renderable");

        uint[] aboveBoundaryPlaceholder = Enumerable.Repeat(black, 96)
            .Concat(Enumerable.Repeat(white, 4))
            .ToArray();
        True(
            !SmoVertexColorUsage.ShouldModulateTexture(
                0x097E, aboveBoundaryPlaceholder),
            "more than 95-percent black character diffuse is a placeholder");

        uint[] transparentBlack = Enumerable.Repeat(0x00000000u, 98)
            .Concat(Enumerable.Repeat(white, 2))
            .ToArray();
        True(
            SmoVertexColorUsage.ShouldModulateTexture(
                0x097E, transparentBlack),
            "non-opaque black is not classified as the exporter placeholder");

        uint[] genuineGradient = Enumerable.Repeat(black, 98)
            .Concat([0xFF0D132E, 0xFF808080])
            .ToArray();
        True(
            SmoVertexColorUsage.ShouldModulateTexture(
                0x097E, genuineGradient),
            "multi-colour character gradient remains renderable");
        True(
            SmoVertexColorUsage.ShouldModulateTexture(
                0x093E, trollPlaceholder),
            "dominant-black exception stays scoped to character layouts");
        True(
            !SmoVertexColorUsage.ShouldModulateTexture(
                0x097E, Enumerable.Repeat(black, 8).ToArray()),
            "uniform character diffuse remains a placeholder");
    }

    private static CorpusOptions ParseCorpusOptions(string[] args)
    {
        string? path = null;
        int? sampleCount = null;
        int seed = 20260808;

        for (int index = 0; index < args.Length; index++)
        {
            switch (args[index])
            {
                case "--sample-count":
                    sampleCount = int.Parse(args[++index]);
                    if (sampleCount <= 0)
                        throw new ArgumentOutOfRangeException(nameof(sampleCount));
                    break;
                case "--seed":
                    seed = int.Parse(args[++index]);
                    break;
                default:
                    if (args[index].StartsWith("--", StringComparison.Ordinal))
                        throw new ArgumentException($"Unknown option: {args[index]}");
                    if (path is not null)
                        throw new ArgumentException("Only one corpus path can be specified.");
                    path = args[index];
                    break;
            }
        }

        return new CorpusOptions(path, sampleCount, seed);
    }

    private static void TestLocalCorpus(string corpusPath, int? sampleCount, int seed)
    {
        bool singleFile = File.Exists(corpusPath);
        string[] allFiles = singleFile
            ? [corpusPath]
            : Directory
                .EnumerateFiles(corpusPath, "*.smo", SearchOption.AllDirectories)
                .OrderBy(path => path, StringComparer.OrdinalIgnoreCase)
                .ToArray();

        True(allFiles.Length > 0, "corpus contains SMO files");

        string[] files = allFiles;

        if (sampleCount.HasValue)
        {
            var random = new Random(seed);
            files = allFiles
                .Select(path => (Path: path, Key: random.NextInt64()))
                .OrderBy(item => item.Key)
                .Take(Math.Min(sampleCount.Value, allFiles.Length))
                .Select(item => item.Path)
                .ToArray();
            Equal(Math.Min(sampleCount.Value, allFiles.Length), files.Length, "sample size");
        }

        int parsed = 0;
        long objects = 0;
        int decodedTextures = 0;
        int resolvedBindings = 0;
        int decodedMeshes = 0;
        int uvMeshes = 0;
        int renderableTexturedMeshes = 0;
        int transformedModelMeshes = 0;
        int decodedStaticTransforms = 0;
        int resolvedMaterialColors = 0;
        int vertexColoredMeshes = 0;
        int meshesUnderStaticObjects = 0;
        int meshesWithoutStaticObjects = 0;
        var unplacedSamples = new List<string>();
        foreach (string file in files)
        {
            SmoDocument document = SmoDocument.Load(file);
            bool verboseFile = singleFile && document.Objects.Count <= 500;
            Equal(new FileInfo(file).Length, (long)document.Data.Length, $"length: {file}");
            Equal("FFPS", document.Header.Signature, $"signature: {file}");
            Equal(
                document.Header.DeclaredDataEnd,
                (ulong)document.Data.Length,
                $"data boundary: {file}");
            Equal(
                (int)document.Header.ObjectCount,
                document.Objects.Count,
                $"object count: {file}");
            parsed++;
            objects += document.Objects.Count;

            foreach (SmoObjectEntry entry in document.Objects.Where(
                         entry => entry.TypeHash == SmoClassIds.TextureData))
            {
                if (!SmoTextureDecoder.TryDecode(
                        document, entry, out SmoTexture? texture, out string textureError))
                {
                    if (verboseFile)
                        Console.WriteLine($"Texture [{entry.Index}] decode failed: {textureError}");
                    continue;
                }

                Equal(
                    checked(texture!.Width * texture.Height * 4),
                    texture.Bgra32Pixels.Length,
                    $"texture byte count: {file} [{entry.Index}]");
                Equal(texture.Width * 4, texture.PixelStride, $"texture stride: {file} [{entry.Index}]");
                if (texture.FormatCode is 0x32E3 or 0x43E3)
                {
                    Equal(SmoTextureLayout.Bgra, texture.SourceLayout,
                        $"newer texture source layout: {file} [{entry.Index}]");
                    ReadOnlySpan<byte> serialized = document.Data.Span.Slice(
                        checked((int)entry.PhysicalOffset),
                        checked((int)entry.SerializedSize));
                    Equal((byte)0x00, serialized[0x3C],
                        $"newer texture serializer marker: {file} [{entry.Index}]");

                    ReadOnlySpan<byte> sourcePixels = serialized.Slice(
                        0x3D, texture.Bgra32Pixels.Length);
                    ReadOnlySpan<byte> decodedPixels = texture.Bgra32Pixels.Span;
                    for (int channel = 0; channel < 4; channel++)
                    {
                        Equal(sourcePixels[channel], decodedPixels[channel],
                            $"newer texture first BGRA[{channel}]: {file} [{entry.Index}]");
                        int lastChannel = sourcePixels.Length - 4 + channel;
                        Equal(sourcePixels[lastChannel], decodedPixels[lastChannel],
                            $"newer texture last BGRA[{channel}]: {file} [{entry.Index}]");
                    }
                }
                decodedTextures++;
                if (verboseFile)
                {
                    ReadOnlySpan<byte> pixels = texture.Bgra32Pixels.Span;
                    int transparentPixels = 0;
                    byte minAlpha = byte.MaxValue;
                    byte maxAlpha = byte.MinValue;
                    for (int offset = 3; offset < pixels.Length; offset += 4)
                    {
                        byte alpha = pixels[offset];
                        minAlpha = Math.Min(minAlpha, alpha);
                        maxAlpha = Math.Max(maxAlpha, alpha);
                        if (alpha == 0)
                            transparentPixels++;
                    }
                    Console.WriteLine(
                        $"Texture [{entry.Index}] {texture.Name}: {texture.Width}x{texture.Height}, " +
                        $"alpha={minAlpha}..{maxAlpha}, transparent={transparentPixels}");
                }
            }

            IReadOnlyDictionary<int, SmoTextureBinding> bindings =
                SmoTextureBindingResolver.ResolveAll(document);
            IReadOnlyDictionary<int, uint> materialColors =
                SmoMaterialColorResolver.ResolveAll(document);
            SmoNodeHierarchy nodeHierarchy = SmoNodeHierarchy.Decode(document);
            foreach (SmoNodeChildLink link in nodeHierarchy.Links)
            {
                True(
                    document.Objects[link.ChildObjectIndex].Id == link.ChildObjectId,
                    $"node child ID resolves: {file} [{link.ParentObjectIndex}] -> " +
                    $"[{link.ChildObjectIndex}]");
                True(
                    link.InlineSerializedSize == 0 ||
                    link.InlineSerializedSize ==
                    document.Objects[link.ChildObjectIndex].SerializedSize,
                    $"node child inline size: {file} [{link.ParentObjectIndex}] -> " +
                    $"[{link.ChildObjectIndex}]");
            }

            if (verboseFile)
            {
                foreach (SmoObjectEntry skin in document.Objects.Where(entry =>
                             entry.TypeHash == SmoClassIds.Skin))
                {
                    Console.WriteLine(
                        $"Skin [{skin.Index}] id={skin.Id} {skin.Name}: " +
                        $"parent={skin.ParentIndex}, off=0x{skin.LogicalOffset:X}, " +
                        $"size=0x{skin.SerializedSize:X}");
                    if (SmoSkinDecoder.TryDecode(
                            document, skin, out SmoSkin? decodedSkin, out string skinError) &&
                        decodedSkin is not null)
                    {
                        Console.WriteLine(
                            $"  palette={string.Join(", ", decodedSkin.Bones.Select(bone =>
                                $"{bone.PaletteIndex}:[{bone.NodeObjectIndex}]" +
                                document.Objects[bone.NodeObjectIndex].Name))}");
                        foreach (SmoSkinBone bone in decodedSkin.Bones.Where(bone =>
                                     document.Objects[bone.NodeObjectIndex].Name is
                                         "Head" or "Bloom_head_geometry"))
                        {
                            if (System.Numerics.Matrix4x4.Invert(
                                    bone.InverseBindMatrix, out var bindMatrix))
                            {
                                Console.WriteLine(
                                    $"  bind {document.Objects[bone.NodeObjectIndex].Name}: " +
                                    $"T=({bindMatrix.M41:G6},{bindMatrix.M42:G6}," +
                                    $"{bindMatrix.M43:G6})");
                            }
                        }
                    }
                    else
                    {
                        Console.WriteLine($"  palette decode failed: {skinError}");
                    }
                }
                foreach (SmoNodeChildLink link in nodeHierarchy.Links)
                {
                    Console.WriteLine(
                        $"Child [{link.ParentObjectIndex}] -> [{link.ChildObjectIndex}] " +
                        $"id={link.ChildObjectId}, inline=0x{link.InlineSerializedSize:X}");
                }
                foreach (SmoObjectEntry node in document.Objects.Where(entry =>
                             entry.TypeHash is SmoClassIds.Node or
                                 SmoClassIds.RenderNode or SmoClassIds.Model))
                {
                    if (SmoNodeTransformDecoder.TryDecode(
                            document, node, out SmoNodeTransform? nodeTransform) &&
                        nodeTransform is not null)
                    {
                        Console.WriteLine(
                            $"Node [{node.Index}] id={node.Id} {node.Name}: " +
                            $"parent={node.ParentIndex}, off=0x{node.LogicalOffset:X}, " +
                            $"size=0x{node.SerializedSize:X}, " +
                            $"P={nodeTransform.Position}, Q={nodeTransform.Rotation}, " +
                            $"S={nodeTransform.Scale}");
                    }
                }
            }
            foreach ((int meshIndex, uint argb) in materialColors)
            {
                True(document.Objects.Any(entry =>
                        entry.Index == meshIndex && entry.TypeHash == SmoClassIds.MeshData),
                    $"material color references a mesh: {file} [{meshIndex}]");
                True((argb & 0x00FFFFFF) != 0,
                    $"material color is visible: {file} [{meshIndex}]");
                resolvedMaterialColors++;
            }
            foreach ((int meshIndex, SmoTextureBinding binding) in bindings)
            {
                True(
                    document.Objects.Any(entry =>
                        entry.Index == meshIndex && entry.TypeHash == SmoClassIds.MeshData),
                    $"binding references a mesh: {file} [{meshIndex}]");
                if (binding.Texture is not null)
                    resolvedBindings++;
            }

            foreach (SmoObjectEntry entry in document.Objects.Where(
                         entry => entry.TypeHash == SmoClassIds.StaticRenderObject))
            {
                if (!SmoStaticRenderObjectTransformDecoder.TryDecode(
                        document, entry, out Matrix4x4 staticTransform))
                    continue;

                True(IsFinite(staticTransform),
                    $"finite static render transform: {file} [{entry.Index}]");
                decodedStaticTransforms++;
            }

            foreach (SmoObjectEntry entry in document.Objects.Where(
                         entry => entry.TypeHash == SmoClassIds.MeshData))
            {
                if (!SmoMeshDecoder.TryDecode(document, entry, out SmoMesh? mesh, out _))
                    continue;
                decodedMeshes++;
                if (mesh!.VertexFormat == 0x093E)
                {
                    True(mesh.HasSkinningData,
                        $"0x093E skinning attributes: {file} [{entry.Index}]");
                    True(
                        mesh.BlendWeights.All(weight =>
                            weight.X >= 0 && weight.Y >= 0 &&
                            weight.Z >= 0 && weight.W >= 0 &&
                            weight.X + weight.Y + weight.Z + weight.W <= 1.001f),
                        $"0x093E normalized blend weights: {file} [{entry.Index}]");
                    True(
                        mesh.BlendWeights.Zip(mesh.BlendIndices).All(item =>
                            (item.First.X <= 0.00001f || item.Second.X < 16) &&
                            (item.First.Y <= 0.00001f || item.Second.Y < 16) &&
                            (item.First.Z <= 0.00001f || item.Second.Z < 16) &&
                            (item.First.W <= 0.00001f || item.Second.W < 16)),
                        $"0x093E active blend indices: {file} [{entry.Index}]");
                }
                Dictionary<int, SmoObjectEntry> entriesByIndex = document.Objects
                    .ToDictionary(item => item.Index);
                SmoObjectEntry? ancestor = entry;
                bool underStaticObject = false;
                while (ancestor.ParentIndex is int ancestorIndex &&
                       entriesByIndex.TryGetValue(ancestorIndex, out ancestor))
                {
                    if (ancestor.TypeHash == SmoClassIds.StaticRenderObject)
                    {
                        underStaticObject = true;
                        break;
                    }
                }
                if (underStaticObject)
                    meshesUnderStaticObjects++;
                else
                    meshesWithoutStaticObjects++;
                if (mesh!.HasDiffuseColors && mesh.DiffuseColorsArgb.Any(
                        color => (color & 0x00FFFFFF) != 0x00FFFFFF))
                    vertexColoredMeshes++;
                Matrix4x4 worldTransform =
                    SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, entry);
                if (singleFile && !underStaticObject && worldTransform == Matrix4x4.Identity &&
                    unplacedSamples.Count < 30)
                {
                    var chain = new List<string>();
                    SmoObjectEntry? chainEntry = entry;
                    while (chainEntry is not null)
                    {
                        chain.Add(
                            $"[{chainEntry.Index}]" +
                            $"{SmoClassRegistry.GetDisplayName(chainEntry.TypeHash)}:" +
                            chainEntry.Name);
                        chainEntry = chainEntry.ParentIndex is int chainParent &&
                                     entriesByIndex.TryGetValue(chainParent, out SmoObjectEntry? parent)
                            ? parent
                            : null;
                    }
                    System.Numerics.Vector3 minimum = new(
                        mesh.Positions.Min(position => position.X),
                        mesh.Positions.Min(position => position.Y),
                        mesh.Positions.Min(position => position.Z));
                    System.Numerics.Vector3 maximum = new(
                        mesh.Positions.Max(position => position.X),
                        mesh.Positions.Max(position => position.Y),
                        mesh.Positions.Max(position => position.Z));
                    unplacedSamples.Add(
                        $"bounds center={(minimum + maximum) * 0.5f}, size={maximum - minimum}; " +
                        string.Join(" <- ", chain));
                }
                if (worldTransform != Matrix4x4.Identity)
                {
                    True(IsFinite(worldTransform), $"finite model transform: {file} [{entry.Index}]");
                    transformedModelMeshes++;
                }
                if (!mesh.HasTextureCoordinates)
                    continue;
                uvMeshes++;
                if (bindings.TryGetValue(entry.Index, out SmoTextureBinding? binding) &&
                    binding.Texture is not null)
                {
                    renderableTexturedMeshes++;
                }

                if (verboseFile)
                {
                    string textureName = bindings.TryGetValue(
                            entry.Index, out SmoTextureBinding? selected) &&
                        selected.Texture is not null
                            ? selected.Texture.Name
                            : "<fallback>";
                    float minU = mesh.TextureCoordinates.Min(uv => uv.X);
                    float maxU = mesh.TextureCoordinates.Max(uv => uv.X);
                    float minV = mesh.TextureCoordinates.Min(uv => uv.Y);
                    float maxV = mesh.TextureCoordinates.Max(uv => uv.Y);
                    System.Numerics.Vector3 rawCenter = new(
                        (mesh.Positions.Min(position => position.X) +
                         mesh.Positions.Max(position => position.X)) * 0.5f,
                        (mesh.Positions.Min(position => position.Y) +
                         mesh.Positions.Max(position => position.Y)) * 0.5f,
                        (mesh.Positions.Min(position => position.Z) +
                         mesh.Positions.Max(position => position.Z)) * 0.5f);
                    System.Numerics.Vector3 transformedCenter =
                        System.Numerics.Vector3.Transform(rawCenter, worldTransform);
                    Console.WriteLine(
                        $"Mesh [{entry.Index}] {entry.Name}: format=0x{mesh.VertexFormat:X}, " +
                        $"stride={mesh.Stride}/{mesh.RuntimeStride}, skin={mesh.HasSkinningData}, " +
                        $"texture={textureName}, UV=({minU:G5}..{maxU:G5}, {minV:G5}..{maxV:G5}), " +
                        $"worldT=({worldTransform.M41:G5},{worldTransform.M42:G5}," +
                        $"{worldTransform.M43:G5}), center={rawCenter}->{transformedCenter}");
                    int degenerateUvTriangles = 0;
                    for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
                    {
                        var a = mesh.TextureCoordinates[checked((int)mesh.TriangleIndices[index])];
                        var b = mesh.TextureCoordinates[checked((int)mesh.TriangleIndices[index + 1])];
                        var c = mesh.TextureCoordinates[checked((int)mesh.TriangleIndices[index + 2])];
                        float area = (b.X - a.X) * (c.Y - a.Y) -
                                     (b.Y - a.Y) * (c.X - a.X);
                        if (MathF.Abs(area) < 0.0000001f)
                            degenerateUvTriangles++;
                    }
                    Console.WriteLine(
                        $"  triangles={mesh.TriangleCount}, degenerate UV={degenerateUvTriangles}");
                    var chain = new List<string>();
                    SmoObjectEntry? cursor = entry;
                    while (cursor is not null)
                    {
                        chain.Add(
                            $"[{cursor.Index}]{SmoClassRegistry.GetDisplayName(cursor.TypeHash)}:{cursor.Name}");
                        cursor = cursor.ParentIndex is int parentIndex
                            ? document.Objects[parentIndex]
                            : null;
                    }
                    Console.WriteLine($"  owners={string.Join(" <- ", chain)}");
                }
            }
        }

        if (!sampleCount.HasValue && !singleFile)
        {
            CheckKnownFile(corpusPath, "fish.smo", expectedObjects: 18, expectedMeshes: 1);
            CheckKnownFile(corpusPath, "bloom_ball.smo", expectedObjects: 123, expectedMeshes: 6);
            CheckKnownFile(corpusPath, "loading.smo", expectedObjects: 11, expectedMeshes: 2);
            CheckKnownFile(corpusPath, "menu.smo", expectedObjects: 238, expectedMeshes: 41);
            CheckE0FinalIndices(corpusPath);
        }

        Console.WriteLine(
            $"Corpus: {parsed}/{allFiles.Length} SMO files, {objects} object-directory entries, " +
            $"decoded textures: {decodedTextures}, resolved bindings: {resolvedBindings}, " +
            $"meshes: {decodedMeshes}, UV meshes: {uvMeshes}, " +
            $"renderable textured meshes: {renderableTexturedMeshes}, " +
            $"transformed model meshes: {transformedModelMeshes}, " +
            $"static transforms: {decodedStaticTransforms}, " +
            $"material colors: {resolvedMaterialColors}, " +
            $"vertex-colored meshes: {vertexColoredMeshes}, " +
            $"meshes under static objects: {meshesUnderStaticObjects}, " +
            $"without static objects: {meshesWithoutStaticObjects}");
        foreach (string sample in unplacedSamples)
            Console.WriteLine($"  unplaced: {sample}");
        foreach (string file in files)
            Console.WriteLine($"  {(singleFile ? Path.GetFileName(file) : Path.GetRelativePath(corpusPath, file))}");

        if (singleFile)
            CheckNativeAlphaDecalDepthStructuralContract(corpusPath);

        if (singleFile && Path.GetFileName(corpusPath).Equals(
                "igmenu_opt_pc.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckGuiOptionsMenu(corpusPath);
        }
        if (singleFile && Path.GetFileName(corpusPath).Equals(
                "gameover.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckGameOverLayout(corpusPath);
        }
        if (singleFile && Path.GetFileName(corpusPath).Equals(
                "Alfea02.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckAlfea02PartitionTransforms(corpusPath);
        }
        if (singleFile && Path.GetFileName(corpusPath).Equals(
                "Alfea01.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckAlfea01FoliageTexture(corpusPath);
        }
        if (singleFile && Path.GetFileName(corpusPath).Equals(
                "Alfea03.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckAlfea03SharedMeshInstances(corpusPath);
        }
        if (singleFile && Path.GetFileName(corpusPath).Equals(
                "Alfea_broken_01.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckAlfeaBroken01Materials(corpusPath);
        }

        if (singleFile && Path.GetFileName(corpusPath).Equals(
                "bloom_school.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckBloomSchool(corpusPath);
        }
        else if (singleFile && Path.GetFileName(corpusPath).Equals(
                     "bloom_princess.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckBloomPrincessTransparentSurface(corpusPath);
        }
        else if (singleFile && Path.GetFileName(corpusPath).Equals(
                     "knutBoss.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckKnutBossShieldTransparency(corpusPath);
        }
        else if (singleFile && Path.GetFileName(corpusPath).Equals(
                     "Iceworm.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckSkinnedAlphaFixture(
                corpusPath,
                materialIndex: 24,
                skinIndex: 23,
                meshIndex: 25,
                fixtureName: "IceWorm");
        }
        else if (singleFile && Path.GetFileName(corpusPath).Equals(
                     "Yeti.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckSkinnedAlphaFixture(
                corpusPath,
                materialIndex: 94,
                skinIndex: 93,
                meshIndex: 95,
                fixtureName: "Yeti");
        }
        else if (singleFile && Path.GetFileName(corpusPath).Equals(
                     "firefly.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckEffectBlendFixture(
                corpusPath, 0x4, [20, 24], 4, "firefly op4");
        }
        else if (singleFile && Path.GetFileName(corpusPath).Equals(
                     "Ui_target.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckEffectBlendFixture(
                corpusPath, 0x4, [3], 2, "Ui_target op4 companion 2");
        }
        else if (singleFile && Path.GetFileName(corpusPath).Equals(
                     "bloom_projectile_01.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckEffectBlendFixture(
                corpusPath, 0x5, [4], 2, "Bloom projectile op5");
        }
        else if (singleFile && Path.GetFileName(corpusPath).Equals(
                     "Droid.smo", StringComparison.OrdinalIgnoreCase))
        {
            CheckSkinnedEffectCounterexample(corpusPath);
        }
        else if (singleFile && Path.GetFileName(corpusPath).Equals(
                     "bloom_jeans_skinned_Model_alpha_surface_rs8_fixed.smo",
                     StringComparison.OrdinalIgnoreCase))
        {
            CheckGeneratedSkinnedAlphaFixture(
                corpusPath, expectedCompanionState: 4);
        }
        else if (singleFile && Path.GetFileName(corpusPath).Equals(
                     "bloom_jeans_skinned_Model_alpha_tuple_fixed.smo",
                     StringComparison.OrdinalIgnoreCase))
        {
            CheckGeneratedSkinnedAlphaFixture(
                corpusPath, expectedCompanionState: 2);
        }
        else if (singleFile && Path.GetFileName(corpusPath).Equals(
                     "bloom_jeans_skinned_Model.smo",
                     StringComparison.OrdinalIgnoreCase))
        {
            // The GUI overwrites this path on every user run. Its native-risk
            // regression is intentionally routed by generated mesh structure
            // above, never by this mutable file's SHA-256.
        }
        else if (singleFile && Path.GetFileName(corpusPath).Equals(
                     "bloom_jeans_layla_princess_alpha_20260821.smo",
                     StringComparison.OrdinalIgnoreCase))
        {
            CheckPrincessAlphaRunGranularityFixture(
                corpusPath,
                expectedSha256Prefix: "EEDC",
                expectedRunCount: 1,
                expectedWarningCount: 1,
                fixtureName: "EEDC combined alpha run");
        }
        else if (singleFile && Path.GetFileName(corpusPath).Equals(
                     "bloom_jeans_layla_princess_alpha_runs5_20260821.smo",
                     StringComparison.OrdinalIgnoreCase))
        {
            CheckPrincessAlphaRunGranularityFixture(
                corpusPath,
                expectedSha256Prefix: "D87B",
                expectedRunCount: 5,
                expectedWarningCount: 0,
                fixtureName: "D87B five source alpha runs");
        }
        else if (singleFile)
        {
            CheckSelectedCharacterBindings(corpusPath);
        }
    }

    private static void CheckPrincessAlphaRunGranularityFixture(
        string path,
        string expectedSha256Prefix,
        int expectedRunCount,
        int expectedWarningCount,
        string fixtureName)
    {
        string sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
        True(sha256.StartsWith(expectedSha256Prefix, StringComparison.Ordinal),
            $"{fixtureName} SHA-256 fixture identity");

        SmoDocument document = SmoDocument.Load(path);
        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        IReadOnlyDictionary<int, SmoMaterialRenderStateInfo> localRenderStates =
            SmoMaterialRenderState.ResolveDetailed(document);
        var runs = new List<(
            SmoMesh Mesh,
            SmoAlphaRunGranularityInfo Granularity)>();
        var nonPrincessSkinnedRuns = new List<SmoAlphaRunGranularityInfo>();

        foreach (SmoObjectEntry entry in document.Objects.Where(
                     item => item.TypeHash == SmoClassIds.MeshData))
        {
            if (!SmoMeshDecoder.TryDecode(
                    document, entry, out SmoMesh? mesh, out _) ||
                mesh is null)
            {
                continue;
            }
            bindings.TryGetValue(entry.Index, out SmoTextureBinding? binding);
            SmoMaterialRenderStateInfo? rawState =
                localRenderStates.GetValueOrDefault(entry.Index) ??
                binding?.MaterialRenderState;
            if (rawState is null)
                continue;

            int? skinIndex = FindAncestorObjectIndex(
                document.Objects, entry, SmoClassIds.Skin);
            SmoSkin? skin = null;
            if (!skinIndex.HasValue ||
                !SmoSkinDecoder.TryDecode(
                    document,
                    document.Objects[skinIndex.Value],
                    out skin,
                    out _) ||
                skin is null)
            {
                continue;
            }

            SmoMaterialRenderStateInfo state =
                SmoMaterialRenderState.BindToRenderable(
                    rawState, mesh, skin, binding?.Texture);
            SmoAlphaRunGranularityInfo granularity =
                SmoAlphaRunGranularityAnalyzer.Analyze(mesh, state, skin);
            if (!state.HasConfirmedPrincessTransparentSurfaceState)
            {
                if (mesh.HasSkinningData)
                    nonPrincessSkinnedRuns.Add(granularity);
                continue;
            }

            runs.Add((mesh, granularity));
            Console.WriteLine(
                $"{fixtureName}: mesh [{mesh.ObjectIndex}] {mesh.Name}; " +
                $"triangles={mesh.TriangleCount}; " +
                $"components={granularity.GeometryComponentCount}/" +
                $"{granularity.SignificantComponentCount}; " +
                $"gap={granularity.MaximumComponentGap:G5}; " +
                $"gap/bounds={granularity.MaximumComponentGapFraction:P1}; " +
                $"gap/median={granularity.MaximumGapToMedianComponentSize:G5}; " +
                $"targets={granularity.ActiveDeformTargetCount}; " +
                $"warning={granularity.HasSuspiciousCombinedRun}");
        }

        Equal(expectedRunCount, runs.Count,
            $"{fixtureName} confirmed princess alpha run count");
        Equal(expectedWarningCount,
            runs.Count(item => item.Granularity.HasSuspiciousCombinedRun),
            $"{fixtureName} alpha granularity warning count");
        True(runs.All(item => item.Granularity.IsApplicable),
            $"{fixtureName} analyzer applicability");
        True(runs.Where(item => item.Granularity.HasSuspiciousCombinedRun)
                .All(item => item.Granularity.Diagnostic?.Contains(
                    "WPF preview does not confirm native alpha sorting",
                    StringComparison.Ordinal) == true),
            $"{fixtureName} warning states the WPF/native boundary");
        True(runs.Where(item => item.Granularity.HasSuspiciousCombinedRun)
                .All(item => item.Granularity.Diagnostic?.Contains(
                    "not proof of a rendering cause",
                    StringComparison.Ordinal) == true),
            $"{fixtureName} warning avoids an unsupported causal claim");
        True(runs.Where(item => !item.Granularity.HasSuspiciousCombinedRun)
                .All(item => item.Granularity.Diagnostic is null),
            $"{fixtureName} ordinary runs have no causal diagnostic");
        True(nonPrincessSkinnedRuns.Count > 0 &&
             nonPrincessSkinnedRuns.All(item =>
                 !item.IsApplicable &&
                 !item.HasSuspiciousCombinedRun &&
                 item.Diagnostic is null),
            $"{fixtureName} opaque/non-princess skinned runs stay untouched");
    }

    private static void CheckNativeAlphaDecalDepthStructuralContract(
        string path)
    {
        IReadOnlyList<(
            SmoMesh Mesh,
            SmoAlphaDecalDepthInfo Depth)> runs =
            AnalyzeNativeAlphaDecalDepth(path);
        string fixtureName = Path.GetFileName(path);

        foreach ((SmoMesh mesh, SmoAlphaDecalDepthInfo depth) in runs)
        {
            Console.WriteLine(
                $"{fixtureName}: mesh [{mesh.ObjectIndex}] {mesh.Name}; " +
                $"candidate/body={depth.CandidateBoundsDiagonalFraction:P2}; " +
                $"threshold={depth.NearSurfaceThreshold:G5}; " +
                $"median={depth.MedianNearestSurfaceDistance:G5}; " +
                $"nearParallel={depth.NearParallelVertexCount}/" +
                $"{depth.CandidateVertexCount} " +
                $"({depth.NearParallelVertexFraction:P1}); " +
                $"warning={depth.HasNearCoplanarDepthRisk}");
        }

        True(runs.All(item => item.Depth.IsApplicable),
            $"{fixtureName} analyzer applicability");
        True(runs.Where(item => item.Depth.HasNearCoplanarDepthRisk)
                .All(item => item.Depth.Diagnostic?.Contains(
                    "NATIVE_ALPHA_DECAL_DEPTH_UNCONFIRMED",
                    StringComparison.Ordinal) == true),
            $"{fixtureName} warning has stable diagnostic code");
        True(runs.Where(item => item.Depth.HasNearCoplanarDepthRisk)
                .All(item => item.Depth.Diagnostic?.Contains(
                    "sampled texture alpha is forced to 0",
                    StringComparison.Ordinal) == true),
            $"{fixtureName} warning labels the overlay-loss simulation");
        True(runs.Where(item => item.Depth.HasNearCoplanarDepthRisk)
                .All(item => item.Depth.Diagnostic?.Contains(
                    "face-overlay draw were rejected completely",
                    StringComparison.Ordinal) == true),
            $"{fixtureName} warning states the simulated failure mode");
        True(runs.Where(item => item.Depth.HasNearCoplanarDepthRisk)
                .All(item => item.Depth.Diagnostic?.Contains(
                    "not an emulation of the game's blend equation",
                    StringComparison.Ordinal) == true),
            $"{fixtureName} warning states the WPF/native boundary");
        True(runs.Where(item => !item.Depth.HasNearCoplanarDepthRisk)
                .All(item => item.Depth.Diagnostic is null),
            $"{fixtureName} non-decal alpha runs remain unchanged");

        // The GUI overwrites its normal output path on every run, so this
        // regression recognizes the generated five-run split by authored
        // structure rather than by filename, SHA-256, or object indices.
        SmoDocument document = SmoDocument.Load(path);
        SmoMesh[] generatedMeshes = document.Objects
            .Where(entry =>
                entry.TypeHash == SmoClassIds.MeshData &&
                (entry.Name.StartsWith("imp_a_x_", StringComparison.Ordinal) ||
                 entry.Name.StartsWith("imp_o_x_", StringComparison.Ordinal)))
            .Select(entry => SmoMeshDecoder.Decode(document, entry))
            .ToArray();
        int[] generatedTriangleCounts = generatedMeshes
            .Select(mesh => mesh.TriangleCount)
            .Order()
            .ToArray();
        int[] expectedGeneratedTriangleCounts = [2, 34, 34, 52, 72];
        if (!generatedTriangleCounts.SequenceEqual(expectedGeneratedTriangleCounts))
            return;

        HashSet<int> generatedMeshIndices = generatedMeshes
            .Select(mesh => mesh.ObjectIndex)
            .ToHashSet();
        (SmoMesh Mesh, SmoAlphaDecalDepthInfo Depth)[] generatedRuns = runs
            .Where(item => generatedMeshIndices.Contains(item.Mesh.ObjectIndex))
            .ToArray();
        int[] princessTriangleCounts = generatedRuns
            .Select(item => item.Mesh.TriangleCount)
            .Order()
            .ToArray();
        int[] flaggedTriangleCounts = generatedRuns
            .Where(item => item.Depth.HasNearCoplanarDepthRisk)
            .Select(item => item.Mesh.TriangleCount)
            .Order()
            .ToArray();

        FixtureRenderable[] boundGenerated = BindFixtureRenderables(document)
            .Where(item => generatedMeshIndices.Contains(item.Mesh.ObjectIndex))
            .ToArray();
        FixtureRenderable[] exactW2Runs = boundGenerated
            .Where(item => item.State
                .HasConfirmedSkinnedTransparentSurfaceState)
            .ToArray();
        int[] exactW2TriangleCounts = exactW2Runs
            .Select(item => item.Mesh.TriangleCount)
            .Order()
            .ToArray();

        bool legacyPartialAlphaFaceRuns = princessTriangleCounts.SequenceEqual(
            expectedGeneratedTriangleCounts);
        bool opaqueFaceOverlayCandidate = princessTriangleCounts.SequenceEqual(
            new[] { 2, 34, 34 });
        bool exactW2AlphaSplit = exactW2TriangleCounts.SequenceEqual(
            new[] { 2, 34, 34 });
        True(legacyPartialAlphaFaceRuns ||
             opaqueFaceOverlayCandidate ||
             exactW2AlphaSplit,
            $"{fixtureName} generated split has either legacy op2 face runs " +
            "or production opaque face overlays with legacy/W2 alpha details");

        SmoMesh[] faceOverlayMeshes = generatedMeshes
            .Where(mesh => mesh.TriangleCount is 52 or 72)
            .ToArray();
        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);

        if (legacyPartialAlphaFaceRuns)
        {
            True(faceOverlayMeshes.All(mesh =>
                    mesh.Name.StartsWith("imp_a_x_", StringComparison.Ordinal) &&
                    bindings.TryGetValue(
                        mesh.ObjectIndex, out SmoTextureBinding? binding) &&
                    binding.MaterialRenderState?.FinalBlendOperation == 0x2),
                $"{fixtureName} legacy eye/mouth runs remain exact op2 overlays");
            True(flaggedTriangleCounts.SequenceEqual(new[] { 52, 72 }),
                $"{fixtureName} only generated eye/mouth runs are native-risk " +
                $"approximations; actual [{string.Join(", ", flaggedTriangleCounts)}]");
            Equal(2, generatedRuns.Count(item =>
                    item.Depth.HasNearCoplanarDepthRisk),
                $"{fixtureName} exact near-coplanar face risk count");
        }
        else
        {
            True(faceOverlayMeshes.All(mesh =>
                    mesh.Name.StartsWith("imp_o_x_", StringComparison.Ordinal) &&
                    bindings.TryGetValue(
                        mesh.ObjectIndex, out SmoTextureBinding? binding) &&
                    binding.MaterialRenderState?.FinalBlendOperation == 0x0),
                $"{fixtureName} eye/mouth runs are exact opaque op0 overlays");
            Equal(0, generatedRuns.Count(item =>
                    item.Depth.HasNearCoplanarDepthRisk),
                $"{fixtureName} opaque face-overlay candidate has no face-risk warning");

            if (exactW2AlphaSplit)
            {
                True(exactW2Runs.All(item =>
                        item.Mesh.Name.StartsWith(
                            "imp_a_x_", StringComparison.Ordinal) &&
                        item.State.FinalBlendOperation == 0x2 &&
                        item.State.MaterialRenderStates.SequenceEqual(
                            new uint[] { 0, 0, 1, 0, 1, 1, 3, 0, 4, 0, 6 }) &&
                        item.Skin?.AlphaSortEnable == 1 &&
                        item.Skin.Priority == 1 &&
                        item.State.VertexDiffuseProfile ==
                            SmoVertexDiffuseProfile.UniformOpaqueBlack &&
                        item.State.TextureUvAlphaCoverage.HasPartialAlpha &&
                        item.State.BlendMode == SmoMaterialBlendMode
                            .SkinnedTransparentSurfaceFinalBlend2 &&
                        item.State.UsesAlphaBlend &&
                        item.State.RequiresTransparentOrdering &&
                        !item.State.UsesEmissiveApproximation &&
                        item.State.Diagnostic is null),
                    $"{fixtureName} W2 alpha detail runs use the exact " +
                    "confirmed non-emissive contract");
                True(faceOverlayMeshes.All(mesh =>
                        SmoMaterialRenderState.ClassifyVertexDiffuse(mesh) ==
                            SmoVertexDiffuseProfile.UniformOpaqueWhite),
                    $"{fixtureName} W2 opaque face overlays use white vertex diffuse");
            }
        }
    }

    private static IReadOnlyList<(
        SmoMesh Mesh,
        SmoAlphaDecalDepthInfo Depth)> AnalyzeNativeAlphaDecalDepth(
        string path)
    {
        SmoDocument document = SmoDocument.Load(path);
        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        IReadOnlyDictionary<int, SmoMaterialRenderStateInfo> localRenderStates =
            SmoMaterialRenderState.ResolveDetailed(document);
        Dictionary<int, SmoSkin> skins = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.Skin)
            .Select(entry => SmoSkinDecoder.TryDecode(
                document, entry, out SmoSkin? skin, out _) ? skin : null)
            .Where(skin => skin is not null)
            .Cast<SmoSkin>()
            .ToDictionary(skin => skin.ObjectIndex);
        var renderables = new List<(
            SmoMesh Mesh,
            SmoMaterialRenderStateInfo State,
            Matrix4x4 WorldTransform)>();

        foreach (SmoObjectEntry entry in document.Objects.Where(
                     item => item.TypeHash == SmoClassIds.MeshData))
        {
            if (!SmoMeshDecoder.TryDecode(
                    document, entry, out SmoMesh? mesh, out _) ||
                mesh is null)
            {
                continue;
            }
            bindings.TryGetValue(entry.Index, out SmoTextureBinding? binding);
            SmoMaterialRenderStateInfo? rawState =
                localRenderStates.GetValueOrDefault(entry.Index) ??
                binding?.MaterialRenderState;
            int? skinIndex = FindAncestorObjectIndex(
                document.Objects, entry, SmoClassIds.Skin);
            if (rawState is null ||
                skinIndex is not int resolvedSkinIndex ||
                !skins.TryGetValue(resolvedSkinIndex, out SmoSkin? skin))
            {
                continue;
            }

            SmoMaterialRenderStateInfo state =
                SmoMaterialRenderState.BindToRenderable(
                    rawState, mesh, skin, binding?.Texture);
            renderables.Add((
                mesh,
                state,
                SmoNodeTransformDecoder.ResolveModelWorldMatrix(
                    document, entry)));
        }

        SmoAlphaDecalOpaqueSurface[] opaqueSurfaces = renderables
            .Where(item =>
                !item.State.UsesAlphaBlend &&
                item.Mesh.HasSkinningData &&
                item.Mesh.HasNormals &&
                item.Mesh.TriangleCount > 0)
            .Select(item => new SmoAlphaDecalOpaqueSurface(
                item.Mesh, item.WorldTransform))
            .ToArray();

        return renderables
            .Where(item =>
                item.State.HasConfirmedPrincessTransparentSurfaceState)
            .Select(item => (
                item.Mesh,
                SmoAlphaDecalDepthAnalyzer.Analyze(
                    item.Mesh,
                    item.State,
                    item.WorldTransform,
                    opaqueSurfaces)))
            .ToArray();
    }

    private static void CheckBloomPrincessTransparentSurface(string path)
    {
        SmoDocument document = SmoDocument.Load(path);
        const int textureIndex = 5;
        const int mainMaterialIndex = 27;
        const int mainSkinIndex = 26;
        const int mainMeshIndex = 28;
        const int tiaraSkinIndex = 81;
        const int tiaraMaterialIndex = 82;
        const int tiaraMeshIndex = 83;

        Equal("b_prince", document.Objects[textureIndex].Name,
            "Bloom princess shared atlas object");
        Equal("Bloom_princess_mesh_1_2899.387222",
            document.Objects[tiaraMeshIndex].Name,
            "Bloom princess tiara mesh identity");

        SmoMesh tiara = SmoMeshDecoder.Decode(
            document, document.Objects[tiaraMeshIndex]);
        True(tiara.HasSkinningData && tiara.HasNormals &&
             tiara.HasTextureCoordinates,
            "Bloom princess tiara is a skinned UV surface");
        Equal(SmoVertexDiffuseProfile.UniformOpaqueBlack,
            SmoMaterialRenderState.ClassifyVertexDiffuse(tiara),
            "Bloom princess tiara vertex diffuse profile");

        True(SmoSkinDecoder.TryDecode(
                document,
                document.Objects[tiaraSkinIndex],
                out SmoSkin? tiaraSkin,
                out string tiaraSkinError),
            $"Bloom princess tiara spSkin decodes: {tiaraSkinError}");
        Equal((uint)0,
            tiaraSkin!.AlphaSortEnable.GetValueOrDefault(uint.MaxValue),
            "Bloom princess tiara AlphaSortEnable");
        Equal((uint)1,
            tiaraSkin.Priority.GetValueOrDefault(uint.MaxValue),
            "Bloom princess tiara Priority");
        Equal("Head",
            document.Objects[tiaraSkin.Bones[14].NodeObjectIndex].Name,
            "Bloom princess tiara effective Head palette slot");
        True(tiara.BlendWeights.Zip(tiara.BlendIndices).All(item =>
                (item.First.X <= 0.000001f || item.Second.X == 14) &&
                (item.First.Y <= 0.000001f || item.Second.Y == 14) &&
                (item.First.Z <= 0.000001f || item.Second.Z == 14) &&
                (item.First.W <= 0.000001f || item.Second.W == 14)),
            "Bloom princess tiara positive weights use only Head slot 14");

        True(SmoMaterialRenderState.TryDecode(
                document,
                document.Objects[tiaraMaterialIndex],
                out SmoMaterialRenderStateInfo? rawTiaraState),
            "Bloom princess tiara material state decodes");
        Equal((uint)0x2, rawTiaraState!.FinalBlendOperation,
            "Bloom princess tiara exact FinalBlendOp");
        True(rawTiaraState.MaterialRenderStates.SequenceEqual(
                new uint[] { 0, 0, 1, 0, 1, 0, 3, 0, 4, 0, 6 }),
            "Bloom princess tiara exact MaterialRenderStates");
        Equal(SmoMaterialBlendMode.OpaqueFinalBlend2,
            rawTiaraState.BlendMode,
            "Bloom princess tiara raw op2 is not globally classified alpha");

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        True(bindings.TryGetValue(
                tiaraMeshIndex, out SmoTextureBinding? tiaraBinding) &&
             tiaraBinding.Texture is not null,
            "Bloom princess tiara resolves its shared b_prince atlas");
        Equal("b_prince", tiaraBinding!.Texture!.Name,
            "Bloom princess tiara shared texture binding");

        ReadOnlySpan<byte> pixels = tiaraBinding.Texture.Bgra32Pixels.Span;
        int textureA0 = 0;
        int texturePartial = 0;
        int textureA255 = 0;
        for (int offset = 3; offset < pixels.Length; offset += 4)
        {
            if (pixels[offset] == 0)
                textureA0++;
            else if (pixels[offset] == byte.MaxValue)
                textureA255++;
            else
                texturePartial++;
        }
        Equal(370, textureA0,
            "Bloom princess b_prince fully transparent texels");
        Equal(15508, texturePartial,
            "Bloom princess b_prince partial-alpha texels");
        Equal(49658, textureA255,
            "Bloom princess b_prince opaque texels");

        SmoTextureUvAlphaCoverage tiaraCoverage =
            SmoTextureUvAlphaAnalyzer.Analyze(tiara, tiaraBinding.Texture);
        Console.WriteLine(
            $"Bloom princess tiara UV alpha: {tiaraCoverage}");
        Equal(3948, tiaraCoverage.SampledTexelCount,
            "Bloom princess tiara UV-covered texel count");
        Equal(53, tiaraCoverage.FullyTransparentTexelCount,
            "Bloom princess tiara UV-covered alpha-zero texels");
        Equal(3746, tiaraCoverage.PartialAlphaTexelCount,
            "Bloom princess tiara UV-covered partial-alpha texels");
        Equal(149, tiaraCoverage.OpaqueTexelCount,
            "Bloom princess tiara UV-covered opaque texels");
        True(tiaraCoverage.IsReliable &&
             tiaraCoverage.FullyTransparentTexelCount > 0 &&
             tiaraCoverage.PartialAlphaTexelCount > 0 &&
             tiaraCoverage.OpaqueTexelCount > 0,
            "Bloom princess tiara UV triangles cover zero, partial and opaque alpha");

        SmoMaterialRenderStateInfo tiaraState =
            SmoMaterialRenderState.BindToRenderable(
                rawTiaraState, tiara, tiaraSkin, tiaraBinding.Texture);
        Equal(
            SmoMaterialBlendMode.PrincessTransparentSurfaceFinalBlend2,
            tiaraState.BlendMode,
            "Bloom princess tiara bound consumer classification");
        True(tiaraState.HasPrincessTransparentSurfaceContext &&
             tiaraState.HasConfirmedPrincessTransparentSurfaceState &&
             tiaraState.UsesAlphaBlend &&
             tiaraState.RequiresTransparentOrdering,
            "Bloom princess tiara is a confirmed transparent op2 surface");
        True(!tiaraState.UsesEmissiveApproximation &&
             tiaraState.Diagnostic is null,
            "Bloom princess tiara uses authored texture alpha without glow warning");

        foreach (int opaqueSiblingMeshIndex in new[] { 85, 87, 89 })
        {
            SmoMesh sibling = SmoMeshDecoder.Decode(
                document, document.Objects[opaqueSiblingMeshIndex]);
            int siblingSkinIndex = document.Objects[opaqueSiblingMeshIndex]
                .ParentIndex!.Value;
            True(SmoSkinDecoder.TryDecode(
                    document,
                    document.Objects[siblingSkinIndex],
                    out SmoSkin? siblingSkin,
                    out string siblingSkinError),
                $"Bloom princess sibling [{opaqueSiblingMeshIndex}] skin decodes: " +
                siblingSkinError);
            True(bindings.TryGetValue(
                    opaqueSiblingMeshIndex,
                    out SmoTextureBinding? siblingBinding) &&
                 siblingBinding.Texture is not null &&
                 siblingBinding.MaterialRenderState is not null,
                $"Bloom princess sibling [{opaqueSiblingMeshIndex}] binding resolves");
            SmoTextureUvAlphaCoverage siblingCoverage =
                SmoTextureUvAlphaAnalyzer.Analyze(
                    sibling, siblingBinding!.Texture!);
            True(siblingCoverage.IsReliable &&
                 siblingCoverage.PartialAlphaTexelCount > 0,
                $"Bloom princess sibling [{opaqueSiblingMeshIndex}] covers partial alpha");
            Equal(0, siblingCoverage.FullyTransparentTexelCount,
                $"Bloom princess sibling [{opaqueSiblingMeshIndex}] covers no alpha-zero texels");
            SmoMaterialRenderStateInfo siblingState =
                SmoMaterialRenderState.BindToRenderable(
                    siblingBinding.MaterialRenderState!,
                    sibling,
                    siblingSkin,
                    siblingBinding.Texture);
            Equal(SmoMaterialBlendMode.OpaqueFinalBlend2,
                siblingState.BlendMode,
                $"Bloom princess sibling [{opaqueSiblingMeshIndex}] remains opaque");
            True(!siblingState.UsesAlphaBlend &&
                 !siblingState.RequiresTransparentOrdering,
                $"Bloom princess sibling [{opaqueSiblingMeshIndex}] avoids transparent pass");
        }

        SmoAlphaRunGranularityInfo tiaraGranularity =
            SmoAlphaRunGranularityAnalyzer.Analyze(
                tiara, tiaraState, tiaraSkin);
        True(tiaraGranularity.IsApplicable &&
             !tiaraGranularity.HasSuspiciousCombinedRun &&
             tiaraGranularity.Diagnostic is null,
            "Bloom princess authored tiara run has no aggregation warning");

        SmoMaterialRenderStateInfo tiaraWithoutTexture =
            SmoMaterialRenderState.BindToRenderable(
                rawTiaraState, tiara, tiaraSkin);
        Equal(SmoMaterialBlendMode.OpaqueFinalBlend2,
            tiaraWithoutTexture.BlendMode,
            "Bloom princess op2 needs real resolved UV/texture evidence");

        SmoMesh mainMesh = SmoMeshDecoder.Decode(
            document, document.Objects[mainMeshIndex]);
        True(SmoSkinDecoder.TryDecode(
                document,
                document.Objects[mainSkinIndex],
                out SmoSkin? mainSkin,
                out string mainSkinError),
            $"Bloom princess main spSkin decodes: {mainSkinError}");
        True(SmoMaterialRenderState.TryDecode(
                document,
                document.Objects[mainMaterialIndex],
                out SmoMaterialRenderStateInfo? mainRawState),
            "Bloom princess main material state decodes");
        SmoMaterialRenderStateInfo mainState =
            SmoMaterialRenderState.BindToRenderable(
                mainRawState!, mainMesh, mainSkin, tiaraBinding.Texture);
        Equal(SmoMaterialBlendMode.OpaqueFinalBlend0, mainState.BlendMode,
            "shared texture partial alpha does not reclassify main op0 surface");
        True(!mainState.HasConfirmedPrincessTransparentSurfaceState,
            "princess classification remains consumer/material specific");

        IReadOnlyList<(
            SmoMesh Mesh,
            SmoAlphaDecalDepthInfo Depth)> nativeDepthRuns =
            AnalyzeNativeAlphaDecalDepth(path);
        Equal(0, nativeDepthRuns.Count(item =>
                item.Depth.HasNearCoplanarDepthRisk),
            "Bloom princess authored op2 surfaces have no native face-decal risk warning");
        (SmoMesh Mesh, SmoAlphaDecalDepthInfo Depth) tiaraDepth =
            nativeDepthRuns.Single(item =>
                item.Mesh.ObjectIndex == tiaraMeshIndex);
        True(tiaraDepth.Depth.IsApplicable &&
             !tiaraDepth.Depth.HasNearCoplanarDepthRisk &&
             tiaraDepth.Depth.Diagnostic is null &&
             tiaraDepth.Depth.MedianNearestSurfaceDistance >
             tiaraDepth.Depth.NearSurfaceThreshold,
            "Bloom princess tiara stays outside the near-coplanar approximation");
    }

    private static void CheckKnutBossShieldTransparency(string path)
    {
        SmoDocument document = SmoDocument.Load(path);
        SmoObjectEntry shieldEntry = document.Objects[13];
        Equal(SmoClassIds.MeshData, shieldEntry.TypeHash,
            "knutBoss shield mesh object");

        SmoMesh shield = SmoMeshDecoder.Decode(document, shieldEntry);
        True(shield.HasTextureCoordinates,
            "knutBoss shield UV channel");

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        True(bindings.TryGetValue(13, out SmoTextureBinding? binding),
            "knutBoss shield texture binding");
        Equal("gr_01", binding!.Texture!.Name,
            "knutBoss shield texture name");

        ReadOnlySpan<byte> pixels = binding.Texture.Bgra32Pixels.Span;
        int transparent = 0;
        int translucent = 0;
        int opaque = 0;
        for (int offset = 3; offset < pixels.Length; offset += 4)
        {
            switch (pixels[offset])
            {
                case 0:
                    transparent++;
                    break;
                case byte.MaxValue:
                    opaque++;
                    break;
                default:
                    translucent++;
                    break;
            }
        }

        Console.WriteLine(
            $"knutBoss shield [13] gr_01 alpha: transparent={transparent}, " +
            $"translucent={translucent}, opaque={opaque}");
        True(transparent > 0,
            "knutBoss shield texture has transparent pixels");
        True(translucent > 0,
            "knutBoss shield texture has translucent pixels");

        True(SmoMaterialRenderState.TryDecode(
                document,
                document.Objects[10],
                out SmoMaterialRenderStateInfo? shieldState),
            "knutBoss shield material render state decodes");
        Equal((uint)0x6, shieldState!.FinalBlendOperation,
            "knutBoss shield exact FinalBlendOp");
        Equal(SmoMaterialBlendMode.FinalBlend6Companion2,
            shieldState.BlendMode,
            "knutBoss shield exact companion tuple family");
        True(shieldState.MaterialRenderStates.SequenceEqual(
                new uint[] { 0, 0, 1, 0, 1, 0, 3, 0, 2, 0, 6 }),
            "knutBoss shield exact MaterialRenderStates");
        SmoMaterialRenderStateInfo rigidShieldState = shieldState.ForConsumer(
            SmoMaterialConsumerKind.RigidOrEffect);
        True(rigidShieldState.HasConfirmedConsumerTuple &&
             !rigidShieldState.HasConsumerStateMismatch,
            "knutBoss shield has the confirmed rigid/effect tuple");

        True(SmoMaterialRenderState.TryDecode(
                document,
                document.Objects[26],
                out SmoMaterialRenderStateInfo? bodyState),
            "knutBoss body material render state decodes");
        Equal((uint)0x2, bodyState!.FinalBlendOperation,
            "knutBoss body exact FinalBlendOp");
        Equal(SmoMaterialBlendMode.OpaqueFinalBlend2, bodyState.BlendMode,
            "knutBoss body opaque tuple family");
        True(binding.UsesAlphaBlend,
            "knutBoss shield binding carries alpha-blend render state");
        Equal(SmoMaterialBlendMode.FinalBlend6Companion2,
            binding.MaterialRenderState!.BlendMode,
            "knutBoss shield binding carries the detailed render state");
        True(bindings.TryGetValue(28, out SmoTextureBinding? bodyBinding) &&
             !bodyBinding.UsesAlphaBlend,
            "knutBoss body binding stays in the opaque render pass");
        Equal(SmoMaterialBlendMode.OpaqueFinalBlend2,
            bodyBinding!.MaterialRenderState!.BlendMode,
            "knutBoss body binding carries the detailed opaque state");

        SmoObjectEntry bodyEntry = document.Objects[28];
        SmoMesh bodyMesh = SmoMeshDecoder.Decode(document, bodyEntry);
        int? bodySkinIndex = FindAncestorObjectIndex(
            document.Objects, bodyEntry, SmoClassIds.Skin);
        SmoSkin? bodySkin = null;
        bool bodySkinDecoded = bodySkinIndex.HasValue && SmoSkinDecoder.TryDecode(
                document,
                document.Objects[bodySkinIndex.Value],
                out bodySkin,
                out _);
        True(bodySkinDecoded,
            "knutBoss body consuming spSkin decodes");
        SmoMaterialRenderStateInfo boundBodyState =
            SmoMaterialRenderState.BindToRenderable(
                bodyBinding.MaterialRenderState,
                bodyMesh,
                bodySkin,
                bodyBinding.Texture);
        Equal(SmoMaterialBlendMode.OpaqueFinalBlend2,
            boundBodyState.BlendMode,
            "knutBoss body op2 stays opaque despite texture alpha");
        True(!boundBodyState.HasConfirmedPrincessTransparentSurfaceState &&
             !boundBodyState.UsesAlphaBlend,
            "non-princess op2 corpus counterexample avoids alpha false positive");
    }

    private static void CheckSkinnedAlphaFixture(
        string path,
        int materialIndex,
        int skinIndex,
        int meshIndex,
        string fixtureName)
    {
        SmoDocument document = SmoDocument.Load(path);
        SmoMesh mesh = SmoMeshDecoder.Decode(document, document.Objects[meshIndex]);
        True(mesh.HasSkinningData,
            $"{fixtureName} alpha consumer is skinned");
        True(mesh.HasNormals,
            $"{fixtureName} alpha consumer is a surface mesh with normals");
        Equal(SmoMaterialConsumerKind.SkinnedSurface,
            SmoMaterialRenderState.ClassifyConsumer(mesh),
            $"{fixtureName} consumer geometry classification");

        True(SmoMaterialRenderState.TryDecode(
                document,
                document.Objects[materialIndex],
                out SmoMaterialRenderStateInfo? rawState),
            $"{fixtureName} alpha material render state decodes");
        Equal((uint)0x6, rawState!.FinalBlendOperation,
            $"{fixtureName} exact FinalBlendOp");
        Equal(SmoMaterialBlendMode.FinalBlend6Companion4,
            rawState.BlendMode,
            $"{fixtureName} exact companion tuple family");
        True(rawState.MaterialRenderStates.SequenceEqual(
                new uint[] { 0, 0, 1, 2, 1, 1, 3, 0, 4, 0, 6 }),
            $"{fixtureName} exact MaterialRenderStates");

        True(SmoSkinDecoder.TryDecode(
                document,
                document.Objects[skinIndex],
                out SmoSkin? skin,
                out string skinError),
            $"{fixtureName} consuming spSkin decodes: {skinError}");
        Equal((uint)1, skin!.AlphaSortEnable.GetValueOrDefault(uint.MaxValue),
            $"{fixtureName} consuming spSkin AlphaSortEnable");
        Equal((uint)1, skin.Priority.GetValueOrDefault(uint.MaxValue),
            $"{fixtureName} consuming spSkin Priority");
        Equal(SmoVertexDiffuseProfile.UniformOpaqueBlack,
            SmoMaterialRenderState.ClassifyVertexDiffuse(mesh),
            $"{fixtureName} alpha surface vertex diffuse profile");

        SmoMaterialRenderStateInfo consumerState =
            SmoMaterialRenderState.BindToRenderable(rawState, mesh, skin);
        Equal(SmoMaterialConsumerKind.SkinnedSurface,
            consumerState.ConsumerKind,
            $"{fixtureName} exact material remains in the surface family");
        True(consumerState.HasConfirmedConsumerTuple &&
             !consumerState.HasConsumerStateMismatch,
            $"{fixtureName} companion 4 is the confirmed skinned surface tuple");
        True(consumerState.HasConfirmedRenderableState,
            $"{fixtureName} exact tuple and consuming skin state are confirmed");
        True(!consumerState.UsesEmissiveApproximation,
            $"{fixtureName} skinned alpha is distinct from op4 glow");
        True(consumerState.Diagnostic is null,
            $"{fixtureName} canonical state has no warning");

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        True(bindings.TryGetValue(meshIndex, out SmoTextureBinding? binding),
            $"{fixtureName} alpha mesh has a material binding");
        True(binding!.UsesAlphaBlend,
            $"{fixtureName} binding carries blended ordering");
        Equal(SmoMaterialBlendMode.FinalBlend6Companion4,
            binding.MaterialRenderState!.BlendMode,
            $"{fixtureName} binding carries companion 4 state");
    }

    private static void CheckEffectBlendFixture(
        string path,
        uint finalBlendOperation,
        IReadOnlyList<int> materialIndices,
        uint companionState,
        string fixtureName)
    {
        SmoDocument document = SmoDocument.Load(path);
        foreach (int materialIndex in materialIndices)
        {
            True(SmoMaterialRenderState.TryDecode(
                    document,
                    document.Objects[materialIndex],
                    out SmoMaterialRenderStateInfo? state),
                $"{fixtureName} material [{materialIndex}] decodes");
            Equal(finalBlendOperation, state!.FinalBlendOperation,
                $"{fixtureName} material [{materialIndex}] exact FinalBlendOp");
            Equal(companionState, state.CompanionBlendState!.Value,
                $"{fixtureName} material [{materialIndex}] exact companion state");
            Equal(
                finalBlendOperation == 0x4
                    ? SmoMaterialBlendMode.EffectFinalBlend4
                    : SmoMaterialBlendMode.EffectFinalBlend5,
                state.BlendMode,
                $"{fixtureName} material [{materialIndex}] effect family");
            True(state.UsesEmissiveApproximation,
                $"{fixtureName} material [{materialIndex}] preview is emissive");
            True(state.Diagnostic?.StartsWith(
                    $"MATERIAL_EFFECT_BLEND_{finalBlendOperation}:",
                    StringComparison.Ordinal) == true,
                $"{fixtureName} material [{materialIndex}] approximation diagnostic");
        }
    }

    private static void CheckSkinnedEffectCounterexample(string path)
    {
        SmoDocument document = SmoDocument.Load(path);
        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        IReadOnlyDictionary<int, SmoMaterialRenderStateInfo> localRenderStates =
            SmoMaterialRenderState.ResolveDetailed(document);
        (int MeshIndex, uint AlphaSortEnable, uint Priority)[] cases =
        [
            (8, 0, 1),
            (19, 1, 0)
        ];

        foreach ((int meshIndex, uint alphaSortEnable, uint priority) in cases)
        {
            SmoObjectEntry meshEntry = document.Objects[meshIndex];
            SmoMesh mesh = SmoMeshDecoder.Decode(document, meshEntry);
            True(mesh.HasSkinningData && mesh.HasNormals,
                $"Droid effect mesh [{meshIndex}] has skinned surface layout");
            bindings.TryGetValue(meshIndex, out SmoTextureBinding? binding);
            SmoMaterialRenderStateInfo? rawState =
                localRenderStates.GetValueOrDefault(meshIndex) ??
                binding?.MaterialRenderState;
            True(rawState?.FinalBlendOperation == 0x6,
                $"Droid effect mesh [{meshIndex}] carries FinalBlendOp 6");

            int? skinIndex = FindAncestorObjectIndex(
                document.Objects, meshEntry, SmoClassIds.Skin);
            SmoSkin? decodedSkin = null;
            bool skinDecoded = skinIndex.HasValue && SmoSkinDecoder.TryDecode(
                document,
                document.Objects[skinIndex.Value],
                out decodedSkin,
                out _);
            True(skinDecoded,
                $"Droid effect mesh [{meshIndex}] consuming spSkin decodes");
            SmoSkin skin = decodedSkin ?? throw new InvalidOperationException(
                $"Droid effect mesh [{meshIndex}] skin was not returned.");
            Equal(alphaSortEnable,
                skin.AlphaSortEnable.GetValueOrDefault(uint.MaxValue),
                $"Droid effect mesh [{meshIndex}] AlphaSortEnable");
            Equal(priority, skin.Priority.GetValueOrDefault(uint.MaxValue),
                $"Droid effect mesh [{meshIndex}] Priority");

            SmoMaterialRenderStateInfo state =
                SmoMaterialRenderState.BindToRenderable(
                    rawState!, mesh, skin);
            Equal(SmoMaterialConsumerKind.SkinnedEffect, state.ConsumerKind,
                $"Droid effect mesh [{meshIndex}] consumer family");
            True(state.HasConfirmedConsumerTuple &&
                 !state.HasConsumerStateMismatch,
                $"Droid effect mesh [{meshIndex}] avoids a surface false positive");
        }
    }

    private static void CheckGeneratedSkinnedAlphaFixture(
        string path,
        uint expectedCompanionState)
    {
        SmoDocument document = SmoDocument.Load(path);
        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        var alphaConsumers = new List<(
            SmoMesh Mesh,
            SmoSkin Skin,
            SmoMaterialRenderStateInfo State)>();
        foreach (SmoObjectEntry entry in document.Objects.Where(
                     item => item.TypeHash == SmoClassIds.MeshData))
        {
            SmoMesh mesh = SmoMeshDecoder.Decode(document, entry);
            if (!mesh.HasSkinningData || !mesh.HasNormals ||
                !bindings.TryGetValue(entry.Index, out SmoTextureBinding? binding) ||
                binding.MaterialRenderState?.FinalBlendOperation != 0x6)
            {
                continue;
            }

            int? skinIndex = FindAncestorObjectIndex(
                document.Objects, entry, SmoClassIds.Skin);
            True(skinIndex.HasValue,
                $"generated Bloom mesh [{entry.Index}] has a consuming spSkin");
            True(SmoSkinDecoder.TryDecode(
                    document,
                    document.Objects[skinIndex!.Value],
                    out SmoSkin? skin,
                    out string skinError),
                $"generated Bloom mesh [{entry.Index}] spSkin decodes: {skinError}");
            SmoSkin consumingSkin = skin ?? throw new InvalidOperationException(
                $"Generated Bloom mesh [{entry.Index}] skin was not returned.");
            SmoMaterialRenderStateInfo renderableState =
                SmoMaterialRenderState.BindToRenderable(
                    binding.MaterialRenderState, mesh, consumingSkin);
            alphaConsumers.Add((mesh, consumingSkin, renderableState));
        }

        True(alphaConsumers.Count > 0,
            "generated Bloom fixture has skinned FinalBlendOp 6 consumers");
        uint[] expectedGeneratedTuple =
            [0, 0, 1, 0, 1, 1, 3, 0, expectedCompanionState, 0, 6];
        foreach ((SmoMesh mesh, SmoSkin skin, SmoMaterialRenderStateInfo state) in
                 alphaConsumers)
        {
            Equal(expectedCompanionState, state.CompanionBlendState!.Value,
                $"generated Bloom mesh [{mesh.ObjectIndex}] exact companion state");
            True(state.MaterialRenderStates.SequenceEqual(expectedGeneratedTuple),
                $"generated Bloom mesh [{mesh.ObjectIndex}] exact generated tuple");
            True(state.HasMaterialTupleDivergence &&
                 !state.HasConfirmedConsumerTuple,
                $"generated Bloom mesh [{mesh.ObjectIndex}] full tuple is non-confirmed");
            True(state.Diagnostic?.Contains(
                    "RS[3]=0 (expected 2)", StringComparison.Ordinal) == true,
                $"generated Bloom mesh [{mesh.ObjectIndex}] reports RS3 divergence");
            if (expectedCompanionState == 2)
            {
                True(state.Diagnostic?.Contains(
                        "RS[8]=2 (expected 4)", StringComparison.Ordinal) == true,
                    $"generated Bloom mesh [{mesh.ObjectIndex}] reports RS8 divergence");
            }

            Equal((uint)0, skin.AlphaSortEnable.GetValueOrDefault(uint.MaxValue),
                $"generated Bloom mesh [{mesh.ObjectIndex}] AlphaSortEnable");
            Equal((uint)1, skin.Priority.GetValueOrDefault(uint.MaxValue),
                $"generated Bloom mesh [{mesh.ObjectIndex}] Priority");
            True(state.HasAlphaSortStateDivergence &&
                 state.HasConsumerStateMismatch &&
                 !state.HasConfirmedRenderableState,
                $"generated Bloom mesh [{mesh.ObjectIndex}] consuming state is non-confirmed");
            Equal(SmoVertexDiffuseProfile.UniformOpaqueWhite,
                state.VertexDiffuseProfile,
                $"generated Bloom mesh [{mesh.ObjectIndex}] vertex diffuse profile");
            True(state.HasVertexDiffuseDivergenceUnconfirmed,
                $"generated Bloom mesh [{mesh.ObjectIndex}] records white/black divergence");
            True(state.Diagnostic?.Contains(
                    "not established as a cause", StringComparison.Ordinal) == true,
                $"generated Bloom mesh [{mesh.ObjectIndex}] avoids a diffuse causal claim");
            True(state.UsesEmissiveApproximation &&
                 state.UsesLuminanceCoverageApproximation,
                $"generated Bloom mesh [{mesh.ObjectIndex}] uses warning preview path");
        }
    }

    private static int? FindAncestorObjectIndex(
        IReadOnlyList<SmoObjectEntry> entries,
        SmoObjectEntry entry,
        uint typeHash)
    {
        SmoObjectEntry? cursor = entry;
        while (cursor.ParentIndex is int parentIndex &&
               (uint)parentIndex < (uint)entries.Count)
        {
            cursor = entries[parentIndex];
            if (cursor.TypeHash == typeHash)
                return cursor.Index;
        }
        return null;
    }

    private static void CheckBloomSchool(string path)
    {
        SmoDocument document = SmoDocument.Load(path);
        SmoMesh[] meshes = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .Select(entry => SmoMeshDecoder.Decode(document, entry))
            .ToArray();
        Equal(6, meshes.Length, "bloom_school mesh count");
        Equal(5, meshes.Count(mesh => mesh.VertexFormat == 0x197E), "bloom_school 0x197E meshes");
        Equal(1, meshes.Count(mesh => mesh.VertexFormat == 0x097E), "bloom_school 0x097E meshes");
        Equal(6, meshes.Count(mesh => mesh.HasTextureCoordinates), "bloom_school UV meshes");
        Equal(6, meshes.Count(mesh => mesh.HasDiffuseColors), "bloom_school diffuse meshes");

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        SmoTextureBinding[] textured = bindings.Values
            .Where(binding => binding.Texture is not null)
            .ToArray();
        Equal(6, textured.Length, "bloom_school textured meshes");
        Equal(
            5,
            textured.Count(binding => binding.Texture!.Name == "bloom_gilet"),
            "bloom_school body texture bindings");
        Equal(
            1,
            textured.Count(binding => binding.Texture!.Name == "bloomeye"),
            "bloom_school eye texture binding");
        True(
            textured.Select(binding => binding.Texture!.Name)
                .All(name => name is "bloom_gilet" or "bloomeye"),
            "bloom_school exact texture names");
    }

    private static void CheckSelectedCharacterBindings(string path)
    {
        IReadOnlyDictionary<string, IReadOnlyDictionary<string, int>> expected =
            new Dictionary<string, IReadOnlyDictionary<string, int>>(
                StringComparer.OrdinalIgnoreCase)
            {
                ["Goopmonster.smo"] = new Dictionary<string, int>
                {
                    ["gooptex"] = 6,
                    ["gooptex2"] = 3
                },
                ["Darcy.smo"] = new Dictionary<string, int> { ["darcy"] = 6 },
                ["knut.smo"] = new Dictionary<string, int> { ["knut"] = 6 },
                ["bloom_silk.smo"] = new Dictionary<string, int>
                {
                    ["bloom_lotus"] = 5,
                    ["bloomeye"] = 1
                },
                ["Generic.smo"] = new Dictionary<string, int>
                {
                    ["spe_body"] = 7,
                    ["spe_g_ey"] = 1,
                    ["spe_g_he"] = 1
                },
                ["Grizelda.smo"] = new Dictionary<string, int>
                {
                    ["grizelda"] = 6,
                    ["grizel_e"] = 1,
                    ["grizel_g"] = 1
                },
                ["fish.smo"] = new Dictionary<string, int> { ["fish"] = 1 },
                ["butterfly.smo"] = new Dictionary<string, int> { ["butter_g"] = 1 },
                ["Griffin.smo"] = new Dictionary<string, int>
                {
                    ["griffin"] = 6,
                    ["griffi_e"] = 1
                },
                ["Bloom_body.smo"] = new Dictionary<string, int>
                {
                    ["bloom_jeans"] = 6,
                    ["bloomeye"] = 2
                },
                ["Droid.smo"] = new Dictionary<string, int> { ["xj5"] = 4 },
                ["Amaryl.smo"] = new Dictionary<string, int> { ["amaryl"] = 6 },
                ["Troll.smo"] = new Dictionary<string, int> { ["troll"] = 6 },
                ["bloom_bike.smo"] = new Dictionary<string, int>
                {
                    ["b_bike"] = 5,
                    ["bloomeye"] = 1
                },
                ["bloomx.smo"] = new Dictionary<string, int>
                {
                    ["bloom"] = 8,
                    ["bloomeye"] = 1,
                    ["sparkles0001"] = 2
                },
                ["bloom_crystal.smo"] = new Dictionary<string, int>
                {
                    ["b_crysta"] = 7,
                    ["bloomeye"] = 1,
                    ["sparkles0001"] = 1
                },
                ["bloom_dating_outfit_02.smo"] = new Dictionary<string, int>
                {
                    ["bloom_jeansd"] = 8,
                    ["bloomeye_testd"] = 2
                },
                ["bloom_dating_outfit_03.smo"] = new Dictionary<string, int>
                {
                    ["b_biked"] = 5,
                    ["bloomeye_testd"] = 2,
                    ["bloom_jeansd"] = 4
                },
                ["bloom_dating_outfit_01.smo"] = new Dictionary<string, int>
                {
                    ["b_balld"] = 6,
                    ["bloomeye_testd"] = 2,
                    ["bloom_jeansd"] = 3
                },
                ["prince_dating_outfit_01.smo"] = new Dictionary<string, int>
                {
                    ["spe_body"] = 6,
                    ["bloomeye_testd"] = 2,
                    ["sky"] = 2
                },
                ["prince_dating_outfit_02.smo"] = new Dictionary<string, int>
                {
                    ["sky_jean"] = 5,
                    ["bloomeye_testd"] = 2,
                    ["sky"] = 2,
                    ["spe_body"] = 2
                },
                ["prince_dating_outfit_03.smo"] = new Dictionary<string, int>
                {
                    ["sky"] = 5,
                    ["bloomeye_testd"] = 2,
                    ["spe_body"] = 5
                }
            };
        if (!expected.TryGetValue(Path.GetFileName(path), out var expectedTextures))
            return;

        SmoDocument document = SmoDocument.Load(path);
        SmoMesh[] decodedMeshes = document.Objects
            .Where(entry => entry.TypeHash == SmoClassIds.MeshData)
            .Select(entry => SmoMeshDecoder.Decode(document, entry))
            .ToArray();
        int expectedMeshCount = Path.GetFileName(path).ToLowerInvariant() switch
        {
            "bloom_dating_outfit_02.smo" => 14,
            "bloom_dating_outfit_03.smo" => 12,
            "bloom_dating_outfit_01.smo" => 12,
            "prince_dating_outfit_01.smo" => 13,
            "bloomx.smo" => 11,
            "knut.smo" => 7,
            "bloom_crystal.smo" => 9,
            "prince_dating_outfit_02.smo" => 14,
            "prince_dating_outfit_03.smo" => 15,
            _ => expectedTextures.Values.Sum()
        };
        Equal(expectedMeshCount, decodedMeshes.Length,
            $"{Path.GetFileName(path)} decoded meshes");
        Equal(decodedMeshes.Length, decodedMeshes.Count(mesh => mesh.HasTextureCoordinates),
            $"{Path.GetFileName(path)} UV meshes");
        if (Path.GetFileName(path).Equals(
                "butterfly.smo", StringComparison.OrdinalIgnoreCase))
        {
            SmoMesh butterfly = decodedMeshes.Single();
            True(butterfly.HasDiffuseColors, "butterfly vertex diffuse colors");
            True(
                butterfly.DiffuseColorsArgb.Distinct().Count() > 1,
                "butterfly varying vertex diffuse colors");
        }
        else if (Path.GetFileName(path).Equals(
                     "Amaryl.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (SmoMesh mesh in decodedMeshes)
            {
                True(mesh.HasDiffuseColors, $"Amaryl [{mesh.ObjectIndex}] vertex diffuse");
                int black = mesh.DiffuseColorsArgb.Count(color => (color & 0x00FFFFFF) == 0);
                int transparent = mesh.DiffuseColorsArgb.Count(color => (color >> 24) == 0);
                Console.WriteLine(
                    $"Amaryl [{mesh.ObjectIndex}]: colors={mesh.DiffuseColorsArgb.Distinct().Count()}, " +
                    $"black={black}/{mesh.VertexCount}, alpha0={transparent}/{mesh.VertexCount}, " +
                    $"first={string.Join(",", mesh.DiffuseColorsArgb.Distinct().Take(8).Select(color => $"0x{color:X8}"))}");
                Equal(mesh.VertexCount, black,
                    $"Amaryl [{mesh.ObjectIndex}] uniform black diffuse sentinel");
            }
        }
        else if (Path.GetFileName(path).Equals(
                     "Troll.smo", StringComparison.OrdinalIgnoreCase))
        {
            SmoMesh head = decodedMeshes.Single(mesh => mesh.ObjectIndex == 86);
            Equal(425, head.DiffuseColorsArgb.Count(color => color == 0xFF000000),
                "Troll mesh 86 black placeholder vertices");
            Equal(10, head.DiffuseColorsArgb.Count(color => color == 0xFFFFFFFF),
                "Troll mesh 86 white default vertices");
            True(
                !SmoVertexColorUsage.ShouldModulateTexture(head),
                "Troll mesh 86 keeps its colour atlas instead of black tint");
            IReadOnlyDictionary<int, SmoTextureBinding> trollBindings =
                SmoTextureBindingResolver.ResolveAll(document);
            True(
                trollBindings.TryGetValue(86, out SmoTextureBinding? trollBinding) &&
                trollBinding.Texture?.Name == "troll",
                "Troll mesh 86 resolves its embedded troll atlas");
        }
        else if (Path.GetFileName(path).Equals(
                     "bloom_bike.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (SmoMesh part in decodedMeshes)
            {
                True(part.HasDiffuseColors, $"bloom_bike [{part.ObjectIndex}] vertex diffuse");
                Console.WriteLine(
                    $"bloom_bike [{part.ObjectIndex}]: colors={part.DiffuseColorsArgb.Distinct().Count()}, " +
                    $"values={string.Join(",", part.DiffuseColorsArgb.Distinct().Take(12).Select(color => $"0x{color:X8}"))}");
            }
            Equal((uint)0xFF202020, decodedMeshes.Single(mesh => mesh.ObjectIndex == 23)
                    .DiffuseColorsArgb.Distinct().Single(),
                "bloom_bike eyes uniform diffuse sentinel");
        }
        else if (Path.GetFileName(path).Equals(
                     "bloomx.smo", StringComparison.OrdinalIgnoreCase))
        {
            IReadOnlyDictionary<int, SmoTextureBinding> bloomXBindings =
                SmoTextureBindingResolver.ResolveAll(document);
            foreach (int wingMeshIndex in new[] { 25, 29 })
            {
                SmoMesh wing = decodedMeshes.Single(mesh =>
                    mesh.ObjectIndex == wingMeshIndex);
                True(bloomXBindings.TryGetValue(
                        wingMeshIndex, out SmoTextureBinding? wingBinding),
                    $"bloomx wing [{wingMeshIndex}] inherited atlas binding");
                Equal("bloom", wingBinding!.Texture!.Name,
                    $"bloomx wing [{wingMeshIndex}] inherited bloom atlas");
                True(bloomXBindings.Any(source =>
                        source.Key < wingMeshIndex &&
                        source.Value.Texture?.ObjectIndex ==
                        wingBinding.Texture.ObjectIndex &&
                        ReferenceEquals(source.Value, wingBinding)),
                    $"bloomx wing [{wingMeshIndex}] inherits the complete material binding");
                True(wing.HasTextureCoordinates,
                    $"bloomx wing [{wingMeshIndex}] UV channel");
                True(wing.DiffuseColorsArgb.All(color => color == 0xFFFFFFFF),
                    $"bloomx wing [{wingMeshIndex}] neutral vertex diffuse");
            }
            foreach (int meshIndex in new[] { 103, 105 })
            {
                SmoMesh part = decodedMeshes.Single(mesh => mesh.ObjectIndex == meshIndex);
                True(bloomXBindings.TryGetValue(
                        meshIndex, out SmoTextureBinding? binding),
                    $"bloomx mesh [{meshIndex}] layered binding");
                Equal("sparkles0001", binding!.Texture!.Name,
                    $"bloomx mesh [{meshIndex}] first animated layer frame");
                Equal("bloom_xc", binding.BaseTexture!.Name,
                    $"bloomx mesh [{meshIndex}] base texture layer");
                Equal(10, binding.AnimationFrames!.Count,
                    $"bloomx mesh [{meshIndex}] sparkle frame count");
                True(binding.FrameDuration > TimeSpan.Zero,
                    $"bloomx mesh [{meshIndex}] positive frame duration");
                True(part.HasTextureCoordinates1,
                    $"bloomx mesh [{meshIndex}] second UV channel");
                True(part.TextureCoordinates.Any(uv =>
                        uv.X < 0 || uv.X > 1 || uv.Y < 0 || uv.Y > 1),
                    $"bloomx mesh [{meshIndex}] tiled base UV channel");
            }

            SmoTexture firstFrame = bloomXBindings[103].AnimationFrames![0];
            SmoTexture bloomXBase = bloomXBindings[103].BaseTexture!;
            True(Enumerable.Range(0, bloomXBase.Width * bloomXBase.Height)
                    .Select(pixel => BitConverter.ToUInt32(
                        bloomXBase.Bgra32Pixels.Span.Slice(pixel * 4, 4)))
                    .All(color => (color & 0x00FFFFFF) == 0x0067CBDF),
                "bloomx base layer preserves its cyan RGB under alpha");
            True(Enumerable.Range(0, firstFrame.Width * firstFrame.Height)
                    .Count(pixel => BitConverter.ToUInt32(
                        firstFrame.Bgra32Pixels.Span.Slice(pixel * 4, 4)) ==
                        0xFF000000) > firstFrame.Width * firstFrame.Height / 2,
                "bloomx sparkle frame uses opaque black as additive transparency");
            True(firstFrame.Bgra32Pixels.ToArray()
                    .Where((_, index) => index % 4 == 3)
                    .Distinct().Count() > 1,
                "bloomx animated overlay uses varying alpha");
        }
        else if (Path.GetFileName(path).Equals(
                     "bloom_crystal.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (SmoObjectEntry entry in document.Objects.Where(entry => entry.Index is >= 75 and <= 95))
            {
                Console.WriteLine(
                    $"crystal object [{entry.Index}] type=0x{entry.TypeHash:X8} " +
                    $"{SmoClassRegistry.GetDisplayName(entry.TypeHash)} name={entry.Name} " +
                    $"parent={entry.ParentIndex} offset=0x{entry.PhysicalOffset:X} size=0x{entry.SerializedSize:X}");
            }
            IReadOnlyDictionary<int, SmoTextureBinding> crystalBindings =
                SmoTextureBindingResolver.ResolveAll(document);
            True(crystalBindings.TryGetValue(93, out SmoTextureBinding? crystalBinding),
                "bloom_crystal mesh 93 animated binding");
            Equal("sparkles0001", crystalBinding!.Texture!.Name,
                "bloom_crystal mesh 93 first frame");
            Equal(10, crystalBinding.AnimationFrames!.Count,
                "bloom_crystal sparkle frame count");
            Equal("b_crysta", crystalBinding.BaseTexture!.Name,
                "bloom_crystal mesh 93 base texture stage");
            True(
                Math.Abs(crystalBinding.FrameDuration!.Value.TotalSeconds - 0.1266667) < 0.0001,
                "bloom_crystal sequence timing from controller keys");
            IReadOnlyDictionary<int, uint> crystalColors =
                SmoMaterialColorResolver.ResolveAll(document);
            Console.WriteLine(
                $"bloom_crystal mesh 93 material color=" +
                (crystalColors.TryGetValue(93, out uint crystalColor)
                    ? $"0x{crystalColor:X8}"
                    : "<none>"));
            SmoMesh crystalMesh = decodedMeshes.Single(mesh => mesh.ObjectIndex == 93);
            True(crystalMesh.HasTextureCoordinates1,
                "bloom_crystal mesh 93 second UV channel");
            Console.WriteLine(
                $"bloom_crystal mesh 93 diffuse=" +
                string.Join(",", crystalMesh.DiffuseColorsArgb.Distinct()
                    .Take(20).Select(color => $"0x{color:X8}")) +
                $", UV1=({crystalMesh.TextureCoordinates1.Min(uv => uv.X):G5}.." +
                $"{crystalMesh.TextureCoordinates1.Max(uv => uv.X):G5}, " +
                $"{crystalMesh.TextureCoordinates1.Min(uv => uv.Y):G5}.." +
                $"{crystalMesh.TextureCoordinates1.Max(uv => uv.Y):G5})");
            foreach (SmoTexture frame in crystalBinding.AnimationFrames)
            {
                int alphaZero = 0;
                long visibleRed = 0;
                long visibleGreen = 0;
                long visibleBlue = 0;
                int visible = 0;
                for (int pixel = 0; pixel < frame.Bgra32Pixels.Length; pixel += 4)
                {
                    byte alpha = frame.Bgra32Pixels.Span[pixel + 3];
                    if (alpha == 0)
                    {
                        alphaZero++;
                        continue;
                    }
                    visibleBlue += frame.Bgra32Pixels.Span[pixel];
                    visibleGreen += frame.Bgra32Pixels.Span[pixel + 1];
                    visibleRed += frame.Bgra32Pixels.Span[pixel + 2];
                    visible++;
                }
                Console.WriteLine(
                    $"  {frame.Name}: alpha0={alphaZero}/{frame.Width * frame.Height}, " +
                    $"visible avg RGB=({visibleRed / Math.Max(1, visible)}," +
                    $"{visibleGreen / Math.Max(1, visible)},{visibleBlue / Math.Max(1, visible)}), " +
                    $"UV1 sample BGRA=" + GetTextureSample(frame, crystalMesh.TextureCoordinates1[0]));
            }
        }
        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        string[] actualTextures = bindings.Values
            .Where(binding => binding.Texture is not null)
            .Select(binding => binding.Texture!.Name)
            .ToArray();
        Equal(expectedTextures.Values.Sum(), actualTextures.Length,
            $"{Path.GetFileName(path)} textured meshes");
        foreach ((string textureName, int expectedCount) in expectedTextures)
        {
            Equal(expectedCount, actualTextures.Count(name => name == textureName),
                $"{Path.GetFileName(path)} {textureName} bindings");
        }

        if (Path.GetFileName(path).Equals(
                "bloom_dating_outfit_02.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (SmoTexture texture in bindings.Values
                         .Select(binding => binding.Texture)
                         .OfType<SmoTexture>()
                         .DistinctBy(texture => texture.ObjectIndex))
            {
                True(
                    texture.Bgra32Pixels.Span[3..].ToArray()
                        .Where((_, index) => index % 4 == 0)
                        .All(alpha => alpha == byte.MaxValue),
                    $"{texture.Name} opaque BGRA alpha");
            }
            foreach (int propMeshIndex in new[] { 6, 10, 38 })
            {
                True(
                    !bindings.TryGetValue(
                        propMeshIndex, out SmoTextureBinding? propBinding) ||
                    propBinding.Texture is null,
                    $"outfit_02 rigid prop [{propMeshIndex}] keeps material color");
            }
            IReadOnlyDictionary<int, uint> datingMaterialColors =
                SmoMaterialColorResolver.ResolveAll(document);
            foreach (int propMeshIndex in new[] { 6, 10, 38 })
            {
                True(
                    datingMaterialColors.ContainsKey(propMeshIndex),
                    $"outfit_02 rigid prop [{propMeshIndex}] has material color");
            }

            SmoDocument datingDocument = document;
            SmoObjectEntry[] skinEntries = datingDocument.Objects
                .Where(entry => entry.TypeHash == SmoClassIds.Skin)
                .ToArray();
            Equal(8, skinEntries.Length, "outfit_02 skin count");
            foreach (SmoObjectEntry skinEntry in skinEntries)
            {
                True(
                    SmoSkinDecoder.TryDecode(
                        datingDocument, skinEntry, out SmoSkin? skin, out _),
                    $"outfit_02 skin palette [{skinEntry.Index}]");
                Equal(16, skin!.Bones.Count,
                    $"outfit_02 16-bone palette [{skinEntry.Index}]");

                SmoObjectEntry meshEntry = datingDocument.Objects.Single(entry =>
                    entry.ParentIndex == skinEntry.Index &&
                    entry.TypeHash == SmoClassIds.MeshData);
                SmoMesh skinnedMesh = SmoMeshDecoder.Decode(datingDocument, meshEntry);
                True(skinnedMesh.HasSkinningData,
                    $"outfit_02 skinning attributes [{meshEntry.Index}]");
                True(
                    skinnedMesh.BlendWeights.All(weight =>
                        weight.X >= 0 && weight.Y >= 0 &&
                        weight.Z >= 0 && weight.W >= 0 &&
                        weight.X + weight.Y + weight.Z + weight.W <= 1.001f),
                    $"outfit_02 normalized blend weights [{meshEntry.Index}]");
                True(
                    skinnedMesh.BlendWeights.Zip(skinnedMesh.BlendIndices).All(item =>
                        (item.First.X <= 0.00001f || item.Second.X < 16) &&
                        (item.First.Y <= 0.00001f || item.Second.Y < 16) &&
                        (item.First.Z <= 0.00001f || item.Second.Z < 16) &&
                        (item.First.W <= 0.00001f || item.Second.W < 16)),
                    $"outfit_02 active blend indices [{meshEntry.Index}]");
            }

            Matrix4x4 leftEye = SmoNodeTransformDecoder.ResolveModelWorldMatrix(
                datingDocument, datingDocument.Objects[82]);
            Matrix4x4 rightEye = SmoNodeTransformDecoder.ResolveModelWorldMatrix(
                datingDocument, datingDocument.Objects[86]);
            True(leftEye.M41 > 0 && rightEye.M41 < 0,
                "outfit_02 eyes preserve left/right bind placement");
            True(
                MathF.Abs(leftEye.M42 - 144.20456f) < 0.01f &&
                MathF.Abs(rightEye.M42 - 144.20456f) < 0.01f,
                "outfit_02 eyes use inverse-bind world height");
        }
        else if (Path.GetFileName(path).Equals(
                     "bloom_dating_outfit_03.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (int faceMeshIndex in new[] { 116, 118 })
            {
                SmoMesh faceMesh = SmoMeshDecoder.Decode(document, document.Objects[faceMeshIndex]);
                True(faceMesh.HasDiffuseColors,
                    $"outfit_03 face [{faceMeshIndex}] vertex diffuse");
                int darkVertices = faceMesh.DiffuseColorsArgb.Count(color =>
                    ((color >> 16) & 0xFF) +
                    ((color >> 8) & 0xFF) +
                    (color & 0xFF) <= 96);
                Console.WriteLine(
                    $"outfit_03 face [{faceMeshIndex}]: " +
                    $"diffuse colors={faceMesh.DiffuseColorsArgb.Distinct().Count()}, " +
                    $"dark vertices={darkVertices}/{faceMesh.VertexCount}, " +
                    $"darkest={string.Join(",", faceMesh.DiffuseColorsArgb
                        .Distinct()
                        .OrderBy(color => ((color >> 16) & 0xFF) +
                                          ((color >> 8) & 0xFF) + (color & 0xFF))
                        .Take(5).Select(color => $"0x{color:X8}"))}");
                True(darkVertices > 0,
                    $"outfit_03 face [{faceMeshIndex}] dark eyelash diffuse");
                True(
                    SmoVertexColorUsage.ShouldModulateTexture(faceMesh),
                    $"outfit_03 face [{faceMeshIndex}] vertex diffuse remains renderable");
            }
        }
        else if (Path.GetFileName(path).Equals(
                     "knut.smo", StringComparison.OrdinalIgnoreCase))
        {
            SmoObjectEntry glassesMesh = document.Objects[6];
            Equal(SmoClassIds.MeshData, glassesMesh.TypeHash,
                "Knut glasses mesh object");
            True(
                SmoRigidBindingResolver.ResolveAnimationNodeObjectIndex(
                    document, glassesMesh) == 2,
                "Knut glasses bind to their animated render node");
            Equal("Knut_TEMP_glasses", document.Objects[2].Name,
                "Knut glasses animation target name");

            IReadOnlyDictionary<int, SmoTextureBinding> knutBindings =
                SmoTextureBindingResolver.ResolveAll(document);
            SmoMesh decodedGlasses = decodedMeshes.Single(mesh =>
                mesh.ObjectIndex == 6);
            True(!knutBindings.TryGetValue(
                     6, out SmoTextureBinding? glassesBinding) ||
                 glassesBinding.Texture is null,
                "Knut glasses keep their solid-color material");
            True(decodedGlasses.HasTextureCoordinates,
                "Knut glasses UV channel");
            True(decodedGlasses.DiffuseColorsArgb.All(color =>
                    (color & 0x00FFFFFF) == 0),
                "Knut glasses black vertex-color sentinel");

            string animationPath = Path.Combine(
                Path.GetDirectoryName(path)!, "Knid.san");
            True(File.Exists(animationPath), "Knut regression animation is present");
            True(
                SmoAnimationDecoder.TryDecode(
                    animationPath, out SmoAnimationClip? animation, out _),
                "Knut regression animation decodes");
            True(
                animation!.Tracks.Any(track =>
                    track.NodeName.Equals(
                        "Knut_TEMP_glasses", StringComparison.OrdinalIgnoreCase) &&
                    track.Positions.Count > 1 && track.Rotations.Count > 1),
                "Knut glasses have an animated transform track");
        }
        else if (Path.GetFileName(path).Equals(
                     "bloom_dating_outfit_01.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (int faceMeshIndex in new[] { 120, 124 })
            {
                SmoMesh faceMesh = SmoMeshDecoder.Decode(document, document.Objects[faceMeshIndex]);
                True(faceMesh.HasNormals,
                    $"outfit_01 face [{faceMeshIndex}] stored normals");
                True(
                    faceMesh.Normals.All(normal =>
                        MathF.Abs(normal.LengthSquared() - 1) < 0.001f),
                    $"outfit_01 face [{faceMeshIndex}] normalized normals");
            }
        }
        else if (Path.GetFileName(path).Equals(
                     "prince_dating_outfit_01.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (int faceMeshIndex in new[] { 131, 133 })
            {
                SmoMesh faceMesh = SmoMeshDecoder.Decode(document, document.Objects[faceMeshIndex]);
                True(faceMesh.HasDiffuseColors,
                    $"prince outfit_01 face [{faceMeshIndex}] vertex diffuse");
                True(
                    SmoVertexColorUsage.ShouldModulateTexture(faceMesh),
                    $"prince outfit_01 face [{faceMeshIndex}] vertex diffuse remains renderable");
                True(faceMesh.HasNormals,
                    $"prince outfit_01 face [{faceMeshIndex}] stored normals");
                int conflictingUvVertices = faceMesh.TextureCoordinates
                    .Select((uv, index) => (Uv: uv, Color: faceMesh.DiffuseColorsArgb[index]))
                    .GroupBy(item => item.Uv)
                    .Where(group => group.Select(item => item.Color).Distinct().Count() > 1)
                    .Sum(group => group.Count());
                Console.WriteLine(
                    $"prince outfit_01 face [{faceMeshIndex}]: " +
                    $"vertices={faceMesh.VertexCount}, " +
                    $"diffuse colors={faceMesh.DiffuseColorsArgb.Distinct().Count()}, " +
                    $"normals={faceMesh.Normals.Distinct().Count()}, " +
                    $"conflicting UV vertices={conflictingUvVertices}, " +
                    $"normal lengths={faceMesh.Normals.Min(normal => normal.Length()):G5}.." +
                    $"{faceMesh.Normals.Max(normal => normal.Length()):G5}, " +
                    $"darkest={string.Join(",", faceMesh.DiffuseColorsArgb
                        .Distinct()
                        .OrderBy(color => ((color >> 16) & 0xFF) +
                                          ((color >> 8) & 0xFF) + (color & 0xFF))
                        .Take(5).Select(color => $"0x{color:X8}"))}");
                for (int triangle = 0; triangle < faceMesh.TriangleIndices.Length; triangle += 3)
                {
                    int ia = checked((int)faceMesh.TriangleIndices[triangle]);
                    int ib = checked((int)faceMesh.TriangleIndices[triangle + 1]);
                    int ic = checked((int)faceMesh.TriangleIndices[triangle + 2]);
                    Vector2 a = faceMesh.TextureCoordinates[ia];
                    Vector2 b = faceMesh.TextureCoordinates[ib];
                    Vector2 c = faceMesh.TextureCoordinates[ic];
                    float uvArea = MathF.Abs(
                        (b.X - a.X) * (c.Y - a.Y) - (b.Y - a.Y) * (c.X - a.X));
                    if (uvArea > 0.0000001f)
                        continue;
                    Console.WriteLine(
                        $"  degenerate triangle {triangle / 3}: indices={ia},{ib},{ic}; " +
                        $"uv={a},{b},{c}; colors=0x{faceMesh.DiffuseColorsArgb[ia]:X8}," +
                        $"0x{faceMesh.DiffuseColorsArgb[ib]:X8},0x{faceMesh.DiffuseColorsArgb[ic]:X8}; " +
                        $"positions={faceMesh.Positions[ia]},{faceMesh.Positions[ib]}," +
                        $"{faceMesh.Positions[ic]}");
                }
            }

            SmoMesh body = SmoMeshDecoder.Decode(document, document.Objects[107]);
            True(
                SmoVertexColorUsage.ShouldModulateTexture(body),
                "prince outfit_01 body [107] black-ended gradient remains renderable");
        }
        else if (Path.GetFileName(path).Equals(
                     "prince_dating_outfit_02.smo", StringComparison.OrdinalIgnoreCase))
        {
            True(
                bindings.TryGetValue(108, out SmoTextureBinding? headBinding) &&
                headBinding.Texture?.Name == "sky",
                "prince outfit_02 mesh 108 inherits preceding sky material");
        }
        else if (Path.GetFileName(path).Equals(
                     "prince_dating_outfit_03.smo", StringComparison.OrdinalIgnoreCase))
        {
            foreach (int headMeshIndex in new[] { 66, 68 })
            {
                True(
                    bindings.TryGetValue(
                        headMeshIndex, out SmoTextureBinding? headBinding) &&
                    headBinding.Texture?.Name == "sky",
                    $"prince outfit_03 mesh {headMeshIndex} inherits parent sky material");
            }
        }
    }

    private static void CheckKnownFile(
        string corpusPath,
        string fileName,
        int expectedObjects,
        int expectedMeshes)
    {
        string path = Path.Combine(corpusPath, fileName);
        if (!File.Exists(path))
        {
            Console.WriteLine($"Known sample not present; skipped: {path}");
            return;
        }

        SmoDocument document = SmoDocument.Load(path);
        Equal(expectedObjects, document.Objects.Count, $"{fileName} objects");
        SmoObjectEntry[] meshEntries = document.Objects
            .Where(item => item.TypeHash == SmoClassIds.MeshData)
            .ToArray();
        Equal(
            expectedMeshes,
            meshEntries.Length,
            $"{fileName} meshes");

        var decodedMeshes = new List<SmoMesh>(meshEntries.Length);
        foreach (SmoObjectEntry entry in meshEntries)
        {
            True(
                SmoMeshDecoder.TryDecode(
                    document,
                    entry,
                    out SmoMesh? mesh,
                    out string error),
                $"{fileName} mesh [{entry.Index}] decodes exactly: {error}");
            decodedMeshes.Add(mesh!);
        }

        Equal(expectedMeshes, decodedMeshes.Count, $"{fileName} decoded meshes");

        if (fileName.Equals("fish.smo", StringComparison.OrdinalIgnoreCase))
        {
            SmoMesh mesh = decodedMeshes.Single();
            Equal(SmoMeshDecoder.E1Marker, mesh.Marker, "fish marker");
            Equal(0x093Eu, mesh.VertexFormat, "fish vertex format");
            Equal(44, mesh.Stride, "fish serialized stride");
            Equal(56, mesh.RuntimeStride, "fish runtime stride");
            True(mesh.HasSkinningData, "fish compressed skinning attributes");
        }

        if (fileName.Equals("loading.smo", StringComparison.OrdinalIgnoreCase))
        {
            Equal(
                2,
                decodedMeshes.Count(mesh => mesh.HasTextureCoordinates),
                "loading confirmed UV meshes");
            True(
                decodedMeshes
                    .SelectMany(mesh => mesh.TextureCoordinates)
                    .All(uv => float.IsFinite(uv.X) && float.IsFinite(uv.Y)),
                "loading UV coordinates are finite");

            IReadOnlyDictionary<int, SmoTextureBinding> bindings =
                SmoTextureBindingResolver.ResolveAll(document);
            SmoTextureBinding[] textured = bindings.Values
                .Where(binding => binding.Texture is not null)
                .ToArray();
            Equal(1, textured.Length, "loading resolved texture bindings");
            SmoTexture texture = textured.Single().Texture!;
            Equal("load_default", texture.Name, "loading texture name");
            Equal((ushort)0x32E3, texture.FormatCode, "loading texture format");
            Equal(
                checked(texture.Width * texture.Height * 4),
                texture.Bgra32Pixels.Length,
                "loading decoded texture size");
        }
    }

    private static void CheckE0FinalIndices(string corpusPath)
    {
        string path = Path.Combine(
            corpusPath,
            "Winx Club",
            "Media",
            "Levels",
            "Gardenia",
            "kt.smo");
        if (!File.Exists(path))
        {
            Console.WriteLine($"Known E0 sample not present; skipped: {path}");
            return;
        }

        SmoDocument document = SmoDocument.Load(path);
        SmoMesh[] meshes = document.Objects
            .Where(item => item.TypeHash == SmoClassIds.MeshData)
            .Select(item => SmoMeshDecoder.Decode(document, item))
            .ToArray();

        True(
            meshes.Any(mesh => mesh.StripIndices.SequenceEqual(
                new ushort[] { 2, 0, 1, 3 })),
            "E0 q+4 word is preserved when it contains two final strip indices");
    }

    private static void CheckGuiOptionsMenu(string path)
    {
        SmoDocument document = SmoDocument.Load(path);
        SmoGuiSceneInfo gui = SmoGuiSceneAnalyzer.Analyze(document);

        True(gui.IsGuiContent, "igmenu_opt_pc is structurally detected as GUI content");
        Equal(99, gui.TotalMeshCount, "igmenu_opt_pc mesh count");
        Equal(99, gui.DecodedMeshCount, "igmenu_opt_pc decoded GUI meshes");
        Equal(99, gui.PlanarMeshCount, "igmenu_opt_pc planar GUI meshes");
        Equal(2, gui.CollisionMeshCount, "igmenu_opt_pc GUICollision meshes");
        Equal(60, gui.StateMeshCount, "igmenu_opt_pc authored state meshes");
        Equal(0, gui.TextClassObjectCount,
            "igmenu_opt_pc has no serialized Sparkplug text classes");
        SmoGuiRootGroupInfo display = gui.RootGroups.Single(group =>
            group.Name.Equals("display", StringComparison.OrdinalIgnoreCase));
        Equal(13, display.MeshCount, "igmenu_opt_pc display screen mesh count");
        SmoObjectEntry resolutionLabel = document.Objects.First(entry =>
            entry.Name.Equals("resolution_label", StringComparison.OrdinalIgnoreCase));
        SmoGuiObjectContext resolutionContext =
            gui.ObjectContextsByObjectIndex[resolutionLabel.Index];
        Equal(display.ObjectIndex, resolutionContext.RootGroupObjectIndex,
            "resolution_label resolves to the assembled display screen");
        Equal(SmoGuiVisualState.Unclassified, resolutionContext.VisualState,
            "resolution_label remains on the common GUI layer");
        SmoObjectEntry highlighted = document.Objects.First(entry =>
            entry.Name.StartsWith("HIGHLIGHTED", StringComparison.OrdinalIgnoreCase));
        Equal(
            SmoGuiVisualState.Highlighted,
            gui.ObjectContextsByObjectIndex[highlighted.Index].VisualState,
            "GUI tree state node resolves to its authored visual state");
        True(gui.Meshes.Any(mesh =>
                mesh.RootGroupObjectIndex == display.ObjectIndex &&
                mesh.ElementName.Contains("scroll", StringComparison.OrdinalIgnoreCase)),
            "igmenu_opt_pc display screen exposes scroll controls");
    }

    private static void CheckGameOverLayout(string path)
    {
        const string expectedSha256 =
            "593DDE72EAFC36532B4976B5269EB53D0B3FAC217AB9B97472C6B8C1DBEBE2AA";
        Equal(
            expectedSha256,
            Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))),
            "gameover pristine fixture identity");

        SmoDocument document = SmoDocument.Load(path);
        SmoGuiSceneInfo gui = SmoGuiSceneAnalyzer.Analyze(document);
        Equal(6, document.Objects.Count, "gameover object count");
        True(gui.IsGuiContent, "gameover is structurally detected as 2D content");
        Equal(
            SmoGuiContentKind.RuntimeNodeLayout,
            gui.ContentKind,
            "gameover uses the node-only runtime layout kind");
        Equal(0, gui.TotalMeshCount, "gameover has no serialized meshes");
        Equal(0, gui.TextClassObjectCount,
            "gameover has no serialized Sparkplug text classes");
        Equal(2, gui.LayoutAnchors.Count, "gameover runtime text anchor count");

        SmoGuiLayoutAnchorInfo shadow = gui.LayoutAnchors.Single(anchor =>
            anchor.Name.Equals("text_text01", StringComparison.OrdinalIgnoreCase));
        SmoGuiLayoutAnchorInfo normal = gui.LayoutAnchors.Single(anchor =>
            anchor.Name.Equals("text_text", StringComparison.OrdinalIgnoreCase));
        Equal(SmoGuiVisualState.Shadow, shadow.VisualState,
            "gameover shadow text slot state");
        Equal(SmoGuiVisualState.Normal, normal.VisualState,
            "gameover normal text slot state");
        Equal("gameover", normal.RootGroupName,
            "gameover normal slot root screen");
        True(Vector3.Distance(
                shadow.WorldPosition,
                new Vector3(0.20526648f, -0.23411977f, 0.14643508f)) < 0.00001f,
            "gameover shadow slot inherited transform");
        True(Vector3.Distance(
                normal.WorldPosition,
                new Vector3(-0.20526601f, 0.23411977f, -0.14643507f)) < 0.00001f,
            "gameover normal slot inherited transform");
    }

    private static void CheckAlfea02PartitionTransforms(string path)
    {
        const string expectedSha256 =
            "1316A81D27254E1B20627433CC8E01327041D560BBEFACB949ADF38A336B79DF";
        Equal(
            expectedSha256,
            Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))),
            "Alfea02 pristine fixture identity");

        SmoDocument document = SmoDocument.Load(path);
        Equal(4266, document.Objects.Count, "Alfea02 object count");
        Equal(702, document.Objects.Count(entry =>
            entry.TypeHash == SmoClassIds.MeshData), "Alfea02 mesh count");

        SmoObjectEntry roomModel = document.Objects[4199];
        Equal(SmoClassIds.Model, roomModel.TypeHash,
            "Alfea02 object 4199 is the room model, not mesh data");
        Equal("dormBigroomNOSH-000", roomModel.Name,
            "Alfea02 object 4199 room model name");
        SmoObjectEntry roomMesh = document.Objects[4201];
        Equal(SmoClassIds.MeshData, roomMesh.TypeHash,
            "Alfea02 room model mesh object");
        Matrix4x4 roomWorld =
            SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, roomMesh);
        True(MatrixDifference(roomWorld, Matrix4x4.Identity) < 0.00001f,
            "Alfea02 baked room mesh does not inherit partition centers as transforms");

        SmoObjectEntry staticObject = document.Objects[1369];
        SmoObjectEntry staticMesh = document.Objects[1372];
        True(SmoStaticRenderObjectTransformDecoder.TryDecode(
                document, staticObject, out Matrix4x4 authoredWorld),
            "Alfea02 static object world transform decodes");
        Matrix4x4 resolvedStaticWorld =
            SmoNodeTransformDecoder.ResolveModelWorldMatrix(document, staticMesh);
        True(MatrixDifference(resolvedStaticWorld, authoredWorld) < 0.00001f,
            "Alfea02 static world transform terminates partition containment chain");
        True(Vector3.Distance(
                new Vector3(
                    resolvedStaticWorld.M41,
                    resolvedStaticWorld.M42,
                    resolvedStaticWorld.M43),
                new Vector3(-4416.12744f, 0f, -1812.98193f)) < 0.001f,
            "Alfea02 Darch_A01 keeps its authored world placement");

        IReadOnlyDictionary<int, SmoTextureBinding> textureBindings =
            SmoTextureBindingResolver.ResolveAll(document);
        SmoMesh seed = SmoMeshDecoder.Decode(document, document.Objects[284]);
        Equal(0x0840u, seed.VertexFormat,
            "Alfea02 dseed vertex format");
        Equal(32, seed.Stride,
            "Alfea02 dseed serialized stride");
        True(seed.HasNormals,
            "Alfea02 dseed 0x0840 normals decode");
        True(seed.HasTextureCoordinates,
            "Alfea02 dseed 0x0840 UV0 decodes");
        True(!seed.HasDiffuseColors,
            "Alfea02 dseed 0x0840 has no vertex diffuse stream");
        True(textureBindings.TryGetValue(284, out SmoTextureBinding? seedBinding),
            "Alfea02 dseed texture binding resolves");
        Equal("dseed", seedBinding!.Texture!.Name,
            "Alfea02 dseed texture name");
        Vector2 seedUvMinimum = new(
            seed.TextureCoordinates.Min(uv => uv.X),
            seed.TextureCoordinates.Min(uv => uv.Y));
        Vector2 seedUvMaximum = new(
            seed.TextureCoordinates.Max(uv => uv.X),
            seed.TextureCoordinates.Max(uv => uv.Y));
        True(Vector2.Distance(
                seedUvMinimum, new Vector2(0.042858098f, 0.014410913f)) < 0.00001f,
            "Alfea02 dseed UV minimum");
        True(Vector2.Distance(
                seedUvMaximum, new Vector2(0.9300215f, 0.9888289f)) < 0.00001f,
            "Alfea02 dseed UV maximum");
        SmoMesh doorFrame =
            SmoMeshDecoder.Decode(document, document.Objects[681]);
        SmoMesh doorTop =
            SmoMeshDecoder.Decode(document, document.Objects[689]);
        True(SmoVertexColorUvConflictAnalyzer.HasConflictingSharedCoordinates(doorFrame),
            "Alfea02 door frame has shared UVs with different vertex RGB");
        True(SmoVertexColorUvConflictAnalyzer.HasConflictingSharedCoordinates(doorTop),
            "Alfea02 door top has shared UVs with different vertex RGB");
        True(!SmoVertexColorUsage.HasAuthoredAlphaGradient(doorFrame),
            "Alfea02 door frame mixed RGB is not mistaken for an alpha gradient");
        True(!SmoVertexColorUsage.HasAuthoredAlphaGradient(doorTop),
            "Alfea02 door top mixed RGB is not mistaken for an alpha gradient");
        Equal(3, doorFrame.DiffuseColorsArgb
                .Select(color => color & 0x00FFFFFF).Distinct().Count(),
            "Alfea02 door frame vertex RGB count");
        Equal(8, doorTop.DiffuseColorsArgb
                .Select(color => color & 0x00FFFFFF).Distinct().Count(),
            "Alfea02 door top vertex RGB count");
        True(!textureBindings.TryGetValue(681, out SmoTextureBinding? doorFrameBinding) ||
             doorFrameBinding.Texture is null,
            "Alfea02 door frame is intentionally vertex-colour-only");
        True(textureBindings.TryGetValue(689, out SmoTextureBinding? doorTopBinding),
            "Alfea02 door top texture binding resolves");
        Equal("noise04w", doorTopBinding!.Texture!.Name,
            "Alfea02 door top texture name");
        SmoMesh chandelierBody =
            SmoMeshDecoder.Decode(document, document.Objects[2859]);
        SmoMesh chandelierGlow =
            SmoMeshDecoder.Decode(document, document.Objects[2863]);
        True(textureBindings.TryGetValue(2859, out SmoTextureBinding? chandelierBodyBinding),
            "Alfea02 chandelier body texture binding resolves");
        Equal("noise03b", chandelierBodyBinding!.Texture!.Name,
            "Alfea02 chandelier body texture name");
        True(chandelierBody.Normals.All(normal =>
                MathF.Abs(normal.Length() - 1) < 0.00001f),
            "Alfea02 chandelier body normals remain normalized");
        IGrouping<Vector3, (Vector3 Position, int Index)>[] sharedChandelierPositions =
            chandelierBody.Positions
                .Select((position, index) => (Position: position, Index: index))
                .GroupBy(item => item.Position)
                .Where(group => group.Count() > 1)
                .ToArray();
        Equal(222, sharedChandelierPositions.Length,
            "Alfea02 chandelier shared-position groups");
        True(sharedChandelierPositions.All(group =>
                group.Select(item => chandelierBody.Normals[item.Index])
                    .Distinct().Count() == 1),
            "Alfea02 chandelier authored smoothing normals agree at shared positions");
        SmoTextureUvAlphaCoverage chandelierBodyAlpha =
            SmoTextureUvAlphaAnalyzer.Analyze(
                chandelierBody, chandelierBodyBinding.Texture);
        True(chandelierBodyAlpha.IsReliable &&
             chandelierBodyAlpha.OpaqueTexelCount == 4096 &&
             chandelierBodyAlpha.PartialAlphaTexelCount == 0,
            "Alfea02 chandelier body noise texture is fully opaque");

        True(textureBindings.TryGetValue(2863, out SmoTextureBinding? chandelierGlowBinding),
            "Alfea02 chandelier glow texture binding resolves");
        Equal("chand", chandelierGlowBinding!.Texture!.Name,
            "Alfea02 chandelier glow texture name");
        SmoTextureUvAlphaCoverage chandelierGlowAlpha =
            SmoTextureUvAlphaAnalyzer.Analyze(
                chandelierGlow, chandelierGlowBinding.Texture);
        True(chandelierGlowAlpha.IsReliable,
            "Alfea02 chandelier repeated integer UV tiles are analyzable");
        Equal(4096, chandelierGlowAlpha.SampledTexelCount,
            "Alfea02 chandelier glow sampled texel count");
        Equal(2412, chandelierGlowAlpha.FullyTransparentTexelCount,
            "Alfea02 chandelier glow transparent texel count");
        Equal(743, chandelierGlowAlpha.PartialAlphaTexelCount,
            "Alfea02 chandelier glow partial-alpha texel count");
        SmoMaterialRenderStateInfo chandelierGlowState =
            SmoMaterialRenderState.BindToRenderable(
                chandelierGlowBinding.MaterialRenderState!,
                chandelierGlow,
                null,
                chandelierGlowBinding.Texture);
        Equal(
            SmoMaterialBlendMode.RigidTextureAlphaSurfaceFinalBlend2,
            chandelierGlowState.BlendMode,
            "Alfea02 chandelier glow blend classification");
        True(chandelierGlowState.UsesAlphaBlend &&
             chandelierGlowState.RequiresTransparentOrdering,
            "Alfea02 chandelier glow renders after its opaque body");
        SmoMesh vertexLight =
            SmoMeshDecoder.Decode(document, document.Objects[2929]);
        True(textureBindings.TryGetValue(2929, out SmoTextureBinding? vertexLightBinding),
            "Alfea02 DVlight04 preserves its textureless material binding");
        True(vertexLightBinding!.Texture is null,
            "Alfea02 DVlight04 material deliberately has no bitmap texture");
        SmoMaterialRenderStateInfo vertexLightState =
            vertexLightBinding.MaterialRenderState!;
        Equal(0x2u, vertexLightState.FinalBlendOperation,
            "Alfea02 DVlight04 FinalBlendOp");
        Equal(SmoMaterialBlendMode.OpaqueFinalBlend2, vertexLightState.BlendMode,
            "Alfea02 DVlight04 opaque blend classification");
        True(!vertexLightState.UsesAlphaBlend,
            "Alfea02 DVlight04 does not request source-alpha blending");
        True(SmoVertexColorUsage.HasUniformRgb(vertexLight),
            "Alfea02 DVlight04 stores uniform pale-yellow vertex RGB");
        True(SmoVertexColorUsage.HasAuthoredAlphaGradient(vertexLight),
            "Alfea02 DVlight04 carries an authored vertex-alpha gradient");
        True(SmoVertexColorUsage.ShouldUseVertexAlphaInPreview(
                vertexLight, vertexLightState),
            "Alfea02 DVlight04 keeps vertex alpha in the transparent preview pass");
        Equal(2, vertexLight.DiffuseColorsArgb
                .Select(color => color >> 24).Distinct().Count(),
            "Alfea02 DVlight04 retains two authored vertex alpha values");
        Dictionary<int, string> expectedTextures = new()
        {
            [176] = "noise03w",
            [2355] = "leaf01",
            [2479] = "noise03w",
            [2522] = "book_a"
        };
        foreach ((int meshIndex, string expectedTexture) in expectedTextures)
        {
            SmoMesh mesh = SmoMeshDecoder.Decode(document, document.Objects[meshIndex]);
            True(textureBindings.TryGetValue(meshIndex, out SmoTextureBinding? binding),
                $"Alfea02 mesh [{meshIndex}] has a resolved material binding");
            Equal(expectedTexture, binding!.Texture!.Name,
                $"Alfea02 mesh [{meshIndex}] texture name");
            True(SmoVertexColorUsage.ShouldModulateTexture(mesh),
                $"Alfea02 mesh [{meshIndex}] uses authored vertex tint");
            True(mesh.TextureCoordinates.Any(uv =>
                    uv.X < 0 || uv.X > 1 || uv.Y < 0 || uv.Y > 1),
                $"Alfea02 mesh [{meshIndex}] contains repeating UV coordinates");
            True(Enumerable.Range(0, mesh.TriangleIndices.Length / 3).Any(triangle =>
                {
                    int offset = triangle * 3;
                    Vector2 a = mesh.TextureCoordinates[
                        checked((int)mesh.TriangleIndices[offset])];
                    Vector2 b = mesh.TextureCoordinates[
                        checked((int)mesh.TriangleIndices[offset + 1])];
                    Vector2 c = mesh.TextureCoordinates[
                        checked((int)mesh.TriangleIndices[offset + 2])];
                    return SmoTextureCoordinateTiling.TryGetTriangleTileBounds(
                        a, b, c, out SmoTextureTileBounds bounds) &&
                        (bounds.FirstX != 0 || bounds.LastX != 0 ||
                         bounds.FirstY != 0 || bounds.LastY != 0);
                }),
                $"Alfea02 mesh [{meshIndex}] exposes a non-unit texture tile");
        }

        Dictionary<int, (string Texture, Vector2 Minimum, Vector2 Maximum)>
            expectedAtlasMeshes = new()
            {
                [1741] = ("bed_flora00",
                    new Vector2(0.00694973f, 0.008915961f),
                    new Vector2(0.47267368f, 0.61204004f)),
                [1753] = ("bed_flora00",
                    new Vector2(0.025848622f, 0.6208551f),
                    new Vector2(0.21545331f, 0.9375875f)),
                [2059] = ("carpet01",
                    new Vector2(0.18790337f, 0.9995086f),
                    new Vector2(0.8340063f, 1.443193f))
            };
        foreach ((int meshIndex, var expected) in expectedAtlasMeshes)
        {
            SmoMesh mesh = SmoMeshDecoder.Decode(document, document.Objects[meshIndex]);
            True(textureBindings.TryGetValue(meshIndex, out SmoTextureBinding? binding),
                $"Alfea02 atlas mesh [{meshIndex}] has a resolved material binding");
            Equal(expected.Texture, binding!.Texture!.Name,
                $"Alfea02 atlas mesh [{meshIndex}] texture name");
            True(SmoVertexColorUsage.ShouldModulateTexture(mesh),
                $"Alfea02 atlas mesh [{meshIndex}] uses authored vertex tint");
            Vector2 minimum = new(
                mesh.TextureCoordinates.Min(uv => uv.X),
                mesh.TextureCoordinates.Min(uv => uv.Y));
            Vector2 maximum = new(
                mesh.TextureCoordinates.Max(uv => uv.X),
                mesh.TextureCoordinates.Max(uv => uv.Y));
            True(Vector2.Distance(minimum, expected.Minimum) < 0.00001f,
                $"Alfea02 atlas mesh [{meshIndex}] UV minimum");
            True(Vector2.Distance(maximum, expected.Maximum) < 0.00001f,
                $"Alfea02 atlas mesh [{meshIndex}] UV maximum");
            True(maximum.X - minimum.X < 1 && maximum.Y - minimum.Y < 1,
                $"Alfea02 atlas mesh [{meshIndex}] keeps its authored atlas subregion");
        }

        SmoMesh vertexColoredRoomPiece =
            SmoMeshDecoder.Decode(document, document.Objects[2705]);
        True(!textureBindings.TryGetValue(2705, out SmoTextureBinding? roomBinding) ||
             roomBinding.Texture is null,
            "Alfea02 mesh [2705] deliberately has no bitmap texture binding");
        True(SmoVertexColorUsage.HasUniformRgb(vertexColoredRoomPiece),
            "Alfea02 mesh [2705] stores its uniform pale-yellow RGB in vertices");
        Equal(2, vertexColoredRoomPiece.DiffuseColorsArgb.Distinct().Count(),
            "Alfea02 mesh [2705] has two authored vertex alpha values");
        Equal(2, vertexColoredRoomPiece.DiffuseColorsArgb
                .Select(color => color >> 24).Distinct().Count(),
            "Alfea02 mesh [2705] alpha variation is preserved");
        True(SmoVertexColorUsage.HasAuthoredAlphaGradient(vertexColoredRoomPiece),
            "Alfea02 mesh [2705] uniform RGB plus varying alpha is a light gradient");
        True(MathF.Abs(SmoTextureCoordinateTiling.Wrap(5.25f) - 0.25f) < 0.00001f,
            "repeating texture coordinates wrap into the unit tile");
    }

    private static void CheckAlfea01FoliageTexture(string path)
    {
        const string expectedSha256 =
            "629232691534A2730E97E63A4B2A2B5D26962D9AF5B4DCEBE98D1D945D94A514";
        Equal(
            expectedSha256,
            Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))),
            "Alfea01 pristine fixture identity");

        SmoDocument document = SmoDocument.Load(path);
        Equal(4411, document.Objects.Count, "Alfea01 object count");
        SmoObjectEntry leafEntry = document.Objects[993];
        Equal(SmoClassIds.MeshData, leafEntry.TypeHash,
            "Alfea01 object 993 is foliage mesh data");
        SmoMesh leaf = SmoMeshDecoder.Decode(document, leafEntry);
        Equal(0x0940u, leaf.VertexFormat,
            "Alfea01 foliage mesh vertex format");
        Equal(147, leaf.VertexCount,
            "Alfea01 foliage mesh vertex count");
        Equal(0xFF3EAB00u, leaf.DiffuseColorsArgb.Distinct().Single(),
            "Alfea01 foliage mesh uniform green vertex tint");
        True(SmoVertexColorUsage.HasUniformRgb(leaf),
            "Alfea01 foliage mesh has uniform vertex RGB");
        True(SmoVertexColorUsage.ShouldModulateTexture(leaf),
            "Alfea01 rigid foliage keeps its uniform vertex tint");

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        True(bindings.TryGetValue(993, out SmoTextureBinding? leafBinding),
            "Alfea01 foliage texture binding resolves");
        Equal("leaf01", leafBinding!.Texture!.Name,
            "Alfea01 foliage texture name");
        SmoTextureUvAlphaCoverage alphaCoverage =
            SmoTextureUvAlphaAnalyzer.Analyze(leaf, leafBinding.Texture);
        True(alphaCoverage.IsReliable,
            "Alfea01 foliage repeated UV tile alpha is analyzable");
        Equal(1766, alphaCoverage.FullyTransparentTexelCount,
            "Alfea01 foliage transparent texel count");
        Equal(1073, alphaCoverage.PartialAlphaTexelCount,
            "Alfea01 foliage partial-alpha texel count");
        SmoMaterialRenderStateInfo leafState =
            SmoMaterialRenderState.BindToRenderable(
                leafBinding.MaterialRenderState!, leaf, null, leafBinding.Texture);
        Equal(
            SmoMaterialBlendMode.RigidTextureAlphaSurfaceFinalBlend2,
            leafState.BlendMode,
            "Alfea01 foliage partial-alpha blend classification");
        True(leafState.RequiresTransparentOrdering,
            "Alfea01 foliage renders in the transparent pass");

        SmoMesh door = SmoMeshDecoder.Decode(document, document.Objects[381]);
        Equal(0x0940u, door.VertexFormat,
            "Alfea01 H_DoorB07 vertex format");
        Equal(2126, door.VertexCount,
            "Alfea01 H_DoorB07 vertex count");
        Equal(1432, door.TriangleCount,
            "Alfea01 H_DoorB07 triangle count");
        Equal(199, door.DiffuseColorsArgb.Distinct().Count(),
            "Alfea01 H_DoorB07 authored vertex colour count");
        True(SmoVertexColorUsage.ShouldModulateTexture(door),
            "Alfea01 H_DoorB07 uses authored vertex lighting");
        True(SmoVertexColorUvConflictAnalyzer.HasConflictingSharedCoordinates(door),
            "Alfea01 H_DoorB07 reuses UV coordinates with different RGB");
        True(door.TriangleCount <=
             SmoVertexColorUvConflictAnalyzer.MaximumTriangleAtlasTriangles,
            "Alfea01 H_DoorB07 is eligible for the private triangle atlas");
        True(bindings.TryGetValue(381, out SmoTextureBinding? doorBinding),
            "Alfea01 H_DoorB07 texture binding resolves");
        Equal("noise03b", doorBinding!.Texture!.Name,
            "Alfea01 H_DoorB07 uses its grayscale noise texture");
        Equal(64, doorBinding.Texture.Width,
            "Alfea01 H_DoorB07 noise texture width");
        Equal(64, doorBinding.Texture.Height,
            "Alfea01 H_DoorB07 noise texture height");
        SmoTriangleAtlasResolutionInfo doorAtlas =
            SmoVertexColorUvConflictAnalyzer.GetPreviewTriangleAtlasResolution(
                door, doorBinding.Texture);
        Equal(80, doorAtlas.DesiredCellSize,
            "Alfea01 H_DoorB07 reports authored texel demand");
        Equal(26, doorAtlas.CellSize,
            "Alfea01 H_DoorB07 reports its WPF atlas allocation");
        True(doorAtlas.IsCapacityLimited,
            "Alfea01 H_DoorB07 does not hide preview capacity loss");

        SmoMesh polishedFloor =
            SmoMeshDecoder.Decode(document, document.Objects[3785]);
        Equal(0x0900u, polishedFloor.VertexFormat,
            "Alfea01 centerfloorR vertex format");
        Equal(44, polishedFloor.VertexCount,
            "Alfea01 centerfloorR vertex count");
        Equal(54, polishedFloor.TriangleCount,
            "Alfea01 centerfloorR triangle count");
        Equal(6, polishedFloor.DiffuseColorsArgb.Distinct().Count(),
            "Alfea01 centerfloorR authored vertex colour count");
        Equal(0xD8u, polishedFloor.DiffuseColorsArgb
                .Select(color => color >> 24).Distinct().Single(),
            "Alfea01 centerfloorR uniform partial vertex alpha");
        True(SmoVertexColorUsage.HasUniformPartialAlpha(polishedFloor),
            "Alfea01 centerfloorR is an authored translucent rigid surface");
        True(bindings.TryGetValue(
                3785, out SmoTextureBinding? polishedFloorBinding),
            "Alfea01 centerfloorR shared material binding resolves");
        Equal("floor02", polishedFloorBinding!.Texture!.Name,
            "Alfea01 centerfloorR shared floor texture");
        Equal(2603, polishedFloorBinding.Texture.ObjectIndex,
            "Alfea01 centerfloorR shared texture object index");
        Equal(SmoMaterialBlendMode.OpaqueFinalBlend2,
            polishedFloorBinding.MaterialRenderState!.BlendMode,
            "Alfea01 centerfloorR referenced material blend profile");
        True(SmoVertexColorUsage.ShouldUseVertexAlphaInPreview(
                polishedFloor, polishedFloorBinding.MaterialRenderState),
            "Alfea01 centerfloorR keeps rigid uniform alpha with its material");

        SmoMesh reflectedHall =
            SmoMeshDecoder.Decode(document, document.Objects[3804]);
        True(reflectedHall.Positions.Min(position => position.Y) < -35.0f &&
             reflectedHall.Positions.Max(position => position.Y) > 1.0f,
            "Alfea01 reflected hall geometry extends below centerfloorR");
        True(reflectedHall.Positions.Min(position => position.X) <=
                 polishedFloor.Positions.Min(position => position.X) + 0.1f &&
             reflectedHall.Positions.Max(position => position.X) >=
                 polishedFloor.Positions.Max(position => position.X) - 0.1f,
            "Alfea01 reflected hall geometry overlaps the polished floor span");

        SmoMesh tiledHall =
            SmoMeshDecoder.Decode(document, document.Objects[3795]);
        Equal(71, tiledHall.VertexCount,
            "Alfea01 centerhallL01-001 vertex count");
        Equal(52, tiledHall.TriangleCount,
            "Alfea01 centerhallL01-001 triangle count");
        Equal(18, tiledHall.DiffuseColorsArgb.Distinct().Count(),
            "Alfea01 centerhallL01-001 authored vertex colour count");
        True(SmoVertexColorUvConflictAnalyzer.HasConflictingSharedCoordinates(
                tiledHall),
            "Alfea01 centerhallL01-001 needs a private triangle atlas");
        True(!document.Objects.Any(entry =>
                entry.ParentIndex == 3794 &&
                entry.TypeHash == SmoClassIds.MaterialData),
            "Alfea01 centerhallL01-001 has no duplicated inline material");
        True(bindings.TryGetValue(3795, out SmoTextureBinding? tiledHallBinding),
            "Alfea01 centerhallL01-001 shared material binding resolves");
        Equal("caro00", tiledHallBinding!.Texture!.Name,
            "Alfea01 centerhallL01-001 shared tile texture");
        Equal(2163, tiledHallBinding.Texture.ObjectIndex,
            "Alfea01 centerhallL01-001 shared tile texture object index");
        Equal(SmoMaterialBlendMode.OpaqueFinalBlend0,
            tiledHallBinding.MaterialRenderState!.BlendMode,
            "Alfea01 centerhallL01-001 referenced material blend profile");

        Equal(SmoClassIds.MaterialData, document.Objects[2756].TypeHash,
            "Alfea01 Garch_B01 object 2756 is its material");
        SmoMesh archLabel =
            SmoMeshDecoder.Decode(document, document.Objects[2757]);
        Equal(60, archLabel.VertexCount,
            "Alfea01 Garch_B01 label vertex count");
        Equal(30, archLabel.TriangleCount,
            "Alfea01 Garch_B01 label triangle count");
        Equal(4, archLabel.TextureCoordinates.Distinct().Count(),
            "Alfea01 Garch_B01 repeated full-tile UV count");
        True(SmoVertexColorUvConflictAnalyzer.HasConflictingSharedCoordinates(
                archLabel),
            "Alfea01 Garch_B01 keeps independent triangle vertex colours");
        True(bindings.TryGetValue(2757, out SmoTextureBinding? archLabelBinding),
            "Alfea01 Garch_B01 texture binding resolves");
        Equal("noise03b", archLabelBinding!.Texture!.Name,
            "Alfea01 Garch_B01 source texture");
        Equal(64, archLabelBinding.Texture.Width,
            "Alfea01 Garch_B01 source texture width");
        SmoTriangleAtlasResolutionInfo archLabelAtlas =
            SmoVertexColorUvConflictAnalyzer.GetPreviewTriangleAtlasResolution(
                archLabel, archLabelBinding.Texture);
        Equal(69, archLabelAtlas.DesiredCellSize,
            "Alfea01 Garch_B01 preserves source texel span plus atlas padding");
        Equal(69, archLabelAtlas.CellSize,
            "Alfea01 Garch_B01 receives its authored texel demand");
        True(!archLabelAtlas.IsCapacityLimited,
            "Alfea01 Garch_B01 preview is not capacity limited");
    }

    private static void CheckAlfeaBroken01Materials(string path)
    {
        const string expectedSha256 =
            "BF3EE78D9299BAC6834D5B07D9BD2AE78D309F289D56CDA224857B362A676CA6";
        Equal(
            expectedSha256,
            Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))),
            "Alfea_broken_01 pristine fixture identity");

        SmoDocument document = SmoDocument.Load(path);
        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);

        SmoMesh beam = SmoMeshDecoder.Decode(document, document.Objects[1078]);
        True(bindings.TryGetValue(1078, out SmoTextureBinding? beamBinding),
            "Alfea_broken_01 beam [1078] material binding resolves");
        Equal(43, beamBinding!.Texture!.ObjectIndex,
            "Alfea_broken_01 beam [1078] marble texture object");
        Equal("marble2", beamBinding.Texture.Name,
            "Alfea_broken_01 beam [1078] marble texture name");
        True(beam.HasTextureCoordinates &&
             SmoVertexColorUsage.ShouldModulateTexture(beam),
            "Alfea_broken_01 beam [1078] passes UV and authored diffuse to GPU");

        SmoMesh beamLines =
            SmoMeshDecoder.Decode(document, document.Objects[1082]);
        True(bindings.TryGetValue(1082, out SmoTextureBinding? beamLinesBinding),
            "Alfea_broken_01 paired beam detail material resolves");
        Equal("linegen00", beamLinesBinding!.Texture!.Name,
            "Alfea_broken_01 paired beam detail uses line texture");

        foreach (int lightIndex in new[] { 1005, 1018, 1022 })
        {
            SmoMesh light =
                SmoMeshDecoder.Decode(document, document.Objects[lightIndex]);
            True(bindings.TryGetValue(
                    lightIndex, out SmoTextureBinding? lightBinding),
                $"Alfea_broken_01 light [{lightIndex}] material binding resolves");
            True(lightBinding!.Texture is null,
                $"Alfea_broken_01 light [{lightIndex}] is vertex-only");
            SmoMaterialRenderStateInfo lightState =
                SmoMaterialRenderState.BindToRenderable(
                    lightBinding.MaterialRenderState!, light, null);
            Equal(0x2u, lightState.FinalBlendOperation,
                $"Alfea_broken_01 light [{lightIndex}] FinalBlendOp");
            Equal(2u, lightState.CompanionBlendState!.Value,
                $"Alfea_broken_01 light [{lightIndex}] companion state");
            Equal(2, light.DiffuseColorsArgb
                    .Select(color => color & 0x00FFFFFF).Distinct().Count(),
                $"Alfea_broken_01 light [{lightIndex}] mixed authored RGB");
            Equal(2, light.DiffuseColorsArgb
                    .Select(color => color >> 24).Distinct().Count(),
                $"Alfea_broken_01 light [{lightIndex}] alpha ramp values");
            True(SmoVertexColorUsage.HasAuthoredAlphaGradient(light, lightState),
                $"Alfea_broken_01 light [{lightIndex}] mixed-RGB alpha is authored");
            True(SmoVertexColorUsage.ShouldUseVertexAlphaInPreview(
                    light, lightState),
                $"Alfea_broken_01 light [{lightIndex}] keeps vertex alpha");
        }

        SmoMesh bakedFloor =
            SmoMeshDecoder.Decode(document, document.Objects[44]);
        SmoMaterialRenderStateInfo bakedFloorState =
            SmoMaterialRenderState.BindToRenderable(
                bindings[44].MaterialRenderState!,
                bakedFloor,
                null,
                bindings[44].Texture);
        True(!SmoVertexColorUsage.HasAuthoredAlphaGradient(
                bakedFloor, bakedFloorState),
            "Alfea_broken_01 ordinary mixed-RGB baked floor is not made transparent");

        SmoMesh wall =
            SmoMeshDecoder.Decode(document, document.Objects[4085]);
        Equal(0x1900u, wall.VertexFormat,
            "Alfea_broken_01 WallA [4085] vertex format");
        Equal(32, wall.Stride,
            "Alfea_broken_01 WallA [4085] serialized stride");
        True(wall.HasDiffuseColors,
            "Alfea_broken_01 WallA [4085] decodes authored vertex diffuse");
        True(wall.HasTextureCoordinates && wall.HasTextureCoordinates1,
            "Alfea_broken_01 WallA [4085] decodes UV0 and UV1");
        True(!wall.HasNormals,
            "Alfea_broken_01 WallA [4085] 0x1900 has no normal stream");
        Equal(303, wall.DiffuseColorsArgb
                .Select(color => color & 0x00FFFFFF).Distinct().Count(),
            "Alfea_broken_01 WallA [4085] authored RGB count");
        True(SmoVertexColorUsage.ShouldModulateTexture(wall),
            "Alfea_broken_01 WallA [4085] uses authored diffuse instead of viewer fallback blue");
        True(bindings.TryGetValue(4085, out SmoTextureBinding? wallBinding),
            "Alfea_broken_01 WallA [4085] material binding resolves");
        True(wallBinding!.Texture is null,
            "Alfea_broken_01 WallA [4085] is intentionally vertex-colour-only");
        True(!SmoVertexColorUsage.ShouldUseVertexAlphaInPreview(
                wall, wallBinding.MaterialRenderState),
            "Alfea_broken_01 WallA [4085] opaque material ignores auxiliary diffuse alpha bytes");
        long wallRed = wall.DiffuseColorsArgb.Sum(
            color => (long)((color >> 16) & 0xFF));
        long wallBlue = wall.DiffuseColorsArgb.Sum(
            color => (long)(color & 0xFF));
        True(wallRed > wallBlue,
            "Alfea_broken_01 WallA [4085] authored tint is warm rather than viewer blue");
    }

    private static void CheckAlfea03SharedMeshInstances(string path)
    {
        const string expectedSha256 =
            "65D0EF30F2AC4C211F8C717F340D08A98F2CE4F1D6E080CE77BC217468350FDF";
        Equal(
            expectedSha256,
            Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))),
            "Alfea03 pristine fixture identity");

        SmoDocument document = SmoDocument.Load(path);
        Equal(4825, document.Objects.Count, "Alfea03 object count");
        Equal(510, document.Objects.Count(entry =>
            entry.TypeHash == SmoClassIds.MeshData),
            "Alfea03 physical mesh count");

        IReadOnlyList<SmoSharedMeshInstanceInfo> instances =
            SmoSharedMeshInstanceResolver.ResolveAll(document);
        Equal(757, instances.Count,
            "Alfea03 reference-only shared mesh instance count");
        Equal(156, instances.Select(item => item.SourceMeshObjectIndex)
                .Distinct().Count(),
            "Alfea03 shared physical mesh source count");

        SmoSharedMeshInstanceInfo[] lockers = instances
            .Where(item => item.SourceMeshObjectIndex == 2674)
            .OrderBy(item => item.ModelObjectIndex)
            .ToArray();
        Equal(21, lockers.Length,
            "Alfea03 casierD shared instances beside the physical mesh");
        True(lockers.All(item =>
                item.SourceMeshObjectId == 2675 &&
                item.MaterialObjectIndex.HasValue),
            "Alfea03 casierD instances retain mesh ID and local material");
        Equal(22, lockers.Length + 1,
            "Alfea03 casierD total placements including physical casierD16");

        SmoSharedMeshInstanceInfo casierD02 = lockers.Single(item =>
            item.StaticObjectName.Equals("casierD02", StringComparison.Ordinal));
        Equal(4229, casierD02.StaticObjectIndex,
            "Alfea03 casierD02 static object index");
        Equal(4230, casierD02.ModelObjectIndex,
            "Alfea03 casierD02 reference-only model index");
        True(MathF.Abs(casierD02.WorldTransform.M41 - 2021.21106f) < 0.001f &&
             MathF.Abs(casierD02.WorldTransform.M42 - 99.6073761f) < 0.001f &&
             MathF.Abs(casierD02.WorldTransform.M43 - 422.180084f) < 0.001f,
            "Alfea03 casierD02 authored level placement");

        SmoSharedMeshInstanceInfo casierD01 = lockers.Single(item =>
            item.StaticObjectName.Equals("casierD01", StringComparison.Ordinal));
        True(MathF.Abs(casierD01.WorldTransform.M43 - 470.084717f) < 0.001f,
            "Alfea03 casierD01 authored wall position");
        True(MathF.Abs(
                casierD01.WorldTransform.M43 - casierD02.WorldTransform.M43 -
                47.904633f) < 0.001f,
            "Alfea03 casierD wall spacing is serialized, not runtime generated");

        True(lockers.All(instance => !document.Objects.Any(entry =>
                entry.ParentIndex == instance.ModelObjectIndex &&
                entry.TypeHash == SmoClassIds.MeshData)),
            "Alfea03 shared instance models contain no duplicated physical mesh");

        True(SmoTextureDecoder.TryDecode(
                document,
                document.Objects[656],
                out SmoTexture? mipmappedTop,
                out string mipmappedTopError),
            $"Alfea03 mipmapped shelf texture decodes: {mipmappedTopError}");
        Equal((ushort)0x0EE3, mipmappedTop!.FormatCode,
            "Alfea03 shelf texture exact mipmapped BGRA format");
        Equal(256, mipmappedTop.Width,
            "Alfea03 shelf texture base-level width");
        Equal(256, mipmappedTop.Height,
            "Alfea03 shelf texture base-level height");
        Equal(
            "61B427309F78A161B7D710DCDD293A9A2FBD4955DCC15FFCE097894DF23805F2",
            Convert.ToHexString(SHA256.HashData(mipmappedTop.Bgra32Pixels.Span)),
            "Alfea03 shelf texture decoded base-level pixels");

        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        True(bindings.TryGetValue(4603, out SmoTextureBinding? crystalBinding),
            "Alfea03 crystal mesh has a material binding");
        Equal(4600, crystalBinding!.BaseTexture!.ObjectIndex,
            "Alfea03 crystal UV0 base texture");
        Equal("crystal2", crystalBinding.BaseTexture.Name,
            "Alfea03 crystal base texture name");
        Equal(4601, crystalBinding.Texture!.ObjectIndex,
            "Alfea03 crystal UV1 highlight texture");
        Equal("cryst_hl", crystalBinding.Texture.Name,
            "Alfea03 crystal highlight texture name");
        True(crystalBinding.Issue is null &&
             crystalBinding.AnimationFrames is null,
            "Alfea03 static two-layer crystal is not ambiguous or animated");
        SmoMesh crystalMesh =
            SmoMeshDecoder.Decode(document, document.Objects[4603]);
        True(crystalMesh.HasTextureCoordinates &&
             crystalMesh.HasTextureCoordinates1,
            "Alfea03 crystal carries UV0 and UV1 for the two layers");

        foreach (int meshIndex in new[] { 26, 29 })
        {
            SmoMesh mesh = SmoMeshDecoder.Decode(document, document.Objects[meshIndex]);
            SmoTextureBinding binding = bindings[meshIndex];
            SmoMaterialRenderStateInfo state =
                SmoMaterialRenderState.BindToRenderable(
                    binding.MaterialRenderState!, mesh, null, binding.Texture);
            Equal(SmoMaterialBlendMode.EffectFinalBlend4, state.BlendMode,
                $"Alfea03 firefly mesh [{meshIndex}] effect blend family");
            True(state.Diagnostic?.StartsWith(
                    "MATERIAL_EFFECT_BLEND_4:", StringComparison.Ordinal) == true &&
                 state.LoadIssueDiagnostic is null,
                $"Alfea03 firefly mesh [{meshIndex}] approximation is not a load issue");
        }

        foreach (int meshIndex in new[] { 273, 278, 294, 298, 313, 317 })
        {
            SmoMesh mesh = SmoMeshDecoder.Decode(document, document.Objects[meshIndex]);
            SmoTextureBinding binding = bindings[meshIndex];
            SmoMaterialRenderStateInfo state =
                SmoMaterialRenderState.BindToRenderable(
                    binding.MaterialRenderState!, mesh, null, binding.Texture);
            True(state.ConsumerKind == SmoMaterialConsumerKind.RigidOrEffect &&
                 state.HasConfirmedConsumerTuple &&
                 !state.HasConsumerStateMismatch,
                $"Alfea03 book effect mesh [{meshIndex}] confirmed rigid tuple variant");
            True(state.UsesEmissiveApproximation &&
                 state.LoadIssueDiagnostic is null,
                $"Alfea03 book effect mesh [{meshIndex}] has a clean effect preview state");
        }

        SmoMesh faragondaSign =
            SmoMeshDecoder.Decode(document, document.Objects[2756]);
        True(bindings.TryGetValue(2756, out SmoTextureBinding? signBinding),
            "Alfea03 plaque02 sign texture binding resolves");
        Equal("sign_faragonda", signBinding!.Texture!.Name,
            "Alfea03 plaque02 sign source texture");
        Equal(128, signBinding.Texture.Width,
            "Alfea03 plaque02 sign source width");
        True(SmoVertexColorUvConflictAnalyzer.HasConflictingSharedCoordinates(
                faragondaSign),
            "Alfea03 plaque02 sign preserves independent vertex colours");
        SmoTriangleAtlasResolutionInfo faragondaAtlas =
            SmoVertexColorUvConflictAnalyzer.GetPreviewTriangleAtlasResolution(
                faragondaSign, signBinding.Texture);
        Equal(133, faragondaAtlas.DesiredCellSize,
            "Alfea03 plaque02 sign preserves full horizontal source detail");
        Equal(133, faragondaAtlas.CellSize,
            "Alfea03 plaque02 receives its authored texel demand");
        True(!faragondaAtlas.IsCapacityLimited,
            "Alfea03 plaque02 preview is not capacity limited");
    }

    private static float MatrixDifference(Matrix4x4 left, Matrix4x4 right)
    {
        ReadOnlySpan<float> leftValues =
        [
            left.M11, left.M12, left.M13, left.M14,
            left.M21, left.M22, left.M23, left.M24,
            left.M31, left.M32, left.M33, left.M34,
            left.M41, left.M42, left.M43, left.M44
        ];
        ReadOnlySpan<float> rightValues =
        [
            right.M11, right.M12, right.M13, right.M14,
            right.M21, right.M22, right.M23, right.M24,
            right.M31, right.M32, right.M33, right.M34,
            right.M41, right.M42, right.M43, right.M44
        ];
        float maximum = 0;
        for (int index = 0; index < leftValues.Length; index++)
            maximum = MathF.Max(maximum, MathF.Abs(leftValues[index] - rightValues[index]));
        return maximum;
    }

    private static void WriteUInt32(byte[] data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data.AsSpan(offset, sizeof(uint)), value);

    private static bool IsFinite(Matrix4x4 matrix) =>
        float.IsFinite(matrix.M11) && float.IsFinite(matrix.M12) &&
        float.IsFinite(matrix.M13) && float.IsFinite(matrix.M14) &&
        float.IsFinite(matrix.M21) && float.IsFinite(matrix.M22) &&
        float.IsFinite(matrix.M23) && float.IsFinite(matrix.M24) &&
        float.IsFinite(matrix.M31) && float.IsFinite(matrix.M32) &&
        float.IsFinite(matrix.M33) && float.IsFinite(matrix.M34) &&
        float.IsFinite(matrix.M41) && float.IsFinite(matrix.M42) &&
        float.IsFinite(matrix.M43) && float.IsFinite(matrix.M44);

    private static string GetTextureSample(SmoTexture texture, Vector2 uv)
    {
        int x = Math.Clamp((int)MathF.Round(uv.X * (texture.Width - 1)), 0, texture.Width - 1);
        int y = Math.Clamp((int)MathF.Round(uv.Y * (texture.Height - 1)), 0, texture.Height - 1);
        int offset = (y * texture.Width + x) * 4;
        ReadOnlySpan<byte> pixels = texture.Bgra32Pixels.Span;
        return $"({pixels[offset]},{pixels[offset + 1]},{pixels[offset + 2]},{pixels[offset + 3]})";
    }

    private sealed record CorpusOptions(string? Path, int? SampleCount, int Seed);

    private sealed record FixtureRenderable(
        SmoMesh Mesh,
        SmoSkin? Skin,
        SmoMaterialRenderStateInfo State,
        Matrix4x4 WorldTransform);

    private static void True(bool condition, string name)
    {
        _assertionCount++;
        if (!condition)
            throw new InvalidOperationException($"Assertion failed: {name}");
    }

    private static void Equal<T>(T expected, T actual, string name)
        where T : notnull
    {
        _assertionCount++;
        if (!EqualityComparer<T>.Default.Equals(expected, actual))
            throw new InvalidOperationException(
                $"Assertion failed: {name}; expected {expected}, actual {actual}");
    }
}
