using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Media.Imaging;
using Avalonia.Platform.Storage;
using SMOTextureTool.Core;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;

namespace SMOTextureTool;

public sealed partial class MainWindow : Window, INotifyPropertyChanged
{
    private const int SafeMaximumDimension = 4096;

    private static readonly FilePickerFileType SmoType = new("SMO model")
        { Patterns = ["*.smo"] };
    private static readonly FilePickerFileType ImageType = new("Images")
        { Patterns = ["*.png", "*.bmp", "*.jpg", "*.jpeg"] };

    private SmoDocument? _document;
    private string? _sourcePath;
    private string _status = "Откройте исходный файл модели.";
    private bool _safeMode = true;
    private string _selectedPreviewMode = "RGBA на шахматном фоне";

    public MainWindow()
    {
        InitializeComponent();
        DataContext = this;
    }

    public ObservableCollection<TextureRowViewModel> Rows { get; } = [];
    public IReadOnlyList<string> PreviewModes { get; } =
    [
        "RGBA на шахматном фоне",
        "RGBA",
        "Только RGB",
        "Альфа-канал",
        "Окраска модели"
    ];
    public string SelectedPreviewMode
    {
        get => _selectedPreviewMode;
        set
        {
            if (!SetField(ref _selectedPreviewMode, value))
                return;
            RefreshPreviews();
        }
    }
    public string FileName => _sourcePath is null ? "Файл не открыт" : Path.GetFileName(_sourcePath);
    public string Summary => _document is null
        ? "—"
        : $"{_document.Textures.Count} текстур · {_document.Length:N0} байт · " +
          $"{Rows.Count(row => row.HasReplacement)} замен";
    public bool HasDocument => _document is not null;
    public bool HasReplacements => Rows.Any(row => row.HasReplacement);
    public bool SafeMode
    {
        get => _safeMode;
        set
        {
            if (!SetField(ref _safeMode, value))
                return;
            Status = value
                ? "Безопасный режим включён: новые текстуры ограничены размером 4096×4096."
                : "Экспериментальный режим: разрешены текстуры до 16384×16384. Возможен сбой игры или нехватка памяти.";
        }
    }

    public string Status
    {
        get => _status;
        private set => SetField(ref _status, value);
    }

