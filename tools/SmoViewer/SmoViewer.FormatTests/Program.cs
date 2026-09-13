using System.Buffers.Binary;
using System.Numerics;
using System.Security.Cryptography;
using System.Text;
using SmoViewer.Corpus;
using SmoViewer.Core;

namespace SmoViewer.FormatTests;

internal static class Program
{
    private static int _assertionCount;

    public static int Main(string[] args)
    {
        try
        {
            if (args.Length == 3 && args[0] == "--picking-provider")
                return SmoScenePickingProviderRegression.Run(args[1], args[2]);
            if (args.Length == 5 && args[0] == "--texture-source")
                return SmoTextureSourceRegression.Run(args[1], args[2], args[3], args[4]);
            if (args.Length is 3 or 4 && args[0] == "--texture-platform-dispatch")
                return SmoTextureSourceRegression.RunPlatformDispatch(args[1], args[2], args.Length == 4 ? args[3] : null);
            if (args.Length == 3 && args[0] == "--ps2-texture-metadata")
                return SmoTextureSourceRegression.RunPs2(args[1], args[2]);
            if (args.Length == 3 && args[0] == "--text-inspection")
                return SmoTextInspectionRegression.Run(args[1], args[2]);
            if (args.Length == 3 && args[0] == "--text-runtime")
                return SmoTextRuntimeRegression.Run(args[1],args[2]);
            if (args.Length == 3 && args[0] == "--occlusion-buffer-inspection")
            {
                TestOcclusionVolumeDecoder();
                Console.WriteLine($"PASS strict occlusion profile: {_assertionCount} assertions");
                return SmoOcclusionBufferInspectionRegression.Run(args[1], args[2]);
            }
            if (args.Length == 3 && args[0] == "--font-inspection")
                return SmoFontInspectionRegression.Run(args[1], args[2]);
            if (args.Length == 3 && args[0] == "--bv-inspection")
                return SmoBoundingVolumeInspectionRegression.Run(args[1], args[2]);
            if (args.Length == 3 && args[0] == "--spatial-inspection")
                return SmoSpatialInspectionRegression.Run(args[1],args[2]);
            if (args.Length == 3 && args[0] == "--light-inspection")
                return SmoLightInspectionRegression.Run(args[1],args[2]);
            if (args.Length == 3 && args[0] == "--fog-inspection")
                return SmoFogInspectionRegression.Run(args[1],args[2]);
            if (args.Length == 3 && args[0] == "--lens-readers")
                return SmoLensFlareRegression.Run(args[1],args[2]);
            if (args.Length == 4 && args[0] == "--particle-init")
                return SmoParticleReaderRegression.RunInitialization(args[1],args[2],args[3]);
            if (args.Length == 3 && args[0] == "--particle-readers")
            {
                TestParticleRegionMemberOrder();
                return SmoParticleReaderRegression.Run(args[1],args[2]);
            }
            if (args.Length == 3 && args[0] == "--navigation-readers")
            {
                TestMeshNavigationSetDecoder();
                return SmoNavigationReaderRegression.Run(args[1],args[2]);
            }
            if (args.Length == 3 && args[0] == "--skybox-readers")
                return SmoSkyBoxReaderRegression.Run(args[1], args[2]);
            if (args.Length == 3 && args[0] == "--octree-readers")
                return SmoOctreeReaderRegression.Run(args[1], args[2]);
            if (args.Length == 3 && args[0] == "--material-runtime")
                return SmoMaterialRuntimeRegression.Run(args[1], args[2]);
            if (args.Length == 3 && args[0] == "--material-draw")
                return SmoMaterialDrawRegression.Run(args[1], args[2]);
            if (args.Length == 3 && args[0] == "--loaded-resources")
                return SmoLoadedResourceRegression.Run(args[1], args[2]);
            if (args.Length == 3 && args[0] == "--render-occurrences")
                return SmoRenderOccurrenceRegression.Run(args[1], args[2]);
            if (args.Length is 3 or 4 && args[0] == "--loaded-scene")
                return SmoLoadedSceneRegression.Run(args[1], args[2], args.Length == 4 ? args[3] : null);
            if (args.Length == 3 && args[0] == "--san-keys")
                return SmoAnimationRegression.Run(args[1], args[2]);
            TestSyntheticDocument();
            TestNativeContainerInspection();
            TestCorpusDatabase();
            TestKnownSerializedFieldRegistry();
            TestSharedNodeScalars();
            TestSharedNodeWorld();
            TestSharedCollisionScalars();
            TestSharedStaticMatrices();
            TestFieldMutationTransaction();
            TestPlacementTransformWriter();
            TestCollisionMeshDecoder();
            // Spatial state uses the actual graph; former metadata-only fixtures
            // contained incomplete dummy objects. Use --spatial-inspection for
            // the explicit native/original-backed spatial acceptance slice.
            TestOcclusionVolumeDecoder();
            TestMeshNavigationSetDecoder();
            TestDataBlockHeaders();
            TestGuiStateClassification();
            TestMaterialRenderStates();
            TestSyntheticMaterialData();
            TestSyntheticMeshData();
            TestNativeMeshBuffers();
            TestSyntheticModel();
            TestNativeModelSkinReaders();
            TestNativeAnimatedTextureReader();
            TestParticleRegionMemberOrder();
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

    private static void TestNativeContainerInspection()
    {
        const uint unknown = 0xDEADBEEF;
        byte[] body = [..BitConverter.GetBytes(unknown), .."SBOO"u8, 0];
        byte[] bytes = CreateSingleObjectDocument("raw", unknown, body);
        SmoDocument raw = SmoDocument.Parse(bytes);
        Equal(unknown, raw.Objects[0].TypeHash, "unknown RTTI remains inspectable without a runtime factory");
        True(SmoLoadedResources.Get(raw).LoadIssue is not null,
            "actual resource graph refuses unknown RTTI without substituting metadata objects");
        True(raw.Objects[0].SignatureMatches, "shared serializer object header reader");
        Equal(SmoHeaderValidationStatus.Valid, raw.Header.NativeValidationStatus, "original header validation status");
        True(raw.Objects[0].RawName.Span.SequenceEqual("raw\0"u8), "native FAT preserves exact stored name extent");
        Equal(32, raw.Objects[0].TableOffset, "native FAT source offset");
        WriteUInt32(bytes, 4, 0x99);
        SmoDocument wrongVersion = SmoDocument.Parse(bytes);
        Equal(SmoHeaderValidationStatus.WrongVersion, wrongVersion.Header.NativeValidationStatus,
            "diagnostic inspection preserves original rejection status");
        WriteUInt32(bytes, 4, 0x26);
        WriteUInt32(bytes, 0x10, 8);
        Equal(SmoHeaderValidationStatus.Valid, SmoDocument.Parse(bytes).Header.NativeValidationStatus, "PS2 metadata inspection");
        WriteUInt32(bytes, checked((int)raw.Header.DataStart), SmoClassIds.Node);
        SmoDocument mismatch = SmoDocument.Parse(bytes);
        True(!mismatch.Objects[0].SignatureMatches && mismatch.Diagnostics.Any(d => d.Code == "OBJECT_SIGNATURE_MISMATCH"),
            "native signature observation keeps diagnostic mismatch");
        void Refuses(byte[] invalid, string label)
        {
            bool refused = false;
            try { SmoDocument.Parse(invalid); } catch (SmoFormatException) { refused = true; }
            True(refused, label);
        }
        Refuses(bytes[..35], "truncated container refuses cleanly");
        var broken = (byte[])bytes.Clone();
        WriteUInt32(broken, 0x1C, uint.MaxValue);Refuses(broken, "oversized FAT count refuses before allocation");
        broken = (byte[])bytes.Clone();
        BinaryPrimitives.WriteUInt16LittleEndian(broken.AsSpan(36,2), ushort.MaxValue);
        Refuses(broken, "name cannot read past FAT into object payload");
        broken = (byte[])bytes.Clone();
        WriteUInt32(broken, raw.Header.TerminatorOffset, 1);Refuses(broken, "unsupported external-file index is explicit");
        // A failed native inspect must release its temporary owners and leave
        // the next independent call usable.
        Equal(1, SmoDocument.Parse(bytes).Objects.Count, "native inspect survives prior refusals");
    }

    private static void TestParticleRegionMemberOrder()
    {
        foreach (int field in new[] {17,18})
        {
            var body=new List<byte>();
            body.AddRange(BitConverter.GetBytes(SmoClassIds.ParticleSystem));body.AddRange("SBOO"u8.ToArray());body.Add(0);
            body.AddRange(SmoDataBlockWriter.BuildField(0,new byte[24]));
            body.AddRange(SmoDataBlockWriter.BuildField(4,[..BitConverter.GetBytes(1f),..BitConverter.GetBytes(1f)]));
            body.AddRange(SmoDataBlockWriter.BuildField(11,BitConverter.GetBytes(7f)));
            body.AddRange(SmoDataBlockWriter.BuildField(7,new byte[]{0})); // real non-looping reader initialization
            float[] region=field==17?[1,2,3,9,2]:[1,2,3,9,2,4];
            byte[] regionBytes=region.SelectMany(BitConverter.GetBytes).ToArray();
            body.AddRange(SmoDataBlockWriter.BuildField(field,regionBytes));
            body.Add(0);
            var fixture=CreateSingleObjectDocument("particle",SmoClassIds.ParticleSystem,body.ToArray());
            WriteUInt32(fixture,0x10,2); // PC platform, independently of resource count
            var document=SmoDocument.Parse(fixture);
            True(SmoParticleSystemDecoder.TryDecode(document,document.Objects[0],out var decoded,out string error),"Particle member-order fixture: "+error);
            if(field==17)
            {
                var cylinder=(SmoParticleCylinderRegion)decoded!.Region;
                Equal(9f,cylinder.Height,"PC writer stores cylinder height first");Equal(2f,cylinder.Radius,"PC writer stores cylinder radius second");
            }
            else
            {
                var cone=(SmoParticleConeRegion)decoded!.Region;
                Equal(9f,cone.Height,"PC writer stores cone height first");Equal(2f,cone.Radius1,"PC writer stores cone radius1 second");Equal(4f,cone.Radius2,"PC writer stores cone radius2 third");
            }
        }
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

    private static void TestCorpusDatabase()
    {
        if (!OperatingSystem.IsWindows())
        {
            Console.WriteLine(
                "Corpus database test skipped: winsqlite3 is Windows-only.");
            return;
        }

        string directory = Path.Combine(
            Path.GetTempPath(), $"smo-corpus-test-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            string sourcePath = Path.Combine(directory, "synthetic.smo");
            string databasePath = Path.Combine(directory, "corpus.sqlite");
            File.WriteAllBytes(sourcePath, CreateSyntheticDocument());

            SmoCorpusUpdateResult initial = SmoCorpusDatabase.Update(
                databasePath, sourcePath);
            Equal(1, initial.ScannedFiles,
                "corpus database initially scans the source");
            Equal(0, initial.UnchangedFiles,
                "corpus database has no unchanged source on first scan");
            Equal(1L, initial.Objects,
                "corpus database stores synthetic object");
            Equal(1L, initial.DirectFields,
                "corpus database stores synthetic direct field terminator");
            Equal(0, initial.UnknownClasses,
                "corpus database resolves the synthetic class");

            SmoCorpusUpdateResult repeat = SmoCorpusDatabase.Update(
                databasePath, sourcePath);
            Equal(0, repeat.ScannedFiles,
                "corpus database skips unchanged source");
            Equal(1, repeat.UnchangedFiles,
                "corpus database records unchanged source");

            SmoCorpusSummary summary = SmoCorpusDatabase.GetSummary(databasePath);
            Equal(1, summary.Files, "corpus database summary file count");
            Equal(1L, summary.Objects, "corpus database summary object count");
            Equal(131, summary.FieldDefinitions,
                "corpus database seeds recovered field definitions");
            IReadOnlyList<SmoCorpusClassSummary> classes =
                SmoCorpusDatabase.GetClasses(databasePath);
            Equal(1, classes.Count,
                "corpus database class query returns observed classes only");
            Equal(SmoClassIds.Node, classes[0].TypeHash,
                "corpus database class query preserves type hash");
            Equal(1L, classes[0].ObjectCount,
                "corpus database class query aggregates objects");
            IReadOnlyList<SmoCorpusClassMetrics> metrics =
                SmoCorpusDatabase.GetClassMetrics(databasePath);
            Equal(1, metrics.Count,
                "corpus database metrics return observed classes only");
            Equal(1, metrics[0].StructuralVariantCount,
                "corpus database metrics aggregate structural signatures");
            Equal(0L, metrics[0].FieldParseErrors,
                "corpus database metrics expose field parse failures");

            string gameDirectory = Path.Combine(directory, "game");
            string pckDirectory = Path.Combine(gameDirectory, "pck");
            Directory.CreateDirectory(pckDirectory);
            string pckPath = Path.Combine(pckDirectory, "TEST.PCK");
            File.WriteAllBytes(pckPath, CreateSyntheticPck(CreateSyntheticDocument()));
            string executablePath = Path.Combine(gameDirectory, "test.exe");
            byte[] syntheticElf = new byte[52];
            syntheticElf[0] = 0x7F;
            syntheticElf[1] = (byte)'E';
            syntheticElf[2] = (byte)'L';
            syntheticElf[3] = (byte)'F';
            syntheticElf[4] = 1;
            syntheticElf[5] = 1;
            BinaryPrimitives.WriteUInt16LittleEndian(syntheticElf.AsSpan(18), 8);
            File.WriteAllBytes(executablePath, syntheticElf);

            SparkplugPckArchive archive = SparkplugPckArchive.Load(pckPath);
            Equal(1, archive.Entries.Count, "synthetic PCK entry count");
            Equal("data/test/synthetic.smo", archive.Entries[0].LogicalPath,
                "synthetic PCK logical path");
            using (var stream = new FileStream(
                       pckPath, FileMode.Open, FileAccess.Read, FileShare.Read))
            {
                True(SparkplugPckArchive.ReadEntry(stream, archive.Entries[0])
                        .SequenceEqual(CreateSyntheticDocument()),
                    "synthetic PCK entry payload");
            }

            string researchPath = Path.Combine(directory, "research.sqlite");
            SmoResearchUpdateResult research =
                SmoResearchDatabase.UpdatePckDirectory(
                    researchPath, "synthetic-pc", "pc", "synthetic",
                    pckDirectory, executablePath);
            Equal(2L, research.Occurrences,
                "research database records PCK and executable occurrences");
            Equal(1L, research.SmoResources,
                "research database records unique PCK SMO");
            Equal(1, research.ParsedSmo,
                "research database parses PCK SMO");
            Equal(0, research.PlatformConflicts,
                "research database validates corpus platform tag");
            SmoResearchUpdateResult researchRepeat =
                SmoResearchDatabase.UpdatePckDirectory(
                    researchPath, "synthetic-pc", "pc", "synthetic",
                    pckDirectory, executablePath);
            Equal(0, researchRepeat.ScannedResources,
                "research database skips unchanged PCK");
            Equal(2, researchRepeat.UnchangedResources,
                "research database reports unchanged PCK and executable resources");
            SmoResearchSummary researchSummary =
                SmoResearchDatabase.GetSummary(researchPath);
            Equal(5, researchSummary.SchemaVersion,
                "research database exposes native-knowledge schema v5");
            Equal(1, researchSummary.Corpora.Count,
                "research database summary corpus count");
            Equal(1, researchSummary.PcOnlyClasses,
                "research database summary platform class presence");
            Equal(1, SmoResearchDatabase.GetClassCounts(researchPath).Count,
                "research database class count query");
            SmoResearchClassReport classReport = SmoResearchDatabase.GetClassReport(
                researchPath, "spNode");
            Equal(SmoClassIds.Node, classReport.TypeHash,
                "research database class report resolves engine name");
            Equal(1, classReport.Profiles.Count,
                "research database class report profiles");
            Equal(1, classReport.Resources.Count,
                "research database class report resources");
            Equal(0, classReport.Counterparts.Count,
                "research database class report cross-platform counterparts");
            Equal(1, classReport.Relations.Count,
                "research database class report hierarchy relations");
            Equal(1, SmoResearchDatabase.FindResources(
                    researchPath, "SYNTHETIC.SMO").Count,
                "research database resource search is case-insensitive");
            Equal(1, classReport.Variants.Count,
                "research database class report variants");
            Equal(1, classReport.Examples.Count,
                "research database class report examples");
            Equal(0, SmoResearchDatabase.GetPlatformConflicts(researchPath).Count,
                "research database conflict query");
            Equal("ok", SmoResearchDatabase.CheckIntegrity(researchPath),
                "research database integrity");
            GameResourceAudit resourceAudit =
                SmoResearchDatabase.GetResourceAudit(researchPath);
            Equal(2L, resourceAudit.TotalResources,
                "resource database includes archived SMO and executable");
            Equal(2L, resourceAudit.AssignedResources,
                "every resource has a format assignment");
            Equal(0L, resourceAudit.UnassignedResources,
                "resource database leaves no unclassified rows");
            Equal(0L, resourceAudit.AnalysisErrors,
                "synthetic resource analysis has no errors");
            True(SmoResearchDatabase.GetResourceFormats(researchPath)
                    .Any(item => item.FormatKey == "elf" && item.UniqueResourceVersions == 1),
                "resource database recognizes ELF by signature");

            string directoryGame = Path.Combine(directory, "directory-game");
            string mediaDirectory = Path.Combine(directoryGame, "Media", "Sounds");
            Directory.CreateDirectory(mediaDirectory);
            File.WriteAllText(Path.Combine(mediaDirectory, "test.snc"),
                "event, foo.wav;\n");
            File.WriteAllBytes(Path.Combine(mediaDirectory, "foo.wav"),
                Encoding.ASCII.GetBytes("RIFFsynthetic"));
            byte[] syntheticWxt = new byte[20];
            WriteUInt32(syntheticWxt, 0, 2);
            WriteUInt32(syntheticWxt, 4, 12);
            WriteUInt32(syntheticWxt, 8, 16);
            Encoding.ASCII.GetBytes("one\0two\0").CopyTo(syntheticWxt, 12);
            File.WriteAllBytes(Path.Combine(mediaDirectory, "strings.wxt"), syntheticWxt);
            byte[] selfDelimitedWxt = new byte[26];
            WriteUInt32(selfDelimitedWxt, 0, 12);
            WriteUInt32(selfDelimitedWxt, 4, 16);
            WriteUInt32(selfDelimitedWxt, 8, 20);
            Encoding.ASCII.GetBytes("one\0two\0three\0").CopyTo(selfDelimitedWxt, 12);
            File.WriteAllBytes(Path.Combine(mediaDirectory, "strings-self.wxt"),
                selfDelimitedWxt);
            string directoryExecutable = Path.Combine(directoryGame, "game.elf");
            File.WriteAllBytes(directoryExecutable, syntheticElf);
            string resourceDatabase = Path.Combine(directory, "resource-directory.sqlite");
            SmoResearchDatabase.UpdateDirectory(
                resourceDatabase, "synthetic-directory", "pc", "synthetic",
                Path.Combine(directoryGame, "Media"), directoryExecutable);
            GameResourceAudit directoryAudit =
                SmoResearchDatabase.GetResourceAudit(resourceDatabase);
            Equal(5L, directoryAudit.TotalResources,
                "directory resource database includes Media and game-root files");
            Equal(5L, directoryAudit.AssignedResources,
                "directory resource database classifies every file");
            Equal(1L, directoryAudit.Dependencies,
                "directory resource database extracts SNC dependency");
            Equal(1L, directoryAudit.ResolvedDependencies,
                "directory resource database resolves adjacent WAV dependency");
            Equal(0L, directoryAudit.AnalysisErrors,
                "directory resource analysis has no errors");
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void TestKnownSerializedFieldRegistry()
    {
        Equal("spFog", SmoClassRegistry.GetDisplayName(SmoClassIds.Fog),
            "confirmed fog class registration");
        Equal("spLightData", SmoClassRegistry.GetDisplayName(SmoClassIds.LightData),
            "confirmed light-data class registration");
        Equal("spNavigationGraph",
            SmoClassRegistry.GetDisplayName(SmoClassIds.NavigationGraph),
            "confirmed navigation-graph class registration");
        Equal("spNavigationSet",
            SmoClassRegistry.GetDisplayName(SmoClassIds.NavigationSet),
            "confirmed navigation-set base-class registration");
        Equal("spMeshNavigationSet",
            SmoClassRegistry.GetDisplayName(SmoClassIds.MeshNavigationSet),
            "confirmed mesh-navigation-set class registration");
        Equal("spNavigationPortal",
            SmoClassRegistry.GetDisplayName(SmoClassIds.NavigationPortal),
            "confirmed navigation-portal class registration");
        Equal("spBSPNode", SmoClassRegistry.GetDisplayName(SmoClassIds.BspNode),
            "confirmed BSP-node class registration");
        Equal("spParticleSystem",
            SmoClassRegistry.GetDisplayName(SmoClassIds.ParticleSystem),
            "confirmed particle-system class registration");
        Equal("spAnimation",
            SmoClassRegistry.GetDisplayName(SmoClassIds.Animation),
            "confirmed animation class registration");
        Equal("spMaterialColorController",
            SmoClassRegistry.GetDisplayName(SmoClassIds.MaterialColorController),
            "confirmed material-color-controller class registration");
        Equal("spSkyBox", SmoClassRegistry.GetDisplayName(SmoClassIds.SkyBox),
            "confirmed sky-box class registration");
        Equal("spOcclusionVolume",
            SmoClassRegistry.GetDisplayName(SmoClassIds.OcclusionVolume),
            "confirmed occlusion-volume class registration");
        Equal("spLensFlare", SmoClassRegistry.GetDisplayName(SmoClassIds.LensFlare),
            "confirmed lens-flare class registration");
        Equal("spAnimTexController",
            SmoClassRegistry.GetDisplayName(SmoClassIds.AnimTextureController),
            "confirmed animated-texture-controller class registration");
        Equal("spSphereBV",
            SmoClassRegistry.GetDisplayName(SmoClassIds.SphereBoundingVolume),
            "confirmed sphere-BV class registration");
        Equal("spOBBBV",
            SmoClassRegistry.GetDisplayName(SmoClassIds.OrientedBoxBoundingVolume),
            "confirmed oriented-box-BV class registration");
        Equal("spBoxBV",
            SmoClassRegistry.GetDisplayName(SmoClassIds.BoxBoundingVolume),
            "confirmed axis-aligned-box-BV class registration");
        Equal("spShadowVolumeManager",
            SmoClassRegistry.GetDisplayName(SmoClassIds.ShadowVolumeManager),
            "confirmed shadow-volume-manager class registration");
        Equal("spDXShadowVolumeManager",
            SmoClassRegistry.GetDisplayName(SmoClassIds.DxShadowVolumeManager),
            "confirmed DX shadow-volume-manager class registration");
        Equal("spDXShadowMeshSerializer",
            SmoClassRegistry.GetDisplayName(SmoClassIds.DxShadowMeshSerializer),
            "confirmed DX shadow-mesh-serializer class registration");
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> nodeDefinitions =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(SmoClassIds.Node);
        Equal(9,nodeDefinitions.Count,
            "node registry exposes all PC/PS2 serializer fields");
        Equal("node.child",nodeDefinitions[5].Key,
            "node field 5 is the logical child relationship");
        Equal("node.is_animated",nodeDefinitions[8].Key,
            "node field 8 is the explicit animated flag");
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor>
            renderNodeDefinitions =
                SmoSerializedFieldRegistry.GetOwnFieldDefinitions(
                    SmoClassIds.RenderNode);
        Equal(1,renderNodeDefinitions.Count,
            "render-node registry exposes its repeated renderable field");
        Equal("render_node.renderable",renderNodeDefinitions[0].Key,
            "render-node field 0 is the renderable relationship");
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor>
            partitionRenderableDefinitions =
                SmoSerializedFieldRegistry.GetOwnFieldDefinitions(
                    SmoClassIds.PartitionRenderable);
        Equal(2,partitionRenderableDefinitions.Count,
            "partition-renderable registry exposes renderable and debug color");
        Equal("partition_renderable.renderable",
            partitionRenderableDefinitions[0].Key,
            "partition-renderable field 0 is the repeated model relationship");
        Equal("partition_renderable.debug_color",
            partitionRenderableDefinitions[1].Key,
            "partition-renderable field 1 is the ARGB debug color");
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor>
            partitionNodeDefinitions =
                SmoSerializedFieldRegistry.GetOwnFieldDefinitions(
                    SmoClassIds.PartitionNode);
        Equal(8,partitionNodeDefinitions.Count,
            "partition-node registry exposes every PC/PS2 serializer field");
        Equal("partition_node.collision_info",partitionNodeDefinitions[0].Key,
            "partition-node field 0 is collision info");
        Equal("partition_node.child",partitionNodeDefinitions[2].Key,
            "partition-node field 2 preserves the unobserved Child capability");
        Equal("partition_node.static_render_object",
            partitionNodeDefinitions[7].Key,
            "partition-node field 7 is the repeated static placement");
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> octreeDefinitions =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(SmoClassIds.OctreeNode);
        Equal(3,octreeDefinitions.Count,
            "octree registry exposes Pivot, Mins and Maxs");
        Equal("octree_node.pivot",octreeDefinitions[0].Key,
            "octree field 0 is the partition pivot");
        Equal("octree_node.minimum",octreeDefinitions[1].Key,
            "octree field 1 is the bounds minimum");
        Equal("octree_node.maximum",octreeDefinitions[2].Key,
            "octree field 2 is the bounds maximum");
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor>
            partitionSystemDefinitions =
                SmoSerializedFieldRegistry.GetOwnFieldDefinitions(
                    SmoClassIds.PartitionSystem);
        Equal(1,partitionSystemDefinitions.Count,
            "partition-system registry exposes its partition root");
        Equal("partition_system.partition_root",
            partitionSystemDefinitions[0].Key,
            "partition-system field 0 is the polymorphic partition root");
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> zoneDefinitions =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(SmoClassIds.Zone);
        Equal(1,zoneDefinitions.Count,
            "zone registry exposes its repeated local partition root");
        Equal("zone.local_partition_root",zoneDefinitions[0].Key,
            "zone field 0 is the local partition root");
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor>
            zonePortalDefinitions =
                SmoSerializedFieldRegistry.GetOwnFieldDefinitions(
                    SmoClassIds.ZonePortal);
        Equal(3,zonePortalDefinitions.Count,
            "zone-portal registry exposes destination, polygon and open fields");
        Equal("zone_portal.destination_zone",zonePortalDefinitions[0].Key,
            "zone-portal field 0 is its destination zone");
        Equal("zone_portal.polygon",zonePortalDefinitions[1].Key,
            "zone-portal field 1 is its polygon");
        Equal("zone_portal.open",zonePortalDefinitions[2].Key,
            "zone-portal field 2 is its open flag");
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor>
            zonePortalNodeDefinitions =
                SmoSerializedFieldRegistry.GetOwnFieldDefinitions(
                    SmoClassIds.ZonePortalNode);
        Equal(1,zonePortalNodeDefinitions.Count,
            "zone-portal-node registry exposes its repeated portal field");
        Equal("zone_portal_node.zone_portal",zonePortalNodeDefinitions[0].Key,
            "zone-portal-node field 0 is its portal relationship");
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> bspDefinitions =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(SmoClassIds.BspNode);
        Equal(2,bspDefinitions.Count,
            "BSP registry exposes plane and executable-supported polygon");
        Equal("bsp.plane",bspDefinitions[0].Key,
            "BSP field 0 is its splitting plane");
        Equal("bsp.polygon",bspDefinitions[1].Key,
            "BSP field 1 is its optional polygon");
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor>
            occlusionDefinitions =
                SmoSerializedFieldRegistry.GetOwnFieldDefinitions(
                    SmoClassIds.OcclusionVolume);
        Equal(2,occlusionDefinitions.Count,
            "occlusion-volume registry exposes both portable buffers");
        Equal("occlusion_volume.index_buffer",occlusionDefinitions[0].Key,
            "occlusion-volume field 0 is its triangle index buffer");
        Equal("occlusion_volume.vertex_buffer",occlusionDefinitions[1].Key,
            "occlusion-volume field 1 is its position vertex buffer");
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> obbDefinitions =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(
                SmoClassIds.OrientedBoxBoundingVolume);
        Equal(3, obbDefinitions.Count,
            "oriented-box registry exposes all executable-confirmed fields");
        Equal("obb.position", obbDefinitions[0].Key,
            "oriented-box field 0 is position");
        Equal("obb.size", obbDefinitions[1].Key,
            "oriented-box field 1 is full size");
        Equal("obb.rotation", obbDefinitions[2].Key,
            "oriented-box field 2 is rotation");
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> boxDefinitions =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(
                SmoClassIds.BoxBoundingVolume);
        Equal(2, boxDefinitions.Count,
            "box-BV registry exposes both executable-confirmed fields");
        Equal("box_bv.position", boxDefinitions[0].Key,
            "box-BV field 0 is position");
        Equal("box_bv.size", boxDefinitions[1].Key,
            "box-BV field 1 is size");
        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> sphereDefinitions =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(
                SmoClassIds.SphereBoundingVolume);
        Equal(2,sphereDefinitions.Count,
            "sphere-BV registry exposes both executable-confirmed fields");
        Equal("sphere_bv.position",sphereDefinitions[0].Key,
            "sphere-BV field 0 is position");
        Equal("sphere_bv.radius",sphereDefinitions[1].Key,
            "sphere-BV field 1 is radius");

        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> materialDefinitions =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(SmoClassIds.MaterialData);
        Equal(12,materialDefinitions.Count,
            "material registry exposes every observed PC/PS2 field");
        Equal("material.render_states",materialDefinitions[0].Key,
            "material field 0 is the global render-state tuple");
        Equal("material.texture_states_legacy",materialDefinitions[8].Key,
            "material field 8 is the legacy texture-state tuple");
        Equal("material.texture_states",materialDefinitions[17].Key,
            "material field 17 is the current texture-state tuple");

        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> meshDefinitions =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(SmoClassIds.MeshData);
        Equal(3,meshDefinitions.Count,
            "mesh registry exposes cross, platform and bounding-box fields");
        Equal("mesh.cross_platform",meshDefinitions[0].Key,
            "mesh field 0 is cross-platform geometry");
        Equal("mesh.platform_specific",meshDefinitions[1].Key,
            "mesh field 1 is platform-specific geometry");
        Equal("mesh.bounding_box",meshDefinitions[2].Key,
            "mesh field 2 is the optional PS2 bounding box");

        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> modelDefinitions =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(SmoClassIds.Model);
        Equal(2,modelDefinitions.Count,
            "model registry exposes base mesh and projection group");
        Equal("model.base_mesh",modelDefinitions[0].Key,
            "model field 0 is the base-mesh relationship");
        Equal("model.projection_group",modelDefinitions[1].Key,
            "model field 1 is the projection group");

        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> skinDefinitions =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(SmoClassIds.Skin);
        Equal(1,skinDefinitions.Count,
            "skin registry exposes the executable-named palette field");
        Equal("skin.palette",skinDefinitions[0].Key,
            "skin field 0 is the bone palette and inverse-bind data");

        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor>
            collisionInfoDefinitions =
                SmoSerializedFieldRegistry.GetOwnFieldDefinitions(
                    SmoClassIds.CollisionInfo);
        Equal(3,collisionInfoDefinitions.Count,
            "collision-info registry exposes primitive, group and transform");
        Equal("collision_info.primitive",collisionInfoDefinitions[0].Key,
            "collision-info field 0 is the bounding-volume relationship");
        Equal("collision_info.group",collisionInfoDefinitions[1].Key,
            "collision-info field 1 is the collision group");
        Equal("collision_info.transform",collisionInfoDefinitions[2].Key,
            "collision-info field 2 is the world transform");

        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> staticDefinitions =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(
                SmoClassIds.StaticRenderObject);
        Equal(3,staticDefinitions.Count,
            "static-render-object registry exposes both matrices and renderable");
        Equal("static_render_object.renderable",staticDefinitions[0].Key,
            "static-render-object field 0 is the model relationship");
        Equal("static_render_object.transform",staticDefinitions[1].Key,
            "static-render-object field 1 is the world transform");
        Equal("static_render_object.inverse_transform",staticDefinitions[2].Key,
            "static-render-object field 2 is Sparkplug's inverse transform");

        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> lightDefinitions =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(SmoClassIds.LightData);
        Equal(9,lightDefinitions.Count,
            "light-data registry exposes all PC/PS2 serializer fields");
        Equal("light.intensity",lightDefinitions[4].Key,
            "light-data field 4 is intensity");
        Equal("Single radians",lightDefinitions[6].PayloadLayout,
            "light hotspot angle records its unit");

        byte[] lightBaseTerminator = SmoDataBlockWriter.BuildField(
            0,ReadOnlySpan<byte>.Empty);
        byte[] lightTypeField = SmoDataBlockWriter.BuildField(
            0,BitConverter.GetBytes((uint)SmoLightType.Point));
        byte[] lightColorField = SmoDataBlockWriter.BuildField(
            2,BitConverter.GetBytes(0xFF9ED6FFu));
        byte[] lightIntensityField = SmoDataBlockWriter.BuildField(
            4,BitConverter.GetBytes(0.5f));
        byte[] lightEnabledField = SmoDataBlockWriter.BuildField(8,[1]);
        byte[] lightOwnTerminator = SmoDataBlockWriter.BuildField(
            0,ReadOnlySpan<byte>.Empty);
        byte[][] lightEncodedFields =
        [
            lightBaseTerminator,lightTypeField,lightColorField,
            lightIntensityField,lightEnabledField,lightOwnTerminator
        ];
        byte[] lightBody = new byte[
            8 + lightEncodedFields.Sum(field => field.Length)];
        WriteUInt32(lightBody,0,SmoClassIds.LightData);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(lightBody,4);
        int lightCursor = 8;
        foreach (byte[] field in lightEncodedFields)
        {
            field.CopyTo(lightBody,lightCursor);
            lightCursor += field.Length;
        }
        SmoDocument lightDocument = SmoDocument.Parse(CreateSingleObjectDocument(
            "Light",SmoClassIds.LightData,lightBody));
        IReadOnlyList<SmoObjectField> lightObjectFields =
            SmoObjectFieldReader.Read(lightDocument,lightDocument.Objects[0]);
        True(SmoLightDataDecoder.TryDecode(
                lightObjectFields,out SmoLightData? light) && light is not null,
            "light-data final serializer section decodes");
        Equal((uint)SmoLightType.Point,light!.Type,
            "light-data type decodes");
        Equal(new Vector4(BitConverter.UInt32BitsToSingle(0x3F1E9E9F),BitConverter.UInt32BitsToSingle(0x3F56D6D8),1,1),light.ColorRgba,"light-data runtime RGBA color decodes");
        Equal(0.5f,light.Intensity,"light-data intensity decodes");
        Equal(SmoLightDataDecoder.DefaultRange,light.Range,
            "omitted light range uses executable default");
        True(light.Enabled && light.IsFieldSerialized(8) &&
             !light.IsFieldSerialized(5),
            "light-data tracks serialized fields separately from effective defaults");
        Equal("directional",SmoLightDataDecoder.GetTypeName(0),
            "light type zero is directional");
        Equal("spot",SmoLightDataDecoder.GetTypeName(2),
            "light type two is spot");
        Equal("ambient",SmoLightDataDecoder.GetTypeName(3),
            "light type three is ambient");
        IReadOnlyList<SmoSerializedFieldValue> inspectedLightFields =
            SmoSerializedFieldInspector.Inspect(
                lightDocument,lightDocument.Objects[0]);
        Equal(4,inspectedLightFields.Count,
            "light inspector ignores the inherited serializer section");
        True(inspectedLightFields.Any(field =>
                 field.Descriptor.Key == "light.type" && field.IsDecoded &&
                 field.DisplayValue == "1 (point); effective state"),
            "light inspector displays the recovered type name");
        True(inspectedLightFields.Any(field =>
                 field.Descriptor.Key == "light.intensity" && field.IsDecoded &&
                 field.DisplayValue == "0.5; effective state"),
            "light inspector displays field-4 intensity");

        IReadOnlyDictionary<int,SmoSerializedFieldDescriptor> uvDefinitions =
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(
                SmoClassIds.UvController);
        Equal(1, uvDefinitions.Count,
            "UV controller registry exposes its embedded transform evaluator");
        Equal("uv_controller.transform_evaluators",uvDefinitions[0].Key,
            "UV controller field 0 is the transform evaluator resource");

        byte[] uvPayload = Convert.FromHexString(
            "A05161CDCCCC3D0061CDCCCC3D000061CDCCCC3D640000803F00" +
            "61CDCCCC3D640000803F00640000803F006004000000610000803E" +
            "639A99993E000000003F0000003F0000000000000000000000000000" +
            "803F00");
        True(SmoUvControllerDecoder.TryDecode(
                uvPayload,out SmoUvControllerData? uvController) &&
             uvController is not null,
            "UV transform evaluator payload decodes");
        Equal(0.1f,uvController!.TranslationX.Frequency,
            "UV translation-X evaluator frequency");
        Equal(1.0f,uvController.ScaleX.YOffset,
            "UV scale-X evaluator encodes its identity offset");
        Equal((uint)4,uvController.Rotation.FunctionType,
            "UV rotation evaluator function type");
        Equal(new Vector3(0.5f,0.5f,0.0f),uvController.UvPivot,
            "UV pivot follows the seven evaluators");
        Equal(Vector3.UnitZ,uvController.RotationAxis,
            "UV rotation axis is serialized after the pivot");
        True(!SmoUvControllerDecoder.TryDecode(
                uvPayload.AsSpan(0,uvPayload.Length - 1),out _),
            "truncated UV controller payload is rejected");

        byte[] controllerPayload = Convert.FromHexString(
            "63CDCCCC3D0063CDCCCC3D0063CDCCCC3D0063CDCCCC3D00" +
            "61CDCCCC3D6200000000640000803F00");
        True(SmoMaterialColorControllerDecoder.TryDecode(
                controllerPayload,
                out SmoMaterialColorControllerData? controller) &&
             controller is not null,
            "material-color-controller evaluator payload decodes");
        Equal(0.1f, controller!.Ambient.Frequency,
            "material ambient evaluator frequency");
        Equal(0.1f, controller.Diffuse.Frequency,
            "material diffuse evaluator frequency");
        Equal(0.1f, controller.Specular.Frequency,
            "material specular evaluator frequency");
        Equal(0.1f, controller.Emissive.Frequency,
            "material emissive evaluator frequency");
        Equal(0.1f, controller.Alpha.Frequency,
            "material alpha evaluator frequency");
        Equal(0.0f, controller.Alpha.Amplitude,
            "material alpha evaluator amplitude");
        Equal(1.0f, controller.Alpha.YOffset,
            "material alpha evaluator y offset");
        Equal(0xFF000000u, controller.Ambient.Color1,
            "omitted material evaluator color uses executable default");

        True(SmoUvControllerDecoder.TryDecode(new byte[] { 0 }, out var defaultUv) &&
             defaultUv!.ScaleX.YOffset == 1 && defaultUv.ScaleY.YOffset == 1 &&
             defaultUv.ScaleZ.YOffset == 1 && defaultUv.RotationAxis == Vector3.UnitZ,
            "original transform defaults survive an empty section");
        var wideUv = new byte[] { 0xE0 }.Concat(BitConverter.GetBytes((uint)uvPayload[1]))
            .Concat(uvPayload.Skip(2).Take(uvPayload[1])).Append((byte)0).ToArray();
        True(SmoUvControllerDecoder.TryDecode(wideUv, out var wideTransform) && wideTransform == uvController,
            "UV reader accepts a wider original field header independently of the writer preference");
        var repeatedColor = SmoDataBlockWriter.BuildField(0, BitConverter.GetBytes(0x11223344u))
            .Concat(SmoDataBlockWriter.BuildField(15, new byte[] { 9, 8, 7 }))
            .Concat(SmoDataBlockWriter.BuildField(0, BitConverter.GetBytes(0x80402010u)))
            .Concat(SmoDataBlockWriter.BuildField(4, BitConverter.GetBytes(0x7FC12345u)))
            .Concat(new byte[5]).ToArray();
        True(SmoMaterialColorControllerDecoder.TryDecode(repeatedColor, out var repeatedController) &&
             repeatedController!.Ambient.Color1 == 0x80402010u &&
             BitConverter.SingleToUInt32Bits(repeatedController.Ambient.Amplitude) == 0x7FC12345u,
            "shared color reader retains last assignment, skips unknown fields and preserves raw IEEE bits");
        True(!SmoMaterialColorControllerDecoder.TryDecode(repeatedColor.AsSpan(0, repeatedColor.Length - 1), out _),
            "packed color evaluator body requires all five complete sections");

        byte[] fogPayload = Convert.FromHexString(
            "03000000224BC8FF00002F4400C0DA4500000000");
        True(SmoFogDecoder.TryDecode(fogPayload, out SmoFogData? fog) &&
             fog is not null,
            "fog payload decodes");
        Equal((uint)SmoFogType.Linear, fog!.Type,
            "fog type is the first UInt32");
        Equal(0xFFC84B22u, fog.Color,
            "fog ARGB color is the second UInt32");
        Equal(700.0f, fog.Start, "fog start distance decodes");
        Equal(7000.0f, fog.End, "fog end distance decodes");
        Equal(0.0f, fog.Density, "fog density decodes");
        Equal("linear", SmoFogDecoder.GetTypeName(fog.Type),
            "fog type name is exposed");
        True(!SmoFogDecoder.TryDecode(fogPayload.AsSpan(0, 19), out _),
            "truncated fog payload is rejected");

        byte[] obbSizePayload = Convert.FromHexString(
            "B3323E425EDA1C43705FFB41");
        True(SmoOrientedBoxBoundingVolumeDecoder.TryDecodeSize(
                obbSizePayload, out SmoOrientedBoxSizeData? obbSize) &&
             obbSize is not null,
            "oriented-box full-size payload decodes");
        Equal(new Vector3(
                47.54951095581055f,156.85299682617188f,31.421600341796875f),
            obbSize!.FullSize,
            "oriented-box stores full dimensions in X/Y/Z order");
        Equal(obbSize.FullSize * 0.5f, obbSize.HalfExtents,
            "oriented-box half-extents follow executable 0.5 calculation");
        Equal(obbSize.HalfExtents.Length(), obbSize.BoundingSphereRadius,
            "oriented-box bounding-sphere radius follows executable calculation");
        True(!SmoOrientedBoxBoundingVolumeDecoder.TryDecodeSize(
                obbSizePayload.AsSpan(0, 11), out _),
            "truncated oriented-box size is rejected");
        Vector3 expectedObbPosition = new(1.25f,-2.5f,3.75f);
        True(SmoOrientedBoxBoundingVolumeDecoder.TryDecodePosition(
                SmoPropertyValueCodec.Encode(expectedObbPosition),
                out Vector3 obbPosition),
            "oriented-box optional position decodes");
        Equal(expectedObbPosition, obbPosition,
            "oriented-box position uses X/Y/Z order");
        Quaternion expectedObbRotation = new(0.1f,0.2f,0.3f,0.9f);
        True(SmoOrientedBoxBoundingVolumeDecoder.TryDecodeRotation(
                SmoPropertyValueCodec.Encode(expectedObbRotation),
                out Quaternion obbRotation),
            "oriented-box optional rotation decodes");
        Equal(expectedObbRotation, obbRotation,
            "oriented-box rotation uses quaternion X/Y/Z/W order");

        byte[] boxSizePayload = Convert.FromHexString(
            "3ABB8E42ACB00B43A1939642");
        True(SmoBoxBoundingVolumeDecoder.TryDecodeSize(
                boxSizePayload, out SmoBoxSizeData? boxSize) && boxSize is not null,
            "axis-aligned box size payload decodes");
        Equal(new Vector3(
                71.36567687988281f,139.69012451171875f,75.28833770751953f),
            boxSize!.FullSize,
            "axis-aligned box size uses X/Y/Z order");
        Equal(boxSize.FullSize * 0.5f,boxSize.HalfExtents,
            "axis-aligned box half-extents follow executable 0.5 calculation");
        Equal(boxSize.HalfExtents.Length(),boxSize.BoundingSphereRadius,
            "axis-aligned box sphere radius follows executable calculation");
        True(!SmoBoxBoundingVolumeDecoder.TryDecodeSize(
                boxSizePayload.AsSpan(0,11),out _),
            "truncated axis-aligned box size is rejected");
        Vector3 expectedBoxPosition = new(-1.0f,2.0f,-3.0f);
        True(SmoBoxBoundingVolumeDecoder.TryDecodePosition(
                SmoPropertyValueCodec.Encode(expectedBoxPosition),
                out Vector3 boxPosition),
            "axis-aligned box optional position decodes");
        Equal(expectedBoxPosition,boxPosition,
            "axis-aligned box position uses X/Y/Z order");

        byte[] sphereRadiusPayload = Convert.FromHexString("011AB842");
        True(SmoSphereBoundingVolumeDecoder.TryDecodeRadius(
                sphereRadiusPayload,out float sphereRadius),
            "sphere radius payload decodes");
        Equal(92.05078887939453f,sphereRadius,
            "sphere field 1 stores radius as a Single");
        True(!SmoSphereBoundingVolumeDecoder.TryDecodeRadius(
                sphereRadiusPayload.AsSpan(0,3),out _),
            "truncated sphere radius is rejected");
        Vector3 expectedSpherePosition = new(4.0f,-5.0f,6.0f);
        True(SmoSphereBoundingVolumeDecoder.TryDecodePosition(
                SmoPropertyValueCodec.Encode(expectedSpherePosition),
                out Vector3 spherePosition),
            "sphere optional position decodes");
        Equal(expectedSpherePosition,spherePosition,
            "sphere position uses X/Y/Z order");

        byte[] fogBody = new byte[8 + 5 + fogPayload.Length + 1];
        WriteUInt32(fogBody, 0, SmoClassIds.Fog);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(fogBody, 4);
        fogBody[8] = 0xE0;
        WriteUInt32(fogBody, 9, (uint)fogPayload.Length);
        fogPayload.CopyTo(fogBody, 13);
        SmoDocument fogDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(string.Empty, SmoClassIds.Fog, fogBody));
        SmoSerializedFieldValue fogField = SmoSerializedFieldInspector.Inspect(
            fogDocument, fogDocument.Objects[0]).Single();
        True(fogField.IsDecoded && fogField.DisplayValue.Contains(
                "type=3 (linear), color=#FFC84B22, start=700, end=7000",
                StringComparison.Ordinal),
            "fog inspector formats the executable-confirmed layout");

        byte[] obbFieldBytes = SmoDataBlockWriter.BuildField(1, obbSizePayload);
        byte[] obbTerminator = SmoDataBlockWriter.BuildField(
            0, ReadOnlySpan<byte>.Empty);
        byte[] obbBody = new byte[
            8 + obbFieldBytes.Length + obbTerminator.Length];
        WriteUInt32(obbBody, 0, SmoClassIds.OrientedBoxBoundingVolume);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(obbBody, 4);
        obbFieldBytes.CopyTo(obbBody, 8);
        obbTerminator.CopyTo(obbBody, 8 + obbFieldBytes.Length);
        SmoDocument obbDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                string.Empty,SmoClassIds.OrientedBoxBoundingVolume,obbBody));
        SmoSerializedFieldValue obbSizeField =
            SmoSerializedFieldInspector.Inspect(
                obbDocument,obbDocument.Objects[0]).Single();
        Equal("obb.size", obbSizeField.Descriptor.Key,
            "oriented-box field 1 is named as full size");
        True(obbSizeField.IsDecoded && obbSizeField.DisplayValue.Contains(
                "full=(47.549511, 156.852997, 31.4216003), half-extents=",
                StringComparison.Ordinal),
            "oriented-box inspector formats full size and half-extents");

        byte[] boxFieldBytes = SmoDataBlockWriter.BuildField(1,boxSizePayload);
        byte[] boxBody = new byte[8 + boxFieldBytes.Length + 1];
        WriteUInt32(boxBody,0,SmoClassIds.BoxBoundingVolume);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(boxBody,4);
        boxFieldBytes.CopyTo(boxBody,8);
        SmoDocument boxDocument = SmoDocument.Parse(CreateSingleObjectDocument(
            string.Empty,SmoClassIds.BoxBoundingVolume,boxBody));
        SmoSerializedFieldValue boxSizeField =
            SmoSerializedFieldInspector.Inspect(
                boxDocument,boxDocument.Objects[0]).Single();
        Equal("box_bv.size",boxSizeField.Descriptor.Key,
            "axis-aligned box field 1 is named as size");
        True(boxSizeField.IsDecoded && boxSizeField.DisplayValue.Contains(
                "full=(71.3656769, 139.690125, 75.2883377), half-extents=",
                StringComparison.Ordinal),
            "axis-aligned box inspector formats full size and half-extents");

        byte[] sphereFieldBytes = SmoDataBlockWriter.BuildField(
            1,sphereRadiusPayload);
        byte[] sphereBody = new byte[8 + sphereFieldBytes.Length + 1];
        WriteUInt32(sphereBody,0,SmoClassIds.SphereBoundingVolume);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(sphereBody,4);
        sphereFieldBytes.CopyTo(sphereBody,8);
        SmoDocument sphereDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                string.Empty,SmoClassIds.SphereBoundingVolume,sphereBody));
        SmoSerializedFieldValue sphereRadiusField =
            SmoSerializedFieldInspector.Inspect(
                sphereDocument,sphereDocument.Objects[0]).Single();
        Equal("sphere_bv.radius",sphereRadiusField.Descriptor.Key,
            "sphere field 1 is named as radius");
        Equal("92.0507889",sphereRadiusField.DisplayValue,
            "sphere inspector formats the radius");

        byte[] controllerBody = new byte[8 + 5 + controllerPayload.Length + 1];
        WriteUInt32(controllerBody, 0, SmoClassIds.MaterialColorController);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(controllerBody, 4);
        controllerBody[8] = 0xE0;
        WriteUInt32(controllerBody, 9, (uint)controllerPayload.Length);
        controllerPayload.CopyTo(controllerBody, 13);
        SmoDocument controllerDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                string.Empty,SmoClassIds.MaterialColorController,controllerBody));
        IReadOnlyList<SmoSerializedFieldValue> controllerFields =
            SmoSerializedFieldInspector.Inspect(
                controllerDocument,controllerDocument.Objects[0]);
        Equal(1, controllerFields.Count,
            "material controller exposes one evaluator field");
        True(controllerFields[0].IsDecoded &&
             controllerFields[0].DisplayValue.Contains(
                 "alpha={type=0, frequency=0.100000001, amplitude=0",
                 StringComparison.Ordinal),
            "material controller inspector formats recovered evaluator semantics");

        byte[] uvFieldBytes = SmoDataBlockWriter.BuildField(0,uvPayload);
        byte[] uvBody = new byte[8 + uvFieldBytes.Length + 1];
        WriteUInt32(uvBody,0,SmoClassIds.UvController);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(uvBody,4);
        uvFieldBytes.CopyTo(uvBody,8);
        SmoDocument uvDocument = SmoDocument.Parse(CreateSingleObjectDocument(
            string.Empty,SmoClassIds.UvController,uvBody));
        SmoSerializedFieldValue uvField = SmoSerializedFieldInspector.Inspect(
            uvDocument,uvDocument.Objects[0]).Single();
        True(uvField.IsDecoded && uvField.DisplayValue.Contains(
                "UV pivot=(0.5, 0.5, 0); rotation axis=(0, 0, 1)",
                StringComparison.Ordinal),
            "UV controller inspector formats evaluator and vector semantics");

        byte[] baseValue = SmoDataBlockWriter.BuildField(2, new byte[4]);
        byte[] terminator = SmoDataBlockWriter.BuildField(0, ReadOnlySpan<byte>.Empty);
        byte[] ownValue = SmoDataBlockWriter.BuildField(5, new byte[4]);
        byte[] body = new byte[
            8 + baseValue.Length + terminator.Length + ownValue.Length + terminator.Length];
        WriteUInt32(body, 0, SmoClassIds.LightData);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(body, 4);
        int offset = 8;
        baseValue.CopyTo(body, offset);
        offset += baseValue.Length;
        terminator.CopyTo(body, offset);
        offset += terminator.Length;
        ownValue.CopyTo(body, offset);
        offset += ownValue.Length;
        terminator.CopyTo(body, offset);

        SmoDocument document = SmoDocument.Parse(CreateSingleObjectDocument(
            "light", SmoClassIds.LightData, body));
        IReadOnlyList<SmoObjectField> fields = SmoObjectFieldReader.Read(
            document, document.Objects[0]);
        True(!SmoSerializedFieldRegistry.TryDescribeOwnField(
                SmoClassIds.LightData, fields, 0, out _),
            "base-class field is not mislabeled as a light field");
        True(SmoSerializedFieldRegistry.TryDescribeOwnField(
                SmoClassIds.LightData, fields, 2,
                out SmoSerializedFieldDescriptor? range) &&
             range?.Key == "light.range",
            "most-derived light field receives its executable serializer name");
        True(!SmoSerializedFieldRegistry.TryDescribeOwnField(
                SmoClassIds.LightData, fields, 3, out _),
            "serialization terminator is not labeled as a value field");
        IReadOnlyList<SmoSerializedFieldValue> inspected =
            SmoSerializedFieldInspector.Inspect(document, document.Objects[0]);
        Equal(1, inspected.Count,
            "serialized-field inspector exposes only the most-derived known field");
        Equal("0; effective state", inspected[0].DisplayValue,
            "serialized-field inspector formats the confirmed loaded Single state");
        True(inspected[0].IsDecoded,
            "serialized-field inspector marks a matching fixed layout as decoded");
        Equal("00 00 00 00", inspected[0].HexPreview,
            "serialized-field inspector preserves a bounded payload preview");

        True(!SmoNodeDecoder.TryDecodeRelationship(BitConverter.GetBytes(2u), out _, out _, out _),
            "nonnull reference requires a size word; no invented legacy ID-only form");
        True(SmoNodeDecoder.TryDecodeRelationship(new byte[4], out uint nullId, out _, out _) && nullId == 0,
            "native null reference consumes only its zero ID");
        SmoDocument decodedNodeDocument = SmoDocument.Parse(
            CreateReferencedNodeDocument());
        SmoObjectEntry decodedNodeEntry = decodedNodeDocument.Objects[0];
        True(SmoNodeDecoder.TryDecode(
                decodedNodeDocument, decodedNodeEntry, out SmoNodeData? decodedNode) &&
             decodedNode is not null,
            "node serializer fields and sized child relationship decode");
        Equal(Vector3.Zero,decodedNode!.Position,
            "omitted node position uses executable zero default");
        Equal(Vector3.One,decodedNode.Scale,
            "omitted node scale uses executable identity default");
        True(decodedNode.IsBone && decodedNode.IsStatic && !decodedNode.IsAnimated,
            "node boolean flags decode independently");
        Equal(2u,decodedNode.BillboardAxis,
            "node billboard engine-axis value decodes");
        Equal(1,decodedNode.Children.Count,
            "node sized child is retained");
        Equal(SmoNodeRelationshipEncoding.SizedReference,
            decodedNode.Children[0].Encoding,
            "nonnull child includes its size word");
        Equal(1,decodedNode.Children[0].TargetObjectIndex!.Value,
            "node sized child resolves through the object directory");
        SmoNodeHierarchy decodedHierarchy = SmoNodeHierarchy.Decode(
            decodedNodeDocument);
        Equal(1,decodedHierarchy.Links.Count,
            "logical hierarchy includes sized child fields");
        IReadOnlyList<SmoSerializedFieldValue> inspectedNodeFields =
            SmoSerializedFieldInspector.Inspect(
                decodedNodeDocument,decodedNodeEntry);
        True(inspectedNodeFields.Any(field =>
                 field.Descriptor.Key == "node.child" && field.IsDecoded &&
                 field.DisplayValue.Contains("sized reference",StringComparison.Ordinal)),
            "node inspector identifies the compact child relationship");
        True(inspectedNodeFields.Any(field =>
                 field.Descriptor.Key == "node.billboard_axis" && field.IsDecoded &&
                 field.DisplayValue.StartsWith("2 ",StringComparison.Ordinal)),
            "node inspector displays the executable billboard value");

        SmoDocument renderNodeDocument = SmoDocument.Parse(
            CreateReferencedRenderNodeDocument());
        SmoObjectEntry renderNodeEntry = renderNodeDocument.Objects[0];
        True(!SmoNodeDecoder.TryDecode(
                renderNodeDocument,renderNodeEntry,out _),
            "concrete node decoder does not erase the render-node subtype");
        True(SmoRenderNodeDecoder.TryDecode(
                renderNodeDocument,renderNodeEntry,
                out SmoRenderNodeData? decodedRenderNode) &&
             decodedRenderNode is not null,
            "render-node inherited state and own relationship decode");
        Equal(new Vector3(2,3,4),decodedRenderNode!.Node.Position,
            "render-node inherits the node position field");
        Equal(1u,decodedRenderNode.Node.BillboardAxis,
            "render-node inherits the billboard-axis field");
        Equal(1,decodedRenderNode.Renderables.Count,
            "render-node retains its repeated renderable list");
        Equal(SmoClassIds.Model,
            decodedRenderNode.Renderables[0].TargetTypeHash!.Value,
            "render-node relationship resolves to its model");
        SmoNodeHierarchy renderNodeHierarchy = SmoNodeHierarchy.Decode(
            renderNodeDocument);
        True(renderNodeHierarchy.Links.Any(link =>
                link.ParentObjectIndex == 0 && link.ChildObjectIndex == 2),
            "logical hierarchy includes inherited render-node child fields");
        IReadOnlyList<SmoSerializedFieldValue> inspectedRenderNodeFields =
            SmoSerializedFieldInspector.Inspect(
                renderNodeDocument,renderNodeEntry);
        True(inspectedRenderNodeFields.Any(field =>
                field.Descriptor.Key == "node.position" && field.IsDecoded),
            "render-node inspector exposes inherited node fields");
        True(inspectedRenderNodeFields.Any(field =>
                field.Descriptor.Key == "render_node.renderable" &&
                field.IsDecoded &&
                field.DisplayValue.Contains("spModel",StringComparison.Ordinal)),
            "render-node inspector exposes its own renderable relationship");
        True(SmoNodeTransformDecoder.TryDecode(
                renderNodeDocument,renderNodeEntry,
                out SmoNodeTransform? renderTransform) &&
             renderTransform?.Position == new Vector3(2,3,4),
            "render-node transform uses the confirmed inherited section");

        SmoDocument nodeDocument = SmoDocument.Parse(CreateSyntheticDocument());
        True(SmoNodeDecoder.TryDecode(
                nodeDocument,nodeDocument.Objects[0],out SmoNodeData? defaultNode) &&
             defaultNode is not null && !defaultNode.IsAnimated &&
             !defaultNode.IsFieldSerialized(8),
            "inspector retains absent Animated as an omitted authored field");
        var clip = new SmoAnimationClip(
            "synthetic.san",
            1.0f,
            [
                new SmoAnimationTrack("root", [], [], []),
                new SmoAnimationTrack("ROOT", [], [], []),
                new SmoAnimationTrack("missing", [], [], [])
            ]);
        SmoAnimationBindingAnalysis binding =
            SmoAnimationBindingAnalyzer.Analyze(nodeDocument, clip);
        Equal(1, binding.ExactMatchCount,
            "animation binding counts exact SMO/SAN node names");
        Equal(1, binding.CaseFoldedOnlyMatchCount,
            "animation binding separates case-only node-name matches");
        Equal(1, binding.MissingMatchCount,
            "animation binding reports unmatched SAN track names");
    }

    private static void TestSharedNodeScalars()
    {
        byte[] scalarBody = [..BitConverter.GetBytes(SmoClassIds.Node),.."SBOO"u8,
            ..SmoDataBlockWriter.BuildField(0,SmoPropertyValueCodec.Encode(new Vector3(1,2,3))),
            ..SmoDataBlockWriter.BuildField(0,SmoPropertyValueCodec.Encode(new Vector3(4,5,6))),
            ..SmoDataBlockWriter.BuildField(1,SmoPropertyValueCodec.Encode(new Quaternion(0,0,.5f,.5f))),
            ..SmoDataBlockWriter.BuildField(3,[0xA5]),..SmoDataBlockWriter.BuildField(4,[1]),
            ..SmoDataBlockWriter.BuildField(4,[0]),..SmoDataBlockWriter.BuildField(8,[0]),0];
        var scalarDocument = SmoDocument.Parse(CreateSingleObjectDocument("original-scalars",SmoClassIds.Node,scalarBody));
        True(SmoNodeDecoder.TryDecode(scalarDocument,scalarDocument.Objects[0],out var scalarNode) && scalarNode is not null,
            "shared Node reader accepts original repeated fields and noncanonical true byte");
        Equal(new Vector3(4,5,6),scalarNode!.Position,"Node last authored position wins");
        Equal(new Quaternion(0,0,.5f,.5f),scalarNode.Rotation,"Node authored quaternion is not normalized");
        True(!scalarNode.IsStatic && !scalarNode.IsAnimated && scalarNode.IsBone &&
            (scalarNode.EffectiveFlags & 0x1C00) == 0x1C00,
            "inspector exposes authored flags separately from the original effective Node state");
        True(SmoNodeTransformDecoder.TryDecode(scalarDocument,scalarDocument.Objects[0],out var scalarTransform) &&
            scalarTransform!.LocalMatrix.M11 == .5f && scalarTransform.LocalMatrix.M12 == .5f,
            "placement consumes the original nonunit quaternion matrix without C# normalization");
        byte[] fakeModel = [..BitConverter.GetBytes(SmoClassIds.Model),.."SBOO"u8,
            ..SmoDataBlockWriter.BuildField(0,new byte[12]),0];
        var fakeModelDocument = SmoDocument.Parse(CreateSingleObjectDocument("not-a-node",SmoClassIds.Model,fakeModel));
        True(!SmoNodeTransformDecoder.TryDecode(fakeModelDocument,fakeModelDocument.Objects[0],out _),
            "Model field resembling a position is not guessed to be a Node transform");
    }

    private static void TestSharedNodeWorld()
    {
        byte[] position = SmoDataBlockWriter.BuildField(0,SmoPropertyValueCodec.Encode(new Vector3(10,20,30)));
        byte[] scale = SmoDataBlockWriter.BuildField(2,SmoPropertyValueCodec.Encode(new Vector3(2,3,4)));
        byte[] child = [..BitConverter.GetBytes(SmoClassIds.Node),.."SBOO"u8,
            ..SmoDataBlockWriter.BuildField(0,SmoPropertyValueCodec.Encode(new Vector3(1,2,3))),
            ..SmoDataBlockWriter.BuildField(1,SmoPropertyValueCodec.Encode(new Quaternion(0,0,.5f,.5f))),
            ..SmoDataBlockWriter.BuildField(2,SmoPropertyValueCodec.Encode(new Vector3(.5f,2,3))),0];
        byte[] childLink = SmoDataBlockWriter.BuildField(5,[..BitConverter.GetBytes(2u),..BitConverter.GetBytes((uint)child.Length),..child]);
        byte[] parent = [..BitConverter.GetBytes(SmoClassIds.Node),.."SBOO"u8,..position,..scale,..childLink,0];
        var document = SmoDocument.Parse(CreateCatalogDocument(parent,
            (1,"parent",SmoClassIds.Node,0,(uint)parent.Length),
            (2,"child",SmoClassIds.Node,(uint)(parent.Length-child.Length-1),(uint)child.Length)));
        True(SmoNodeTransformDecoder.TryResolveNodeWorldMatrix(document,document.Objects[1],out var world),
            "shared world resolver accepts an original nonunit child quaternion");
        var expected = new Matrix4x4(.5f,.5f,0,0,-3,3,0,0,0,0,12,0,12,26,42,1);
        Equal(expected,world,"parent-child world matches actual PC PRS composition exactly");
        SmoNodeTransformDecoder.TryDecode(document,document.Objects[0],out var parentTransform);
        SmoNodeTransformDecoder.TryDecode(document,document.Objects[1],out var childTransform);
        True(MatrixDifference(parentTransform!.LocalMatrix,Matrix4x4.Identity)>1 &&
            MatrixDifference(childTransform!.LocalMatrix*parentTransform.LocalMatrix,world)>.1f,
            "nonuniform parent scale cannot be replaced by naive local-matrix multiplication");
        var desired=world;desired.M41+=5;desired.M42+=3;
        bool rejected=false;
        try { SmoPlacementTransformWriter.Patch(document,[new SmoPlacementTransformEdit(1,world,desired)]); }
        catch (NotSupportedException error) when (error.Message.Contains("NODE_WORLD_WRITE_UNSUPPORTED",StringComparison.Ordinal))
        { rejected=true; }
        True(rejected,"editor refuses the paused inverse-PRS case instead of returning incorrectly placed bytes");
    }

    private static void TestSharedCollisionScalars()
    {
        SmoDocument Make(params byte[][] fields)
        {
            byte[] body = [..BitConverter.GetBytes(SmoClassIds.CollisionInfo),.."SBOO"u8,
                ..fields.SelectMany(f => f),0];
            return SmoDocument.Parse(CreateSingleObjectDocument("collision_scalars",SmoClassIds.CollisionInfo,body));
        }
        var defaults = Make();
        True(SmoCollisionInfoDecoder.TryDecode(defaults,defaults.Objects[0],out var empty,out _) &&
             empty.EffectiveCollisionGroup == 1 && empty.CollisionGroup is null &&
             empty.Primitive.ObjectId == 0 && empty.SerializedFieldMask == 0,
            "CollisionInfo inspector preserves original fresh defaults without inventing a primitive");
        byte[] nonunit = new float[] {7,8,9,0,0,.5f,.5f,2,3,4}.SelectMany(BitConverter.GetBytes).ToArray();
        var first = Make(SmoDataBlockWriter.BuildField(2,nonunit),SmoDataBlockWriter.BuildField(0,new byte[4]));
        True(SmoCollisionInfoDecoder.TryDecode(first,first.Objects[0],out var parsed,out _),
            "CollisionInfo accepts transform before an explicit null primitive");
        Equal(new Quaternion(0,0,.5f,.5f),parsed!.Transform!.Rotation,
            "CollisionInfo quaternion is not normalized by the host");
        Equal(new Matrix4x4(1,1,0,0,-1.5f,1.5f,0,0,0,0,4,0,7,8,9,1),parsed.Transform.WorldMatrix,
            "CollisionInfo world matrix matches original nonunit quaternion calculation");
        byte[] zero = (byte[])nonunit.Clone(); zero.AsSpan(12,16).Clear();
        var repeated = Make(SmoDataBlockWriter.BuildField(2,nonunit),SmoDataBlockWriter.BuildField(9,[0xaa]),
            SmoDataBlockWriter.BuildField(1,BitConverter.GetBytes(7u)),SmoDataBlockWriter.BuildField(2,zero),
            SmoDataBlockWriter.BuildField(1,BitConverter.GetBytes(0u)));
        True(SmoCollisionInfoDecoder.TryDecode(repeated,repeated.Objects[0],out var last,out _) &&
             last.CollisionGroup == 0 && last.EffectiveCollisionGroup == 0 && last.SerializedFieldMask == 6 &&
             last.Transform!.Rotation == new Quaternion(0,0,0,0),
            "Original CollisionInfo accepts zero quaternion, unknown fields and last repeated scalar values");
        True(SmoCollisionInfoTransformDecoder.TryDecode(repeated,repeated.Objects[0],out var lastTransform) &&
             lastTransform!.Rotation == new Quaternion(0,0,0,0),
            "Placement helper uses the same last CollisionInfo transform");
        var truncated = Make(SmoDataBlockWriter.BuildField(2,zero.AsSpan(1)));
        True(!SmoCollisionInfoDecoder.TryDecode(truncated,truncated.Objects[0],out _,out _),
            "CollisionInfo rejects a truncated bounded transform");
    }

    private static void TestFieldMutationTransaction()
    {
        byte[] first = SmoDataBlockWriter.BuildField(0, new byte[250]);
        byte[] terminal = SmoDataBlockWriter.BuildField(7, ReadOnlySpan<byte>.Empty);
        byte[] body = new byte[8 + first.Length + terminal.Length];
        WriteUInt32(body, 0, SmoClassIds.Node);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(body, sizeof(uint));
        first.CopyTo(body, 8);
        terminal.CopyTo(body, 8 + first.Length);
        SmoDocument source = SmoDocument.Parse(CreateSingleObjectDocument(
            "mutable", SmoClassIds.Node, body));

        var transaction = new SmoMutationTransaction(source);
        transaction.SetFieldPayload(0, new SmoFieldSelector(0), new byte[300]);
        transaction.AddField(
            0,
            42,
            new byte[] { 1, 2, 3, 4, 5 },
            SmoFieldInsertion.BeforeTerminal);
        SmoMutationCommitResult committed = transaction.Commit();
        SmoDocument grown = SmoDocument.Parse(committed.Data);
        IReadOnlyList<SmoObjectField> grownFields =
            SmoObjectFieldReader.Read(grown, grown.Objects[0]);

        Equal(2, committed.Mutations.Count, "field transaction mutation count");
        Equal((uint)300, grownFields.Single(field => field.FieldType == 0 && field.PayloadSize != 0).PayloadSize,
            "field transaction promotes payload size header");
        True(grownFields.Single(field => field.FieldType == 0 && field.PayloadSize != 0).SizeKind ==
             SmoDataBlockSizeCode.UInt16,
            "field transaction promotes UInt8 header to UInt16");
        True(grownFields.Single(field => field.FieldType == 42).Payload.Span
                .SequenceEqual(new byte[] { 1, 2, 3, 4, 5 }),
            "field transaction writes extended field type");
        Equal((uint)(body.Length + 59), grown.Objects[0].SerializedSize,
            "field transaction updates object directory size");

        var removal = new SmoMutationTransaction(grown);
        removal.RemoveField(0, new SmoFieldSelector(42));
        SmoDocument removed = SmoDocument.Parse(removal.Commit().Data);
        True(SmoObjectFieldReader.Read(removed, removed.Objects[0])
                .All(field => field.FieldType != 42),
            "field transaction removes arbitrary field");

        SmoDocument nested = SmoDocument.Parse(CreateNestedMutationDocument());
        uint oldParentSize = nested.Objects[0].SerializedSize;
        uint oldChildSize = nested.Objects[1].SerializedSize;
        uint oldChildOffset = nested.Objects[1].LogicalOffset;
        var nestedTransaction = new SmoMutationTransaction(nested);
        nestedTransaction.SetFieldPayload(
            1, new SmoFieldSelector(0), new byte[300]);
        SmoDocument nestedResult = SmoDocument.Parse(nestedTransaction.Commit().Data);
        SmoObjectField parentField = SmoObjectFieldReader.Read(
            nestedResult, nestedResult.Objects[0]).Single(field => field.FieldType == 4);
        Equal(oldParentSize + 82, nestedResult.Objects[0].SerializedSize,
            "nested mutation grows enclosing object and promoted header");
        Equal(oldChildSize + 81, nestedResult.Objects[1].SerializedSize,
            "nested mutation grows inline child directory entry");
        Equal(oldChildOffset + 1, nestedResult.Objects[1].LogicalOffset,
            "nested mutation shifts inline child after promoted parent header");
        Equal((uint)320, parentField.PayloadSize,
            "nested mutation updates enclosing field payload size");
        int prefix = checked((int)nestedResult.Header.DataStart +
            (int)nestedResult.Objects[1].LogicalOffset - 8);
        Equal(nestedResult.Objects[1].SerializedSize,
            BinaryPrimitives.ReadUInt32LittleEndian(
                nestedResult.Data.Span.Slice(prefix + 4, 4)),
            "nested mutation updates inline serialized-size prefix");
    }

    private static byte[] CreateNestedMutationDocument()
    {
        byte[] childValue = SmoDataBlockWriter.BuildField(0, new byte[220]);
        byte[] childTerminal = SmoDataBlockWriter.BuildField(7, ReadOnlySpan<byte>.Empty);
        byte[] child = new byte[8 + childValue.Length + childTerminal.Length];
        WriteUInt32(child, 0, SmoClassIds.Node);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(child, 4);
        childValue.CopyTo(child, 8);
        childTerminal.CopyTo(child, 8 + childValue.Length);

        byte[] inlinePayload = new byte[8 + child.Length];
        WriteUInt32(inlinePayload, 0, 2);
        WriteUInt32(inlinePayload, 4, checked((uint)child.Length));
        child.CopyTo(inlinePayload, 8);
        byte[] parentInline = SmoDataBlockWriter.BuildField(4, inlinePayload);
        byte[] parentTerminal = SmoDataBlockWriter.BuildField(7, ReadOnlySpan<byte>.Empty);
        byte[] parent = new byte[8 + parentInline.Length + parentTerminal.Length];
        WriteUInt32(parent, 0, SmoClassIds.Node);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(parent, 4);
        parentInline.CopyTo(parent, 8);
        parentTerminal.CopyTo(parent, 8 + parentInline.Length);

        byte[] parentName = Encoding.ASCII.GetBytes("parent\0");
        byte[] childName = Encoding.ASCII.GetBytes("child\0");
        int tableSize = 18 + parentName.Length + 18 + childName.Length;
        int dataStart = SmoHeader.Size + tableSize + 4;
        byte[] data = new byte[dataStart + parent.Length];
        Encoding.ASCII.GetBytes("FFPS").CopyTo(data, 0);
        WriteUInt32(data, 0x04, 0x26);
        WriteUInt32(data, 0x0C, checked((uint)data.Length));
        WriteUInt32(data, 0x10, 2);
        WriteUInt32(data, 0x14, checked((uint)dataStart));
        WriteUInt32(data, 0x18, checked((uint)parent.Length));
        WriteUInt32(data, 0x1C, 2);
        int cursor = SmoHeader.ObjectTableOffset;
        WriteDirectoryEntry(data, ref cursor, 1, parentName, SmoClassIds.Node,
            0, checked((uint)parent.Length));
        int childLogicalOffset = 8 + parentInline.Length - child.Length;
        WriteDirectoryEntry(data, ref cursor, 2, childName, SmoClassIds.Node,
            checked((uint)childLogicalOffset), checked((uint)child.Length));
        parent.CopyTo(data, dataStart);
        return data;
    }

    private static byte[] CreateReferencedNodeDocument()
    {
        byte[] rotation = SmoDataBlockWriter.BuildField(
            1,SmoPropertyValueCodec.Encode(Quaternion.Identity));
        byte[] bone = SmoDataBlockWriter.BuildField(3,[1]);
        byte[] isStatic = SmoDataBlockWriter.BuildField(4,[1]);
        byte[] animated = SmoDataBlockWriter.BuildField(8,[0]);
        byte[] childLink = SmoDataBlockWriter.BuildField(
            5,[..BitConverter.GetBytes(2u),0,0,0,0]);
        byte[] billboard = SmoDataBlockWriter.BuildField(
            6,BitConverter.GetBytes(2u));
        byte[] terminator = SmoDataBlockWriter.BuildField(
            0,ReadOnlySpan<byte>.Empty);
        byte[][] parentFields =
            [rotation,bone,isStatic,animated,childLink,billboard,terminator];
        byte[] parent = new byte[8 + parentFields.Sum(field => field.Length)];
        WriteUInt32(parent,0,SmoClassIds.Node);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(parent,4);
        int parentCursor = 8;
        foreach (byte[] field in parentFields)
        {
            field.CopyTo(parent,parentCursor);
            parentCursor += field.Length;
        }

        byte[] childAnimated = SmoDataBlockWriter.BuildField(8,[0]);
        byte[] child = new byte[8 + childAnimated.Length + terminator.Length];
        WriteUInt32(child,0,SmoClassIds.Node);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(child,4);
        childAnimated.CopyTo(child,8);
        terminator.CopyTo(child,8 + childAnimated.Length);

        byte[] parentName = Encoding.ASCII.GetBytes("parent\0");
        byte[] childName = Encoding.ASCII.GetBytes("child\0");
        int tableSize = 18 + parentName.Length + 18 + childName.Length;
        int dataStart = SmoHeader.Size + tableSize + sizeof(uint);
        byte[] data = new byte[dataStart + parent.Length + child.Length];
        Encoding.ASCII.GetBytes("FFPS").CopyTo(data,0);
        WriteUInt32(data,0x04,0x26);
        WriteUInt32(data,0x0C,checked((uint)data.Length));
        WriteUInt32(data,0x10,2);
        WriteUInt32(data,0x14,checked((uint)dataStart));
        WriteUInt32(data,0x18,checked((uint)(parent.Length + child.Length)));
        WriteUInt32(data,0x1C,2);
        int cursor = SmoHeader.ObjectTableOffset;
        WriteDirectoryEntry(data,ref cursor,1,parentName,SmoClassIds.Node,
            0,checked((uint)parent.Length));
        WriteDirectoryEntry(data,ref cursor,2,childName,SmoClassIds.Node,
            checked((uint)parent.Length),checked((uint)child.Length));
        parent.CopyTo(data,dataStart);
        child.CopyTo(data,dataStart + parent.Length);
        return data;
    }

    private static byte[] CreateReferencedRenderNodeDocument()
    {
        byte[] position = SmoDataBlockWriter.BuildField(
            0,SmoPropertyValueCodec.Encode(new Vector3(2,3,4)));
        byte[] animated = SmoDataBlockWriter.BuildField(8,[1]);
        byte[] childLink = SmoDataBlockWriter.BuildField(
            5,[..BitConverter.GetBytes(3u),0,0,0,0]);
        byte[] billboard = SmoDataBlockWriter.BuildField(
            6,BitConverter.GetBytes(1u));
        byte[] terminator = SmoDataBlockWriter.BuildField(
            0,ReadOnlySpan<byte>.Empty);
        byte[] renderablePayload = new byte[2 * sizeof(uint)];
        WriteUInt32(renderablePayload,0,2);
        WriteUInt32(renderablePayload,4,0);
        byte[] renderable = SmoDataBlockWriter.BuildField(0,renderablePayload);
        byte[][] renderNodeFields =
        [
            position,animated,childLink,billboard,terminator,
            renderable,terminator
        ];
        byte[] renderNode = new byte[
            8 + renderNodeFields.Sum(field => field.Length)];
        WriteUInt32(renderNode,0,SmoClassIds.RenderNode);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(renderNode,4);
        int renderCursor = 8;
        foreach (byte[] field in renderNodeFields)
        {
            field.CopyTo(renderNode,renderCursor);
            renderCursor += field.Length;
        }

        byte[] model = new byte[8 + terminator.Length];
        WriteUInt32(model,0,SmoClassIds.Model);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(model,4);
        terminator.CopyTo(model,8);

        byte[] child = new byte[8 + animated.Length + terminator.Length];
        WriteUInt32(child,0,SmoClassIds.Node);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(child,4);
        animated.CopyTo(child,8);
        terminator.CopyTo(child,8 + animated.Length);

        byte[] renderName = Encoding.ASCII.GetBytes("render\0");
        byte[] modelName = Encoding.ASCII.GetBytes("model\0");
        byte[] childName = Encoding.ASCII.GetBytes("child\0");
        int tableSize = 18 + renderName.Length + 18 + modelName.Length +
                        18 + childName.Length;
        int dataStart = SmoHeader.Size + tableSize + sizeof(uint);
        int serializedSize = renderNode.Length + model.Length + child.Length;
        byte[] data = new byte[dataStart + serializedSize];
        Encoding.ASCII.GetBytes("FFPS").CopyTo(data,0);
        WriteUInt32(data,0x04,0x26);
        WriteUInt32(data,0x0C,checked((uint)data.Length));
        WriteUInt32(data,0x10,3);
        WriteUInt32(data,0x14,checked((uint)dataStart));
        WriteUInt32(data,0x18,checked((uint)serializedSize));
        WriteUInt32(data,0x1C,3);
        int cursor = SmoHeader.ObjectTableOffset;
        WriteDirectoryEntry(data,ref cursor,1,renderName,SmoClassIds.RenderNode,
            0,checked((uint)renderNode.Length));
        WriteDirectoryEntry(data,ref cursor,2,modelName,SmoClassIds.Model,
            checked((uint)renderNode.Length),checked((uint)model.Length));
        WriteDirectoryEntry(data,ref cursor,3,childName,SmoClassIds.Node,
            checked((uint)(renderNode.Length + model.Length)),
            checked((uint)child.Length));
        renderNode.CopyTo(data,dataStart);
        model.CopyTo(data,dataStart + renderNode.Length);
        child.CopyTo(data,dataStart + renderNode.Length + model.Length);
        return data;
    }

    private static void WriteDirectoryEntry(
        byte[] data,
        ref int cursor,
        uint id,
        byte[] rawName,
        uint typeHash,
        uint logicalOffset,
        uint serializedSize)
    {
        WriteUInt32(data, cursor, id);
        BinaryPrimitives.WriteUInt16LittleEndian(
            data.AsSpan(cursor + 4), checked((ushort)rawName.Length));
        rawName.CopyTo(data, cursor + 6);
        int fields = cursor + 6 + rawName.Length;
        WriteUInt32(data, fields, typeHash);
        WriteUInt32(data, fields + 4, logicalOffset);
        WriteUInt32(data, fields + 8, serializedSize);
        cursor += 18 + rawName.Length;
    }

    private static void TestSharedStaticMatrices()
    {
        static SmoDocument Document(params byte[][] fields)
        {
            byte[] body = [..BitConverter.GetBytes(SmoClassIds.StaticRenderObject), .."SBOO"u8,
                ..fields.SelectMany(item => item), 0];
            return SmoDocument.Parse(CreateSingleObjectDocument("static",SmoClassIds.StaticRenderObject,body));
        }
        static byte[] MatrixField(int id,Matrix4x4 value)
        {
            byte[] bytes = new byte[64];WriteMatrix(bytes,value);
            return SmoDataBlockWriter.BuildField(id,bytes);
        }
        var empty=Document();
        True(SmoStaticRenderObjectDecoder.TryDecode(empty,empty.Objects[0],out var defaults,out _) &&
            defaults.Transform==Matrix4x4.Identity && defaults.EngineInverseTransform==Matrix4x4.Identity &&
            defaults.Renderables.Count==0 && defaults.SerializedFieldMask==0,"original static constructor defaults without authored fields");
        Matrix4x4 nonAffine=new(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);
        Matrix4x4 independent=Matrix4x4.CreateTranslation(-7,-8,-9);
        var document=Document(MatrixField(2,independent),MatrixField(1,Matrix4x4.Identity),
            SmoDataBlockWriter.BuildField(9,new byte[]{42}),MatrixField(1,nonAffine));
        True(SmoStaticRenderObjectDecoder.TryDecode(document,document.Objects[0],out var actual,out _) &&
            actual.Transform==nonAffine && actual.EngineInverseTransform==independent && actual.SerializedFieldMask==6,
            "original static independent matrices, arbitrary order, unknown field and last repeated value");
        var nullReference=Document(SmoDataBlockWriter.BuildField(0,new byte[4]));
        True(!SmoStaticRenderObjectDecoder.TryDecode(nullReference,nullReference.Objects[0],out _,out _),
            "original static null renderable refusal");
        var truncated=Document(SmoDataBlockWriter.BuildField(1,new byte[63]));
        True(!SmoStaticRenderObjectDecoder.TryDecode(truncated,truncated.Objects[0],out _,out _),
            "host bounded static matrix descriptor refusal");
        byte[] reference=[2,0,0,0,0,0,0,0];
        byte[] field=SmoDataBlockWriter.BuildField(0,reference);
        byte[] body=[..BitConverter.GetBytes(SmoClassIds.StaticRenderObject),.."SBOO"u8,..field,..field,0];
        byte[] model=[..BitConverter.GetBytes(SmoClassIds.Model),.."SBOO"u8,0];
        var repeated=SmoDocument.Parse(CreateMultiObjectDocument(
            (1u,"static",SmoClassIds.StaticRenderObject,body),(2u,"model",SmoClassIds.Model,model)));
        True(SmoStaticRenderObjectDecoder.TryDecode(repeated,repeated.Objects[0],out var links,out _) &&
            links.Renderables.Count==2 && links.Renderables.All(item=>item.ObjectId==2 &&
                item.Encoding==SmoNodeRelationshipEncoding.SizedReference) && links.SerializedFieldMask==1,
            "static metadata preserves repeated sized renderable references in order");
    }

    private static void TestPlacementTransformWriter()
    {
        Matrix4x4 original = Matrix4x4.CreateScale(2,3,4) *
            Matrix4x4.CreateRotationY(0.35f) *
            Matrix4x4.CreateTranslation(10, 20, 30);
        Matrix4x4 originalInverse =
            SmoPlacementTransformWriter.CreateLegacyStaticInverseTransform(original);
        byte[] modelBody = new byte[9];
        WriteUInt32(modelBody,0,SmoClassIds.Model);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(modelBody,4);
        byte[] renderablePayload = new byte[8 + modelBody.Length];
        WriteUInt32(renderablePayload,0,2);
        WriteUInt32(renderablePayload,4,checked((uint)modelBody.Length));
        modelBody.CopyTo(renderablePayload,8);
        byte[] renderable = SmoDataBlockWriter.BuildField(0,renderablePayload);
        byte[] terminator = SmoDataBlockWriter.BuildField(
            0,ReadOnlySpan<byte>.Empty);
        byte[] body = new byte[
            8 + 2 + 64 + 2 + 64 + renderable.Length + terminator.Length];
        WriteUInt32(body, 0, SmoClassIds.StaticRenderObject);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(body, sizeof(uint));
        body[8] = 0xA1;
        body[9] = 64;
        WriteMatrix(body.AsSpan(10, 64), original);
        body[74] = 0xA2;
        body[75] = 64;
        WriteMatrix(body.AsSpan(76, 64), originalInverse);
        renderable.CopyTo(body,140);
        terminator.CopyTo(body,140 + renderable.Length);

        int modelLogicalOffset = 140 + (renderable.Length-renderablePayload.Length) + 8;
        SmoDocument document = SmoDocument.Parse(CreateInlineObjectDocument(
            "placement",SmoClassIds.StaticRenderObject,body,
            "model",SmoClassIds.Model,modelLogicalOffset,modelBody.Length));
        True(Matrix4x4.Invert(original,out Matrix4x4 mathematicalInverse),
            "scaled placement has a mathematical inverse fixture");
        byte[] mathematicalBody = (byte[])body.Clone();
        WriteMatrix(mathematicalBody.AsSpan(76,64),mathematicalInverse);
        SmoDocument mathematicalDocument = SmoDocument.Parse(CreateInlineObjectDocument(
            "mathematical-placement",SmoClassIds.StaticRenderObject,mathematicalBody,
            "model",SmoClassIds.Model,modelLogicalOffset,modelBody.Length));
        True(SmoStaticRenderObjectDecoder.TryDecode(
                mathematicalDocument,mathematicalDocument.Objects[0],
                out SmoStaticRenderObjectData? mathematicalStatic,out _) &&
             MatrixDifference(mathematicalStatic.EngineInverseTransform,mathematicalInverse) < 0.00001f,
            "static decoder preserves the independent stored inverse");
        True(SmoStaticRenderObjectTransformDecoder.TryDecode(
                mathematicalDocument,mathematicalDocument.Objects[0],
                out Matrix4x4 mathematicalTransform) &&
             MatrixDifference(mathematicalTransform,original) < 0.00001f,
            "shared transform decoder preserves mathematical-inverse placement");
        Matrix4x4 desired = original;
        desired.M41 += 5;
        desired.M42 -= 2;
        desired.M43 += 9;
        SmoPlacementTransformPatchResult patch =
            SmoPlacementTransformWriter.Patch(document,
            [
                new SmoPlacementTransformEdit(0, original, desired)
            ]);
        SmoDocument verified = SmoDocument.Parse(patch.Data);

        Equal(1, patch.EditedSceneObjectCount,
            "placement writer edit count");
        Equal(1, patch.StaticObjectIndices.Count,
            "placement writer owner count");
        Equal(document.Data.Length, verified.Data.Length,
            "placement writer preserves file size");
        True(SmoStaticRenderObjectTransformDecoder.TryDecode(
                verified, verified.Objects[0], out Matrix4x4 actual),
            "placement writer output decodes");
        True(MatrixDifference(actual, desired) < 0.00001f,
            "placement writer stores requested world transform");
        Matrix4x4 storedInverse = ReadMatrix(
            verified.Data.Span.Slice(
                checked((int)verified.Objects[0].PhysicalOffset + 76), 64));
        Matrix4x4 expectedEngineInverse =
            SmoPlacementTransformWriter.CreateLegacyStaticInverseTransform(desired);
        True(MatrixDifference(storedInverse,expectedEngineInverse) < 0.00001f,
            "legacy placement policy retains the observed transpose-basis convention");
        True(MatrixDifference(actual * storedInverse,Matrix4x4.Identity) > 0.1f,
            "scaled Sparkplug inverse is deliberately not a mathematical inverse");
        True(SmoStaticRenderObjectDecoder.TryDecode(
                verified,verified.Objects[0],
                out SmoStaticRenderObjectData? decodedStatic,out string staticError) &&
             decodedStatic is not null &&
             MatrixDifference(decodedStatic.EngineInverseTransform,expectedEngineInverse) < 0.00001f &&
             decodedStatic.Renderable.TargetObjectIndex == 1,
            $"complete static-render-object structure decodes: {staticError}");
        IReadOnlyList<SmoSerializedFieldValue> inspectedStatic =
            SmoSerializedFieldInspector.Inspect(verified,verified.Objects[0]);
        Equal(3,inspectedStatic.Count,
            "static-render-object inspector exposes all three fields");
        True(inspectedStatic.All(field => field.IsDecoded),
            "static-render-object inspector decodes matrices and model relationship");
        True(document.Data.Span[..checked((int)document.Objects[0].PhysicalOffset + 10)]
                .SequenceEqual(verified.Data.Span[..checked((int)verified.Objects[0].PhysicalOffset + 10)]),
            "placement writer preserves bytes before matrix payload");
        int physicalOffset = checked((int)document.Objects[0].PhysicalOffset);
        True(Enumerable.Range(0, patch.Data.Length).All(index =>
                (index >= physicalOffset + 10 && index < physicalOffset + 74) ||
                (index >= physicalOffset + 76 && index < physicalOffset + 140) ||
                document.Data.Span[index] == patch.Data[index]),
            "placement writer changes only the two matrix payloads");

        byte[] nodeBody = new byte[8 + 14 + 18 + 14 + 1];
        WriteUInt32(nodeBody, 0, SmoClassIds.Node);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(nodeBody, sizeof(uint));
        int nodeOffset = 8;
        nodeBody[nodeOffset++] = 0xA0;
        nodeBody[nodeOffset++] = 12;
        WriteVector3(nodeBody.AsSpan(nodeOffset, 12), new Vector3(1, 2, 3));
        nodeOffset += 12;
        nodeBody[nodeOffset++] = 0xA1;
        nodeBody[nodeOffset++] = 16;
        WriteVector3(nodeBody.AsSpan(nodeOffset, 12), Vector3.Zero);
        WriteSingle(nodeBody.AsSpan(nodeOffset + 12, 4), 1);
        nodeOffset += 16;
        nodeBody[nodeOffset++] = 0xA2;
        nodeBody[nodeOffset++] = 12;
        WriteVector3(nodeBody.AsSpan(nodeOffset, 12), Vector3.One);
        SmoDocument nodeDocument = SmoDocument.Parse(CreateSingleObjectDocument(
            "collision", SmoClassIds.Node, nodeBody));
        Matrix4x4 nodeBefore = Matrix4x4.CreateTranslation(1, 2, 3);
        Matrix4x4 nodeAfter = Matrix4x4.CreateTranslation(11, -2, 8);
        SmoPlacementTransformPatchResult nodePatch =
            SmoPlacementTransformWriter.Patch(nodeDocument,
            [
                new SmoPlacementTransformEdit(0, nodeBefore, nodeAfter)
            ]);
        SmoDocument nodeVerified = SmoDocument.Parse(nodePatch.Data);
        True(nodePatch.StaticObjectIndices.Count == 0,
            "node transform patch does not claim a static placement");
        True(nodePatch.NodeObjectIndices.SequenceEqual([0]),
            "node transform patch reports its node owner");
        True(SmoNodeTransformDecoder.TryDecode(
                nodeVerified, nodeVerified.Objects[0], out SmoNodeTransform? movedNode) &&
             movedNode is not null,
            "node transform patch remains decodable");
        True(Vector3.Distance(movedNode!.Position, new Vector3(11, -2, 8)) < 0.00001f,
            "node transform patch stores the requested world translation");

        byte[] sparseNodeBody = new byte[8 + 14 + 2];
        WriteUInt32(sparseNodeBody, 0, SmoClassIds.RenderNode);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(sparseNodeBody, sizeof(uint));
        sparseNodeBody[8] = 0xA0;
        sparseNodeBody[9] = 12;
        WriteVector3(sparseNodeBody.AsSpan(10, 12), new Vector3(2, 3, 4));
        sparseNodeBody[^2] = 0;
        sparseNodeBody[^1] = 0;
        SmoDocument sparseNode = SmoDocument.Parse(CreateSingleObjectDocument(
            "sparse", SmoClassIds.RenderNode, sparseNodeBody));
        SmoObjectCapabilities sparseCapabilities = SmoSchemaRegistry.Describe(
            sparseNode, sparseNode.Objects[0]);
        True(sparseCapabilities.Supports(SmoPropertyKeys.Rotation) &&
             sparseCapabilities.Supports(SmoPropertyKeys.Scale),
            "node schema exposes materializable optional transforms");
        Matrix4x4 sparseOriginal = Matrix4x4.CreateTranslation(2, 3, 4);
        Matrix4x4 sparseDesired =
            Matrix4x4.CreateScale(1.5f, 0.75f, 2f) *
            Matrix4x4.CreateRotationY(0.4f) *
            Matrix4x4.CreateTranslation(8, 9, 10);
        SmoDocument sparseResult = SmoDocument.Parse(
            SmoPlacementTransformWriter.Patch(
                sparseNode,
                [new SmoPlacementTransformEdit(0, sparseOriginal, sparseDesired)]).Data);
        True(SmoNodeTransformDecoder.TryDecode(
                sparseResult, sparseResult.Objects[0], out SmoNodeTransform? expanded) &&
             expanded is not null &&
             MatrixDifference(expanded.LocalMatrix, sparseDesired) < 0.001f,
            "node writer materializes missing rotation and scale fields");
        IReadOnlyList<SmoObjectField> expandedFields = SmoObjectFieldReader.Read(
            sparseResult, sparseResult.Objects[0]);
        True(expandedFields.Any(field => field.FieldType == 1 && field.PayloadSize == 16) &&
             expandedFields.Any(field => field.FieldType == 2 && field.PayloadSize == 12),
            "materialized node transform remains schema-readable");
    }

    private static void TestCollisionMeshDecoder()
    {
        const int payloadSize = 66;
        byte[] geometryPayload = new byte[payloadSize];
        WriteUInt32(geometryPayload, 0, 2);
        WriteUInt32(geometryPayload, 4, 1);
        WriteUInt32(geometryPayload, 8, 0);
        BinaryPrimitives.WriteUInt16LittleEndian(geometryPayload.AsSpan(12), 0);
        BinaryPrimitives.WriteUInt16LittleEndian(geometryPayload.AsSpan(14), 1);
        BinaryPrimitives.WriteUInt16LittleEndian(geometryPayload.AsSpan(16), 2);
        WriteUInt32(geometryPayload, 18, 0);
        WriteUInt32(geometryPayload, 22, 3);
        WriteUInt32(geometryPayload, 26, 0);
        WriteVector3(geometryPayload.AsSpan(30), Vector3.Zero);
        WriteVector3(geometryPayload.AsSpan(42), Vector3.UnitX);
        WriteVector3(geometryPayload.AsSpan(54), Vector3.UnitY);
        byte[] geometryField = SmoDataBlockWriter.BuildField(0,geometryPayload);
        byte[] facePayload =
        [
            0x17,0x4C,0x3C,0x31, 1,0,0,0,
            1,1,9, 2,2,0x34,0x12, 3,1,7, 0
        ];
        byte[] faceField = SmoDataBlockWriter.BuildField(1,facePayload);
        byte[] meshTerminator = SmoDataBlockWriter.BuildField(
            0,ReadOnlySpan<byte>.Empty);
        var meshBody = new List<byte>();
        meshBody.AddRange(BitConverter.GetBytes(SmoClassIds.MeshBoundingVolume));
        meshBody.AddRange(Encoding.ASCII.GetBytes("SBOO"));
        meshBody.AddRange(geometryField);
        meshBody.AddRange(faceField);
        meshBody.AddRange(meshTerminator);
        byte[] body = meshBody.ToArray();
        SmoDocument document = SmoDocument.Parse(CreateSingleObjectDocument(
            "collision-shape", SmoClassIds.MeshBoundingVolume, body));

        True(SmoCollisionMeshDecoder.TryDecodeShape(
                document, document.Objects[0], out Vector3[] positions,
                out int[] indices),
            "spMeshBV triangle geometry decodes");
        Equal(3, positions.Length, "spMeshBV vertex count");
        True(indices.SequenceEqual([0, 1, 2]), "spMeshBV triangle indices");
        True(positions.SequenceEqual([Vector3.Zero, Vector3.UnitX, Vector3.UnitY]),
            "spMeshBV vertex positions");
        True(SmoMeshBoundingVolumeDecoder.TryDecode(
                document,document.Objects[0],out SmoMeshBoundingVolumeData? meshBv,
                out string meshBvError) && meshBv is not null,
            "complete spMeshBV decodes: " + meshBvError);
        Equal(1,meshBv!.FaceData!.Count,"spMeshBV wxFaceData count");
        Equal((byte)9,meshBv.FaceData[0].SurfaceType,
            "spMeshBV surface type");
        Equal((ushort)0x1234,meshBv.FaceData[0].Flags,"spMeshBV face flags");
        Equal((byte)7,meshBv.FaceData[0].SurfaceId,"spMeshBV surface ID");
        Equal((byte)0b111,meshBv.FaceData[0].SerializedFieldMask,
            "spMeshBV sparse face mask");
        Equal(2u,meshBv.PrimitiveType,"spMeshBV first word is index-buffer primitive type");
        Equal(geometryField.Length - geometryPayload.Length + 30,meshBv.VertexPayloadOffset,
            "MeshBV vertex edit offset is observed by the original reader");
        True(SmoMeshBoundingVolumeDecoder.TryDecodeGeometry(geometryPayload,
                out uint primitiveType,out var leafPositions,out var leafIndices,
                out int leafVertexOffset,out var leafError),
            "standalone geometry uses shared buffer readers: " + leafError);
        True(primitiveType == 2 && leafVertexOffset == 30 &&
             leafPositions.SequenceEqual(positions) && leafIndices.SequenceEqual(indices),
            "whole-resource and direct-field native geometry views agree");
        byte[] unusualFaces = Convert.FromHexString(
            "174C3C3101000000AC0202AA550301110101090202CDAB0301DE00");
        True(SmoMeshBoundingVolumeDecoder.TryDecodeFaceData(unusualFaces,23,
                out var unusual,out var unusualError),
            "original face reader accepts independent count and unknown/repeated fields: " + unusualError);
        True(unusual.Length == 1 && unusual[0] == new SmoMeshBoundingVolumeFaceData(9,0xABCD,222,7),
            "original PC face reader result survives the C# bridge");
        byte[] explicitZero = Convert.FromHexString("174C3C310100000001010000");
        True(SmoMeshBoundingVolumeDecoder.TryDecodeFaceData(explicitZero,1,out var zero,out _) &&
             zero[0].SurfaceType == 0 && zero[0].SerializedFieldMask == 1,
            "face field observation distinguishes explicit zero from an absent member");
        True(!SmoMeshBoundingVolumeDecoder.TryDecodeFaceData(unusualFaces.AsSpan(0,unusualFaces.Length-1),
                1,out _,out _),"native face reader rejects a truncated record");
        True(SmoMeshBoundingVolumeDecoder.TryDecodeFaceData(facePayload,1,out var afterFailure,out _) &&
             afterFailure[0].Flags == 0x1234,"failed leaf reads do not poison the next native owner");
        IReadOnlyList<SmoSerializedFieldValue> meshFields =
            SmoSerializedFieldInspector.Inspect(document,document.Objects[0]);
        Equal(2,meshFields.Count,"spMeshBV inspector field count");
        True(meshFields.All(field => field.IsDecoded) &&
             meshFields.Any(field => field.Descriptor.Key == "mesh_bv.geometry") &&
             meshFields.Any(field => field.Descriptor.Key == "mesh_bv.face_data"),
            "spMeshBV inspector exposes geometry and face data");

        byte[] collisionInfoBody = new byte[8 + 2 + 40 + 1]; // complete original section, including terminator
        WriteUInt32(collisionInfoBody, 0, SmoClassIds.CollisionInfo);
        Encoding.ASCII.GetBytes("SBOO").CopyTo(
            collisionInfoBody, sizeof(uint));
        collisionInfoBody[8] = 0xA2;
        collisionInfoBody[9] = 40;
        Vector3 collisionPosition = new(12.5f, -7.25f, 42.0f);
        Quaternion collisionRotation = Quaternion.CreateFromAxisAngle(
            Vector3.UnitY, 0.75f);
        Vector3 collisionScale = new(2.0f, 3.0f, 4.0f);
        WriteVector3(collisionInfoBody.AsSpan(10), collisionPosition);
        WriteSingle(collisionInfoBody.AsSpan(22), collisionRotation.X);
        WriteSingle(collisionInfoBody.AsSpan(26), collisionRotation.Y);
        WriteSingle(collisionInfoBody.AsSpan(30), collisionRotation.Z);
        WriteSingle(collisionInfoBody.AsSpan(34), collisionRotation.W);
        WriteVector3(collisionInfoBody.AsSpan(38), collisionScale);
        SmoDocument collisionInfoDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "collision-info", SmoClassIds.CollisionInfo, collisionInfoBody));
        True(SmoCollisionInfoTransformDecoder.TryDecode(
                collisionInfoDocument,
                collisionInfoDocument.Objects[0],
                out SmoCollisionInfoTransform? collisionTransform) &&
             collisionTransform is not null,
            "spCollisionInfo transform decodes");
        True(Vector3.Distance(
                collisionTransform!.Position, collisionPosition) < 0.00001f &&
             Quaternion.Dot(collisionTransform.Rotation, collisionRotation) > 0.99999f &&
             Vector3.Distance(collisionTransform.Scale, collisionScale) < 0.00001f,
            "spCollisionInfo transform preserves position, rotation, and scale");

        byte[] primitivePayload = new byte[2 * sizeof(uint) + body.Length];
        WriteUInt32(primitivePayload,0,2);
        WriteUInt32(primitivePayload,sizeof(uint),checked((uint)body.Length));
        body.CopyTo(primitivePayload,2 * sizeof(uint));
        byte[] primitiveField = SmoDataBlockWriter.BuildField(0,primitivePayload);
        byte[] groupField = SmoDataBlockWriter.BuildField(1,BitConverter.GetBytes(2u));
        byte[] transformPayload = new byte[SmoCollisionInfoDecoder.TransformPayloadSize];
        WriteVector3(transformPayload,collisionPosition);
        WriteSingle(transformPayload.AsSpan(12),collisionRotation.X);
        WriteSingle(transformPayload.AsSpan(16),collisionRotation.Y);
        WriteSingle(transformPayload.AsSpan(20),collisionRotation.Z);
        WriteSingle(transformPayload.AsSpan(24),collisionRotation.W);
        WriteVector3(transformPayload.AsSpan(28),collisionScale);
        byte[] transformField = SmoDataBlockWriter.BuildField(2,transformPayload);
        byte[] terminator = SmoDataBlockWriter.BuildField(
            0,ReadOnlySpan<byte>.Empty);
        var fullCollisionBody = new List<byte>();
        fullCollisionBody.AddRange(BitConverter.GetBytes(SmoClassIds.CollisionInfo));
        fullCollisionBody.AddRange(Encoding.ASCII.GetBytes("SBOO"));
        fullCollisionBody.AddRange(primitiveField);
        fullCollisionBody.AddRange(groupField);
        fullCollisionBody.AddRange(transformField);
        fullCollisionBody.AddRange(terminator);
        int childOffset = 8 + primitiveField.Length - body.Length;
        SmoDocument fullCollisionDocument = SmoDocument.Parse(
            CreateInlineObjectDocument(
                "collision",SmoClassIds.CollisionInfo,fullCollisionBody.ToArray(),
                "primitive",SmoClassIds.MeshBoundingVolume,childOffset,body.Length));
        True(SmoCollisionInfoDecoder.TryDecode(
                fullCollisionDocument,fullCollisionDocument.Objects[0],
                out SmoCollisionInfoData? fullCollision,out string fullError) &&
             fullCollision is not null,
            "complete spCollisionInfo shape decodes: " + fullError);
        Equal(2u,fullCollision!.CollisionGroup!.Value,
            "spCollisionInfo collision group decodes");
        Equal(SmoClassIds.MeshBoundingVolume,
            fullCollision.Primitive.TargetTypeHash!.Value,
            "spCollisionInfo primitive resolves to bounding-volume child");
        Equal((byte)0b111,fullCollision.SerializedFieldMask,
            "spCollisionInfo full field-presence mask");
        IReadOnlyList<SmoSerializedFieldValue> collisionFields =
            SmoSerializedFieldInspector.Inspect(
                fullCollisionDocument,fullCollisionDocument.Objects[0]);
        Equal(3,collisionFields.Count,
            "spCollisionInfo inspector exposes all three fields");
        True(collisionFields.All(item => item.IsDecoded),
            "spCollisionInfo inspector decodes primitive, group and transform");

        var primitiveOnlyBody = new List<byte>();
        primitiveOnlyBody.AddRange(BitConverter.GetBytes(SmoClassIds.CollisionInfo));
        primitiveOnlyBody.AddRange(Encoding.ASCII.GetBytes("SBOO"));
        primitiveOnlyBody.AddRange(primitiveField);
        primitiveOnlyBody.AddRange(terminator);
        SmoDocument primitiveOnlyDocument = SmoDocument.Parse(
            CreateInlineObjectDocument(
                "collision",SmoClassIds.CollisionInfo,primitiveOnlyBody.ToArray(),
                "primitive",SmoClassIds.MeshBoundingVolume,childOffset,body.Length));
        True(SmoCollisionInfoDecoder.TryDecode(
                primitiveOnlyDocument,primitiveOnlyDocument.Objects[0],
                out SmoCollisionInfoData? primitiveOnly,out string primitiveError) &&
             primitiveOnly is not null &&
             primitiveOnly.CollisionGroup is null && primitiveOnly.Transform is null &&
             primitiveOnly.SerializedFieldMask == 0b001,
            "primitive-only spCollisionInfo historical shape decodes: " +
            primitiveError);
    }

    private static void TestOcclusionVolumeDecoder()
    {
        static byte[] CreateBody(string variant = "")
        {
            byte[] position = new byte[SmoNodeDecoder.VectorPayloadSize];
            WriteVector3(position,new Vector3(10,20,30));
            byte[] indexBuffer = new byte[
                SmoOcclusionVolumeDecoder.BufferHeaderSize + 6 * sizeof(ushort)];
            WriteUInt32(indexBuffer,0,
                SmoOcclusionVolumeDecoder.TriangleListPrimitiveType);
            WriteUInt32(indexBuffer,4,2);
            WriteUInt32(indexBuffer,8,
                variant == "index-flags" ? 2u : SmoOcclusionVolumeDecoder.UInt16IndexFormat);
            ushort[] indices = [0,1,2,0,2,variant == "range" ? (ushort)4 : (ushort)3];
            if (variant == "unused") indices[5] = 1;
            if (variant == "degenerate") indices[2] = 1;
            for (int index = 0;index < indices.Length;index++)
            {
                BinaryPrimitives.WriteUInt16LittleEndian(
                    indexBuffer.AsSpan(
                        SmoOcclusionVolumeDecoder.BufferHeaderSize +
                        index * sizeof(ushort),sizeof(ushort)),indices[index]);
            }
            Vector3[] positions =
            [
                new(0,0,0),new(4,0,0),new(4,3,0),new(0,3,0)
            ];
            if (variant == "nonfinite") positions[0] = new(float.NaN, 0, 0);
            byte[] vertexBuffer = new byte[
                SmoOcclusionVolumeDecoder.BufferHeaderSize +
                positions.Length * 3 * sizeof(float)];
            WriteUInt32(vertexBuffer,0,
                SmoOcclusionVolumeDecoder.PositionOnlyVertexDeclaration);
            WriteUInt32(vertexBuffer,4,(uint)positions.Length);
            WriteUInt32(vertexBuffer,8,variant == "vertex-flags" ? 1u : 0u);
            for (int index = 0;index < positions.Length;index++)
            {
                WriteVector3(vertexBuffer.AsSpan(
                    SmoOcclusionVolumeDecoder.BufferHeaderSize +
                    index * 3 * sizeof(float)),positions[index]);
            }
            byte[] terminator = SmoDataBlockWriter.BuildField(
                0,ReadOnlySpan<byte>.Empty);
            byte[][] fields =
            [
                SmoDataBlockWriter.BuildField(0,position),
                SmoDataBlockWriter.BuildField(8,[0]),
                terminator,
                SmoDataBlockWriter.BuildField(0,indexBuffer),
                SmoDataBlockWriter.BuildField(1,vertexBuffer),
                terminator
            ];
            byte[] body = new byte[8 + fields.Sum(item => item.Length)];
            WriteUInt32(body,0,SmoClassIds.OcclusionVolume);
            Encoding.ASCII.GetBytes("SBOO").CopyTo(body,4);
            int cursor = 8;
            foreach (byte[] field in fields)
            {
                field.CopyTo(body,cursor);
                cursor += field.Length;
            }
            return body;
        }

        byte[] body = CreateBody();
        SmoDocument document = SmoDocument.Parse(CreateSingleObjectDocument(
            "occlusion_wall",SmoClassIds.OcclusionVolume,body));
        True(SmoOcclusionVolumeDecoder.TryDecode(
                document,document.Objects[0],out SmoOcclusionVolumeData? decoded,
                out string error) && decoded is not null,
            "synthetic spOcclusionVolume decodes: " + error);
        Equal(new Vector3(10,20,30),decoded!.Node.Position,
            "occlusion volume preserves inherited placement");
        Equal(2u,decoded.IndexBuffer.PrimitiveCount,
            "occlusion volume exposes triangle count");
        Equal(6,decoded.IndexBuffer.TriangleIndices.Count,
            "occlusion volume exposes UInt16 triangle indices");
        Equal(4,decoded.VertexBuffer.Positions.Count,
            "occlusion volume exposes position vertices");
        Equal(new Vector3(4,3,0),decoded.VertexBuffer.Positions[2],
            "occlusion volume preserves vertex coordinates");

        IReadOnlyList<SmoSerializedFieldValue> inspected =
            SmoSerializedFieldInspector.Inspect(document,document.Objects[0]);
        Equal(4,inspected.Count,
            "occlusion inspector exposes inherited placement and both buffers");
        True(inspected.All(item => item.IsDecoded),
            "occlusion inspector decodes every observed field");
        True(inspected.Any(item =>
                item.Descriptor.Key == "occlusion_volume.index_buffer" &&
                item.DisplayValue.Contains("triangles=2",StringComparison.Ordinal)),
            "occlusion inspector summarizes the triangle list");
        True(inspected.Any(item =>
                item.Descriptor.Key == "occlusion_volume.vertex_buffer" &&
                item.DisplayValue.Contains("vertices=4",StringComparison.Ordinal)),
            "occlusion inspector summarizes positions and bounds");

        foreach (string variant in new[] { "range", "unused", "degenerate", "nonfinite", "index-flags", "vertex-flags" })
        {
            byte[] invalidBody = CreateBody(variant);
            SmoDocument invalid = SmoDocument.Parse(CreateSingleObjectDocument(
                "invalid-occluder",SmoClassIds.OcclusionVolume,invalidBody));
            True(!SmoOcclusionVolumeDecoder.TryDecode(
                    invalid,invalid.Objects[0],out _,out _),
                "existing strict occlusion inspection profile rejects " + variant);
        }
    }

    private static void TestMeshNavigationSetDecoder()
    {
        static byte[] Matrix(uint rows,uint columns,params uint[] selectors)
        {
            Equal(checked((int)(rows * columns)),selectors.Length,
                "synthetic navigation matrix cardinality");
            byte[] payload = new byte[
                SmoMeshNavigationSetDecoder.MatrixHeaderSize + selectors.Length * 4];
            WriteUInt32(payload,0,rows);
            WriteUInt32(payload,4,columns);
            for (int index = 0;index < selectors.Length;index++)
                WriteUInt32(payload,8 + index * 4,selectors[index]);
            return payload;
        }

        static byte[] MeshBody()
        {
            ushort[] indices = [0,1,2,2,1,3];
            Vector3[] positions =
            [
                new(0,0,0),new(1,0,0),new(0,0,1),new(1,0,1)
            ];
            byte[] geometry = new byte[
                3 * sizeof(uint) + indices.Length * sizeof(ushort) +
                3 * sizeof(uint) + positions.Length * 3 * sizeof(float)];
            WriteUInt32(geometry,0,SmoMeshBoundingVolumeDecoder.CurrentVersion);
            WriteUInt32(geometry,4,2);
            WriteUInt32(geometry,8,0);
            int cursor = 12;
            foreach (ushort index in indices)
            {
                BinaryPrimitives.WriteUInt16LittleEndian(
                    geometry.AsSpan(cursor,sizeof(ushort)),index);
                cursor += sizeof(ushort);
            }
            WriteUInt32(geometry,cursor,0);
            WriteUInt32(geometry,cursor + 4,(uint)positions.Length);
            WriteUInt32(geometry,cursor + 8,0);
            cursor += 12;
            foreach (Vector3 position in positions)
            {
                WriteVector3(geometry.AsSpan(cursor),position);
                cursor += 12;
            }
            byte[] geometryField = SmoDataBlockWriter.BuildField(0,geometry);
            byte[] terminator = SmoDataBlockWriter.BuildField(
                0,ReadOnlySpan<byte>.Empty);
            byte[] body = new byte[8 + geometryField.Length + terminator.Length];
            WriteUInt32(body,0,SmoClassIds.MeshBoundingVolume);
            Encoding.ASCII.GetBytes("SBOO").CopyTo(body,4);
            geometryField.CopyTo(body,8);
            terminator.CopyTo(body,8 + geometryField.Length);
            return body;
        }

        static (byte[] Document,int NavigationIndex) Create(
            bool invalidSelector,bool invalidTerminal=false)
        {
            byte[] mesh = MeshBody();
            byte[] position = new byte[SmoNodeDecoder.VectorPayloadSize];
            WriteVector3(position,new Vector3(4,5,6));
            byte[] links = new byte[16];
            WriteUInt32(links,0,2);
            links[4] = 0;
            WriteUInt32(links,5,1);
            links[9] = 1;
            links[10] = 1;
            WriteUInt32(links,11,1);
            links[15] = 0;
            byte[] terminator = SmoDataBlockWriter.BuildField(
                0,ReadOnlySpan<byte>.Empty);
            byte[][] inherited =
            [
                SmoDataBlockWriter.BuildField(0,position),
                SmoDataBlockWriter.BuildField(8,[1]),
                terminator,
                SmoDataBlockWriter.BuildField(0,BitConverter.GetBytes(2u)),
                SmoDataBlockWriter.BuildField(1,Matrix(
                    2,2,invalidTerminal ? 2u : 3u,
                    invalidSelector ? 1u : 0u,0,3)),
                SmoDataBlockWriter.BuildField(2,Matrix(0,2)),
                SmoDataBlockWriter.BuildField(3,links),
                SmoDataBlockWriter.BuildField(5,[1]),
                terminator
            ];
            byte[] relationship = new byte[8 + mesh.Length];
            WriteUInt32(relationship,0,2);
            WriteUInt32(relationship,4,(uint)mesh.Length);
            mesh.CopyTo(relationship,8);
            byte[] meshField = SmoDataBlockWriter.BuildField(0,relationship);
            int inheritedSize = 8 + inherited.Sum(field => field.Length);
            int meshFieldHeaderSize = meshField.Length - relationship.Length;
            int meshOffset = inheritedSize + meshFieldHeaderSize + 8;
            byte[] body = new byte[
                inheritedSize + meshField.Length + terminator.Length];
            WriteUInt32(body,0,SmoClassIds.MeshNavigationSet);
            Encoding.ASCII.GetBytes("SBOO").CopyTo(body,4);
            int cursor = 8;
            foreach (byte[] field in inherited)
            {
                field.CopyTo(body,cursor);
                cursor += field.Length;
            }
            meshField.CopyTo(body,cursor);
            cursor += meshField.Length;
            terminator.CopyTo(body,cursor);
            return (CreateInlineObjectDocument(
                "nav",SmoClassIds.MeshNavigationSet,body,
                "",SmoClassIds.MeshBoundingVolume,meshOffset,mesh.Length),0);
        }

        (byte[] bytes,int index) = Create(invalidSelector:false);
        SmoDocument document = SmoDocument.Parse(bytes);
        True(SmoMeshNavigationSetDecoder.TryDecode(
                document,document.Objects[index],out SmoMeshNavigationSetData? decoded,
                out string error) && decoded is not null,
            "synthetic spMeshNavigationSet decodes: " + error);
        Equal(2u,decoded!.NodeCount,"navigation node count");
        Equal(new Vector3(4,5,6),decoded.Node.Position,
            "navigation set preserves inherited placement");
        Equal(4,decoded.NodeTransitions.LinkSelectors.Count,
            "navigation set exposes dense routing selectors");
        Equal(2,decoded.Links.Count,"navigation set exposes per-node links");
        Equal((byte)1,decoded.Links[0].NeighbourNodeIds.Single(),
            "navigation set preserves ordered UInt8 neighbour IDs");
        True(decoded.HasReciprocalLinks,
            "synthetic navigation mesh has reciprocal links");
        Equal(SmoClassIds.MeshBoundingVolume,
            decoded.NavigationMesh.TargetTypeHash.GetValueOrDefault(),
            "navigation set resolves its spMeshBV");
        True(decoded.Enabled,"navigation set preserves Enabled");

        IReadOnlyList<SmoSerializedFieldValue> inspected =
            SmoSerializedFieldInspector.Inspect(document,document.Objects[index]);
        Equal(8,inspected.Count,
            "navigation inspector exposes node, base and mesh fields");
        True(inspected.All(item => item.IsDecoded),
            "navigation inspector decodes every synthetic field");
        True(inspected.Any(item =>
                item.Descriptor.Key == "navigation_set.transition_table" &&
                item.DisplayValue.Contains("2x2",StringComparison.Ordinal)),
            "navigation inspector summarizes the routing matrix");
        True(inspected.Any(item =>
                item.Descriptor.Key == "navigation_set.links" &&
                item.DisplayValue.Contains("directed links=2",StringComparison.Ordinal)),
            "navigation inspector summarizes adjacency");

        SmoDocument invalid = SmoDocument.Parse(Create(invalidSelector:true).Document);
        True(SmoMeshNavigationSetDecoder.TryDecode(
                invalid,invalid.Objects[0],out var retainedSelector,out _),
            "original reader stores a selector without validating graph routing");
        Equal(1u,retainedSelector!.NodeTransitions[0,1],"original two-bit table retains selector 1");

        SmoDocument invalidTerminal = SmoDocument.Parse(
            Create(invalidSelector:false,invalidTerminal:true).Document);
        True(SmoMeshNavigationSetDecoder.TryDecode(
                invalidTerminal,invalidTerminal.Objects[0],out var retainedTerminal,out _),
            "original reader stores a diagonal value without imposing terminal semantics");
        Equal(2u,retainedTerminal!.NodeTransitions[0,0],"original table retains diagonal value 2");
    }

    private static void WriteMatrix(Span<byte> destination, Matrix4x4 value)
    {
        float[] cells =
        [
            value.M11, value.M12, value.M13, value.M14,
            value.M21, value.M22, value.M23, value.M24,
            value.M31, value.M32, value.M33, value.M34,
            value.M41, value.M42, value.M43, value.M44
        ];
        for (int index = 0; index < cells.Length; index++)
        {
            BinaryPrimitives.WriteInt32LittleEndian(
                destination.Slice(index * 4, 4),
                BitConverter.SingleToInt32Bits(cells[index]));
        }
    }

    private static void WriteVector3(Span<byte> destination, Vector3 value)
    {
        WriteSingle(destination, value.X);
        WriteSingle(destination[4..], value.Y);
        WriteSingle(destination[8..], value.Z);
    }

    private static void WriteSingle(Span<byte> destination, float value) =>
        BinaryPrimitives.WriteInt32LittleEndian(
            destination, BitConverter.SingleToInt32Bits(value));

    private static Matrix4x4 ReadMatrix(ReadOnlySpan<byte> source)
    {
        Span<float> cells = stackalloc float[16];
        for (int index = 0; index < cells.Length; index++)
        {
            cells[index] = BitConverter.Int32BitsToSingle(
                BinaryPrimitives.ReadInt32LittleEndian(source.Slice(index * 4, 4)));
        }
        return new Matrix4x4(
            cells[0], cells[1], cells[2], cells[3],
            cells[4], cells[5], cells[6], cells[7],
            cells[8], cells[9], cells[10], cells[11],
            cells[12], cells[13], cells[14], cells[15]);
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

    private static byte[] CreateMultiObjectDocument(
        params (uint Id,string Name,uint TypeHash,byte[] Body)[] objects)
    {
        if (objects.Length == 0)
            throw new ArgumentException("At least one object is required.",nameof(objects));
        byte[][] names = objects
            .Select(item => Encoding.ASCII.GetBytes(item.Name + "\0"))
            .ToArray();
        int tableLength = objects
            .Select((_,index) => 18 + names[index].Length)
            .Sum();
        int dataStart = SmoHeader.Size + tableLength + sizeof(uint);
        int serializedSize = objects.Sum(item => item.Body.Length);
        byte[] data = new byte[dataStart + serializedSize];
        Encoding.ASCII.GetBytes("FFPS").CopyTo(data,0);
        WriteUInt32(data,0x04,0x26);
        WriteUInt32(data,0x0C,checked((uint)data.Length));
        WriteUInt32(data,0x10,checked((uint)(objects.Length + 1)));
        WriteUInt32(data,0x14,checked((uint)dataStart));
        WriteUInt32(data,0x18,checked((uint)serializedSize));
        WriteUInt32(data,0x1C,checked((uint)objects.Length));

        int tableCursor = SmoHeader.ObjectTableOffset;
        uint logicalOffset = 0;
        for (int index = 0;index < objects.Length;index++)
        {
            (uint id,_,uint typeHash,byte[] body) = objects[index];
            WriteDirectoryEntry(
                data,ref tableCursor,id,names[index],typeHash,logicalOffset,
                checked((uint)body.Length));
            logicalOffset = checked(logicalOffset + (uint)body.Length);
        }
        int bodyCursor = dataStart;
        foreach ((_,_,_,byte[] body) in objects)
        {
            body.CopyTo(data,bodyCursor);
            bodyCursor += body.Length;
        }
        return data;
    }

    private static byte[] CreateCatalogDocument(
        byte[] storage,
        params (uint Id,string Name,uint TypeHash,uint Offset,uint Size)[] objects)
    {
        byte[][] names=objects.Select(item=>
            Encoding.ASCII.GetBytes(item.Name+"\0")).ToArray();
        int tableLength=objects.Select((_,index)=>18+names[index].Length).Sum();
        int dataStart=SmoHeader.Size+tableLength+sizeof(uint);
        byte[] data=new byte[dataStart+storage.Length];
        Encoding.ASCII.GetBytes("FFPS").CopyTo(data,0);
        WriteUInt32(data,0x04,0x26);
        WriteUInt32(data,0x0C,checked((uint)data.Length));
        WriteUInt32(data,0x10,checked((uint)(objects.Length+1)));
        WriteUInt32(data,0x14,checked((uint)dataStart));
        WriteUInt32(data,0x18,checked((uint)storage.Length));
        WriteUInt32(data,0x1C,checked((uint)objects.Length));
        int cursor=SmoHeader.ObjectTableOffset;
        for (int index=0;index<objects.Length;index++)
        {
            var item=objects[index];
            WriteDirectoryEntry(data,ref cursor,item.Id,names[index],item.TypeHash,
                item.Offset,item.Size);
        }
        storage.CopyTo(data,dataStart);
        return data;
    }

    private static byte[] CreateInlineObjectDocument(
        string parentName,
        uint parentTypeHash,
        byte[] parentBody,
        string childName,
        uint childTypeHash,
        int childLogicalOffset,
        int childSerializedSize)
    {
        if (childLogicalOffset < 0 || childSerializedSize < 8 ||
            childLogicalOffset + childSerializedSize > parentBody.Length)
        {
            throw new ArgumentOutOfRangeException(nameof(childLogicalOffset));
        }
        byte[] parentNameBytes = Encoding.ASCII.GetBytes(parentName + "\0");
        byte[] childNameBytes = Encoding.ASCII.GetBytes(childName + "\0");
        int tableLength = 18 + parentNameBytes.Length + 18 + childNameBytes.Length;
        int dataStart = SmoHeader.Size + tableLength + sizeof(uint);
        byte[] data = new byte[dataStart + parentBody.Length];
        Encoding.ASCII.GetBytes("FFPS").CopyTo(data,0);
        WriteUInt32(data,0x04,0x26);
        WriteUInt32(data,0x0C,checked((uint)data.Length));
        WriteUInt32(data,0x10,2);
        WriteUInt32(data,0x14,checked((uint)dataStart));
        WriteUInt32(data,0x18,checked((uint)parentBody.Length));
        WriteUInt32(data,0x1C,2);
        int cursor = SmoHeader.ObjectTableOffset;
        WriteDirectoryEntry(data,ref cursor,1,parentNameBytes,parentTypeHash,0,
            checked((uint)parentBody.Length));
        WriteDirectoryEntry(data,ref cursor,2,childNameBytes,childTypeHash,
            checked((uint)childLogicalOffset),checked((uint)childSerializedSize));
        parentBody.CopyTo(data,dataStart);
        return data;
    }

    private static byte[] CreateSyntheticPck(byte[] payload)
    {
        byte[] directory = Encoding.ASCII.GetBytes("data/test\0");
        byte[] name = Encoding.ASCII.GetBytes("synthetic.smo\0");
        byte[] stringTable = [.. directory, .. name];
        const int payloadOffset = 0x800;
        byte[] pck = new byte[payloadOffset + payload.Length];
        WriteUInt32(pck, 0, (uint)stringTable.Length);
        stringTable.CopyTo(pck, sizeof(uint));
        int index = sizeof(uint) + stringTable.Length;
        WriteUInt32(pck, index, 1);
        WriteUInt32(pck, index + 4, 0);
        int entry = index + 8;
        WriteUInt32(pck, entry, (uint)directory.Length);
        WriteUInt32(pck, entry + 4, 1);
        WriteUInt32(pck, entry + 8, payloadOffset);
        WriteUInt32(pck, entry + 12, (uint)payload.Length);
        WriteUInt32(pck, entry + 16, 0);
        payload.CopyTo(pck, payloadOffset);
        return pck;
    }

    private static byte[] CreateSyntheticTextureObject(bool includeSecondMip = false, byte nativeFlag = 1)
    {
        const int width = 8;
        const int height = 8;
        const int pixelBytes = width * height * 4;
        byte[] pixels = new byte[pixelBytes];
        for (int offset = 0; offset < pixels.Length; offset += 4)
        {
            pixels[offset] = 0x33;
            pixels[offset + 1] = 0x22;
            pixels[offset + 2] = 0x11;
            pixels[offset + 3] = 0x44;
        }
        pixels[^4] = 0xB6;
        pixels[^3] = 0xC7;
        pixels[^2] = 0xD8;
        pixels[^1] = 0xA5;

        byte[] firstMip = new byte[26 + pixels.Length];
        firstMip[0] = nativeFlag;
        WriteUInt32(firstMip, 1, width);
        WriteUInt32(firstMip, 5, height);
        WriteUInt32(firstMip, 9, 0);
        firstMip[13] = 1;
        WriteUInt32(firstMip, 14, width);
        WriteUInt32(firstMip, 18, width * 4);
        WriteUInt32(firstMip, 22, height);
        pixels.CopyTo(firstMip, 26);

        var specific = new List<byte>();
        specific.AddRange(SmoDataBlockWriter.BuildField(0, firstMip));
        if (includeSecondMip)
        {
            byte[] secondMip = new byte[12 + 4 * 4 * 4];
            WriteUInt32(secondMip, 0, 4);
            WriteUInt32(secondMip, 4, 16);
            WriteUInt32(secondMip, 8, 4);
            Array.Fill(secondMip, (byte)0x7F, 12, secondMip.Length - 12);
            specific.AddRange(SmoDataBlockWriter.BuildField(1, secondMip));
        }
        specific.Add(0);

        var embedded = new List<byte>();
        embedded.AddRange(SmoDataBlockWriter.BuildField(2, new byte[] { 0 }));
        embedded.Add(0);
        byte[] platformType = new byte[sizeof(uint)];
        WriteUInt32(platformType, 0, 6);
        embedded.AddRange(SmoDataBlockWriter.BuildField(6, platformType));
        embedded.AddRange(SmoDataBlockWriter.BuildField(1, specific.ToArray()));
        embedded.Add(0);

        var body = new List<byte>();
        body.AddRange(BitConverter.GetBytes(SmoClassIds.TextureData));
        body.AddRange("SBOO"u8.ToArray());
        body.AddRange(SmoDataBlockWriter.BuildField(3, embedded.ToArray()));
        body.Add(0);
        return body.ToArray();
    }

    private static byte[] CreateSyntheticCrossTextureObject()
    {
        const int width = 8;
        const int height = 8;
        byte[] raw = new byte[16 + width * height * 4];
        WriteUInt32(raw, 0, width);
        WriteUInt32(raw, 4, height);
        WriteUInt32(raw, 8, 0);
        WriteUInt32(raw, 12, 4);
        for (int offset = 16; offset < raw.Length; offset += 4)
        {
            raw[offset] = 0x33;
            raw[offset + 1] = 0x22;
            raw[offset + 2] = 0x11;
            raw[offset + 3] = 0x44;
        }
        var cross = new List<byte>();
        cross.AddRange(SmoDataBlockWriter.BuildField(5, raw));
        cross.Add(0);
        var body = new List<byte>();
        body.AddRange(BitConverter.GetBytes(SmoClassIds.TextureData));
        body.AddRange("SBOO"u8.ToArray());
        body.AddRange(SmoDataBlockWriter.BuildField(0, cross.ToArray()));
        body.Add(0);
        return body.ToArray();
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

        uint[] replacedStates = effectStates.ToArray(); replacedStates[8] = 3;
        byte[] statesBytes = replacedStates.SelectMany(BitConverter.GetBytes).ToArray();
        byte[] colors = new uint[] { 0xFF010203, 0xFF000000, 0xFFFFFFFF, 0, 0x40800000 }
            .SelectMany(BitConverter.GetBytes).ToArray();
        byte[] originalBody = synthetic.Data.Span.Slice(
            checked((int)synthetic.Objects[0].PhysicalOffset), checked((int)synthetic.Objects[0].SerializedSize - 1)).ToArray();
        byte[] repeatedBody = originalBody.Concat(SmoDataBlockWriter.BuildField(0, statesBytes))
            .Concat(SmoDataBlockWriter.BuildField(2, colors)).Append((byte)0).ToArray();
        var repeated = SmoDocument.Parse(CreateSingleObjectDocument("repeated", SmoClassIds.MaterialData, repeatedBody));
        True(SmoMaterialRenderState.TryDecode(repeated, repeated.Objects[0], out var lastState) &&
             lastState!.MaterialRenderStates.SequenceEqual(replacedStates),
            "shared material reader applies repeated render states after the first pass");
        True(SmoMaterialColorResolver.TryDecodeDiffuse(repeated, repeated.Objects[0], out uint black) && black == 0xFF000000,
            "authored black diffuse is a valid material color");
        True(SmoMaterialRenderState.TryDecode(synthetic, synthetic.Objects[0], out var originalState) &&
             originalState!.MaterialRenderStates.SequenceEqual(effectStates),
            "cached material state belongs to its immutable document, independently of equal object indices");
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
        IReadOnlyDictionary<int, SmoTextureBinding> bindings =
            SmoTextureBindingResolver.ResolveAll(document);
        Equal(4, bindings.Values.Count(binding => binding.Issue?.StartsWith(
                "AMBIGUOUS_SKIN_MATERIAL_INHERITANCE:",
                StringComparison.Ordinal) == true),
            "D90D material-less chunks expose ambiguous native state");
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
        Equal(1, diffuse.RetainedOpaqueWhiteSurfaceCount,
            "D90D only explicitly bound white skinned surface count");
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

        byte[] body = new byte[8 + 2 + 11 * sizeof(uint) + 1 + sizeof(uint) + 1];
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

    private static void TestSyntheticMaterialData()
    {
        uint[] renderStates = [0,0,1,2,1,1,3,0,4,0,6];
        byte[] renderPayload = new byte[renderStates.Length * sizeof(uint)];
        for (int index = 0; index < renderStates.Length; index++)
            WriteUInt32(renderPayload,index * sizeof(uint),renderStates[index]);
        uint[] textureStates = [0,3,3,0,0,0xFF000000,2,0,0];
        byte[] texturePayload = new byte[textureStates.Length * sizeof(uint)];
        for (int index = 0; index < textureStates.Length; index++)
            WriteUInt32(texturePayload,index * sizeof(uint),textureStates[index]);
        byte[] staticUv = new byte[40];
        WriteUInt32(staticUv,0,1);
        float[] identity = [1,0,0,0,1,0,0,0,1];
        for (int index = 0; index < identity.Length; index++)
            BitConverter.GetBytes(identity[index]).CopyTo(staticUv,4 + index * 4);
        byte[] color = new byte[20];
        WriteUInt32(color,0,0xFF000000);
        WriteUInt32(color,4,0xFFFFFFFF);
        WriteUInt32(color,8,0xFFFFFFFF);
        WriteUInt32(color,12,0xFF000000);
        BitConverter.GetBytes(16f).CopyTo(color,16);

        var body = new List<byte>();
        body.AddRange(BitConverter.GetBytes(SmoClassIds.MaterialData));
        body.AddRange(Encoding.ASCII.GetBytes("SBOO"));
        body.AddRange(SmoDataBlockWriter.BuildField(1,[1]));
        body.AddRange(SmoDataBlockWriter.BuildField(0,renderPayload));
        body.AddRange(SmoDataBlockWriter.BuildField(3,BitConverter.GetBytes(6u)));
        body.AddRange(SmoDataBlockWriter.BuildField(
            4,BitConverter.GetBytes(SmoClassIds.StandardLayer)));
        body.AddRange(SmoDataBlockWriter.BuildField(17,texturePayload));
        body.AddRange(SmoDataBlockWriter.BuildField(9,staticUv));
        body.AddRange(SmoDataBlockWriter.BuildField(2,color));
        body.AddRange(SmoDataBlockWriter.BuildField(6,new byte[4]));
        body.AddRange(SmoDataBlockWriter.BuildField(0,ReadOnlySpan<byte>.Empty));

        SmoDocument document = SmoDocument.Parse(CreateSingleObjectDocument(
            "material",SmoClassIds.MaterialData,body.ToArray()));
        SmoObjectEntry entry = document.Objects.Single();
        True(SmoMaterialDataDecoder.TryDecode(
                document,entry,out SmoMaterialDataInfo? material,out string error) &&
             material is not null,
            "complete synthetic material decodes: " + error);
        Equal(1,material!.Passes.Count,"synthetic material pass count");
        Equal(6u,material.Passes[0].FinalBlendOperation,
            "synthetic material FinalBlendOp");
        Equal(SmoClassIds.StandardLayer,material.Passes[0].LayerClassId,
            "synthetic material layer class");
        True(material.Passes[0].TextureStates.SequenceEqual(textureStates),
            "synthetic material texture states");
        True(material.UsesVertexAlpha &&
             material.Passes[0].StaticUvTransform is { Enabled: true },
            "synthetic material optional flags");
        Equal(0xFFFFFFFFu,material.Color.DiffuseArgb,
            "synthetic material diffuse color");
        Equal(16f,material.Color.SpecularPower,
            "synthetic material specular power");
        Equal(SmoMaterialRelationshipStorageKind.NullId,
            material.ColorController.StorageKind,
            "synthetic material null color controller");

        IReadOnlyList<SmoSerializedFieldValue> inspected =
            SmoSerializedFieldInspector.Inspect(document,entry);
        Equal(8,inspected.Count,"synthetic material inspected field count");
        True(inspected.All(item => item.IsDecoded),
            "synthetic material fields all render as decoded values");
        True(inspected.Any(item => item.Descriptor.Key == "material.pass" &&
                                  item.DisplayValue.Contains("FinalBlendOp=6",
                                      StringComparison.Ordinal)),
            "synthetic material pass display");
        var repeatedBody = body.Take(body.Count - 1).ToList();
        repeatedBody.AddRange(SmoDataBlockWriter.BuildField(18, "ignored"u8));
        repeatedBody.AddRange(SmoDataBlockWriter.BuildField(1, [0xA5]));
        WriteUInt32(staticUv, 0, 0);
        BitConverter.GetBytes(99f).CopyTo(staticUv, 4);
        repeatedBody.AddRange(SmoDataBlockWriter.BuildField(9, staticUv));
        repeatedBody.AddRange(SmoDataBlockWriter.BuildField(8, texturePayload));
        WriteUInt32(color, 4, 0xABCDEF01);
        repeatedBody.AddRange(SmoDataBlockWriter.BuildField(2, color));
        repeatedBody.Add(0);
        var repeatedDocument = SmoDocument.Parse(CreateSingleObjectDocument(
            "material-repeated", SmoClassIds.MaterialData, repeatedBody.ToArray()));
        True(SmoMaterialDataDecoder.TryDecode(repeatedDocument, repeatedDocument.Objects[0], out var repeated, out error) &&
             repeated!.UsesVertexAlpha && repeated.UsesLegacyTextureStates && repeated.Color.DiffuseArgb == 0xABCDEF01,
            "shared material reader accepts nonboolean alpha, repeated/reordered fields, unknown fields and raw ARGB: " + error);
        True(repeated!.Passes[0].StaticUvTransform is { Enabled: true } uv && uv.Matrix3x3.SequenceEqual(identity),
            "original zero static-UV flag preserves the earlier matrix and enabled state");
    }

    private static void TestNativeMeshBuffers()
    {
        // PC429A40 ignores all five planning values, then uses ordinary IB/VB.
        // Original-directed evidence: native-pc-dx-materialization.md.
        byte[] portable = new byte[30 + 3 * 24];
        WriteUInt32(portable,0,2); WriteUInt32(portable,4,1);
        BinaryPrimitives.WriteUInt16LittleEndian(portable.AsSpan(14),1);
        BinaryPrimitives.WriteUInt16LittleEndian(portable.AsSpan(16),2);
        WriteUInt32(portable,18,0x40); WriteUInt32(portable,22,3);
        for (int i = 0; i < 3; ++i)
        {
            WriteVector3(portable.AsSpan(30 + i * 24),new Vector3(i,i + 1,0));
            WriteVector3(portable.AsSpan(42 + i * 24),new Vector3(0,0,2));
        }
        byte[] native = Enumerable.Repeat((byte)0xA5,17).Concat(portable).ToArray();
        byte[] ObjectBody(int field,byte[] payload) => BitConverter.GetBytes(SmoClassIds.MeshData)
            .Concat("SBOO"u8.ToArray()).Concat(SmoDataBlockWriter.BuildField(field,payload)).Append((byte)0).ToArray();
        SmoDocument Document(int field,byte[] payload,uint platform)
        {
            byte[] bytes = CreateSingleObjectDocument("native-mesh",SmoClassIds.MeshData,ObjectBody(field,payload));
            WriteUInt32(bytes,16,platform);
            return SmoDocument.Parse(bytes);
        }
        SmoDocument pc = Document(1,native,2);
        True(SmoMeshDecoder.TryDecode(pc,pc.Objects[0],out var mesh,out var error),
            "native mesh ignores contradictory planning header: " + error);
        True(mesh!.VertexCount == 3 && mesh.PrimitiveCount == 1 && mesh.Stride == 24 &&
             mesh.RuntimeStride == 24 && mesh.RuntimeVertexBufferSize == 72,
            "actual IB/VB objects determine mesh metadata");
        True(mesh.Normals.All(normal => normal == new Vector3(0,0,2)),
            "raw mesh normals retain their original magnitude");
        var field = SmoObjectFieldReader.Read(pc,pc.Objects[0])[0];
        Equal((long)field.AbsolutePayloadOffset + 47,mesh.VertexDataOffset,
            "native reader observes vertex payload offset");
        True(SmoMeshDataDecoder.TryDecode(pc,pc.Objects[0],out var metadata,out _) &&
             metadata!.PlatformSpecific!.Pc!.VertexFormat == 0x40,
            "inspector uses shared mesh metadata despite contradictory planning words");
        var cross = Document(0,portable,1);
        True(SmoMeshDecoder.TryDecode(cross,cross.Objects[0],out var portableMesh,out _) &&
             portableMesh.Positions.SequenceEqual(mesh.Positions),
            "portable and PC branches share the same buffer readers");
        var wrongPlatform = Document(0,portable,2);
        True(!SmoMeshDecoder.TryDecode(wrongPlatform,wrongPlatform.Objects[0],out _,out _),
            "PC reader does not fall back to unselected field0");
        byte[] strip = new byte[32 + 4 * 12];
        WriteUInt32(strip,0,3); WriteUInt32(strip,4,2);
        ushort[] indices = [2,0,1,3];
        for (int i = 0; i < indices.Length; ++i)
            BinaryPrimitives.WriteUInt16LittleEndian(strip.AsSpan(12 + i * 2),indices[i]);
        WriteUInt32(strip,24,4);
        for (int i = 0; i < 4; ++i) WriteVector3(strip.AsSpan(32 + i * 12),new Vector3(i & 1,i >> 1,0));
        var stripDocument = Document(0,strip,1);
        True(SmoMeshDecoder.TryDecode(stripDocument,stripDocument.Objects[0],out var stripMesh,out _)
             && stripMesh.StripIndices.SequenceEqual(indices) && stripMesh.PrimitiveCount == 2,
            "original strip primitive count includes the two final indices without guessing");
        True(stripMesh!.TriangleIndices.SequenceEqual(new uint[]{2,0,1,1,0,3}),
            "modern triangle-list conversion preserves source strip parity");
        var truncated = Document(1,native[..^1],2);
        True(!SmoMeshDecoder.TryDecode(truncated,truncated.Objects[0],out _,out _),
            "native mesh reader rejects truncated VB");
        True(SmoMeshDecoder.TryDecode(pc,pc.Objects[0],out _,out _),
            "mesh owners and renderer cache survive a failed independent read");
    }

    private static void TestSyntheticMeshData()
    {
        byte[] native = new byte[SmoMeshDataDecoder.Ps2HeaderSize +
                                 SmoMeshDataDecoder.Ps2DmaQwordSize];
        BitConverter.GetBytes(1f).CopyTo(native,0);
        BitConverter.GetBytes(2f).CopyTo(native,4);
        BitConverter.GetBytes(3f).CopyTo(native,8);
        BitConverter.GetBytes(4f).CopyTo(native,12);
        WriteUInt32(native,16,2);
        WriteUInt32(native,20,4);
        WriteUInt32(native,24,0x197E);
        WriteUInt32(native,28,1);
        WriteUInt32(native,32,1);
        WriteUInt32(native,36,4);
        for (int index = SmoMeshDataDecoder.Ps2HeaderSize;
             index < native.Length;index++)
        {
            native[index] = checked((byte)index);
        }
        byte[] bounds = new byte[SmoMeshDataDecoder.BoundingBoxSize];
        float[] boundValues = [-1,-2,-3,3,4,5];
        for (int index = 0;index < boundValues.Length;index++)
            BitConverter.GetBytes(boundValues[index]).CopyTo(bounds,index * 4);

        var body = new List<byte>();
        body.AddRange(BitConverter.GetBytes(SmoClassIds.MeshData));
        body.AddRange(Encoding.ASCII.GetBytes("SBOO"));
        body.Add(0xE1);
        body.AddRange(BitConverter.GetBytes((uint)native.Length));
        body.AddRange(native);
        body.Add(0xE2);
        body.AddRange(BitConverter.GetBytes((uint)bounds.Length));
        body.AddRange(bounds);
        body.Add(0);

        byte[] ps2Document = CreateSingleObjectDocument("ps2_mesh",SmoClassIds.MeshData,body.ToArray());
        WriteUInt32(ps2Document,16,8); // actual PS2 platform mask, not the helper's PC default
        SmoDocument document = SmoDocument.Parse(ps2Document);
        SmoObjectEntry entry = document.Objects.Single();
        True(SmoMeshDataDecoder.TryDecode(
                document,entry,out SmoMeshDataInfo? meshData,out string error) &&
             meshData?.PlatformSpecific?.Ps2Native is not null,
            "synthetic PS2 mesh container decodes: " + error);
        SmoPs2NativeMeshData ps2 = meshData!.PlatformSpecific!.Ps2Native!;
        Equal(0x197Eu,ps2.VertexFormat,"synthetic PS2 vertex format");
        Equal(1u,ps2.DmaQwordCount,"synthetic PS2 DMA qword count");
        Equal(1u,ps2.AdditionalTextureCoordinateCount,
            "synthetic PS2 additional UV channel count");
        Equal(4u,ps2.BlendWeightCount,"synthetic PS2 blend weight count");
        Equal(new Vector3(-1,-2,-3),meshData.BoundingBox!.Minimum,
            "synthetic PS2 mesh minimum bounds");
        Equal(new Vector3(3,4,5),meshData.BoundingBox.Maximum,
            "synthetic PS2 mesh maximum bounds");
        True(!SmoMeshDecoder.TryDecode(document,entry,out _,out string geometryError) &&
             geometryError.Contains("no selected geometry field",StringComparison.Ordinal),
            "native PS2 DMA remains structural-only geometry");

        IReadOnlyList<SmoSerializedFieldValue> inspected =
            SmoSerializedFieldInspector.Inspect(document,entry);
        Equal(2,inspected.Count,"synthetic mesh inspected field count");
        True(inspected.All(item => item.IsDecoded),
            "synthetic mesh fields render as decoded metadata");
        True(inspected[0].DisplayValue.Contains("DMA=1 qwords/16 bytes",
                StringComparison.Ordinal),
            "synthetic PS2 DMA metadata is visible");

        // Original metadata prefix preserves these values without correlating
        // channel counters to the format mask. This is inspection, not DMA validation.
        byte[] unusual = new byte[SmoMeshDataDecoder.Ps2HeaderSize];
        WriteSingle(unusual.AsSpan(12),-4);
        WriteUInt32(unusual,20,999); WriteUInt32(unusual,32,7); WriteUInt32(unusual,36,2);
        byte[] unusualBody = BitConverter.GetBytes(SmoClassIds.MeshData).Concat("SBOO"u8.ToArray())
            .Concat(SmoDataBlockWriter.BuildField(1,unusual)).Append((byte)0).ToArray();
        byte[] unusualFile = CreateSingleObjectDocument("ps2-raw-counters",SmoClassIds.MeshData,unusualBody);
        WriteUInt32(unusualFile,16,8);
        var unusualDocument = SmoDocument.Parse(unusualFile);
        True(SmoMeshDataDecoder.TryDecode(unusualDocument,unusualDocument.Objects[0],out var unusualInfo,out _) &&
             unusualInfo.PlatformSpecific!.Ps2Native is { BoundingSphere.W: -4, AdditionalTextureCoordinateCount: 7, BlendWeightCount: 2 },
            "original PS2 prefix values remain inspectable without invented channel restrictions");
        byte[] reversed = new byte[24];
        WriteVector3(reversed,new Vector3(3,4,5)); WriteVector3(reversed.AsSpan(12),new Vector3(-1,-2,-3));
        byte[] boundsBody = BitConverter.GetBytes(SmoClassIds.MeshData).Concat("SBOO"u8.ToArray())
            .Concat(SmoDataBlockWriter.BuildField(2,reversed)).Append((byte)0).ToArray();
        var boundsOnly = SmoDocument.Parse(CreateSingleObjectDocument("bounds-only",SmoClassIds.MeshData,boundsBody));
        True(SmoMeshDataDecoder.TryDecode(boundsOnly,boundsOnly.Objects[0],out var boundsInfo,out _) &&
             boundsInfo.BoundingBox == new SmoMeshBoundingBoxData(new Vector3(3,4,5),new Vector3(-1,-2,-3)),
            "original bounds-only field reader preserves reversed endpoints");
        WriteUInt32(unusual,28,uint.MaxValue);
        byte[] oversizedBody = BitConverter.GetBytes(SmoClassIds.MeshData).Concat("SBOO"u8.ToArray())
            .Concat(SmoDataBlockWriter.BuildField(1,unusual)).Append((byte)0).ToArray();
        byte[] oversizedFile = CreateSingleObjectDocument("oversized-packet",SmoClassIds.MeshData,oversizedBody);
        WriteUInt32(oversizedFile,16,8);
        var oversized = SmoDocument.Parse(oversizedFile);
        True(!SmoMeshDataDecoder.TryDecode(oversized,oversized.Objects[0],out _,out _),
            "host rejects overflowing PS2 packet extent without allocating it");
    }

    private static void TestSyntheticModel()
    {
        static byte[] Relationship(uint objectId)
        {
            byte[] payload = new byte[2 * sizeof(uint)];
            WriteUInt32(payload,0,objectId);
            return payload;
        }

        static byte[] TargetBody(uint typeHash)
        {
            byte[] body = new byte[9];
            WriteUInt32(body,0,typeHash);
            Encoding.ASCII.GetBytes("SBOO").CopyTo(body,4);
            return body;
        }

        static byte[] ModelBody(bool compact)
        {
            var body = new List<byte>();
            body.AddRange(BitConverter.GetBytes(SmoClassIds.Model));
            body.AddRange("SBOO"u8.ToArray());
            body.AddRange(SmoDataBlockWriter.BuildField(0,Relationship(2)));
            body.AddRange(SmoDataBlockWriter.BuildField(1,Relationship(3)));
            if (!compact)
            {
                body.AddRange(SmoDataBlockWriter.BuildField(
                    2,BitConverter.GetBytes(1u)));
                body.AddRange(SmoDataBlockWriter.BuildField(
                    3,BitConverter.GetBytes(7u)));
            }
            body.Add(0);
            body.AddRange(SmoDataBlockWriter.BuildField(0,Relationship(4)));
            if (!compact)
            {
                body.AddRange(SmoDataBlockWriter.BuildField(
                    1,BitConverter.GetBytes(3u)));
            }
            body.Add(0);
            return body.ToArray();
        }

        foreach (bool compact in new[] { false,true })
        {
            byte[] bytes = CreateMultiObjectDocument(
                (1,"model",SmoClassIds.Model,ModelBody(compact)),
                (2,"material",SmoClassIds.MaterialData,
                    TargetBody(SmoClassIds.MaterialData)),
                (3,"fog",SmoClassIds.Fog,TargetBody(SmoClassIds.Fog)),
                (4,"mesh",SmoClassIds.MeshData,TargetBody(SmoClassIds.MeshData)));
            SmoDocument document = SmoDocument.Parse(bytes);
            SmoObjectEntry entry = document.Objects[0];
            True(SmoModelDecoder.TryDecode(
                    document,entry,out SmoModelData? model,out string error) &&
                 model is not null,
                $"synthetic {(compact ? "compact" : "full")} model decodes: " +
                error);
            Equal(1,model!.Renderable.Material!.TargetObjectIndex!.Value,
                "model material relationship target");
            Equal(2,model.Renderable.Fog!.TargetObjectIndex!.Value,
                "model fog relationship target");
            Equal(3,model.BaseMesh.TargetObjectIndex!.Value,
                "model base-mesh relationship target");
            Equal(SmoNodeRelationshipEncoding.SizedReference,
                model.BaseMesh.Encoding,"model relationship encoding");
            True(model.Renderable.AlphaSortEnable == (compact ? null : 1u),
                "model alpha-sort value or omitted legacy default");
            True(model.Renderable.Priority == (compact ? null : 7u),
                "model priority value or omitted legacy default");
            True(model.ProjectionGroup == (compact ? null : 3u),
                "model projection group value or omitted default");

            IReadOnlyList<SmoSerializedFieldValue> inspected =
                SmoSerializedFieldInspector.Inspect(document,entry);
            Equal(compact ? 3 : 6,inspected.Count,
                "model inspector exposes every serialized semantic field");
            True(inspected.All(item => item.IsDecoded),
                "model inspector decodes all observed model fields");
            True(inspected.Any(item =>
                    item.Descriptor.Key == "renderable.material") &&
                 inspected.Any(item => item.Descriptor.Key == "model.base_mesh"),
                "model inspector combines inherited and own serializer sections");
        }
    }

    private static void TestNativeModelSkinReaders()
    {
        static byte[] Prefix(uint type) => BitConverter.GetBytes(type).Concat("SBOO"u8.ToArray()).ToArray();
        static byte[] Reference(uint id) => BitConverter.GetBytes(id).Concat(new byte[4]).ToArray();
        var model = Prefix(SmoClassIds.Model).ToList();
        model.AddRange(SmoDataBlockWriter.BuildField(3, BitConverter.GetBytes(7u)));
        model.AddRange(SmoDataBlockWriter.BuildField(2, BitConverter.GetBytes(256u)));
        model.AddRange(SmoDataBlockWriter.BuildField(18, "ignored"u8));
        model.AddRange(SmoDataBlockWriter.BuildField(0, new byte[4]));
        model.Add(0);
        model.AddRange(SmoDataBlockWriter.BuildField(1, BitConverter.GetBytes(9u)));
        model.AddRange(SmoDataBlockWriter.BuildField(0, Reference(4)));
        model.AddRange(SmoDataBlockWriter.BuildField(1, BitConverter.GetBytes(5u)));
        model.Add(0);
        var modelDocument = SmoDocument.Parse(CreateMultiObjectDocument(
            (1, "repeated-model", SmoClassIds.Model, model.ToArray()),
            (4, "mesh", SmoClassIds.MeshData, Prefix(SmoClassIds.MeshData).Append((byte)0).ToArray())));
        True(SmoModelDecoder.TryDecode(modelDocument, modelDocument.Objects[0], out var decoded, out var error) &&
             decoded.Renderable.Material is null && decoded.Renderable.AlphaSortEnable == 1 &&
             decoded.Renderable.Priority == 7 && decoded.ProjectionGroup == 5,
            "original Model field order, unknown skip, full-DWORD alpha normalization and last group: " + error);

        var skin = Prefix(SmoClassIds.Skin).ToList();
        skin.Add(0);
        skin.AddRange(SmoDataBlockWriter.BuildField(0, Reference(4)));
        skin.Add(0);
        var palette = BitConverter.GetBytes(uint.MaxValue).Concat(BitConverter.GetBytes(2u)).ToList();
        uint[] bits = [0x80000000, 0x7FC12345, 0x7F800000, 0xFF800000, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12];
        for (int i = 0; i < 2; ++i)
        {
            palette.AddRange(Reference(7));
            foreach (uint value in bits) palette.AddRange(BitConverter.GetBytes(value));
        }
        skin.AddRange(SmoDataBlockWriter.BuildField(0, palette.ToArray()));
        skin.Add(0);
        SmoDocument SkinDocument(byte[] body) => SmoDocument.Parse(CreateMultiObjectDocument(
            (1, "raw-skin", SmoClassIds.Skin, body),
            (4, "mesh", SmoClassIds.MeshData, Prefix(SmoClassIds.MeshData).Append((byte)0).ToArray()),
            (7, "bone", SmoClassIds.Node, Prefix(SmoClassIds.Node).Append((byte)0).ToArray())));
        var skinDocument = SkinDocument(skin.ToArray());
        True(SmoSkinDecoder.TryDecode(skinDocument, skinDocument.Objects[0], out var decodedSkin, out error) &&
             decodedSkin.Bones.Count == 2 && decodedSkin.BlendInfluenceCountHint == uint.MaxValue &&
             decodedSkin.Renderable.Fog is null && decodedSkin.AlphaSortEnable is null,
            "Skin reader accepts omitted inherited fields and repeated bones with original weight count: " + error);
        var matrix = decodedSkin!.Bones[0].InverseBindMatrix;
        True(BitConverter.SingleToUInt32Bits(matrix.M11) == bits[0] &&
             BitConverter.SingleToUInt32Bits(matrix.M12) == bits[1] &&
             BitConverter.SingleToUInt32Bits(matrix.M13) == bits[2] &&
             BitConverter.SingleToUInt32Bits(matrix.M14) == bits[3],
            "Skin matrix crosses the source/ABI/managed boundary with signed zero, NaN and infinity bits intact");
        skin.RemoveAt(skin.Count - 1);
        skin.AddRange(SmoDataBlockWriter.BuildField(0, new byte[8]));
        skin.Add(0);
        var cleared = SkinDocument(skin.ToArray());
        True(SmoSkinDecoder.TryDecode(cleared, cleared.Objects[0], out var clearedSkin, out error) &&
             clearedSkin.Bones.Count == 0 && clearedSkin.BlendInfluenceCountHint == 0,
            "last original Skin field clears the palette: " + error);

        byte[] LinkedModel(uint material, uint meshId, uint? previousMesh = null)
        {
            var body = Prefix(SmoClassIds.Model).ToList();
            body.AddRange(SmoDataBlockWriter.BuildField(0, material == 0 ? new byte[4] : Reference(material)));
            body.Add(0);
            if (previousMesh.HasValue) body.AddRange(SmoDataBlockWriter.BuildField(0, Reference(previousMesh.Value)));
            body.AddRange(SmoDataBlockWriter.BuildField(0, Reference(meshId))); body.Add(0);
            return body.ToArray();
        }
        byte[] EmptyResource(uint type) => Prefix(type).Append((byte)0).ToArray();
        var shared = SmoDocument.Parse(CreateMultiObjectDocument(
            (1, "first", SmoClassIds.Model, LinkedModel(2, 4)),
            (5, "second", SmoClassIds.Model, LinkedModel(6, 4)),
            (9, "redirected", SmoClassIds.Model, LinkedModel(0, 8, 4)),
            (2, "material", SmoClassIds.MaterialData, EmptyResource(SmoClassIds.MaterialData)),
            (6, "material", SmoClassIds.MaterialData, EmptyResource(SmoClassIds.MaterialData)),
            (4, "mesh", SmoClassIds.MeshData, EmptyResource(SmoClassIds.MeshData)),
            (8, "mesh", SmoClassIds.MeshData, EmptyResource(SmoClassIds.MeshData))));
        var catalog = SmoRenderableCatalog.Get(shared);
        True(catalog.Issues.Count == 0 && catalog.GetMeshConsumers(5).Count == 2 &&
             catalog.GetMeshConsumers(5).Select(item => item.Renderable.Material!.ObjectId).SequenceEqual(new uint[] { 2, 6 }),
            "shared mesh occurrences retain their own explicit material IDs despite equal resource names");
        True(catalog.GetMeshConsumers(6).Count == 1 && catalog.GetMeshConsumers(6)[0].ObjectIndex == 2 &&
             catalog.ByObjectIndex[2].Renderable.Material is null,
            "catalog uses the final mesh assignment and preserves NULL material without guessing a fallback");
    }

    private static void TestNativeAnimatedTextureReader()
    {
        byte[] Prefix(uint type) => BitConverter.GetBytes(type).Concat("SBOO"u8.ToArray()).ToArray();
        var frames = BitConverter.GetBytes(3u).Concat(BitConverter.GetBytes(1f))
            .Concat(BitConverter.GetBytes(1f)).Concat(BitConverter.GetBytes(3f)).ToList();
        frames.AddRange(BitConverter.GetBytes(7u)); frames.AddRange(new byte[4]);
        frames.AddRange(new byte[4]);
        frames.AddRange(BitConverter.GetBytes(7u)); frames.AddRange(new byte[4]);
        byte[] Body(byte[] payload) => Prefix(SmoClassIds.AnimTextureController)
            .Concat(SmoDataBlockWriter.BuildField(0, payload)).Append((byte)0).ToArray();
        SmoDocument Document(byte[] body) => SmoDocument.Parse(CreateMultiObjectDocument(
            (1, "timeline", SmoClassIds.AnimTextureController, body),
            (7, "frame", SmoClassIds.TextureData, Prefix(SmoClassIds.TextureData).Append((byte)0).ToArray())));
        var document = Document(Body(frames.ToArray()));
        True(SmoAnimTextureControllerDecoder.TryDecode(document, document.Objects[0], out var track, out var error) &&
             track.Frames.Count == 3 && track.Duration == 3 && track.Frames[1].Texture.ObjectId == 0,
            "shared animation reader accepts repeated endpoints and an explicit NULL frame: " + error);
        True(track!.TryGetFrameIndex(0, out int first) && first == 0 &&
             track.TryGetFrameIndex(1, out int repeated) && repeated == 2 &&
             track.TryGetFrameIndex(3, out int last) && last == 2,
            "original end-time selector skips equal earlier endpoints and includes the last endpoint");
        var empty = Document(Body(new byte[4]));
        True(SmoAnimTextureControllerDecoder.TryDecode(empty, empty.Objects[0], out var emptyTrack, out error) &&
             emptyTrack.Frames.Count == 0 && emptyTrack.Duration == 0 && emptyTrack.TryGetFrameIndex(0, out int none) && none == -1,
            "memory-backed inspection and original track selector retain an empty track: " + error);
        byte[] unsorted = frames.ToArray(); BitConverter.GetBytes(2f).CopyTo(unsorted, 4);
        var malformed = Document(Body(unsorted));
        True(SmoAnimTextureControllerDecoder.TryDecode(malformed, malformed.Objects[0], out var retained, out error) &&
             retained.Frames[0].Time == 2 && !retained.TryGetFrameIndex(0, out _),
            "raw unsorted timing remains inspectable while shared runtime safety rejects evaluation: " + error);
    }

    private static void TestSyntheticTextures()
    {
        foreach ((byte[] body, SmoTextureRepresentationKind kind, int mipCount,
                     string name) in new[]
                 {
                     (CreateSyntheticTextureObject(includeSecondMip: true),
                         SmoTextureRepresentationKind.Direct3DBgra32, 2,
                         "Direct3D BGRA mip chain"),
                     (CreateSyntheticTextureObject(nativeFlag: 2),
                         SmoTextureRepresentationKind.Direct3DBgra32, 1,
                         "nonzero native presence flag"),
                     (CreateSyntheticCrossTextureObject(),
                         SmoTextureRepresentationKind.CrossPlatformBgra32, 1,
                         "legacy cross-platform BGRA")
                 })
        {
            byte[] data = CreateSingleObjectDocument(
                name,
                SmoClassIds.TextureData,
                body);
            if (kind == SmoTextureRepresentationKind.CrossPlatformBgra32)
                WriteUInt32(data, 16, 1); // Explicit legacy-common metadata, as in original book.smo.
            SmoDocument document = SmoDocument.Parse(data, name + ".smo");

            True(
                SmoTextureDecoder.TryDecode(
                    document,
                    document.Objects.Single(),
                    out SmoTexture? texture,
                    out string error),
                $"synthetic {name} texture decodes: {error}");
            Equal(8, texture!.Width, $"synthetic {name} width");
            Equal(8, texture.Height, $"synthetic {name} height");
            Equal(kind, texture.RepresentationKind,
                $"synthetic {name} representation");
            Equal(mipCount, texture.MipLevelCount,
                $"synthetic {name} mip count");
            Equal(SmoTextureLayout.Bgra, texture.SourceLayout,
                $"synthetic {name} source layout");
            Equal(32, texture.PixelStride, $"synthetic {name} pixel stride");
            Equal(8 * 8 * 4, texture.Bgra32Pixels.Length, $"synthetic {name} size");

            ReadOnlySpan<byte> pixels = texture.Bgra32Pixels.Span;
            Equal((byte)0x33, pixels[0], $"synthetic {name} first blue");
            Equal((byte)0x22, pixels[1], $"synthetic {name} first green");
            Equal((byte)0x11, pixels[2], $"synthetic {name} first red");
            Equal((byte)0x44, pixels[3], $"synthetic {name} first alpha");

            True(SmoTextureDataDecoder.TryDecode(
                    document,document.Objects.Single(),
                    out SmoTextureDataInfo? textureData,out string dataError) &&
                 textureData is not null,
                $"synthetic {name} structure decodes: {dataError}");
            IReadOnlyList<SmoSerializedFieldValue> textureFields =
                SmoSerializedFieldInspector.Inspect(
                    document,document.Objects.Single());
            Equal(1,textureFields.Count,
                $"synthetic {name} exposes one direct source field");
            True(textureFields[0].IsDecoded &&
                 textureFields[0].DisplayValue.Contains("mips=",
                     StringComparison.Ordinal),
                $"synthetic {name} source metadata is visible in inspector");
        }

        byte[] invalidSectionBody = CreateSyntheticTextureObject();
        invalidSectionBody[^2] = 0x02;
        var alternateTerminatorDocument = SmoDocument.Parse(CreateSingleObjectDocument(
            "alternate_terminator", SmoClassIds.TextureData, invalidSectionBody));
        True(SmoTextureDecoder.TryDecode(alternateTerminatorDocument, alternateTerminatorDocument.Objects.Single(), out _, out _),
            "actual reader accepts zero-size form regardless of low field bits");
        invalidSectionBody[^2] = 0x22; // A nonempty field with no byte left in its declared section.
        SmoDocument invalidSectionDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "invalid_section",
                SmoClassIds.TextureData,
                invalidSectionBody));
        True(
            !SmoTextureDecoder.TryDecode(
                invalidSectionDocument,
                invalidSectionDocument.Objects.Single(),
                out _,
                out string invalidSectionError),
            "missing embedded section terminator is rejected");
        True(
            invalidSectionError.StartsWith("TEXTURE_PC_SOURCE_INVALID:"),
            "embedded section failure has a stable diagnostic code");

        byte[] invalidStrideBody = CreateSyntheticTextureObject();
        byte[] descriptor = [8, 0, 0, 0, 32, 0, 0, 0, 8, 0, 0, 0];
        int descriptorOffset = invalidStrideBody.AsSpan().IndexOf(descriptor);
        True(descriptorOffset >= 0, "synthetic Direct3D descriptor is located");
        WriteUInt32(invalidStrideBody, descriptorOffset + 4, 4);
        SmoDocument invalidStrideDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "invalid_stride",
                SmoClassIds.TextureData,
                invalidStrideBody));
        True(
            !SmoTextureDecoder.TryDecode(
                invalidStrideDocument,
                invalidStrideDocument.Objects.Single(),
                out _,
                out string invalidStrideError),
            "inconsistent Direct3D row stride is rejected");
        True(
            invalidStrideError.StartsWith("TEXTURE_PC_SOURCE_INVALID:"),
            "Direct3D layout failure has a stable diagnostic code");

        byte[] unsupportedSourceBody = CreateSyntheticTextureObject();
        unsupportedSourceBody[8] = checked((byte)(
            (unsupportedSourceBody[8] & 0xE0) | 2));
        SmoDocument unsupportedSourceDocument = SmoDocument.Parse(
            CreateSingleObjectDocument(
                "unsupported_source",
                SmoClassIds.TextureData,
                unsupportedSourceBody));
        True(
            !SmoTextureDecoder.TryDecode(
                unsupportedSourceDocument,
                unsupportedSourceDocument.Objects.Single(),
                out _,
                out string unsupportedSourceError),
            "unobserved source form is rejected without guessing");
        True(
            unsupportedSourceError.StartsWith("TEXTURE_PC_SOURCE_INVALID:"),
            "unobserved source has a stable diagnostic code");

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
            truncatedError.StartsWith("TEXTURE_PC_SOURCE_INVALID:"),
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
        int decodedSkins = 0;
        int decodedSkinPaletteSlots = 0;
        int resolvedMaterialColors = 0;
        int vertexColoredMeshes = 0;
        int meshesUnderStaticObjects = 0;
        int meshesWithoutStaticObjects = 0;
        int recoveredSerializerFields = 0;
        int decodedSerializerFields = 0;
        int hexOnlySerializerFields = 0;
        var serializerFieldLayouts = new Dictionary<string, int>(
            StringComparer.Ordinal);
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

            foreach (SmoObjectEntry entry in document.Objects)
            {
                True(
                    SmoObjectFieldReader.TryRead(
                        document, entry, out _, out string fieldError),
                    $"direct field stream: {file} [{entry.Index}] {fieldError}");
                IReadOnlyList<SmoSerializedFieldValue> inspectedFields =
                    SmoSerializedFieldInspector.Inspect(document, entry);
                foreach (SmoSerializedFieldValue inspected in inspectedFields)
                {
                    True(inspected.PayloadSize == 0 || inspected.HexPreview.Length > 0,
                        $"known field has payload preview: {file} [{entry.Index}] " +
                        inspected.Descriptor.Key);
                    recoveredSerializerFields++;
                    if (inspected.Descriptor.Key is "uv_controller.transform_evaluators" or "material_color_controller.evaluators")
                        True(inspected.IsDecoded, $"shared material function reader: {file} [{entry.Index}] {inspected.Descriptor.Key}");
                    if (inspected.IsDecoded)
                        decodedSerializerFields++;
                    else
                        hexOnlySerializerFields++;
                    string layoutKey =
                        $"{inspected.Descriptor.Key} · {inspected.PayloadSize} B · " +
                        (inspected.IsDecoded ? "decoded" : "hex-only");
                    serializerFieldLayouts[layoutKey] =
                        serializerFieldLayouts.GetValueOrDefault(layoutKey) + 1;
                }
            }

            foreach (SmoObjectEntry entry in document.Objects.Where(item => item.TypeHash == SmoClassIds.AnimTextureController))
            {
                True(SmoAnimTextureControllerDecoder.TryDecode(document, entry, out var track, out var trackError),
                    $"shared texture controller decodes: {file} [{entry.Index}]: " + trackError);
                if (track!.Frames.Count != 0)
                    True(track.TryGetFrameIndex(track.Duration, out int last) && last == track.Frames.Count - 1,
                        $"original texture-track last endpoint: {file} [{entry.Index}]");
            }

            foreach (SmoObjectEntry entry in document.Objects.Where(
                         item => item.TypeHash == SmoClassIds.MaterialData))
            {
                True(SmoMaterialDataDecoder.TryDecode(
                        document,entry,out SmoMaterialDataInfo? material,
                        out string materialError) && material is not null,
                    $"material structure decodes: {file} [{entry.Index}]: " +
                    materialError);
                True(material!.Passes.Count is >= 1 and <= 3,
                    $"observed material pass count: {file} [{entry.Index}]");
            }

            foreach (SmoObjectEntry entry in document.Objects.Where(
                         item => item.TypeHash == SmoClassIds.MeshData))
            {
                True(SmoMeshDataDecoder.TryDecode(
                        document,entry,out SmoMeshDataInfo? meshData,
                        out string meshDataError) && meshData is not null,
                    $"mesh container decodes: {file} [{entry.Index}]: " +
                    meshDataError);
                True(meshData!.CrossPlatform is not null ||
                     meshData.PlatformSpecific is not null,
                    $"mesh has a representation: {file} [{entry.Index}]");
            }

            foreach (SmoObjectEntry entry in document.Objects.Where(
                         item => item.TypeHash == SmoClassIds.Model))
            {
                True(SmoModelDecoder.TryDecode(
                        document,entry,out SmoModelData? model,
                        out string modelError) && model is not null,
                    $"model structure decodes: {file} [{entry.Index}]: " +
                    modelError);
                True(model!.BaseMesh.TargetTypeHash == SmoClassIds.MeshData,
                    $"model base relationship targets a mesh: " +
                    $"{file} [{entry.Index}]");
            }

            foreach (SmoObjectEntry entry in document.Objects.Where(
                         item => item.TypeHash == SmoClassIds.Skin))
            {
                True(SmoSkinDecoder.TryDecode(
                        document,entry,out SmoSkin? skin,out string skinError) &&
                     skin is not null,
                    $"skin structure decodes: {file} [{entry.Index}]: {skinError}");
                Equal(16,skin!.Bones.Count,
                    $"PC skin has a 16-slot palette: {file} [{entry.Index}]");
                True(skin.Bones.All(item =>
                        Matrix4x4.Invert(item.InverseBindMatrix,out _)),
                    $"skin inverse-bind matrices are invertible: " +
                    $"{file} [{entry.Index}]");
                if (skin.BlendInfluenceCountHint != 0 &&
                    skin.BaseMesh.TargetObjectIndex is int meshIndex &&
                    SmoMeshDecoder.TryDecode(
                        document,document.Objects[meshIndex],out SmoMesh? skinMesh,
                        out _) && skinMesh?.HasSkinningData == true)
                {
                    int maximumInfluences = skinMesh.BlendWeights.Max(weight =>
                        (weight.X > 0.000001f ? 1 : 0) +
                        (weight.Y > 0.000001f ? 1 : 0) +
                        (weight.Z > 0.000001f ? 1 : 0) +
                        (weight.W > 0.000001f ? 1 : 0));
                    Equal((uint)maximumInfluences,skin.BlendInfluenceCountHint,
                        $"nonzero skin blend-influence hint: " +
                        $"{file} [{entry.Index}]");
                }
                decodedSkins++;
                decodedSkinPaletteSlots += skin.Bones.Count;
            }

            foreach (SmoObjectEntry entry in document.Objects.Where(
                         entry => entry.TypeHash == SmoClassIds.TextureData))
            {
                True(SmoTextureDataDecoder.TryDecode(
                        document,entry,out SmoTextureDataInfo? textureData,
                        out string textureDataError) && textureData is not null,
                    $"texture structure decodes: {file} [{entry.Index}]: " +
                    textureDataError);
                SmoTextureRepresentationData? preview = textureData!.CrossPlatform ??
                    (textureData.PlatformSpecific?.Kind ==
                        SmoTextureRepresentationKind.Direct3DBgra32
                        ? textureData.PlatformSpecific
                        : null);
                if (preview is null)
                {
                    True(!SmoTextureDecoder.TryDecode(
                            document,entry,out _,out string nativePs2Error) &&
                         nativePs2Error.StartsWith(
                             "PS2_TEXTURE_PREVIEW_UNSUPPORTED:",
                             StringComparison.Ordinal),
                        $"native PS2 texture remains structural-only: " +
                        $"{file} [{entry.Index}]");
                    continue;
                }

                True(SmoTextureDecoder.TryDecode(
                        document, entry, out SmoTexture? texture,
                        out string textureError),
                    $"BGRA texture preview decodes: {file} [{entry.Index}]: " +
                    textureError);

                Equal(
                    checked(texture!.Width * texture.Height * 4),
                    texture.Bgra32Pixels.Length,
                    $"texture byte count: {file} [{entry.Index}]");
                Equal(texture.Width * 4, texture.PixelStride, $"texture stride: {file} [{entry.Index}]");
                Equal(SmoTextureLayout.Bgra, texture.SourceLayout,
                    $"texture source layout: {file} [{entry.Index}]");
                Equal(preview.Kind,texture.RepresentationKind,
                    $"texture representation: {file} [{entry.Index}]");
                Equal(preview.MipLevels.Count,texture.MipLevelCount,
                    $"texture mip count: {file} [{entry.Index}]");
                if (preview.Kind == SmoTextureRepresentationKind.CrossPlatformBgrx32)
                {
                    var sourcePixels = preview.MipLevels[0].PixelData.Span;
                    var actualPixels = texture.Bgra32Pixels.Span;
                    bool matches = sourcePixels.Length == actualPixels.Length;
                    for (int pixel = 0; matches && pixel < actualPixels.Length; pixel += 4)
                        matches = sourcePixels.Slice(pixel, 3).SequenceEqual(actualPixels.Slice(pixel, 3)) && actualPixels[pixel + 3] == 255;
                    True(matches, $"XRGB preview preserves BGR and uses opaque alpha: {file} [{entry.Index}]");
                }
                else
                    True(preview.MipLevels[0].PixelData.Span.SequenceEqual(texture.Bgra32Pixels.Span),
                        $"texture base BGRA bytes: {file} [{entry.Index}]");
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
                True(SmoStaticRenderObjectDecoder.TryDecode(
                        document,entry,
                        out SmoStaticRenderObjectData? staticObject,
                        out string staticError) && staticObject is not null,
                    $"strict static render object decode: {file} " +
                    $"[{entry.Index}]: {staticError}");
                Matrix4x4 staticTransform = staticObject!.Transform;

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
            $"skins: {decodedSkins}/{decodedSkinPaletteSlots} palette slots, " +
            $"material colors: {resolvedMaterialColors}, " +
            $"vertex-colored meshes: {vertexColoredMeshes}, " +
            $"serializer fields: {recoveredSerializerFields} " +
            $"(decoded: {decodedSerializerFields}, hex-only: {hexOnlySerializerFields}), " +
            $"meshes under static objects: {meshesUnderStaticObjects}, " +
            $"without static objects: {meshesWithoutStaticObjects}");
        foreach (string sample in unplacedSamples)
            Console.WriteLine($"  unplaced: {sample}");
        foreach ((string layout, int count) in serializerFieldLayouts
                     .OrderBy(item => item.Key, StringComparer.Ordinal))
        {
            Console.WriteLine($"  serializer: {layout} · {count}");
        }
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
                    ["bloomeye"] = 1
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
                    $"bloomx wing [{wingMeshIndex}] resolved material binding");
                Equal("bloom", wingBinding!.Texture!.Name,
                    $"bloomx wing [{wingMeshIndex}] explicit bloom texture");
                var wingModelEntry = document.Objects[document.Objects[wingMeshIndex].ParentIndex!.Value];
                True(SmoModelDecoder.TryDecode(document, wingModelEntry, out var wingModel, out _) &&
                     wingModel.Renderable.Material?.TargetObjectIndex is int wingMaterialIndex &&
                     SmoMaterialDataDecoder.TryDecode(document, document.Objects[wingMaterialIndex], out var wingMaterial, out _) &&
                     wingMaterial!.Passes[0].Texture?.ObjectId == document.Objects[wingBinding.Texture.ObjectIndex].Id &&
                     wingBinding.DiffuseArgb == wingMaterial.Color.DiffuseArgb &&
                     wingBinding.MaterialRenderState?.FinalBlendOperation == wingMaterial.Passes[0].FinalBlendOperation &&
                     wingBinding.MaterialRenderState.MaterialRenderStates.SequenceEqual(wingMaterial.RenderStates),
                    $"bloomx wing [{wingMeshIndex}] binding follows its actual Model/Material/Texture relationships and states");
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
                True(binding!.Texture is null && binding.Issue?.StartsWith("MATERIAL_FRONTEND_SHAPE:") == true,
                    $"bloomx mesh [{meshIndex}] explicitly reports the frontend's single-layer limit");
                SmoLoadedMaterial loadedMaterial = binding.LoadedMaterial!;
                Equal(3, loadedMaterial.Passes.Count,
                    $"bloomx mesh [{meshIndex}] retains three actual passes");
                Equal("bloom_xc", loadedMaterial.Passes[0].Layers[0].Texture!.Texture!.Name,
                    $"bloomx mesh [{meshIndex}] actual first pass texture");
                foreach (var pass in loadedMaterial.Passes.Skip(1))
                {
                    var track = pass.Layers[0].Animation!;
                    Equal(38, track.Keys.Count, $"bloomx mesh [{meshIndex}] original end-time key count");
                    Equal(10, track.Keys.Select(key => key.Texture!.ObjectId).Distinct().Count(),
                        $"bloomx mesh [{meshIndex}] ten distinct referenced textures");
                    Equal("sparkles0001", track.Keys[0].Texture!.Texture!.Name,
                        $"bloomx mesh [{meshIndex}] actual first key texture");
                    True(Math.Abs(track.Duration - 1.26666677f) < 0.000001f,
                        $"bloomx mesh [{meshIndex}] source duration remains unmodified");
                }
                True(binding.AnimationFrames is null && binding.FrameDuration is null,
                    $"bloomx mesh [{meshIndex}] no inferred uniform playback");
                True(part.HasTextureCoordinates1,
                    $"bloomx mesh [{meshIndex}] second UV channel");
                True(part.TextureCoordinates.Any(uv =>
                        uv.X < 0 || uv.X > 1 || uv.Y < 0 || uv.Y > 1),
                    $"bloomx mesh [{meshIndex}] tiled base UV channel");
            }

            SmoTexture firstFrame = bloomXBindings[103].LoadedMaterial!.Passes[1].Layers[0].Animation!.Keys[0].Texture!.Texture!;
            SmoTexture bloomXBase = bloomXBindings[103].LoadedMaterial!.Passes[0].Layers[0].Texture!.Texture!;
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
            True(SmoTextureDataDecoder.TryDecode(document,
                    document.Objects[firstFrame.ObjectIndex],
                    out SmoTextureDataInfo? storedFrame, out string storedError),
                $"bloomx animated frame source: {storedError}");
            Equal(SmoTextureRepresentationKind.Direct3DBgra32,
                storedFrame!.PlatformSpecific!.Kind,
                "bloomx animated frame native BGRA representation");
            True(storedFrame.PlatformSpecific.MipLevels[0].PixelData.Span
                    .SequenceEqual(firstFrame.Bgra32Pixels.Span),
                "bloomx animated frame preserves stored BGRA including alpha");
            True(firstFrame.Bgra32Pixels.ToArray()
                    .Where((_, index) => index % 4 == 3).All(alpha => alpha == 255),
                "bloomx pristine animated frame stores opaque alpha");
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
            Equal(SmoTextureRepresentationKind.Direct3DBgra32,
                texture.RepresentationKind,"loading texture representation");
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

        IReadOnlyList<SmoCollisionMesh> collisions =
            SmoCollisionMeshDecoder.DecodeAll(document);
        Equal(132, collisions.Count, "Alfea02 collision mesh count");
        Equal(3333, collisions.Sum(collision =>
            collision.TriangleIndices.Count / 3), "Alfea02 collision triangle count");
        SmoCollisionMesh writableCollision = collisions.First(collision =>
            SmoPlacementTransformWriter.CanWriteNodeTransform(
                document,
                collision.NodeObjectIndex));
        Matrix4x4 movedCollisionWorld = writableCollision.WorldTransform;
        movedCollisionWorld.M41 += 17.25f;
        movedCollisionWorld.M42 -= 3.5f;
        movedCollisionWorld.M43 += 8.75f;
        SmoPlacementTransformPatchResult collisionPatch =
            SmoPlacementTransformWriter.Patch(document,
            [
                new SmoPlacementTransformEdit(
                    writableCollision.NodeObjectIndex,
                    writableCollision.WorldTransform,
                    movedCollisionWorld)
            ]);
        SmoDocument movedCollisionDocument = SmoDocument.Parse(collisionPatch.Data);
        True(collisionPatch.NodeObjectIndices.SequenceEqual(
                [writableCollision.NodeObjectIndex]),
            "Alfea02 collision patch reports node owner");
        True(SmoNodeTransformDecoder.TryResolveNodeWorldMatrix(
                movedCollisionDocument,
                movedCollisionDocument.Objects[writableCollision.NodeObjectIndex],
                out Matrix4x4 actualCollisionWorld) &&
             MatrixDifference(actualCollisionWorld, movedCollisionWorld) < 0.001f,
            "Alfea02 collision node world translation round-trips");
        SmoObjectEntry originalShape =
            document.Objects[writableCollision.MeshBoundingVolumeObjectIndex];
        SmoObjectEntry patchedShape =
            movedCollisionDocument.Objects[writableCollision.MeshBoundingVolumeObjectIndex];
        True(document.Data.Span.Slice(
                checked((int)originalShape.PhysicalOffset),
                checked((int)originalShape.SerializedSize)).SequenceEqual(
                movedCollisionDocument.Data.Span.Slice(
                    checked((int)patchedShape.PhysicalOffset),
                    checked((int)patchedShape.SerializedSize))),
            "Alfea02 collision patch preserves spMeshBV geometry bytes");

        SmoCollisionMesh independentCollision = collisions[0];
        Matrix4x4 independentWorld = independentCollision.WorldTransform;
        Vector3 collisionDelta = new(12.5f, -4.25f, 7.75f);
        independentWorld.M41 += collisionDelta.X;
        independentWorld.M42 += collisionDelta.Y;
        independentWorld.M43 += collisionDelta.Z;
        SmoPlacementTransformPatchResult geometryPatch =
            SmoPlacementTransformWriter.Patch(document,
            [
                new SmoPlacementTransformEdit(
                    independentCollision.CollisionInfoObjectIndex,
                    independentCollision.WorldTransform,
                    independentWorld)
            ]);
        True(geometryPatch.CollisionMeshObjectIndices.SequenceEqual(
                [independentCollision.MeshBoundingVolumeObjectIndex]),
            "Alfea02 independent collision patch reports spMeshBV owner");
        SmoDocument geometryPatchedDocument = SmoDocument.Parse(geometryPatch.Data);
        True(SmoCollisionMeshDecoder.TryDecodeShape(
                geometryPatchedDocument,
                geometryPatchedDocument.Objects[
                    independentCollision.MeshBoundingVolumeObjectIndex],
                out Vector3[] movedVertices,
                out _),
            "Alfea02 independently moved collision geometry remains decodable");
        Vector3 worldVertexBefore = Vector3.Transform(
            independentCollision.Positions[0],
            independentCollision.WorldTransform);
        Vector3 worldVertexAfter = Vector3.Transform(
            movedVertices[0],
            independentCollision.WorldTransform);
        True(Vector3.Distance(
                worldVertexAfter,
                worldVertexBefore + collisionDelta) < 0.001f,
            "Alfea02 collision vertex translation round-trips in world space");

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
        True(SmoVertexLayoutRegistry.TryGet(chandelierBody.VertexFormat, out var chandelierLayout) &&
             chandelierLayout!.NormalOffset.HasValue, "Alfea02 chandelier native normal layout");
        for (int vertex = 0; vertex < chandelierBody.Normals.Length; vertex++)
        {
            int offset = checked((int)chandelierBody.VertexDataOffset +
                vertex * chandelierBody.Stride + chandelierLayout!.NormalOffset!.Value);
            var stored = document.Data.Span.Slice(offset, 12);
            var expectedNormal = new Vector3(BinaryPrimitives.ReadSingleLittleEndian(stored),
                BinaryPrimitives.ReadSingleLittleEndian(stored[4..]),
                BinaryPrimitives.ReadSingleLittleEndian(stored[8..]));
            Equal(expectedNormal, chandelierBody.Normals[vertex],
                "Alfea02 chandelier normals preserve original buffer values");
        }
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
        Equal(SmoTextureRepresentationKind.Direct3DBgra32,
            mipmappedTop!.RepresentationKind,
            "Alfea03 shelf texture Direct3D BGRA representation");
        Equal(9,mipmappedTop.MipLevelCount,
            "Alfea03 shelf texture exact mip count");
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
