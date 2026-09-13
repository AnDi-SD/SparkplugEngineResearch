using System.Numerics;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

public static partial class SmoResearchDatabase
{
    private static SmoResearchClassAnalysisResult AnalyzeFont(
        string databasePath,SmoResearchClassReport report)
    {
        using SqliteConnection connection=OpenResearch(Path.GetFullPath(databasePath),false);
        TextCorpus corpus=LoadTextCorpus(connection);
        List<FontObservation> items=corpus.Fonts;
        if (items.Count!=20||items.Sum(item=>item.Fields.Count)!=20||
            items.Any(item=>item.Data.Glyphs.Count!=224||item.Data.Height!=32||
                item.Data.Baseline!=6)||items.Count(item=>item.OwnsAtlas)!=2)
            throw new InvalidDataException("spFont corpus profile changed.");
        NavigationPairing pc=CompareNavigation(items,item=>item.CorpusKey,item=>item.PairingKey,
            item=>item.SemanticSignature,item=>item.SerializedSha256,"pc-working","pc-pristine");
        if (pc!=new NavigationPairing(10,10,10))
            throw new InvalidDataException("spFont PC copies changed.");
        ExecutableIdentity pcExe=FindExecutable(connection,"pc");
        ExecutableIdentity ps2Exe=FindExecutable(connection,"ps2");
        string[] tokens=["spFont","spFontSerializer","esfFont","pFont->GetImage",
            "pFont->GetHeight","pFont->GetBaseline","FontChar.uWidth","FontChar.UV"];
        RequireAsciiTokens(pcExe.Path,tokens);RequireAsciiTokens(ps2Exe.Path,tokens);
        string now=DateTime.UtcNow.ToString("O");using SqliteTransaction tx=connection.BeginTransaction();
        ClearNavigationAnalysis(connection,tx,report.TypeHash,"font_%");
        UpsertNavigationDefinitions(connection,tx,report.TypeHash,items.SelectMany(x=>x.Fields),
            [(0,0,0,"font.data","Font data","font_atlas",
                "spTextureData relationship; UInt32 height/baseline; 224 packed UInt8 width + Vector2 UV0/UV1 records for 0x20..0xFF",
                false,"Required complete character table.")],false,false);
        AnnotateNavigationFields(connection,tx,items.SelectMany(item=>item.Fields.Select(
            field=>(item.FileId,item.ObjectIndex,field))));
        int owner=InsertNavigationVariant(connection,tx,report.TypeHash,"font_inline_atlas_owner",
            "Inline font atlas owner",JsonSerializer.Serialize(new { inlineImage=true,objects=2 }),
            "First font in each PC menu copy physically owns the shared spTextureData atlas.",now);
        int reference=InsertNavigationVariant(connection,tx,report.TypeHash,"font_shared_atlas_reference",
            "Shared font atlas reference",JsonSerializer.Serialize(new { inlineImage=false,objects=18 }),
            "Remaining fonts use a sized reference to the first font's atlas.",now);
        foreach (FontObservation item in items)
            AssignNavigationVariant(connection,tx,item.FileId,item.ObjectIndex,
                item.OwnsAtlas?owner:reference,"strict atlas/metrics/224-glyph decode");
        int evidence=0;
        evidence+=InsertTextExecutableEvidence(connection,tx,report.TypeHash,owner,pcExe,now,true,"font");
        evidence+=InsertTextExecutableEvidence(connection,tx,report.TypeHash,owner,ps2Exe,now,false,"font");
        evidence+=InsertEvidence(connection,tx,report.TypeHash,owner,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict nested atlas relationship and exhaustive 0x20..0xFF glyph decode",
            "All 20 PC objects decode: image relationship, height 32, baseline 6 and " +
            "224 packed glyphs each. UV rectangles are finite, ordered and inside [0,1]. " +
            "Two fonts inline-own identical menu atlas objects; 18 reference them.",null,now);
        evidence+=InsertEvidence(connection,tx,report.TypeHash,owner,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "Menus/menu.smo font ordinal comparison",
            $"PC working/pristine: {pc.EqualBytes}/{pc.Paired} exact glyph/atlas and full-byte " +
            "matches. PS2 executable supports the identical serializer, but no spFont " +
            "object occurs in the available PS2 SMO corpus.",null,now);
        UpdateNavigationClass(connection,tx,report.TypeHash,"bitmap_font",
            "Bitmap font atlas, height/baseline and fixed 0x20..0xFF glyph width/UV table",
            "Complete PC read-only decode plus independent PS2 executable confirmation. " +
            "The first menu font owns the atlas and later fonts reference it; edits must " +
            "preserve the complete 224-entry table and shared ownership.");
        tx.Commit();Checkpoint(connection);
        return TextResult(report,items.Select(x=>x.SerializedSha256),items.Count,evidence,
            "Atlas-owner/reference variants; all 4,480 glyph records decoded.");
    }

