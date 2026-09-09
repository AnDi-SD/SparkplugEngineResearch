using System.Buffers.Binary;
using System.Diagnostics;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SmoImporter.Core;
using SmoViewer.Core;

internal static class TextureTemplateRegression
{
    public static void Run(string[] args)
    {
        bool probe = args[0] == "--probe";
        if (probe) args = args.Skip(1).ToArray();
        if (args.Length is < 2 or > 7) throw new ArgumentException("OUTPUT SMO [up to6 specimens]");
        string output = Path.GetFullPath(args[0]); Directory.CreateDirectory(output);
        var watch = Stopwatch.StartNew(); var rows = new List<object>(); int checks = 0, failures = 0, destinationGuards = 0;
        SmoDocument? pcGuardSource = null;
        var imports = new List<(ImportedTexture Texture,byte[] Pixels)>();
        foreach (var size in new[] {(8,8),(17,9)})
        {
            using var image = new Image<Rgba32>(size.Item1,size.Item2,new Rgba32(17,71,139,53));
            using var stream = new MemoryStream(); image.SaveAsPng(stream);
            imports.Add((new ImportedTexture("template-regression","image/png",size.Item1,size.Item2,stream.ToArray()),
                Enumerable.Range(0,size.Item1*size.Item2).SelectMany(_=>new byte[] {139,71,17,53}).ToArray()));
        }
        foreach (string path in args.Skip(1))
        {
            byte[] source = File.ReadAllBytes(path); var doc = SmoDocument.ParseOwned(source);
            if (doc.HasErrors) throw new InvalidDataException("Invalid source catalog");
            if ((doc.Header.PlatformMask & 2) == 0)
            {
                int rejected = VerifyNonPcDestination(doc, imports[0].Texture);
                destinationGuards += rejected;
                rows.Add(new { source = Path.GetFullPath(path), sourceSha256 = Hash(source),
                    operation = "non-PC destination rejected before texture serialization", passed = true, checks = rejected });
                if (!File.ReadAllBytes(path).AsSpan().SequenceEqual(source)) throw new InvalidDataException("Source was changed");
                continue;
            }
            pcGuardSource ??= doc;
            var entries = doc.Objects.Where(e => e.TypeHash == SmoClassIds.TextureData)
                .Where(e => SmoTextureDataDecoder.TryDecode(doc,e,out var d,out _) &&
                    (d.SourceKind == SmoTextureSourceKind.LegacyCrossPlatform || SmoTextureDataWriter.CanReplace(d,out _))).Take(3).ToArray();
            if (entries.Length == 0) throw new InvalidDataException("No writable template");
            foreach (var entry in entries)
            {
                var variants = new List<(string,SmoDocument)> { ("original",doc) };
                SmoTextureDataDecoder.TryDecode(doc,entry,out var data,out _);
                if (SmoTextureDataWriter.CanReplace(data!,out _))
                {
                    foreach (var size in new[] {(1,1),(13,7)})
                    {
                        byte[] pixels = new byte[size.Item1*size.Item2*4];
                        var resized = SmoDocument.ParseOwned(SmoTextureDataWriter.ReplaceBgra(doc,entry.Index,size.Item1,size.Item2,pixels));
                        variants.Add(($"resized-{size.Item1}x{size.Item2}",resized));
                        if (size.Item1 != 1) continue;
                        var current = resized.Objects[entry.Index];
                        byte[] signature = resized.Data.Slice((int)current.PhysicalOffset,8).ToArray();
                        byte[] compact = signature.Concat(SmoObjectFieldReader.Read(resized,current)
                            .SelectMany(f => SmoDataBlockWriter.BuildField(f.FieldType,f.Payload.Span))).ToArray();
                        variants.Add(("compact-top",SmoDocument.ParseOwned(SmoLeafObjectReplacer.Replace(resized,entry.Index,compact))));
                    }
                }
                foreach (var (variant,fixture) in variants)
                {
                    SmoObjectEntry current = fixture.Objects[entry.Index];
                    SmoTextureDataDecoder.TryDecode(fixture,current,out var info,out _);
                    string prefix = Path.GetFileNameWithoutExtension(path)+"-"+entry.Index+"-"+variant;
                    if (info!.SourceKind == SmoTextureSourceKind.LegacyCrossPlatform)
                    {
                        uint wireFormat = CrossFormat(fixture,current);
                        bool ok = info.CrossPlatform!.FormatValue == wireFormat;
                        Record("metadata-format",ok,$"wire={wireFormat}, decoded={info.CrossPlatform.FormatValue}");
                    }
                    try
                    {
                        var method = typeof(SmoLevelModelGraphReplacer).GetMethod("FindTextureTemplate",BindingFlags.NonPublic|BindingFlags.Static)!;
                        var selected = (SmoObjectEntry)method.Invoke(null,new object[] {fixture,
                            new Dictionary<uint,uint[]> { [1] = new[] {current.Id} },false})!;
                        Record("select-template",selected.Id==current.Id,$"selected={selected.Index}, requested={current.Index}");
                    }
                    catch(Exception error) { Record("select-template",false,Unwrap(error).Message); }
                    foreach (var (imported,expected) in imports)
                    foreach (string writer in variant=="original" && entry.Index==entries[0].Index
                        ? new[] {"level","skin-branch","canonical"} : new[] {"level","skin-branch"})
                    {
                        try
                        {
                            byte[] replacement;
                            if (writer == "level") replacement = SmoLevelModelGraphReplacer.BuildTextureObject(fixture,current,imported);
                            else if(writer=="canonical")
                            {
                                var method=typeof(SmoLevelModelGraphReplacer).GetMethod("BuildCanonicalTextureObject",BindingFlags.NonPublic|BindingFlags.Static)!;
                                replacement=(byte[])method.Invoke(null,new object[] {fixture,imported})!;
                            }
                            else
                            {
                                var method = typeof(SmoSkinnedBranchSplitBuilder).GetMethod("BuildTextureObject",BindingFlags.NonPublic|BindingFlags.Static)!;
                                object built = method.Invoke(null,new object[] {fixture,current,1u,"probe",imported})!;
                                replacement = (byte[])built.GetType().GetProperty("Data")!.GetValue(built)!;
                            }
                            byte[] result = SmoLeafObjectReplacer.Replace(fixture,current.Index,replacement);
                            var after = SmoDocument.ParseOwned(result);
                            bool valid = !after.HasErrors && SmoTextureDataDecoder.TryDecode(after,after.Objects[current.Index],out _,out _);
                            if (!valid) throw new InvalidDataException("Written object no longer decodes");
                            SmoTextureDataDecoder.TryDecode(after,after.Objects[current.Index],out var actual,out _);
                            var mip = actual!.SelectedRepresentation!.MipLevels.Single();
                            if (mip.Width!=imported.Width || mip.Height!=imported.Height || !mip.PixelData.Span.SequenceEqual(expected))
                                throw new InvalidDataException("Wrong dimensions or BGRA pixels");
                            if (actual.SourceKind != SmoTextureSourceKind.Embedded || actual.CrossPlatform is not null ||
                                actual.PlatformSpecific is not {Kind:SmoTextureRepresentationKind.Direct3DBgra32,FormatValue:0})
                                throw new InvalidDataException("Imported pixels require the native PC BGRA wrapper and format0");
                            string target = Path.Combine(output,prefix+"-"+writer+$"-{imported.Width}x{imported.Height}.smo"); File.WriteAllBytes(target,result);
                            Record(writer+$"-{imported.Width}x{imported.Height}",true,target,Hash(result));
                        }
                        catch(Exception error) { Record(writer+$"-{imported.Width}x{imported.Height}",false,Unwrap(error).Message); }
                    }
                    void Record(string operation,bool passed,string detail,string? outputSha256=null)
                    {
                        checks++; if(!passed) failures++;
                        rows.Add(new {source=Path.GetFullPath(path),sourceSha256=Hash(source),fixtureSha256=Hash(fixture.Data.Span),
                            objectIndex=entry.Index,variant,operation,passed,detail,outputSha256});
                        Console.WriteLine($"{(passed?"PASS":"FAIL")} {prefix} {operation}: {(passed?"ok":detail)}");
                    }
                }
            }
            if(!File.ReadAllBytes(path).AsSpan().SequenceEqual(source))throw new InvalidDataException("Source was changed");
        }
        int guardChecks=pcGuardSource is null ? 0 : VerifyUnsupportedTemplates(pcGuardSource,imports[0].Texture);
        File.WriteAllText(Path.Combine(output,"report.json"),JsonSerializer.Serialize(new {status=failures==0?"passed":"failed",checks,failures,guardChecks,destinationGuards,
            elapsedSeconds=watch.Elapsed.TotalSeconds,peakWorkingSet=Process.GetCurrentProcess().PeakWorkingSet64,rows},new JsonSerializerOptions {WriteIndented=true}));
        if(failures>0 && !probe)throw new InvalidDataException($"{failures}/{checks} texture-template checks failed");
    }
    private static uint CrossFormat(SmoDocument doc,SmoObjectEntry entry)
    {
        var cross=SmoObjectFieldReader.Read(doc,entry).Single(f=>f.FieldType==0 && f.PayloadSize>0);
        if(!SmoDataBlockReader.TryReadHeader(cross.Payload.Span,0,out var header) || header.FieldType!=5)
            throw new InvalidDataException("Missing cross-platform raw header");
        return BinaryPrimitives.ReadUInt32LittleEndian(cross.Payload.Span[(header.PayloadOffset+8)..]);
    }
    private static Exception Unwrap(Exception e)=>e is TargetInvocationException {InnerException:not null} t?t.InnerException!:e;
    private static string Hash(ReadOnlySpan<byte> raw)=>Convert.ToHexString(SHA256.HashData(raw));

