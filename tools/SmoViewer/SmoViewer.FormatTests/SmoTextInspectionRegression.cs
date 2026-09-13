using System.Numerics;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using SmoViewer.Core;
namespace SmoViewer.FormatTests;
internal static class SmoTextInspectionRegression
{
    private delegate bool RelationshipFormatter(SmoDocument document,ReadOnlySpan<byte> payload,int count,out string value);
    private static byte[] Word(uint value) => BitConverter.GetBytes(value);
    // Test-only explicit short/UInt16 wire envelopes, independent of the
    // production Text inspector. These are fixtures, not an application writer.
    private static byte[] Field(byte id,byte[] payload) =>
        [(byte)(0xC0|id),..BitConverter.GetBytes(checked((ushort)payload.Length)),..payload];
    private static byte[] Text(params byte[] value) => [..BitConverter.GetBytes(checked((ushort)value.Length)),..value];
    private static SmoDocument Document(byte[] fields,uint type=SmoClassIds.TextRenderable)
    {
        byte[] body=[..Word(type),..Word(0x4F4F4253),..fields];
        using var stream=new MemoryStream();using var writer=new BinaryWriter(stream);
        foreach(uint word in new uint[]{0x53504646,0x26,0,(uint)(54+body.Length),2,54,(uint)body.Length,1,1})writer.Write(word);
        writer.Write((ushort)0);writer.Write(type);writer.Write(0u);writer.Write((uint)body.Length);writer.Write(0u);writer.Write(body);
        return SmoDocument.ParseOwned(stream.ToArray(),"synthetic-text-inspection");
    }
    private static int CheckRelationshipSequences(SmoDocument document,Action<bool,string> check)
    {
        // Only navigation_portal.sets currently calls this formatter with count2;
        // its actual loader rejects NULL endpoints. Exercise the formatting
        // boundary through a test-only Span delegate without inventing a loaded
        // navigation graph or adding a production API solely for the test.
        var format=typeof(SmoSerializedFieldInspector).GetMethod("TryFormatRelationships",BindingFlags.NonPublic|BindingFlags.Static)
            ?.CreateDelegate<RelationshipFormatter>()??throw new InvalidDataException("Missing relationship formatter under test.");
        const string nil="id=0x00000000, inline=0, unresolved";
        const string sized="id=0x11223344, inline=0, unresolved";
        byte[] reference=[..Word(0x11223344),..Word(0)];
        // Opaque inline bytes test reference framing only; no Font is created
        // or claimed initialized by this metadata formatter.
        byte[] inline=[..Word(0x11223344),..Word(9),..Word(SmoClassIds.Font),..Word(0x4F4F4253),0];
        (string Name,byte[] Payload,int Count,bool Decoded,string? Display)[] cases=
        [
            ("single NULL is a four-byte ID",Word(0),1,true,nil),
            ("two NULL references consume two IDs",[..Word(0),..Word(0)],2,true,$"[{nil}; {nil}]"),
            ("NULL then sized reference keeps both entries",[..Word(0),..reference],2,true,$"[{nil}; {sized}]"),
            ("sized reference then NULL keeps the four-byte tail",[..reference,..Word(0)],2,true,$"[{sized}; {nil}]"),
            ("inline object uses its exact declared extent",inline,1,true,"id=0x11223344, inline=9, unresolved"),
            ("nonzero ID requires its size word",Word(0x11223344),1,false,null),
            ("oversized inline extent is not decoded",[..Word(0x11223344),..Word(uint.MaxValue)],1,false,null),
            ("truncated inline body is not decoded",inline[..^1],1,false,null),
            ("a second NULL is trailing data when one reference is expected",[..Word(0),..Word(0)],1,false,null),
            ("non-reference trailing byte is not decoded",[..reference,0x7F],1,false,null)
        ];
        foreach(var item in cases)
        {
            bool decoded=format(document,item.Payload,item.Count,out string display);
            check(decoded==item.Decoded&&(decoded?display==item.Display:display.Length==0),
                $"Relationship inspector: {item.Name}; decoded={decoded}, display='{display}'.");
        }
        return cases.Length;
    }
    internal static int Run(string source,string output)
    {
        int checks=0;
        void Check(bool ok,string text) { if(!ok)throw new InvalidDataException(text);++checks; }
        SmoTextRenderableData Decode(SmoDocument document)
        {
            Check(SmoTextRenderableDecoder.TryDecode(document,document.Objects[0],out var text,out var error),error);
            return text!;
        }
        var defaults=Decode(Document([0,0]));
        Check(defaults.TextWasNull&&defaults.Text==""&&defaults.RawTextBytes.Length==0&&defaults.Color==uint.MaxValue&&
            defaults.Font.ObjectId==0&&defaults.WrapWidth is null&&defaults.Alignment is null,"proven omitted scalar defaults");
        Check(!defaults.HasDerivedLayout&&defaults.SerializedFieldMask==0,"metadata mode does not claim layout");
        byte[] body=[..Field(2,Word(2)),..Field(3,Word(9)),0,
            ..Field(4,Word(0)),..Field(0,Text((byte)'A',(byte)'B',0)),..Field(2,Word(uint.MaxValue)),
            ..Field(3,Word(2)),..Field(1,Word(0x80706050)),0];
        var oddDocument=Document(body);var odd=Decode(oddDocument);
        Check(odd.Text=="AB"&&odd.RawTextBytes.Span.SequenceEqual(new byte[]{65,66,0})&&!odd.TextWasNull,"odd byte-count ASCII string");
        Check(odd.WrapWidth==uint.MaxValue&&odd.Color==0x80706050&&odd.Alignment==2,"UInt32 wrap/color/alignment");
        Check(odd.Renderable.AlphaSortEnable==1&&odd.Renderable.Priority==9,"actual inherited Renderable assignments");
        Check(odd.SerializedFieldMask==31&&!odd.HasDerivedLayout,"all own fields observed without layout");
        Check(SmoSerializedFieldInspector.Inspect(oddDocument,oddDocument.Objects[0]).Any(field=>
            field.DisplayValue.Contains("4294967295",StringComparison.Ordinal)),"inspector displays integer wrap without float conversion");
        var nullFont=SmoSerializedFieldInspector.Inspect(oddDocument,oddDocument.Objects[0])
            .Single(field=>field.Descriptor.Key=="text_renderable.font");
        Check(nullFont.IsDecoded&&nullFont.PayloadSize==4&&nullFont.DisplayValue=="id=0x00000000, inline=0, unresolved",
            $"public inspector decodes authored four-byte NULL Font; decoded={nullFont.IsDecoded}, display='{nullFont.DisplayValue}'");
        int relationshipSequenceChecks=CheckRelationshipSequences(oddDocument,Check);
        byte[] raw=[65,0,66,0xE9,0];
        var repeated=Decode(Document([0,..Field(0,Text(90,0)),..Field(2,Word(1)),..Field(7,[0xFF]),
            ..Field(0,Text(raw)),..Field(2,Word(19)),0]));
        Check(repeated.RawTextBytes.Span.SequenceEqual(raw)&&repeated.Text=="A"&&repeated.WrapWidth==19,"repeat order and interior NUL/raw high byte preserved");
        var unterminated=Decode(Document([0,..Field(0,Text(65,0xE9)),0]));
        Check(unterminated.Text=="A\u00e9"&&unterminated.RawTextBytes.Length==2&&!unterminated.TextWasNull,
            "unterminated bytes remain metadata with reversible Latin-1 display");
        var explicitNull=Decode(Document([0,..Field(0,Text()),0]));
        Check(explicitNull.TextWasNull&&explicitNull.SerializedFieldMask==1,"explicit NULL distinct from omission");
        var foreign=Document(body);
        Check(!SmoTextRenderableDecoder.TryDecode(foreign,oddDocument.Objects[0],out _,out _),"foreign entry rejected");
        var malformed=Document([0,..Field(0,[3,0,65]),0]);
        Check(!SmoTextRenderableDecoder.TryDecode(malformed,malformed.Objects[0],out _,out var error)&&
            error.Contains("byte-string extent",StringComparison.Ordinal),"malformed string diagnostic");
        var emptyNode=Document([0,0],SmoClassIds.TextNode);
        Check(SmoTextNodeDecoder.TryDecode(emptyNode,emptyNode.Objects[0],out var node,out _)&&
            node!.RenderNode.Renderables.Count==0,"TextNode shares RenderNode metadata, no invented one-child requirement");
        var real=SmoDocument.Load(source);int texts=0,nodes=0;
        foreach(var entry in real.Objects)
        {
            if(entry.TypeHash==SmoClassIds.TextRenderable)
            {
                Check(SmoTextRenderableDecoder.TryDecode(real,entry,out var text,out var detail),detail);
                Check(text!.Text=="0"&&text.Color==0xFFD59AE5&&text.Font.TargetTypeHash==SmoClassIds.Font&&
                    text.RawTextBytes.Span.SequenceEqual(new byte[]{0x30,0}),"original menu text/font/color");++texts;
            }
            if(entry.TypeHash==SmoClassIds.TextNode)
            {
                Check(SmoTextNodeDecoder.TryDecode(real,entry,out var textNode,out var detail),detail);
                Check(textNode!.RenderNode.Renderables.Count==1,"original menu TextNode child observed");++nodes;
            }
        }
        Check(texts>0&&nodes>0,"selected real menu contains both classes");
        File.WriteAllText(output,JsonSerializer.Serialize(new {status="passed",checks,texts,nodes,metadataOnly=true,relationshipSequenceChecks,
            sourceSha256=Convert.ToHexString(SHA256.HashData(real.Data.Span)),
            nativeSha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory,"SparkplugViewerNative.dll"))))},
            new JsonSerializerOptions{WriteIndented=true})+"\n");
        Console.WriteLine($"Text inspection: {checks} checks, {texts} TextRenderable, {nodes} TextNode");return 0;
    }
}
