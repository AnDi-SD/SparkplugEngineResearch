using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
using SmoExporter.Core;
using SmoViewer.Core;

internal static class CleanSkinnedPoseRegression
{
    internal static int Prepare(string[] args)
    {
        if(args.Length is <3 or >11 || args.Length%2!=1)throw new ArgumentException("NEW_OUTPUT_DIR (SMO SAN) [at most five pairs]");
        string directory=Path.GetFullPath(args[0]);
        if(Directory.Exists(directory))throw new InvalidOperationException("Use a new reference directory");
        Directory.CreateDirectory(directory);var cases=new List<object>();
        for(int index=1;index<args.Length;index+=2)
        {
            string smo=Path.GetFullPath(args[index]),san=Path.GetFullPath(args[index+1]);
            var document=SmoDocument.Load(smo);
            var scene=SmoSceneBuilder.Build(document,new SmoExportOptions(AnimationPaths:[san]));
            if(scene.Animations.Count!=1 || !SmoAnimationDecoder.TryDecode(san,out var clip,out var error) || clip is null)
                throw new InvalidDataException("Selected stock SAN must decode and bind to the generated scene");
            string reference="source-"+cases.Count+".json";
            File.WriteAllText(Path.Combine(directory,reference),JsonSerializer.Serialize(new
            {
                smo,smoSha256=Hash(smo),
                nodes=scene.Nodes.Select(node=>
                {
                    if(!Matrix4x4.Decompose(node.BindLocalMatrix,out var scale,out var rotation,out var position))throw new InvalidDataException("Invalid local transform");
                    return new {index=node.ObjectIndex,name=node.Name,parent=node.ParentObjectIndex,position=Vector(position),rotation=QuaternionValues(rotation),scale=Vector(scale),bindWorld=Matrix(node.BindWorldMatrix)};
                }),
                placements=scene.MeshPlacements.Select(p=>new {name="placement_"+p.SceneObjectIndex,mesh=p.MeshObjectIndex,parent=p.ParentNodeObjectIndex,world=Matrix(p.WorldMatrix)}),
                skins=scene.Skins.Select(s=>new {index=s.ObjectIndex,joints=s.JointObjectIndices,inverseBinds=s.InverseBindMatrices.Select(Matrix)}),
                meshes=scene.Meshes.Select(m=>new {index=m.ObjectIndex,skin=m.SkinObjectIndex,positions=m.Positions.Select(Vector),weights=m.BlendWeights.Select(QuaternionValues),joints=m.JointIndices.Select(QuaternionValues)}),
                nativeSkins=document.Objects.Where(e=>e.TypeHash==SmoClassIds.Skin).Select(entry=>
                {
                    if(!SmoSkinDecoder.TryDecode(document,entry,out var skin,out var skinError) || skin is null)throw new InvalidDataException(skinError);
                    return new {id=entry.Id,blendInfluences=skin.BlendInfluenceCountHint,
                        edges=new[] {skin.Renderable.Material?.ObjectId??0,skin.Renderable.Fog?.ObjectId??0,skin.BaseMesh.ObjectId}.Concat(skin.Bones.Select(b=>b.NodeObjectId)),
                        inverseBindHex=Convert.ToHexString(skin.Bones.SelectMany(b=>Matrix(b.InverseBindMatrix)).SelectMany(BitConverter.GetBytes).ToArray())};
                })
            }));
            var warnings=new List<string>();var bound=SmoAnimationBinding.BindByName(clip.Tracks,warnings);
            float duration=clip.Duration;
            float[] times=new[] {0f,MathF.Min(1f/30,duration),duration*.25f,duration*.5f,MathF.Max(0,duration-1f/30),duration}.Distinct().Order().ToArray();
            var samples=times.Select(seconds=>new {seconds,tracks=bound.Values.ToDictionary(track=>track.NodeName,track=>
            {
                var pose=track.Sample(seconds,Vector3.Zero,Quaternion.Identity,Vector3.One);var values=new Dictionary<string,float[]>();
                if(track.Positions.Count>0)values["2"]=Vector(pose.Position);
                if(track.Rotations.Count>0)values["3"]=QuaternionValues(pose.Rotation);
                if(track.Scales.Count>0)values["4"]=Vector(pose.Scale);
                return values;
            })});
            cases.Add(new {name=Path.GetFileNameWithoutExtension(smo),reference,referenceSha256=Hash(Path.Combine(directory,reference)),san,sanSha256=Hash(san),nativePrsReference=false,samples,warnings=scene.Warnings});
        }
        File.WriteAllText(Path.Combine(directory,"input.json"),JsonSerializer.Serialize(new {kind="imported-skinned-stock-san-references",cases,
            scope="Existing PC-validated C# SAN sampler; independent NumPy FK will compare actual Viewer positions. Native skin palette bytes and IDs are recorded separately for original-loader comparison."}));
        Console.WriteLine($"Prepared {cases.Count} stock SAN / generated SMO pairs");return 0;
    }

    private static string Hash(string path)=>Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
    private static float[] Vector(Vector3 v)=>[v.X,v.Y,v.Z];
    private static float[] QuaternionValues(Quaternion v)=>[v.X,v.Y,v.Z,v.W];
    private static float[] QuaternionValues(Vector4 v)=>[v.X,v.Y,v.Z,v.W];
    private static float[] Matrix(Matrix4x4 m)=>[m.M11,m.M12,m.M13,m.M14,m.M21,m.M22,m.M23,m.M24,m.M31,m.M32,m.M33,m.M34,m.M41,m.M42,m.M43,m.M44];
}