    private static int VerifyNonPcDestination(SmoDocument source,ImportedTexture imported)
    {
        var entry=source.Objects.First(e=>e.TypeHash==SmoClassIds.TextureData);
        Action[] operations=
        [
            ()=>typeof(SmoLevelModelGraphReplacer).GetMethod("FindTextureTemplate",BindingFlags.NonPublic|BindingFlags.Static)!
                .Invoke(null,new object[]{source,new Dictionary<uint,uint[]>(),false}),
            ()=>SmoLevelModelGraphReplacer.BuildTextureObject(source,entry,imported),
            ()=>typeof(SmoSkinnedBranchSplitBuilder).GetMethod("BuildTextureObject",BindingFlags.NonPublic|BindingFlags.Static)!
                .Invoke(null,new object[]{source,entry,1u,"guard",imported}),
            ()=>typeof(SmoLevelModelGraphReplacer).GetMethod("BuildCanonicalTextureObject",BindingFlags.NonPublic|BindingFlags.Static)!
                .Invoke(null,new object[]{source,imported})
        ];
        foreach(var operation in operations)
        {
            try { operation(); throw new InvalidDataException("Non-PC destination accepted a new PC texture"); }
            catch(Exception error) when(Unwrap(error) is NotSupportedException unsupported &&
                unsupported.Message.StartsWith("TEXTURE_DESTINATION_PLATFORM:",StringComparison.Ordinal)) {}
        }
        Console.WriteLine($"PASS non-PC destination mask={source.Header.PlatformMask}: {operations.Length} early guards");
        return operations.Length;
    }

