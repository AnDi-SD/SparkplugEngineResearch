using System.Collections;
using System.Reflection;
using System.Security.Cryptography;
using Colors = System.Windows.Media.Colors;
using System.Windows.Media.Media3D;
using OpenTK.Graphics.OpenGL4;
using SmoViewer.Core;
using SmoViewer.Rendering.Wpf;
using SmoViewer.Scene;

internal static class GpuMipChainRegression
{
    internal static object? VerifyLateUpgrade(SmoPreparedScene scene,Action<bool,string> check)
    {
        if(scene.Texts.Count==0)return null;
        var text=scene.Texts[0];var atlas=text.Text.Geometry!.Atlas!;
        check(SmoTextureDecoder.TryDecode(scene.Document,scene.Document.Objects[atlas.ObjectIndex],out var preview,out var error),error);
        check(preview is not null&&!preview.HasRuntimeMipChain,"Old standalone preview has only base pixels");
        var renderer=new SmoGpuSceneRenderer();renderer.SetAntialiasingSamples(0);
        var type=typeof(SmoGpuSceneRenderer);const BindingFlags hidden=BindingFlags.Instance|BindingFlags.NonPublic;
        object key=type.GetMethod("RegisterTexture",hidden)!.Invoke(renderer,new object[]{0,preview!})!;
        var camera=new PerspectiveCamera(new Point3D(0,30,70),new Vector3D(0,-30,-70),new Vector3D(0,1,0),45){NearPlaneDistance=.01,FarPlaneDistance=1000};
        void Render()=>renderer.Render(16,16,camera,Colors.Black,0,new Point3D(),false,Colors.Gray,0,10,10);
        Render();GL.Finish();var initial=renderer.ConsumeUploadReport();
        check(initial?.GeneratedMipTextureCount==1&&initial.UploadedMipCount==1,"Standalone base preview retains explicit backend mip generation");
        var uploaded=(IDictionary)type.GetField("_textures",hidden)!.GetValue(renderer)!;int oldHandle=(int)uploaded[key]!;
        renderer.AddText(text,new(0,text.Text.ObjectIndex,text.OccurrenceKey));Render();GL.Finish();
        var upgrade=renderer.ConsumeUploadReport();int newHandle=(int)uploaded[key]!;
        check(upgrade?.RuntimeMipTextureCount==1&&upgrade.GeneratedMipTextureCount==0&&upgrade.UploadedMipCount==atlas.Texture!.MipLevels.Count,
            "A later actual material replaces an already-uploaded base preview with the full chain");
        check(newHandle!=oldHandle&&!GL.IsTexture(oldHandle)&&uploaded.Count==1,"Replaced GPU texture is deleted and canonical identity stays unique");
        var last=atlas.Texture!.MipLevels[^1];GL.BindTexture(TextureTarget.Texture2D,newHandle);var pixels=new byte[last.Bgra32Pixels.Length];
        GL.GetTexImage(TextureTarget.Texture2D,atlas.Texture.MipLevels.Count-1,PixelFormat.Bgra,PixelType.UnsignedByte,pixels);
        check(pixels.AsSpan().SequenceEqual(last.Bgra32Pixels.Span),"Late upgrade keeps the original final mip pixels");
        check(upgrade!.CopiedTextureBytes==0&&GL.GetError()==ErrorCode.NoError,"Late upgrade has no extra array copy or GL error");
        renderer.Clear();Render();return new{initial,upgrade,oldHandle,newHandle};
    }