    private async void OpenFile_Click(object? sender, RoutedEventArgs e)
    {
        var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "Открыть SMO", AllowMultiple = false, FileTypeFilter = [SmoType]
        });
        if (files.Count == 0)
            return;

        try
        {
            string path = files[0].Path.LocalPath;
            SmoDocument document = SmoDocument.Load(path);
            ClearRows();
            _document = document;
            _sourcePath = path;
            foreach (TextureInfo texture in document.Textures)
            {
                using SixLabors.ImageSharp.Image<Rgba32> image = document.Decode(texture);
                VertexColorBindingInfo? binding = null;
                if (SelectedPreviewMode == "Окраска модели")
                    document.TryApplyVertexColors(texture, image, out binding);
                var row = new TextureRowViewModel
                {
                    Texture = texture,
                    OriginalPreview = ToPreviewBitmap(image)
                };
                row.SetVertexColorBinding(binding);
                Rows.Add(row);
            }
            Status = document.Textures.Count == 0
                ? "Поддерживаемые текстуры не найдены."
                : "Файл разобран и проверен.";
            NotifyDocumentState();
        }
        catch (Exception ex)
        {
            Status = $"Не удалось открыть файл: {ex.Message}";
        }
    }

    private async void ExtractAll_Click(object? sender, RoutedEventArgs e)
    {
        if (_document is null)
            return;
        var folders = await StorageProvider.OpenFolderPickerAsync(
            new FolderPickerOpenOptions { Title = "Папка для текстур", AllowMultiple = false });
        if (folders.Count == 0)
            return;
        try
        {
            _document.ExportAll(folders[0].Path.LocalPath);
            Status = $"Экспортировано текстур: {_document.Textures.Count}.";
        }
        catch (Exception ex)
        {
            Status = $"Ошибка экспорта: {ex.Message}";
        }
    }

    private async void SelectFolder_Click(object? sender, RoutedEventArgs e)
    {
        if (_document is null)
            return;
        var folders = await StorageProvider.OpenFolderPickerAsync(
            new FolderPickerOpenOptions { Title = "Папка с заменами", AllowMultiple = false });
        if (folders.Count == 0)
            return;

        int found = 0;
        int rejected = 0;
        foreach (TextureRowViewModel row in Rows)
        {
            string path = Path.Combine(folders[0].Path.LocalPath, row.Texture.FileName);
            if (!File.Exists(path))
                continue;
            try
            {
                using SixLabors.ImageSharp.Image<Rgba32> image =
                    SixLabors.ImageSharp.Image.Load<Rgba32>(path);
                ValidateDimensions(row.Texture, image.Width, image.Height);
                row.SetReplacement(path, ToPreviewBitmap(image));
                found++;
            }
            catch
            {
                rejected++;
            }
        }
        Status = $"Найдено замен: {found} из {Rows.Count}; отклонено: {rejected}.";
        NotifyReplacementState();
    }

    private async void Save_Click(object? sender, RoutedEventArgs e)
    {
        if (_document is null || _sourcePath is null)
            return;
        var file = await StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions
        {
            Title = "Сохранить новый SMO",
            SuggestedFileName = Path.GetFileNameWithoutExtension(_sourcePath) + "_mod.smo",
            DefaultExtension = "smo", FileTypeChoices = [SmoType]
        });
        if (file is null)
            return;
        try
        {
            TextureRowViewModel[] selected =
                Rows.Where(row => row.ReplacementPath is not null).ToArray();
            foreach (TextureRowViewModel row in selected)
            {
                ImageInfo info = SixLabors.ImageSharp.Image.Identify(row.ReplacementPath!);
                ValidateDimensions(row.Texture, info.Width, info.Height);
            }
            var replacements = selected.ToDictionary(
                row => row.Texture.Index, row => row.ReplacementPath!);
            byte[] result = _document.Repack(replacements);
            await File.WriteAllBytesAsync(file.Path.LocalPath, result);
            Status = $"Новый SMO сохранён и повторно проверен ({result.Length:N0} байт).";
        }
        catch (Exception ex)
        {
            Status = $"Пересборка отменена: {ex.Message}";
        }
    }

    private async void ExportOne_Click(object? sender, RoutedEventArgs e)
    {
        if (_document is null || sender is not Button { Tag: TextureRowViewModel row })
            return;
        var file = await StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions
        {
            Title = "Сохранить текстуру", SuggestedFileName = row.Texture.FileName,
            DefaultExtension = "png"
        });
        if (file is null)
            return;
        try
        {
            _document.ExportTexture(row.Texture, file.Path.LocalPath);
            Status = $"Сохранено: {row.Texture.FileName}";
        }
        catch (Exception ex)
        {
            Status = $"Ошибка экспорта: {ex.Message}";
        }
    }

    private async void ChooseReplacement_Click(object? sender, RoutedEventArgs e)
    {
        if (sender is not Button { Tag: TextureRowViewModel row })
            return;
        var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = $"Замена для {row.Texture.FileName}", AllowMultiple = false,
            FileTypeFilter = [ImageType]
        });
        if (files.Count == 0)
            return;
        try
        {
            string path = files[0].Path.LocalPath;
            using SixLabors.ImageSharp.Image<Rgba32> image =
                SixLabors.ImageSharp.Image.Load<Rgba32>(path);
            ValidateDimensions(row.Texture, image.Width, image.Height);
            row.SetReplacement(path, ToPreviewBitmap(image));
            double mib = (long)image.Width * image.Height * 4 / 1024d / 1024d;
            Status =
                $"Выбрана замена {image.Width}×{image.Height} для текстуры " +
                $"{row.Texture.Index} · несжатые пиксели {mib:N0} МиБ.";
            NotifyReplacementState();
        }
        catch (Exception ex)
        {
            Status = $"Изображение не принято: {ex.Message}";
        }
    }

    private void ResetReplacement_Click(object? sender, RoutedEventArgs e)
    {
        if (sender is not Button { Tag: TextureRowViewModel row })
            return;
        row.ResetReplacement();
        Status = $"Замена текстуры {row.Texture.Index} сброшена.";
        NotifyReplacementState();
    }

    protected override void OnClosed(EventArgs e)
    {
        ClearRows();
        base.OnClosed(e);
    }

    private void ValidateDimensions(TextureInfo texture, int width, int height)
    {
        bool resized = width != texture.Width || height != texture.Height;
        if (resized && !texture.CanResize)
            throw new SmoFormatException("Для этого формата требуется исходный размер.");
        if (resized && (!IsPowerOfTwo(width) || !IsPowerOfTwo(height)))
            throw new SmoFormatException("Стороны нового изображения должны быть степенями двойки.");
        int maximumDimension = SafeMode
            ? SafeMaximumDimension
            : TextureInfo.MaximumCurrentHeaderDimension;
        if (resized &&
            (width > maximumDimension || height > maximumDimension))
            throw new SmoFormatException(
                SafeMode
                    ? "Безопасный режим допускает максимум 4096×4096. " +
                      "Для больших текстур отключите его в верхней панели."
                    : $"Максимальная сторона — {maximumDimension} пикселей.");
    }

    private static bool IsPowerOfTwo(int value) => value > 0 && (value & (value - 1)) == 0;

    private static Bitmap ToBitmap(SixLabors.ImageSharp.Image<Rgba32> image)
    {
        using var stream = new MemoryStream();
        image.SaveAsPng(stream);
        stream.Position = 0;
        return new Bitmap(stream);
    }

    private Bitmap ToPreviewBitmap(SixLabors.ImageSharp.Image<Rgba32> image)
    {
        const int maximumPreviewSide = 512;
        SixLabors.ImageSharp.Image<Rgba32> preview;
        if (image.Width <= maximumPreviewSide && image.Height <= maximumPreviewSide)
        {
            preview = image.Clone();
        }
        else
        {
            double scale = Math.Min(
                maximumPreviewSide / (double)image.Width,
                maximumPreviewSide / (double)image.Height);
            int width = Math.Max(1, (int)Math.Round(image.Width * scale));
            int height = Math.Max(1, (int)Math.Round(image.Height * scale));
            preview = image.Clone(context => context.Resize(width, height));
        }

        using (preview)
        {
            ApplyPreviewMode(preview, SelectedPreviewMode);
            return ToBitmap(preview);
        }
    }

    private static void ApplyPreviewMode(
        SixLabors.ImageSharp.Image<Rgba32> image, string mode)
    {
        image.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < accessor.Height; y++)
            {
                Span<Rgba32> row = accessor.GetRowSpan(y);
                for (int x = 0; x < row.Length; x++)
                {
                    Rgba32 pixel = row[x];
                    row[x] = mode switch
                    {
                        "Только RGB" => new Rgba32(pixel.R, pixel.G, pixel.B, 255),
                        "Альфа-канал" => new Rgba32(pixel.A, pixel.A, pixel.A, 255),
                        "RGBA на шахматном фоне" => CompositeOnCheckerboard(pixel, x, y),
                        _ => pixel
                    };
                }
            }
        });
    }

    private static Rgba32 CompositeOnCheckerboard(Rgba32 pixel, int x, int y)
    {
        byte background = ((x / 12 + y / 12) & 1) == 0 ? (byte)72 : (byte)112;
        int alpha = pixel.A;
        return new Rgba32(
            (byte)((pixel.R * alpha + background * (255 - alpha)) / 255),
            (byte)((pixel.G * alpha + background * (255 - alpha)) / 255),
            (byte)((pixel.B * alpha + background * (255 - alpha)) / 255),
            255);
    }

    private void RefreshPreviews()
    {
        if (_document is null)
            return;

        try
        {
            foreach (TextureRowViewModel row in Rows)
            {
                using SixLabors.ImageSharp.Image<Rgba32> original =
                    _document.Decode(row.Texture);
                VertexColorBindingInfo? binding = null;
                if (SelectedPreviewMode == "Окраска модели")
                    _document.TryApplyVertexColors(
                        row.Texture, original, out binding);
                row.SetVertexColorBinding(binding);
                Bitmap originalPreview = ToPreviewBitmap(original);
                Bitmap? replacementPreview = null;
                if (row.ReplacementPath is not null)
                {
                    using SixLabors.ImageSharp.Image<Rgba32> replacement =
                        SixLabors.ImageSharp.Image.Load<Rgba32>(row.ReplacementPath);
                    if (SelectedPreviewMode == "Окраска модели")
                        _document.TryApplyVertexColors(row.Texture, replacement);
                    replacementPreview = ToPreviewBitmap(replacement);
                }
                row.UpdatePreviews(originalPreview, replacementPreview);
            }
            Status = $"Режим предпросмотра: {SelectedPreviewMode}.";
        }
        catch (Exception ex)
        {
            Status = $"Не удалось обновить предпросмотр: {ex.Message}";
        }
    }

    private void ClearRows()
    {
        foreach (TextureRowViewModel row in Rows)
            row.Dispose();
        Rows.Clear();
    }

    private void NotifyDocumentState()
    {
        OnPropertyChanged(nameof(FileName));
        OnPropertyChanged(nameof(Summary));
        OnPropertyChanged(nameof(HasDocument));
        OnPropertyChanged(nameof(HasReplacements));
    }

    private void NotifyReplacementState()
    {
        OnPropertyChanged(nameof(Summary));
        OnPropertyChanged(nameof(HasReplacements));
    }

    public new event PropertyChangedEventHandler? PropertyChanged;

    private bool SetField<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value))
            return false;
        field = value;
        OnPropertyChanged(name);
        return true;
    }

    private void OnPropertyChanged([CallerMemberName] string? name = null) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
}