    private static SmoResearchClassAnalysisResult AnalyzeTextRenderable(
        string databasePath,SmoResearchClassReport report)
    {
        using SqliteConnection connection=OpenResearch(Path.GetFullPath(databasePath),false);
        TextCorpus corpus=LoadTextCorpus(connection);List<TextRenderableObservation> items=corpus.Renderables;
        if (items.Count!=20||items.Sum(x=>x.Fields.Count)!=140||
            items.Any(x=>x.Data.Text!="0"||x.Data.Color!=0xFFD59AE5||
                x.Data.WrapWidth.HasValue||x.Data.Alignment.HasValue)||
            items.Count(x=>x.FontOwnsAtlas)!=2)
            throw new InvalidDataException("spTextRenderable profile changed.");
        NavigationPairing pc=CompareNavigation(items,x=>x.CorpusKey,x=>x.PairingKey,
            x=>x.SemanticSignature,x=>x.SerializedSha256,"pc-working","pc-pristine");
        if (pc!=new NavigationPairing(10,10,10))
            throw new InvalidDataException("Text-renderable PC copies changed.");
        ExecutableIdentity pcExe=FindExecutable(connection,"pc");
        ExecutableIdentity ps2Exe=FindExecutable(connection,"ps2");
        string[] tokens=["spTextRenderable","spTextRenderableSerializer",
            "esfTextRenderableFont","esfTextRenderableText","esfTextRenderableColor",
            "esfTextRenderableWrap","esfTextRenderableAlignment"];
        RequireAsciiTokens(pcExe.Path,tokens);RequireAsciiTokens(ps2Exe.Path,tokens);
        string now=DateTime.UtcNow.ToString("O");using SqliteTransaction tx=connection.BeginTransaction();
        ClearNavigationAnalysis(connection,tx,report.TypeHash,"text_renderable_%");
        UpsertNavigationDefinitions(connection,tx,report.TypeHash,items.SelectMany(x=>x.Fields),
            TextRenderableDefinitions(),false,false);
        AnnotateNavigationFields(connection,tx,items.SelectMany(item=>item.Fields.Select(
            field=>(item.FileId,item.ObjectIndex,field))));
        int owner=InsertNavigationVariant(connection,tx,report.TypeHash,
            "text_renderable_atlas_owner_chain","Atlas-owner text chain",
            JsonSerializer.Serialize(new { fontInline=true,fontAtlasInline=true,objects=2 }),
            "Text renderable owns the font which owns the shared atlas.",now);
        int reference=InsertNavigationVariant(connection,tx,report.TypeHash,
            "text_renderable_shared_atlas_chain","Shared-atlas text chain",
            JsonSerializer.Serialize(new { fontInline=true,fontAtlasInline=false,objects=18 }),
            "Text renderable owns a font which references the first font atlas.",now);
        foreach (TextRenderableObservation item in items)
            AssignNavigationVariant(connection,tx,item.FileId,item.ObjectIndex,
                item.FontOwnsAtlas?owner:reference,"strict renderable/text/font chain decode");
        int evidence=0;
        evidence+=InsertTextExecutableEvidence(connection,tx,report.TypeHash,owner,pcExe,now,true,"renderable");
        evidence+=InsertTextExecutableEvidence(connection,tx,report.TypeHash,owner,ps2Exe,now,false,"renderable");
        evidence+=InsertEvidence(connection,tx,report.TypeHash,owner,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict inherited renderable plus font/text/color decode",
            "All 20 objects decode with inline spFont, byte-string text '0', ARGB 0xFFD59AE5, " +
            "material/fog, alpha-sort true and priority zero. Wrap and alignment are " +
            "omitted defaults. Every nested font/atlas chain validates completely.",null,now);
        evidence+=InsertEvidence(connection,tx,report.TypeHash,owner,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "Menus/menu.smo text-renderable ordinal comparison",
            $"PC working/pristine: {pc.EqualBytes}/{pc.Paired} exact. The independent PS2 " +
            "executable names the same five fields and value getters, although available " +
            "PS2 SMO files contain no serialized text-renderable objects.",null,now);
        UpdateNavigationClass(connection,tx,report.TypeHash,"text_renderable",
            "spRenderable-derived text draw state with font, byte-string text, ARGB color and optional wrap/alignment",
            "Complete PC read-only decode plus PS2 executable confirmation. Field order is " +
            "font (4), text (0), color (1), then optional wrap (2) and alignment (3). " +
            "Editing must preserve nested font/atlas ownership.");
        tx.Commit();Checkpoint(connection);
        return TextResult(report,items.Select(x=>x.SerializedSha256),items.Count,evidence,
            "Two nested atlas-ownership variants; all text/renderable/font fields decoded.");
    }

