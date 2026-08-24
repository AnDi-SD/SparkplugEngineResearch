using SixLabors.ImageSharp;

namespace SmoImporter.Core;

public static class ImportedTextureFileReader
{
    public static ImportedTexture Read(string path)
    {
        string fullPath = Path.GetFullPath(path);
        var file = new FileInfo(fullPath);
        if (!file.Exists)
            throw new FileNotFoundException("Texture file was not found.", fullPath);
        if (file.Length > ImportedModelResourceLimits.MaximumEncodedTextureBytes)
        {
            throw new InvalidDataException(
                $"Texture {fullPath} is {file.Length / (1024d * 1024d):N1} MiB, " +
                "above the safe encoded-texture limit.");
        }
        byte[] data = File.ReadAllBytes(fullPath);
        ImageInfo info = Image.Identify(data) ?? throw new InvalidDataException(
            $"Texture {fullPath} has an unsupported or invalid image payload.");
        ImportedModelResourceLimits.AddTexturePixels(
            0, info.Width, info.Height, $"Texture '{Path.GetFileName(fullPath)}'");
        string mimeType = Path.GetExtension(fullPath).ToLowerInvariant() switch
        {
            ".png" => "image/png",
            ".jpg" or ".jpeg" => "image/jpeg",
            ".bmp" => "image/bmp",
            ".tga" => "image/x-tga",
            _ => "application/octet-stream"
        };
        return new ImportedTexture(
            Path.GetFileNameWithoutExtension(fullPath),
            mimeType,
            info.Width,
            info.Height,
            data,
            fullPath);
    }
}