    internal static object Verify(SmoGpuSceneRenderer renderer,SmoPreparedScene scene,Action<bool,string> check)
    {
        var textures=scene.Meshes.SelectMany(m=>m.MaterialDraw?.Passes??[])
            .Concat(scene.Texts.SelectMany(t=>t.Text.Geometry!.MaterialDraw.Passes))
            .SelectMany(p=>p.Stages).Select(s=>s.Texture?.Texture).OfType<SmoTexture>()
            .Where(t=>t.HasRuntimeMipChain).DistinctBy(t=>t.ObjectIndex).ToArray();
        var uploaded=(IDictionary)typeof(SmoGpuSceneRenderer).GetField("_textures",BindingFlags.Instance|BindingFlags.NonPublic)!.GetValue(renderer)!;
        var handles=new Dictionary<int,int>();
        foreach(DictionaryEntry entry in uploaded)
            handles.Add((int)entry.Key.GetType().GetProperty("ObjectIndex")!.GetValue(entry.Key)!,(int)entry.Value!);
        int active=GL.GetInteger(GetPName.ActiveTexture);GL.ActiveTexture(TextureUnit.Texture0);
        int prior=GL.GetInteger(GetPName.TextureBinding2D);var rows=new List<object>();
        try
        {
            foreach(var texture in textures)
            {
                check(handles.TryGetValue(texture.ObjectIndex,out int handle),"Runtime mip texture is uploaded");
                GL.BindTexture(TextureTarget.Texture2D,handle);
                GL.GetTexParameter(TextureTarget.Texture2D,GetTextureParameter.TextureMaxLevel,out int maximum);
                check(maximum==texture.MipLevels.Count-1,"GPU maximum mip is the actual runtime chain end");
                var hashes=new List<string>();long bytes=0;
                for(int level=0;level<texture.MipLevels.Count;++level)
                {
                    var mip=texture.MipLevels[level];
                    GL.GetTexLevelParameter(TextureTarget.Texture2D,level,GetTextureParameter.TextureWidth,out int width);
                    GL.GetTexLevelParameter(TextureTarget.Texture2D,level,GetTextureParameter.TextureHeight,out int height);
                    check(width==mip.Width&&height==mip.Height,"GPU mip dimensions equal original runtime shadow");
                    var pixels=new byte[mip.Bgra32Pixels.Length];
                    GL.GetTexImage(TextureTarget.Texture2D,level,PixelFormat.Bgra,PixelType.UnsignedByte,pixels);
                    check(pixels.AsSpan().SequenceEqual(mip.Bgra32Pixels.Span),"Every GPU mip byte equals common runtime pixels");
                    hashes.Add(Convert.ToHexString(SHA256.HashData(pixels)));bytes+=pixels.Length;
                }
                // Explicit old-backend comparison, not a second production mip
                // algorithm: ask this same GPU to regenerate only from level0.
                int generated=GL.GenTexture();int changed=0;
                try
                {
                    GL.BindTexture(TextureTarget.Texture2D,generated);
                    GL.TexImage2D(TextureTarget.Texture2D,0,PixelInternalFormat.Rgba8,texture.Width,texture.Height,0,
                        PixelFormat.Bgra,PixelType.UnsignedByte,texture.Bgra32Pixels.ToArray());
                    GL.GenerateMipmap(GenerateMipmapTarget.Texture2D);
                    for(int level=1;level<texture.MipLevels.Count;++level)
                    {
                        var mip=texture.MipLevels[level];var pixels=new byte[mip.Bgra32Pixels.Length];
                        GL.GetTexImage(TextureTarget.Texture2D,level,PixelFormat.Bgra,PixelType.UnsignedByte,pixels);
                        for(int i=0;i<pixels.Length;++i)if(pixels[i]!=mip.Bgra32Pixels.Span[i])++changed;
                    }
                }
                finally{GL.DeleteTexture(generated);}
                rows.Add(new{texture.ObjectIndex,levels=texture.MipLevels.Count,bytes,hashes,regeneratedDifferentComponents=changed});
            }
            check(GL.GetError()==ErrorCode.NoError,"Mip readback and old-backend comparison have no GL errors");
        }
        finally{GL.BindTexture(TextureTarget.Texture2D,prior);GL.ActiveTexture((TextureUnit)active);}
        return rows;
    }
}