    private static SmoResearchClassAnalysisResult AnalyzeTextNode(
        string databasePath,SmoResearchClassReport report)
    {
        using SqliteConnection connection=OpenResearch(Path.GetFullPath(databasePath),false);
        TextCorpus corpus=LoadTextCorpus(connection);List<TextNodeObservation> items=corpus.Nodes;
        if (items.Count!=20||items.Sum(x=>x.Fields.Count)!=100||
            items.Count(x=>x.AtlasOwnerChain)!=2)
            throw new InvalidDataException("spTextNode profile changed: "+
                JsonSerializer.Serialize(new { Count=items.Count,
                    Fields=items.Sum(x=>x.Fields.Count),
                    Owners=items.Count(x=>x.AtlasOwnerChain) }));
        NavigationPairing pc=CompareNavigation(items,x=>x.CorpusKey,x=>x.PairingKey,
            x=>x.SemanticSignature,x=>x.SerializedSha256,"pc-working","pc-pristine");
        if (pc!=new NavigationPairing(10,10,10))
            throw new InvalidDataException("Text-node PC copies changed.");
        ExecutableIdentity pcExe=FindExecutable(connection,"pc");
        ExecutableIdentity ps2Exe=FindExecutable(connection,"ps2");
        RequireAsciiTokens(pcExe.Path,["spTextNode","spTextNodeSerializer"]);
        RequireAsciiTokens(ps2Exe.Path,["spTextNode","spTextNodeSerializer"]);
        string now=DateTime.UtcNow.ToString("O");using SqliteTransaction tx=connection.BeginTransaction();
        ClearNavigationAnalysis(connection,tx,report.TypeHash,"text_node_%");
        UpsertNavigationDefinitions(connection,tx,report.TypeHash,items.SelectMany(x=>x.Fields),
            [(0,0,0,"render_node.renderable","Text renderable","object_relationship",
                "required inline relationship to spTextRenderable",false,
                "Concrete spTextNode renderable; exactly one observed.")],true,false);
        AnnotateNavigationFields(connection,tx,items.SelectMany(item=>item.Fields.Select(
            field=>(item.FileId,item.ObjectIndex,field))));
        int owner=InsertNavigationVariant(connection,tx,report.TypeHash,"text_node_atlas_owner_chain",
            "Atlas-owner text node",JsonSerializer.Serialize(new { atlasOwnerChain=true,objects=2 }),
            "Node owns text renderable/font/atlas chain.",now);
        int reference=InsertNavigationVariant(connection,tx,report.TypeHash,"text_node_shared_atlas_chain",
            "Shared-atlas text node",JsonSerializer.Serialize(new { atlasOwnerChain=false,objects=18 }),
            "Node owns text renderable/font; font references shared atlas.",now);
        foreach (TextNodeObservation item in items)
            AssignNavigationVariant(connection,tx,item.FileId,item.ObjectIndex,
                item.AtlasOwnerChain?owner:reference,"strict node/renderable/font/atlas chain decode");
        int evidence=0;
        evidence+=InsertTextExecutableEvidence(connection,tx,report.TypeHash,owner,pcExe,now,true,"node");
        evidence+=InsertTextExecutableEvidence(connection,tx,report.TypeHash,owner,ps2Exe,now,false,"node");
        evidence+=InsertEvidence(connection,tx,report.TypeHash,owner,null,null,
            "class_analysis:corpus","smo-corpus-v2.sqlite",
            "strict node transform and complete nested text-renderable/font/atlas chain",
            "All 20 objects decode with position, rotation, scale, static flag and exactly " +
            "one inline spTextRenderable. Each four-level ownership/reference chain resolves " +
            "and decodes without scanning or inferred boundaries.",null,now);
        evidence+=InsertEvidence(connection,tx,report.TypeHash,owner,null,null,
            "class_analysis:cross_corpus","smo-corpus-v2.sqlite",
            "Menus/menu.smo text-node name comparison",
            $"PC working/pristine: {pc.EqualBytes}/{pc.Paired} exact. Class registration " +
            "in both executables proves spTextNode derives from spRenderNode; available PS2 " +
            "SMO corpus contains no text-node instances.",null,now);
        UpdateNavigationClass(connection,tx,report.TypeHash,"text_node",
            "spRenderNode-derived placed text node inline-owning one spTextRenderable",
            "Complete PC read-only node/renderable/font/atlas decode plus independent PS2 " +
            "class/serializer confirmation. No separate TextNode data section exists beyond " +
            "the concrete renderable relationship.");
        tx.Commit();Checkpoint(connection);
        return TextResult(report,items.Select(x=>x.SerializedSha256),items.Count,evidence,
            "Two nested atlas-ownership variants; all four-level text chains decoded.");
    }

