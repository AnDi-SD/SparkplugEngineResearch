using System.ComponentModel;
using System.Runtime.CompilerServices;
using Avalonia.Media.Imaging;
using SMOTextureTool.Core;

namespace SMOTextureTool;

public sealed class TextureRowViewModel : INotifyPropertyChanged, IDisposable
{
    private Bitmap? _replacementPreview;
    private string? _replacementPath;
    private string _vertexColorDetails = "";

    public required TextureInfo Texture { get; init; }
    public required Bitmap OriginalPreview { get; set; }
    public string Header =>
        $"{Texture.FileName} · {Texture.Width}×{Texture.Height} · 0x{Texture.FormatCode:X4}";
    public string Details =>
        $"Блок 0x{Texture.BlockOffset:X} · пиксели 0x{Texture.PixelDataOffset:X} · " +
        $"{Texture.PixelDataSize:N0} байт";
    public string ChannelDetails
    {
        get
        {
            TextureChannelInfo channels = Texture.Channels;
            string rgb = channels.RgbChannelsIdentical
                ? $"RGB совпадают · диапазон {channels.RedMin}–{channels.RedMax}"
                : $"R {channels.RedMin}–{channels.RedMax} · " +
                  $"G {channels.GreenMin}–{channels.GreenMax} · " +
                  $"B {channels.BlueMin}–{channels.BlueMax}";
            string alpha = channels.AlphaMin == 255 && channels.AlphaMax == 255
                ? "Alpha постоянна: 255"
                : $"Alpha {channels.AlphaMin}–{channels.AlphaMax} · " +
                  $"значений: {channels.AlphaValueCount}";
            return $"Хранение: {Texture.Layout} · {rgb} · {alpha}";
        }
    }
    public string ResourceDetails
    {
        get
        {
            string content = Texture.ContentKind switch
            {
                TextureContentKind.Color => "Цветовая текстура",
                TextureContentKind.Monochrome
                    when Texture.Material?.UsesColorModulation == true =>
                    "Монохромная основа: игра умножает её на цвет материала/геометрии",
                TextureContentKind.Monochrome =>
                    "Монохромный ресурс материала — точная роль в SMO не указана",
                _ => "Ресурс неизвестного типа"
            };
            string alpha = Texture.HasVariableAlpha
                ? "использует изменяемый Alpha-канал"
                : Texture.HasTransparency
                    ? $"Alpha постоянный: {Texture.Channels.AlphaMin}"
                    : "без прозрачности";
            return $"Тип содержимого: {content} · {alpha}.";
        }
    }
    public string VertexColorDetails
    {
        get => _vertexColorDetails;
        private set => SetField(ref _vertexColorDetails, value);
    }

    public void SetVertexColorBinding(VertexColorBindingInfo? binding)
    {
        VertexColorDetails = binding is null
            ? ""
            : $"Точная окраска: spModel @ 0x{binding.ModelOffset:X} · " +
              $"spMeshData @ 0x{binding.MeshOffset:X} · " +
              $"{binding.TriangleCount} треугольников · " +
              $"{binding.InfluencingVertexIndices.Count} из " +
              $"{binding.VertexCount} вершин · " +
              $"UV-конфликтов: {binding.ConflictingPixelWrites:N0}.";
    }
    public string MaterialDetails => Texture.Material is { } material
        ? $"Владелец: spMaterialData #{material.Index} @ 0x{material.BlockOffset:X} · " +
          $"pass {material.PassIndex}, layer {material.LayerIndex} " +
          $"({material.LayerClassName}) · FinalBlendOp={material.FinalBlendOperation} · " +
          $"esfMaterialLayerTexture @ 0x{material.TextureContainerOffset:X}\n" +
          $"RGB op={material.ColorOperation} · Alpha op={material.AlphaOperation} · " +
          $"Address UV={material.AddressU}/{material.AddressV} · Filter={material.Filter} · " +
          $"UV set={material.TextureCoordinateIndex} · " +
          $"Transform={material.TextureTransformFlags} · State[0]={material.UnknownLayerState0}\n" +
          $"MaterialRenderStates=[{string.Join(", ", material.MaterialRenderStates)}]"
        : "Владелец материала не найден.";
    public string ResizeNotice =>
        "Безопасный предел — 4096². Экспериментальный режим разрешает до 16384²; " +
        "стороны должны быть степенями двойки.";

    public Bitmap? ReplacementPreview
    {
        get => _replacementPreview;
        private set => SetField(ref _replacementPreview, value);
    }

    public string? ReplacementPath
    {
        get => _replacementPath;
        private set => SetField(ref _replacementPath, value);
    }

    public bool HasReplacement => ReplacementPath is not null;
    public bool HasNoReplacement => !HasReplacement;

    public void SetReplacement(string path, Bitmap preview)
    {
        ReplacementPreview?.Dispose();
        ReplacementPreview = preview;
        ReplacementPath = path;
        NotifyState();
    }

    public void UpdatePreviews(Bitmap original, Bitmap? replacement)
    {
        OriginalPreview.Dispose();
        OriginalPreview = original;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(OriginalPreview)));

        ReplacementPreview?.Dispose();
        ReplacementPreview = replacement;
    }

    public void ResetReplacement()
    {
        ReplacementPreview?.Dispose();
        ReplacementPreview = null;
        ReplacementPath = null;
        NotifyState();
    }

    public void Dispose()
    {
        OriginalPreview.Dispose();
        ReplacementPreview?.Dispose();
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    private bool SetField<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value))
            return false;
        field = value;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
        return true;
    }

    private void NotifyState()
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(HasReplacement)));
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(HasNoReplacement)));
    }
}
