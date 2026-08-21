using SixLabors.ImageSharp;

namespace SmoImporter.Core;

public static class ImportedTextureFileReader
{
    public static ImportedTexture Read(string path)
    {
        string fullPath = Path.GetFullPath(path);
        byte[] data = File.ReadAllBytes(fullPath);
        ImageInfo info = Image.Identify(data) ?? throw new InvalidDataException(
            $"Texture {fullPath} has an unsupported or invalid image payload.");
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