    private static SmoResearchClassAnalysisResult TextResult(
        SmoResearchClassReport report,IEnumerable<string> hashes,int count,int evidence,string notes)=>
        new(report.TypeHash,report.EngineName??"<unknown>","confirmed_read_only",
            report.Profiles.Count,count,report.Profiles.Sum(x=>x.UniqueResourceCount),
            hashes.Distinct().Count(),count,evidence,notes);

    private static int InsertTextExecutableEvidence(
        SqliteConnection connection,SqliteTransaction tx,uint typeHash,int variant,
        ExecutableIdentity executable,string now,bool pc,string kind)
    {
        string location=kind switch
        {
            "font" when pc=>"x86 writer RVA 0x00042AB9..0x00042E1D; character loop 0x20..0xFF",
            "renderable" when pc=>"x86 writer RVA 0x000420C0..0x00042305",
            "node" when pc=>"class registration RVA 0x002D1EE0; serializer registration RVA 0x002D30F0",
            _=>"independent MIPS class, serializer and matching field/assertion strings"
        };
        string statement=kind switch
        {
            "font"=>"Executable code confirms image relationship, UInt32 height/baseline and the fixed character loop 0x20..0xFF writing UInt8 width plus UV[0]/UV[1].",
            "renderable"=>"Executable code confirms font, byte-string text, ARGB color, wrap width and UInt32 alignment fields and their getter identities.",
            _=>"Class registration proves spTextNode derives from spRenderNode; its serializer preserves the concrete renderable relationship."
        };
        return InsertEvidence(connection,tx,typeHash,variant,executable.PlatformId,
            executable.CorpusId,pc?"class_analysis:pc_executable":"class_analysis:ps2_executable",
            executable.Path,location,statement,executable.Sha256,now);
    }

    private static IReadOnlyList<(int,int,int,string,string,string,string,bool,string)>
        TextRenderableDefinitions()=>
    [
        (1,0,0,"renderable.material","Material","object_relationship","relationship to spMaterialData",false,"Required/inline in corpus."),
        (1,1,0,"renderable.fog","Fog","object_relationship","relationship to spFog",false,"Required sized reference."),
        (1,2,0,"renderable.alpha_sort","Alpha sort","uint32_boolean","little-endian UInt32 0/1",false,"True."),
        (1,3,0,"renderable.priority","Priority","uint32","little-endian UInt32",false,"Zero."),
        (0,0,0,"text_renderable.text","Text","byte_string","UInt16 byte count followed by raw bytes",false,"Required; value '0'."),
        (0,1,0,"text_renderable.color","Text color","argb","ARGB UInt32",false,"Required; 0xFFD59AE5."),
        (0,2,0,"text_renderable.wrap_width","Wrap width","uint32","little-endian UInt32 pixels",false,"Optional; not observed."),
        (0,3,0,"text_renderable.alignment","Alignment","uint32_enum","little-endian UInt32",false,"Optional; not observed."),
        (0,4,0,"text_renderable.font","Font","object_relationship","inline relationship to spFont",false,"Required; nested font fully decoded.")
    ];