    private static int VerifyUnsupportedTemplates(SmoDocument source,ImportedTexture imported)
    {
        int checks=0;int index=source.Objects.First(e=>e.TypeHash==SmoClassIds.TextureData).Index;
        static byte[] Join(params byte[][] values)=>values.SelectMany(v=>v).ToArray();
        static byte[] UInt(uint value){byte[] raw=new byte[4];BinaryPrimitives.WriteUInt32LittleEndian(raw,value);return raw;}
        static byte[] Field(int type,byte[] payload)=>SmoDataBlockWriter.BuildField(type,payload);
        byte[] baseRaw=Join(new byte[]{1},UInt(8),UInt(8),UInt(0),new byte[]{1},UInt(8),UInt(32),UInt(8),new byte[256]);
        byte[] nextRaw=Join(UInt(4),UInt(16),UInt(4),new byte[64]);
        byte[] cross=Join(Field(5,Join(UInt(8),UInt(8),UInt(0),UInt(4),new byte[256])),new byte[]{0});
        byte[] Object(byte[] local)=>Join(UInt(SmoClassIds.TextureData),"SBOO"u8.ToArray(),local,new byte[]{0});
        foreach(string kind in new[]{"multiple-mips","two-representations","legacy-invalid-format"})
        {
            byte[] specific=Join(Field(0,baseRaw),kind=="multiple-mips"?Field(1,nextRaw):Array.Empty<byte>(),new byte[]{0});
            byte[] local=Join(Field(2,new byte[]{0}),new byte[]{0},Field(6,UInt(6)),
                kind=="two-representations"?Field(0,cross):Array.Empty<byte>(),Field(1,specific),new byte[]{0});
            byte[] replacement=kind=="legacy-invalid-format"
                ? Object(Field(0,Join(Field(5,Join(UInt(8),UInt(8),UInt(4),UInt(4),new byte[256])),new byte[]{0})))
                : Object(Field(3,local));
            var doc=SmoDocument.ParseOwned(SmoLeafObjectReplacer.Replace(source,index,replacement));
            bool decoded=SmoTextureDataDecoder.TryDecode(doc,doc.Objects[index],out var data,out var diagnostic);
            if(kind=="legacy-invalid-format")
            {
                // A bare source field0 is skipped by the actual PC factory;
                // the old metadata-only decoder did not prove initialization.
                if(decoded || !diagnostic.StartsWith("TEXTURE_PC_SOURCE_INVALID:",StringComparison.Ordinal))
                    throw new InvalidDataException("Bare cross source unexpectedly initialized a PC texture");
            }
            else if(!decoded || SmoTextureDataWriter.CanReplace(data!,out _))
                throw new InvalidDataException("Expected structurally decoded but unwritable fixture: "+kind);
            checks++;
            foreach(string writer in new[]{"level","skin-branch"})
            {
                try
                {
                    if(writer=="level")SmoLevelModelGraphReplacer.BuildTextureObject(doc,doc.Objects[index],imported);
                    else typeof(SmoSkinnedBranchSplitBuilder).GetMethod("BuildTextureObject",BindingFlags.NonPublic|BindingFlags.Static)!
                        .Invoke(null,new object[]{doc,doc.Objects[index],1u,"guard",imported});
                    throw new InvalidDataException("Unsupported template was accepted: "+kind);
                }
                catch(Exception error) when(Unwrap(error) is NotSupportedException ||
                    (kind=="legacy-invalid-format" && Unwrap(error) is InvalidDataException invalid &&
                     invalid.Message.StartsWith("TEXTURE_PC_SOURCE_INVALID:",StringComparison.Ordinal))){checks++;}
            }
        }
        return checks;
    }
}