    private static TextCorpus LoadTextCorpus(SqliteConnection connection)
    {
        var fonts=new List<FontObservation>();var renderables=new List<TextRenderableObservation>();
        var nodes=new List<TextNodeObservation>();
        foreach (MeshNavigationFileLocation location in
                 LoadNavigationLocations(connection,SmoClassIds.TextNode))
        {
            SmoDocument document=SmoDocument.Parse(ReadMeshNavigationResource(location),location.RelativePath);
            foreach (SmoObjectEntry entry in document.Objects)
            {
                if (entry.TypeHash==SmoClassIds.Font)
                {
                    if (!SmoFontDecoder.TryDecode(document,entry,out var data,out string error)||data is null)
                        throw new InvalidDataException($"Could not decode font: {error}");
                    var fields=FontAnnotations(document,entry,data);
                    string key=TextKey(location,entry);
                    string semantic=JsonSerializer.Serialize(new { data.Height,data.Baseline,
                        ImageType=data.Image.TargetTypeHash,Glyphs=data.Glyphs.Select(GlyphValue) });
                    fonts.Add(new FontObservation(location.FileId,entry.Index,location.CorpusKey,key,
                        data,data.Image.Encoding==SmoNodeRelationshipEncoding.InlineObject,
                        ObjectHash(document,entry),semantic,fields));
                }
                else if (entry.TypeHash==SmoClassIds.TextRenderable)
                {
                    if (!SmoTextRenderableDecoder.TryDecode(document,entry,out var data,out string error)||data is null)
                        throw new InvalidDataException($"Could not decode text renderable: {error}");
                    if (data.Font.TargetObjectIndex is not int fontIndex ||
                        !SmoFontDecoder.TryDecode(document,document.Objects[fontIndex],out SmoFontData? font,out _))
                        throw new InvalidDataException("This corpus Text profile requires a bound Font; scalar inspection also supports missing/default references.");
                    bool owns=font!.Image.Encoding==SmoNodeRelationshipEncoding.InlineObject;
                    var fields=TextRenderableAnnotations(document,entry,data);
                    string semantic=JsonSerializer.Serialize(new { data.Text,data.Color,data.WrapWidth,
                        data.Alignment,data.Renderable.AlphaSortEnable,data.Renderable.Priority,
                        FontType=data.Font.TargetTypeHash });
                    renderables.Add(new TextRenderableObservation(location.FileId,entry.Index,
                        location.CorpusKey,TextKey(location,entry),data,owns,ObjectHash(document,entry),
                        semantic,fields));
                }
                else if (entry.TypeHash==SmoClassIds.TextNode)
                {
                    if (!SmoTextNodeDecoder.TryDecode(document,entry,out var data,out string error)||data is null)
                        throw new InvalidDataException($"Could not decode text node: {error}");
                    if (data.RenderNode.Renderables.Count != 1 ||
                        data.RenderNode.Renderables[0].TargetObjectIndex is not int textIndex ||
                        !SmoTextRenderableDecoder.TryDecode(document,document.Objects[textIndex],out var text,out _) ||
                        text.Font.TargetObjectIndex is not int fontIndex ||
                        !SmoFontDecoder.TryDecode(document,document.Objects[fontIndex],out var font,out _))
                        throw new InvalidDataException("This corpus TextNode profile requires one TextRenderable with a bound Font.");
                    SmoNodeRelationship relation=data.RenderNode.Renderables[0];
                    bool owns=font!.Image.Encoding==SmoNodeRelationshipEncoding.InlineObject;
                    var fields=TextNodeAnnotations(document,entry,data);
                    SmoNodeData node=data.RenderNode.Node;
                    string semantic=JsonSerializer.Serialize(new
                    {
                        Position=VectorValue(node.Position),Rotation=new[] { node.Rotation.X,
                            node.Rotation.Y,node.Rotation.Z,node.Rotation.W },Scale=VectorValue(node.Scale),
                        node.IsStatic,RenderableType=relation.TargetTypeHash
                    });
                    nodes.Add(new TextNodeObservation(location.FileId,entry.Index,
                        location.CorpusKey,TextKey(location,entry),data,owns,ObjectHash(document,entry),
                        semantic,fields));
                }
            }
        }
        return new TextCorpus(fonts,renderables,nodes);
    }

    private static string TextKey(MeshNavigationFileLocation location,SmoObjectEntry entry)=>
        $"{GetCanonicalResourcePath(location.RelativePath).ToLowerInvariant()}|{entry.Name.TrimEnd('\0').ToLowerInvariant()}|{entry.Index}";

    private static IReadOnlyList<MeshNavigationFieldAnnotation> FontAnnotations(
        SmoDocument document,SmoObjectEntry entry,SmoFontData data)
    {
        var field=SmoObjectFieldReader.Read(document,entry).First();
        return [new MeshNavigationFieldAnnotation(0,"font.data",
            SmoSerializedFieldRegistry.GetOwnFieldDefinitions(entry.TypeHash)[0].PayloadLayout,
            JsonSerializer.Serialize(new { Image=RelationshipValue(data.Image),data.Height,
                data.Baseline,Glyphs=data.Glyphs.Select(GlyphValue) }))];
    }

    private static object GlyphValue(SmoFontGlyph glyph)=>new { glyph.Character,glyph.Width,
        Uv0=new[] { glyph.Uv0.X,glyph.Uv0.Y },Uv1=new[] { glyph.Uv1.X,glyph.Uv1.Y } };

    private static IReadOnlyList<MeshNavigationFieldAnnotation> TextRenderableAnnotations(
        SmoDocument document,SmoObjectEntry entry,SmoTextRenderableData data)
    {
        IReadOnlyList<SmoObjectField> direct=SmoObjectFieldReader.Read(document,entry);
        var result=new List<MeshNavigationFieldAnnotation>();
        foreach ((SmoObjectField field,int index) in direct.Select((field,index)=>(field,index)))
        {
            if (field.PayloadSize==0) continue;
            SmoSerializedFieldRegistry.TryDescribeField(entry.TypeHash,direct,index,out var descriptor);
            object value=descriptor!.Key switch
            {
                "renderable.material"=>RelationshipValue(data.Renderable.Material!),
                "renderable.fog"=>RelationshipValue(data.Renderable.Fog!),
                "renderable.alpha_sort"=>data.Renderable.AlphaSortEnable!,
                "renderable.priority"=>data.Renderable.Priority!,
                "text_renderable.font"=>RelationshipValue(data.Font),
                "text_renderable.text"=>data.Text,"text_renderable.color"=>data.Color,
                "text_renderable.wrap_width"=>data.WrapWidth!,
                "text_renderable.alignment"=>data.Alignment!,
                _=>throw new InvalidDataException("Unexpected text-renderable semantic.")
            };
            result.Add(new MeshNavigationFieldAnnotation(index,descriptor.Key,
                descriptor.PayloadLayout,JsonSerializer.Serialize(value)));
        }
        return result.AsReadOnly();
    }

    private static IReadOnlyList<MeshNavigationFieldAnnotation> TextNodeAnnotations(
        SmoDocument document,SmoObjectEntry entry,SmoTextNodeData data)
    {
        IReadOnlyList<SmoObjectField> direct=SmoObjectFieldReader.Read(document,entry);
        var result=new List<MeshNavigationFieldAnnotation>();
        foreach ((SmoObjectField field,int index) in direct.Select((field,index)=>(field,index)))
        {
            if (field.PayloadSize==0) continue;
            SmoSerializedFieldRegistry.TryDescribeField(entry.TypeHash,direct,index,out var descriptor);
            SmoNodeData node=data.RenderNode.Node;
            object value=descriptor!.Key switch
            {
                "node.position"=>VectorValue(node.Position),
                "node.rotation"=>new[] { node.Rotation.X,node.Rotation.Y,node.Rotation.Z,node.Rotation.W },
                "node.scale"=>VectorValue(node.Scale),"node.is_static"=>node.IsStatic,
                "render_node.renderable"=>RelationshipValue(data.RenderNode.Renderables.Single()),
                _=>throw new InvalidDataException("Unexpected text-node semantic.")
            };
            result.Add(new MeshNavigationFieldAnnotation(index,descriptor.Key,
                descriptor.PayloadLayout,JsonSerializer.Serialize(value)));
        }
        return result.AsReadOnly();
    }

    private sealed record TextCorpus(List<FontObservation> Fonts,
        List<TextRenderableObservation> Renderables,List<TextNodeObservation> Nodes);
    private sealed record FontObservation(int FileId,int ObjectIndex,string CorpusKey,
        string PairingKey,SmoFontData Data,bool OwnsAtlas,string SerializedSha256,
        string SemanticSignature,IReadOnlyList<MeshNavigationFieldAnnotation> Fields);
    private sealed record TextRenderableObservation(int FileId,int ObjectIndex,string CorpusKey,
        string PairingKey,SmoTextRenderableData Data,bool FontOwnsAtlas,string SerializedSha256,
        string SemanticSignature,IReadOnlyList<MeshNavigationFieldAnnotation> Fields);
    private sealed record TextNodeObservation(int FileId,int ObjectIndex,string CorpusKey,
        string PairingKey,SmoTextNodeData Data,bool AtlasOwnerChain,string SerializedSha256,
        string SemanticSignature,IReadOnlyList<MeshNavigationFieldAnnotation> Fields);
}
