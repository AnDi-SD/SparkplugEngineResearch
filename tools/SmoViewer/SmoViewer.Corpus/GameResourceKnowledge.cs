using System.Buffers.Binary;
using System.Globalization;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Xml.Linq;
using SmoViewer.Core;

namespace SmoViewer.Corpus;

internal sealed record GameResourceFormatDefinition(
    string Key,
    string DisplayName,
    string Category,
    string Description,
    string DecodeStatus,
    string WriteStatus,
    string EvidenceStatus,
    string? Notes,
    IReadOnlyList<string> Extensions);

internal sealed record GameResourceVariantDefinition(
    string FormatKey,
    string ScopeKind,
    string ScopeKey,
    string Key,
    string DisplayName,
    string Status,
    string? DiscriminatorJson,
    string? Notes);

internal sealed record GameResourceEvidenceDefinition(
    string FormatKey,
    string? VariantKey,
    string? PlatformKey,
    string EvidenceKind,
    string SourcePath,
    string? Locator,
    string Observation,
    string Confidence);

internal sealed record GameResourceProperty(
    string Key,
    int Occurrence,
    string ValueKind,
    string ValueJson,
    string EvidenceStatus,
    string? Locator = null,
    string? Notes = null);

internal sealed record GameResourceSymbol(
    string Kind,
    int Ordinal,
    string Name,
    string? Locator,
    string EvidenceStatus);

internal sealed record GameResourceDependency(
    string Kind,
    string TargetPath,
    string TargetExtension,
    string EvidenceStatus,
    string Confidence,
    string? Locator,
    string? Notes = null);

internal sealed record GameResourceAnalysis(
    string FormatKey,
    string? VariantKey,
    string RecognitionMethod,
    string RecognitionStatus,
    string DecodeStatus,
    string? SummaryJson,
    string? Error,
    IReadOnlyList<GameResourceProperty> Properties,
    IReadOnlyList<GameResourceSymbol> Symbols,
    IReadOnlyList<GameResourceDependency> Dependencies);

internal static class GameResourceKnowledge
{
    public const int ParserRevision = 3;

    private static readonly Regex ResourceReferenceRegex = new(
        @"(?i)([A-Za-z0-9_./\\() -]+\.(?:smo|spt|spl|snc|anm|san|stx|wav|dds|sft|sps))",
        RegexOptions.CultureInvariant | RegexOptions.Compiled);

    private static readonly HashSet<string> PayloadExtensions = new(
        [
            ".smo", ".san", ".anm", ".spt", ".spl", ".stx", ".sft",
            ".snc", ".sps", ".ccc", ".wxd", ".wxs", ".wxt", ".dat",
            ".dds", ".txt", ".xml", ".xml~", ".lua", ".ini", ".bat",
            ".vsh", ".psh", ".rfx", ".exe", ".dll"
        ],
        StringComparer.OrdinalIgnoreCase);

    public static IReadOnlyList<GameResourceFormatDefinition> Formats { get; } =
    [
        new("unknown", "Unknown resource", "unknown",
            "Файл присутствует в корпусе, но его формат ещё не классифицирован.",
            "inventory_only", "unsupported", "observed_extension",
            "Назначение и структура не угадываются по содержимому.", []),
        new("directory", "Directory corpus", "container",
            "Обычный каталог ресурсов или дополнительных файлов установки.",
            "indexed", "not_applicable", "confirmed_scanner", null, []),
        new("pck", "Sparkplug PCK", "container",
            "PS2 stage-архив с секторными offsets и строковым каталогом.",
            "full_observed_read_only", "unsupported", "confirmed_corpus",
            "Reader проверяет offset=sector*0x800 и границы каждой записи.", [".pck"]),
        new("smo", "Sparkplug object resource", "scene_object_graph",
            "FFPS object graph: scene, model, material, texture, skin, collision и другие классы.",
            "full_observed_read_only", "supported_operations", "confirmed_corpus_runtime",
            "36/36 наблюдаемых SMO class ID имеют строгий decoder.", [".smo"]),
        new("san", "Sparkplug animation", "animation",
            "FFPS spAnimation с именованными transform tracks.",
            "pc_decoded_ps2_partial", "unsupported", "confirmed_pc_corpus_runtime",
            "PC timing и interpolation подтверждены; PS2 playback не закрыт.", [".san"]),
        new("anm", "Animation state table", "animation",
            "Текстовая восьмиколоночная таблица состояний со ссылкой на SAN.",
            "syntax_and_dependencies", "unsupported", "confirmed_disk_corpus",
            "Семантика первых семи колонок остаётся открытой.", [".anm"]),
        new("spt", "Sparkplug component template", "gameplay",
            "Бинарные игровые компоненты, свойства и ссылки на внешние ресурсы.",
            "partial", "unsupported", "confirmed_partial_corpus",
            "Полный component/runtime contract не восстановлен.", [".spt"]),
        new("spl", "Sparkplug placement list", "gameplay",
            "Бинарные placements и подключения SPT-шаблонов.",
            "partial", "unsupported", "confirmed_partial_corpus",
            "Полный placement contract не восстановлен.", [".spl"]),
        new("stx", "Standalone texture", "texture",
            "Standalone texture с несколькими несовместимыми PC/PS2 layouts.",
            "pc_partial_ps2_research", "unsupported", "confirmed_corpus",
            "PC имеет legacy, compact E0/E5 и raw20; PS2 swizzle не восстановлен.", [".stx"]),
        new("sft", "Sparkplug font", "font",
            "FFPS font resource.", "ffps_container_partial", "unsupported",
            "observed_corpus_and_class_decoder", null, [".sft"]),
        new("snc", "Sound control table", "audio_control",
            "Текстовые правила звуковых событий со ссылками на WAV.",
            "syntax_partial", "unsupported", "confirmed_corpus_observation", null, [".snc"]),
        new("sps", "Subtitle stream", "subtitle",
            "Бинарные интервалы, локализованные строки и индексы субтитров.",
            "partial_observation", "unsupported", "observed_corpus", null, [".sps"]),
        new("ccc", "Character color control", "gameplay_configuration",
            "Текстовая таблица RGB-коррекции Bloom по моделям и уровням.",
            "syntax_partial", "unsupported", "confirmed_corpus_observation", null, [".ccc"]),
        new("wxt", "Localized string table", "localization",
            "Offset table и нуль-терминированные локализованные строки.",
            "structural_corpus", "unsupported", "confirmed_corpus_structure", null, [".wxt"]),
        new("wxd", "String definition table", "localization",
            "Бинарные определения строковых сущностей.",
            "inventory_only", "unsupported", "observed_corpus", null, [".wxd"]),
        new("wxs", "Save/settings data", "save_data",
            "Сохранение игры или пользовательских настроек.",
            "inventory_only", "unsupported", "observed_corpus", null, [".wxs"]),
        new("dat", "OneLiner database", "gameplay_data",
            "Бинарная база OneLiner.", "inventory_only", "unsupported",
            "observed_corpus", null, [".dat"]),
        new("installer_data", "Installer data", "installation",
            "Служебные данные деинсталлятора, находящиеся рядом с игрой.",
            "inventory_only", "unsupported", "confirmed_path_and_signature", null, []),
        new("wav", "RIFF WAVE", "audio", "PC PCM/RIFF audio.",
            "external_standard", "external_standard", "confirmed_magic_and_extension", null, [".wav"]),
        new("mpeg_video", "MPEG video", "video", "MPEG-1 video stream.",
            "external_standard", "external_standard", "confirmed_extension", null, [".m1v", ".mpg"]),
        new("mpeg_audio", "MPEG audio", "audio", "MPEG layer-2 audio stream.",
            "external_standard", "external_standard", "confirmed_extension", null, [".mp2"]),
        new("dds", "DirectDraw Surface", "texture", "DDS texture used by fonts.",
            "external_standard", "external_standard", "confirmed_magic_and_extension", null, [".dds"]),
        new("vag", "PlayStation VAG audio", "audio", "PS2 audio resource.",
            "external_standard", "external_standard", "confirmed_extension", null, [".vag"]),
        new("mic", "PS2 MIC audio", "audio", "PS2 audio resource stored under Sounds.",
            "inventory_only", "unsupported", "observed_path_and_extension", null, [".mic"]),
        new("sb2", "PS2 sound bank", "audio", "PS2 sound-bank resource.",
            "inventory_only", "unsupported", "observed_path_and_extension", null, [".sb2"]),
        new("ps2_pss", "PlayStation 2 PSS video", "video",
            "PS2 movie stream stored under DATA/MOVIES.",
            "inventory_only", "unsupported", "observed_path_and_extension", null, [".pss"]),
        new("ps2_icon_metadata", "PlayStation 2 icon metadata", "platform_metadata",
            "PS2 ICON.SYS metadata file.", "inventory_only", "unsupported",
            "confirmed_filename_and_path", null, []),
        new("ps2_iop_image", "PlayStation 2 IOP module image", "runtime_module",
            "IOPRP module image stored under MODULES.", "inventory_only", "unsupported",
            "confirmed_filename_and_path", null, []),
        new("text", "Plain text", "text", "Plain text auxiliary resource.",
            "text", "text", "confirmed_content", null, [".txt"]),
        new("xml", "XML data", "configuration", "XML gameplay/template data.",
            "xml", "text", "confirmed_content", null, [".xml", ".xml~"]),
        new("lua", "Lua source", "script", "Lua source; runtime use is not established.",
            "text", "text", "observed_corpus", null, [".lua"]),
        new("ini", "INI configuration", "configuration", "Game/runtime configuration.",
            "text", "text", "confirmed_runtime", null, [".ini"]),
        new("system_config", "System configuration", "configuration",
            "Текстовая конфигурация запуска, включая PS2 SYSTEM.CNF.",
            "text", "text", "confirmed_content", null, [".cnf"]),
        new("batch", "Windows batch", "tooling", "Auxiliary batch script.",
            "text", "text", "confirmed_content", null, [".bat"]),
        new("shader_asm", "Direct3D shader assembly", "shader",
            "Textual Direct3D 8/9 vertex or pixel shader assembly.",
            "text", "text", "confirmed_content", null, [".vsh", ".psh"]),
        new("rendermonkey", "RenderMonkey workspace", "shader",
            "RenderMonkey XML workspace containing render states and HLSL.",
            "xml", "text", "confirmed_content", null, [".rfx"]),
        new("pe", "Portable Executable", "executable",
            "Windows PE executable or library.", "header_and_targeted_reverse",
            "signature_guarded_patches", "confirmed_binary_and_reverse", null, [".exe", ".dll"]),
        new("elf", "ELF executable", "executable",
            "ELF executable; the PS2 game uses ELF32 MIPS little-endian.",
            "header_and_targeted_reverse", "unsupported", "confirmed_binary_and_reverse", null, []),
        new("icon", "Windows icon", "image", "Windows ICO image.",
            "external_standard", "external_standard", "confirmed_extension", null, [".ico"])
    ];

    public static IReadOnlyList<GameResourceVariantDefinition> Variants { get; } =
    [
        new("smo", "common", "*", "ffps-object-graph", "FFPS SMO object graph",
            "confirmed", "{\"magic\":\"FFPS\",\"serializer\":38}", null),
        new("san", "platform", "pc", "pc-spanimation", "PC FFPS spAnimation",
            "confirmed", "{\"class\":\"spAnimation\"}", null),
        new("san", "platform", "ps2", "ps2-opaque-animation", "PS2 SAN",
            "research", null, "Полный PS2 playback/layout не подтверждён."),
        new("anm", "common", "*", "text-eight-columns", "Eight-column ANM table",
            "confirmed", "{\"columns\":8,\"terminator\":\"end\"}", null),
        new("stx", "platform", "pc", "legacy-tagged", "PC legacy/tagged BGRA8",
            "confirmed", "{\"prefix\":\"220000\"}", null),
        new("stx", "platform", "pc", "compact-e0-e5", "PC compact E0/E5",
            "confirmed", "{\"outer\":224,\"inner\":229}", null),
        new("stx", "platform", "pc", "raw20", "PC raw 20-byte header",
            "confirmed", "{\"headerBytes\":20}", null),
        new("stx", "platform", "ps2", "ps2-indexed", "PS2 indexed/swizzled",
            "research", "{\"prefix\":\"220000\"}", "Palette известна, deswizzle открыт."),
        new("sft", "common", "*", "ffps-font", "FFPS font resource",
            "observed", "{\"magic\":\"FFPS\"}", null),
        new("snc", "common", "*", "text-sound-controls", "Text sound controls",
            "observed", null, null),
        new("ccc", "common", "*", "text-rgb-table", "Text RGB table",
            "observed", null, null),
        new("wxt", "common", "*", "offset-string-table", "Count-prefixed offset string table",
            "confirmed", null, null),
        new("wxt", "common", "*", "self-delimiting-offset-table",
            "Self-delimiting offset string table", "confirmed", null, null),
        new("spt", "common", "*", "binary-components-partial", "Binary component records",
            "research", null, null),
        new("spl", "common", "*", "binary-placements-partial", "Binary placement records",
            "research", null, null),
        new("pe", "platform", "pc", "pe32-x86", "PE32 x86",
            "confirmed", "{\"machine\":332}", null),
        new("elf", "platform", "ps2", "elf32-mips-le", "ELF32 MIPS little-endian",
            "confirmed", "{\"class\":1,\"endianness\":1,\"machine\":8}", null)
    ];

    public static IReadOnlyList<GameResourceEvidenceDefinition> Evidence { get; } =
    [
        new("smo", null, null, "corpus_and_runtime",
            "docs/engine/README.md", "Движок Sparkplug",
            "Все 36 наблюдаемых SMO class ID структурно разобраны; поддерживаемые mutations проверяются native runtime.", "confirmed"),
        new("san", "pc-spanimation", "pc", "corpus_and_runtime",
            "docs/engine/README.md", "Движок Sparkplug",
            "PC SAN layout, timing, interpolation и name binding подтверждены.", "confirmed"),
        new("anm", "text-eight-columns", null, "full_disk_audit",
            "docs/formats/game-resources.md", "Игровые ресурсы помимо SMO",
            "Последняя колонка ANM ссылается на SAN; весь дисковый корпус проверен.", "confirmed"),
        new("spt", "binary-components-partial", null, "corpus_observation",
            "docs/formats/game-resources.md", "Игровые ресурсы помимо SMO",
            "SPT содержит игровые компоненты и ссылки на SMO; полный contract открыт.", "confirmed_partial"),
        new("spl", "binary-placements-partial", null, "corpus_observation",
            "docs/formats/game-resources.md", "Игровые ресурсы помимо SMO",
            "SPL ссылается на SPT и внешние gameplay templates.", "confirmed_partial"),
        new("stx", null, "pc", "corpus_specification",
            "docs/formats/stx.md", "Важная граница",
            "В PC-корпусе наблюдаются legacy/tagged, compact E0/E5 и raw20 layouts.", "confirmed"),
        new("stx", "ps2-indexed", "ps2", "sample_structure",
            "docs/formats/stx.md", "PS2 — отдельная задача",
            "PS2 STX использует palette/index payload; точный deswizzle не восстановлен.", "confirmed_partial"),
        new("pck", null, "ps2", "full_container_inventory",
            "docs/formats/pck.md", "Контейнер PCK",
            "78 PCK и 12 229 записей прошли проверку строковой таблицы и границ.", "confirmed"),
        new("pe", "pe32-x86", "pc", "static_and_runtime_reverse",
            "docs/engine/platforms/pc-and-ps2.md", "PC и PS2",
            "PC executable исследован выборочно: loader, resources, debug menu, hair и display paths.", "confirmed_partial"),
        new("elf", "elf32-mips-le", "ps2", "static_reverse",
            "docs/engine/platforms/pc-and-ps2.md", "PC и PS2",
            "SLES_532.19 подтверждает Sparkplug registrations и serializer paths.", "confirmed_partial")
    ];

    private static readonly IReadOnlyDictionary<string, string> ExtensionFormats =
        Formats.SelectMany(format => format.Extensions.Select(extension => (extension, format.Key)))
            .ToDictionary(item => item.extension, item => item.Key, StringComparer.OrdinalIgnoreCase);

    public static bool RequiresPayload(string extension) =>
        PayloadExtensions.Contains(extension);

    public static GameResourceAnalysis Analyze(
        string platformKey,
        string logicalPath,
        byte[]? data)
    {
        string extension = Path.GetExtension(logicalPath).ToLowerInvariant();
        string formatKey = DetectExecutableFormat(extension, data) ??
                           DetectPathSpecificFormat(logicalPath, extension) ??
                           ExtensionFormats.GetValueOrDefault(extension, "unknown");
        var properties = new List<GameResourceProperty>
        {
            Property("path.family", GetFamily(logicalPath), "string", "confirmed_path"),
            Property("path.extension", extension, "string", "confirmed_path")
        };
        var symbols = new List<GameResourceSymbol>();
        var dependencies = new List<GameResourceDependency>();

        try
        {
            return formatKey switch
            {
                "smo" => AnalyzeFfpsResource(formatKey, "ffps-object-graph", data,
                    properties, symbols, dependencies),
                "san" => AnalyzeSan(platformKey, logicalPath, data,
                    properties, symbols, dependencies),
                "sft" => AnalyzeFfpsResource(formatKey, "ffps-font", data,
                    properties, symbols, dependencies),
                "anm" => AnalyzeAnm(data, properties, symbols, dependencies),
                "snc" => AnalyzeSnc(data, properties, symbols, dependencies),
                "ccc" => AnalyzeCcc(data, properties, symbols, dependencies),
                "wxt" => AnalyzeWxt(data, properties, symbols, dependencies),
                "stx" => AnalyzeStx(platformKey, data, properties, symbols, dependencies),
                "spt" or "spl" => AnalyzeComponentBinary(
                    formatKey, data, properties, symbols, dependencies),
                "dds" => AnalyzeDds(data, properties, symbols, dependencies),
                "xml" or "rendermonkey" => AnalyzeXml(
                    formatKey, data, properties, symbols, dependencies),
                "text" or "lua" or "ini" or "system_config" or "batch" or "shader_asm" =>
                    AnalyzeText(formatKey, data, properties, symbols, dependencies),
                "pe" => AnalyzePe(data, properties, symbols, dependencies),
                "elf" => AnalyzeElf(data, properties, symbols, dependencies),
                "sps" => Partial(formatKey, null, "extension_and_corpus",
                    "confirmed_partial", properties, symbols, dependencies,
                    "Бинарная структура субтитров только частично наблюдалась."),
                "wxd" or "wxs" or "dat" or "mic" or "sb2" or "ps2_pss" or
                    "ps2_icon_metadata" or "ps2_iop_image" =>
                    Inventory(formatKey, properties, symbols, dependencies),
                _ => ExternalOrInventory(formatKey, properties, symbols, dependencies)
            };
        }
        catch (Exception exception) when (exception is InvalidDataException or
                                           IOException or OverflowException or
                                           DecoderFallbackException or
                                           System.Xml.XmlException)
        {
            return new GameResourceAnalysis(
                formatKey, null, "content_parser", "recognized", "error", null,
                exception.Message, properties, symbols, dependencies);
        }
    }

    private static GameResourceAnalysis AnalyzeFfpsResource(
        string formatKey,
        string variant,
        byte[]? data,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies)
    {
        if (data is null)
            return Inventory(formatKey, properties, symbols, dependencies);
        SmoDocument document = SmoDocument.ParseOwned(data, formatKey);
        properties.Add(Property("ffps.serializer_version", document.Header.SerializerVersion,
            "uint32", "confirmed_parser", "0x04"));
        properties.Add(Property("ffps.unknown08", document.Header.Unknown08,
            "uint32", "observed_unknown", "0x08"));
        properties.Add(Property("ffps.platform_mask", document.Header.PlatformMask,
            "uint32", "confirmed_parser", "0x10"));
        properties.Add(Property("ffps.object_count", document.Objects.Count,
            "integer", "confirmed_parser"));
        string summary = JsonSerializer.Serialize(new
        {
            document.Header.SerializerVersion,
            document.Header.Unknown08,
            document.Header.PlatformMask,
            ObjectCount = document.Objects.Count
        });
        return new(formatKey, variant, "ffps_magic_and_strict_parser", "confirmed",
            "ok", summary, null, properties, symbols, dependencies);
    }

    private static GameResourceAnalysis AnalyzeSan(
        string platformKey,
        string logicalPath,
        byte[]? data,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies)
    {
        if (platformKey == "ps2")
            return Partial("san", "ps2-opaque-animation", "extension_and_platform",
                "observed", properties, symbols, dependencies,
                "PS2 SAN сохранён без применения PC decoder.");
        string error = string.Empty;
        if (data is null)
            throw new InvalidDataException("PC SAN payload is unavailable.");
        if (!SmoAnimationDecoder.TryDecode(
                data, logicalPath, out SmoAnimationClip? clip, out error) || clip is null)
        {
            SmoDocument document = SmoDocument.ParseOwned(data, logicalPath);
            properties.Add(Property("ffps.object_count", document.Objects.Count,
                "integer", "confirmed_parser"));
            properties.Add(Property("animation.decode_limit", error,
                "string", "confirmed_parser_diagnostic"));
            return new("san", "pc-spanimation", "ffps_parser_animation_decoder",
                "confirmed", "partial",
                JsonSerializer.Serialize(new { document.Objects.Count, DecoderDiagnostic = error }),
                null, properties, symbols, dependencies);
        }
        int positionKeys = clip.Tracks.Sum(track => track.Positions.Count);
        int rotationKeys = clip.Tracks.Sum(track => track.Rotations.Count);
        int scaleKeys = clip.Tracks.Sum(track => track.Scales.Count);
        properties.Add(Property("animation.duration_seconds", clip.Duration,
            "float", "confirmed_parser"));
        properties.Add(Property("animation.track_count", clip.Tracks.Count,
            "integer", "confirmed_parser"));
        properties.Add(Property("animation.position_key_count", positionKeys,
            "integer", "confirmed_parser"));
        properties.Add(Property("animation.rotation_key_count", rotationKeys,
            "integer", "confirmed_parser"));
        properties.Add(Property("animation.scale_key_count", scaleKeys,
            "integer", "confirmed_parser"));
        for (int i = 0; i < clip.Tracks.Count; i++)
            symbols.Add(new("animation_track", i, clip.Tracks[i].NodeName,
                $"track[{i}]", "confirmed_parser"));
        return new("san", "pc-spanimation", "ffps_spanimation_decoder", "confirmed",
            "ok", JsonSerializer.Serialize(new
            {
                clip.Duration,
                TrackCount = clip.Tracks.Count,
                PositionKeys = positionKeys,
                RotationKeys = rotationKeys,
                ScaleKeys = scaleKeys
            }), null, properties, symbols, dependencies);
    }

    private static GameResourceAnalysis AnalyzeAnm(
        byte[]? data,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies)
    {
        string text = DecodeText(data);
        int rows = 0;
        bool hasEnd = false;
        var targets = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        int lineNumber = 0;
        foreach (string rawLine in SplitLines(text))
        {
            lineNumber++;
            string line = rawLine.Trim();
            if (line.Length == 0 || line.StartsWith('#'))
                continue;
            line = line.TrimEnd(';').Trim();
            string[] fields = line.Split(',').Select(field => field.Trim()).ToArray();
            if (fields[0].Equals("end", StringComparison.OrdinalIgnoreCase))
            {
                hasEnd = true;
                continue;
            }
            if (fields.Length != 8)
                throw new InvalidDataException($"ANM line {lineNumber} has {fields.Length} fields; expected 8.");
            if (!fields[7].EndsWith(".san", StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException($"ANM line {lineNumber} has no SAN in column 8.");
            rows++;
            if (targets.Add(fields[7]))
            {
                dependencies.Add(new("animation_clip", fields[7], ".san",
                    "confirmed_syntax", "confirmed", $"line:{lineNumber}"));
            }
        }
        if (!hasEnd)
            throw new InvalidDataException("ANM terminator row 'end' is missing.");
        properties.Add(Property("anm.row_count", rows, "integer", "confirmed_parser"));
        properties.Add(Property("anm.unique_san_count", targets.Count, "integer", "confirmed_parser"));
        properties.Add(Property("anm.has_end_row", true, "boolean", "confirmed_parser"));
        return new("anm", "text-eight-columns", "strict_text_parser", "confirmed",
            "ok", JsonSerializer.Serialize(new { Rows = rows, SanReferences = targets.Count }),
            null, properties, symbols, dependencies);
    }

    private static GameResourceAnalysis AnalyzeSnc(
        byte[]? data,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies)
    {
        string text = DecodeText(data);
        int rows = 0;
        var wav = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        int lineNumber = 0;
        foreach (string rawLine in SplitLines(text))
        {
            lineNumber++;
            string line = rawLine.Trim();
            if (line.Length == 0 || line.StartsWith('#'))
                continue;
            rows++;
            foreach (string token in line.TrimEnd(';').Split(','))
            {
                string value = token.Trim().TrimEnd(';', '#').Trim();
                if (!value.EndsWith(".wav", StringComparison.OrdinalIgnoreCase) ||
                    !wav.Add(value))
                    continue;
                dependencies.Add(new("audio_sample", value, ".wav",
                    "confirmed_text_token", "confirmed", $"line:{lineNumber}"));
            }
        }
        properties.Add(Property("snc.row_count", rows, "integer", "confirmed_parser"));
        properties.Add(Property("snc.unique_wav_count", wav.Count, "integer", "confirmed_parser"));
        return new("snc", "text-sound-controls", "text_rows_and_wav_tokens",
            "confirmed", "ok", JsonSerializer.Serialize(new { Rows = rows, Wav = wav.Count }),
            null, properties, symbols, dependencies);
    }

    private static GameResourceAnalysis AnalyzeCcc(
        byte[]? data,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies)
    {
        string text = DecodeText(data);
        int rows = SplitLines(text).Count(line =>
            line.Trim().Length > 0 && !line.TrimStart().StartsWith('#'));
        properties.Add(Property("ccc.row_count", rows, "integer", "confirmed_parser"));
        return new("ccc", "text-rgb-table", "text_table", "confirmed", "ok",
            JsonSerializer.Serialize(new { Rows = rows }), null,
            properties, symbols, dependencies);
    }

    private static GameResourceAnalysis AnalyzeWxt(
        byte[]? data,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies)
    {
        if (data is null || data.Length < 8)
            throw new InvalidDataException("WXT is too short.");
        int first = checked((int)BinaryPrimitives.ReadUInt32LittleEndian(data));
        int count;
        int offsetTableStart;
        int headerBytes;
        string variant;
        int countPrefixedHeader = first >= 0 && first <= (data.Length - 4) / 4
            ? checked(4 + first * 4)
            : -1;
        if (countPrefixedHeader >= 4 && first > 0 &&
            BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(4)) == countPrefixedHeader)
        {
            count = first;
            offsetTableStart = 4;
            headerBytes = countPrefixedHeader;
            variant = "offset-string-table";
        }
        else if (first >= 4 && first <= data.Length && first % 4 == 0)
        {
            count = first / 4;
            offsetTableStart = 0;
            headerBytes = first;
            variant = "self-delimiting-offset-table";
        }
        else
        {
            throw new InvalidDataException("WXT has no recognized offset-table header.");
        }
        int previous = -1;
        var offsets = new int[count];
        for (int i = 0; i < count; i++)
        {
            int offset = checked((int)BinaryPrimitives.ReadUInt32LittleEndian(
                data.AsSpan(offsetTableStart + i * 4)));
            if (offset < headerBytes || offset > data.Length || offset < previous)
                throw new InvalidDataException($"WXT offset {i} is invalid: {offset}.");
            offsets[i] = offset;
            previous = offset;
        }
        for (int i = 0; i < count; i++)
        {
            int end = i + 1 < count ? offsets[i + 1] : data.Length;
            int length = Math.Max(0, end - offsets[i]);
            ReadOnlySpan<byte> bytes = data.AsSpan(offsets[i], length);
            int zero = bytes.IndexOf((byte)0);
            if (zero >= 0) bytes = bytes[..zero];
            symbols.Add(new("localized_string", i,
                Encoding.Latin1.GetString(bytes), $"offset:{offsets[i]}",
                "confirmed_parser"));
        }
        properties.Add(Property("wxt.string_count", count, "integer", "confirmed_parser"));
        properties.Add(Property("wxt.table_layout", variant, "string", "confirmed_parser"));
        return new("wxt", variant, "strict_offset_table", "confirmed",
            "ok", JsonSerializer.Serialize(new { Strings = count, Variant = variant }), null,
            properties, symbols, dependencies);
    }

    private static GameResourceAnalysis AnalyzeStx(
        string platformKey,
        byte[]? data,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies)
    {
        if (data is null || data.Length < 20)
            throw new InvalidDataException("STX is too short.");
        string variant;
        string status;
        if (data[0] == 0x22 && data[1] == 0 && data[2] == 0)
        {
            variant = platformKey == "ps2" ? "ps2-indexed" : "legacy-tagged";
            status = platformKey == "ps2" ? "partial" : "ok";
        }
        else if (data[0] == 0xE0 && data.Length >= 26 && data[5] == 0xE5)
        {
            variant = "compact-e0-e5";
            uint width = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(10));
            uint height = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(14));
            uint bytesPerPixel = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(22));
            properties.Add(Property("texture.width", width, "uint32", "confirmed_parser", "0x0A"));
            properties.Add(Property("texture.height", height, "uint32", "confirmed_parser", "0x0E"));
            properties.Add(Property("texture.bytes_per_pixel", bytesPerPixel, "uint32", "confirmed_parser", "0x16"));
            status = "ok";
        }
        else
        {
            uint width = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(4));
            uint height = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(8));
            uint bytesPerPixel = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(16));
            long expected = 20L + (long)width * height * bytesPerPixel;
            if (data[0] != 0 || bytesPerPixel != 4 || expected != data.Length)
                throw new InvalidDataException("STX does not match an observed layout.");
            variant = "raw20";
            properties.Add(Property("texture.width", width, "uint32", "confirmed_parser", "0x04"));
            properties.Add(Property("texture.height", height, "uint32", "confirmed_parser", "0x08"));
            properties.Add(Property("texture.bytes_per_pixel", bytesPerPixel, "uint32", "confirmed_parser", "0x10"));
            status = "ok";
        }
        return new("stx", variant, "content_layout", "confirmed", status,
            JsonSerializer.Serialize(new { Variant = variant }), null,
            properties, symbols, dependencies);
    }

    private static GameResourceAnalysis AnalyzeComponentBinary(
        string formatKey,
        byte[]? data,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies)
    {
        if (data is null)
            return Inventory(formatKey, properties, symbols, dependencies);
        var references = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        string latin = Encoding.Latin1.GetString(data);
        foreach (Match match in ResourceReferenceRegex.Matches(latin))
        {
            string target = match.Groups[1].Value.Trim(' ', '\0', '\u00CD');
            if (!references.Add(target)) continue;
            string extension = Path.GetExtension(target).ToLowerInvariant();
            dependencies.Add(new(RelationForExtension(extension), target, extension,
                "observed_binary_string", "probable", $"byte:{match.Index}",
                "Ссылка извлечена из строкового payload; owning field ещё не восстановлен."));
        }
        int ordinal = 0;
        foreach (string value in ExtractNullTerminatedStrings(data))
        {
            if (!Regex.IsMatch(value, @"(?i)^wx[A-Za-z0-9_]+\s+-\s+\d+$"))
                continue;
            symbols.Add(new(formatKey == "spt" ? "component_instance" : "placement_instance",
                ordinal++, value, null, "observed_binary_string"));
        }
        properties.Add(Property($"{formatKey}.observed_reference_count", references.Count,
            "integer", "observed_binary_strings"));
        properties.Add(Property($"{formatKey}.observed_instance_name_count", symbols.Count,
            "integer", "observed_binary_strings"));
        string variant = formatKey == "spt" ? "binary-components-partial" :
            "binary-placements-partial";
        return new(formatKey, variant, "extension_and_binary_strings", "confirmed_partial",
            "partial", JsonSerializer.Serialize(new
            {
                ObservedReferences = references.Count,
                ObservedInstances = symbols.Count
            }), null, properties, symbols, dependencies);
    }

    private static GameResourceAnalysis AnalyzeDds(
        byte[]? data,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies)
    {
        if (data is null || data.Length < 128 || !data.AsSpan(0, 4).SequenceEqual("DDS "u8))
            throw new InvalidDataException("DDS magic/header is invalid.");
        properties.Add(Property("texture.height",
            BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(12)), "uint32", "confirmed_standard", "0x0C"));
        properties.Add(Property("texture.width",
            BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(16)), "uint32", "confirmed_standard", "0x10"));
        return new("dds", null, "dds_magic", "confirmed", "external_standard",
            null, null, properties, symbols, dependencies);
    }

    private static GameResourceAnalysis AnalyzeXml(
        string formatKey,
        byte[]? data,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies)
    {
        string text = DecodeText(data);
        properties.Add(Property("text.line_count", SplitLines(text).Length,
            "integer", "confirmed_parser"));
        try
        {
            XDocument document = XDocument.Parse(text, LoadOptions.PreserveWhitespace);
            properties.Add(Property("xml.root", document.Root?.Name.LocalName ?? string.Empty,
                "string", "confirmed_parser"));
            return new(formatKey, null, "xml_parser", "confirmed", "ok", null, null,
                properties, symbols, dependencies);
        }
        catch (System.Xml.XmlException exception)
        {
            properties.Add(Property("xml.strict_parse_diagnostic", exception.Message,
                "string", "confirmed_parser_diagnostic"));
            return new(formatKey, null, "xml_like_text", "confirmed_extension_and_text",
                "partial", JsonSerializer.Serialize(new { StrictXml = false }), null,
                properties, symbols, dependencies);
        }
    }

    private static GameResourceAnalysis AnalyzeText(
        string formatKey,
        byte[]? data,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies)
    {
        string text = DecodeText(data);
        properties.Add(Property("text.line_count", SplitLines(text).Length,
            "integer", "confirmed_parser"));
        properties.Add(Property("text.encoding", "single-byte/UTF-8-compatible",
            "string", "observed_decoder"));
        return new(formatKey, null, "text_decode", "confirmed", "ok", null, null,
            properties, symbols, dependencies);
    }

    private static GameResourceAnalysis AnalyzePe(
        byte[]? data,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies)
    {
        if (data is null || data.Length < 0x40 || data[0] != 'M' || data[1] != 'Z')
            throw new InvalidDataException("PE DOS header is missing.");
        int peOffset = BinaryPrimitives.ReadInt32LittleEndian(data.AsSpan(0x3C));
        if (peOffset < 0 || peOffset > data.Length - 24 ||
            !data.AsSpan(peOffset, 4).SequenceEqual(new byte[] { (byte)'P', (byte)'E', 0, 0 }))
            throw new InvalidDataException("PE signature is missing.");
        ushort machine = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(peOffset + 4));
        ushort sections = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(peOffset + 6));
        properties.Add(Property("executable.machine", machine, "uint16", "confirmed_header",
            $"0x{peOffset + 4:X}"));
        properties.Add(Property("executable.section_count", sections, "uint16", "confirmed_header",
            $"0x{peOffset + 6:X}"));
        string variant = machine == 0x14C ? "pe32-x86" : null!;
        return new("pe", variant, "pe_header", "confirmed", "ok",
            JsonSerializer.Serialize(new { Machine = machine, Sections = sections }), null,
            properties, symbols, dependencies);
    }

    private static GameResourceAnalysis AnalyzeElf(
        byte[]? data,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies)
    {
        if (data is null || data.Length < 52 || data[0] != 0x7F || data[1] != 'E' ||
            data[2] != 'L' || data[3] != 'F')
            throw new InvalidDataException("ELF header is missing.");
        byte elfClass = data[4];
        byte endian = data[5];
        ushort machine = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(18));
        uint entry = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(24));
        properties.Add(Property("executable.elf_class", elfClass, "byte", "confirmed_header", "0x04"));
        properties.Add(Property("executable.endianness", endian, "byte", "confirmed_header", "0x05"));
        properties.Add(Property("executable.machine", machine, "uint16", "confirmed_header", "0x12"));
        properties.Add(Property("executable.entry_point", entry, "uint32", "confirmed_header", "0x18"));
        return new("elf", machine == 8 ? "elf32-mips-le" : null,
            "elf_header", "confirmed", "ok",
            JsonSerializer.Serialize(new { Class = elfClass, Endian = endian, Machine = machine, Entry = entry }),
            null, properties, symbols, dependencies);
    }

    private static GameResourceAnalysis Partial(
        string formatKey,
        string? variant,
        string method,
        string recognition,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies,
        string note)
    {
        properties.Add(Property("analysis.known_limit", note, "string", "documented_limit"));
        return new(formatKey, variant, method, recognition, "partial", null, null,
            properties, symbols, dependencies);
    }

    private static GameResourceAnalysis Inventory(
        string formatKey,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies) =>
        new(formatKey, null, "extension_and_corpus_path", "observed",
            "inventory_only", null, null, properties, symbols, dependencies);

    private static GameResourceAnalysis ExternalOrInventory(
        string formatKey,
        List<GameResourceProperty> properties,
        List<GameResourceSymbol> symbols,
        List<GameResourceDependency> dependencies)
    {
        GameResourceFormatDefinition definition = Formats.First(item => item.Key == formatKey);
        string status = definition.DecodeStatus == "external_standard"
            ? "external_standard" : "inventory_only";
        return new(formatKey, null, "extension", "observed", status,
            null, null, properties, symbols, dependencies);
    }

    private static GameResourceProperty Property(
        string key,
        object value,
        string kind,
        string evidence,
        string? locator = null,
        string? notes = null,
        int occurrence = 0) =>
        new(key, occurrence, kind, JsonSerializer.Serialize(value), evidence, locator, notes);

    private static string DecodeText(byte[]? data)
    {
        if (data is null)
            throw new InvalidDataException("Text payload is unavailable.");
        if (data.AsSpan().StartsWith(new byte[] { 0xEF, 0xBB, 0xBF }))
            return Encoding.UTF8.GetString(data, 3, data.Length - 3);
        return Encoding.Latin1.GetString(data);
    }

    private static string[] SplitLines(string text) =>
        text.Replace("\r\n", "\n", StringComparison.Ordinal)
            .Replace('\r', '\n').Split('\n');

    private static IEnumerable<string> ExtractNullTerminatedStrings(byte[] data)
    {
        int start = 0;
        for (int i = 0; i <= data.Length; i++)
        {
            if (i < data.Length && data[i] != 0) continue;
            int length = i - start;
            if (length >= 4)
            {
                ReadOnlySpan<byte> value = data.AsSpan(start, length);
                if (IsPrintableAscii(value))
                    yield return Encoding.ASCII.GetString(value);
            }
            start = i + 1;
        }
    }

    private static bool IsPrintableAscii(ReadOnlySpan<byte> value)
    {
        foreach (byte item in value)
        {
            if (item is < 0x20 or > 0x7E)
                return false;
        }
        return true;
    }

    private static string RelationForExtension(string extension) => extension switch
    {
        ".smo" => "object_resource",
        ".spt" => "component_template",
        ".spl" => "placement_list",
        ".snc" => "sound_controls",
        ".anm" => "animation_state_table",
        ".san" => "animation_clip",
        ".stx" or ".dds" => "texture",
        ".wav" => "audio_sample",
        ".sft" => "font",
        ".sps" => "subtitle_stream",
        _ => "resource_reference"
    };

    private static string? DetectExecutableFormat(string extension, byte[]? data)
    {
        if (data is { Length: >= 4 } && data[0] == 0x7F && data[1] == 'E' &&
            data[2] == 'L' && data[3] == 'F')
            return "elf";
        if (data is { Length: >= 2 } && data[0] == 'M' && data[1] == 'Z')
            return "pe";
        return null;
    }

    private static string? DetectPathSpecificFormat(string logicalPath, string extension)
    {
        string normalized = logicalPath.Replace('\\', '/');
        if (normalized.StartsWith("@game/", StringComparison.OrdinalIgnoreCase) &&
            extension.Equals(".dat", StringComparison.OrdinalIgnoreCase) &&
            Path.GetFileName(normalized).StartsWith("unins", StringComparison.OrdinalIgnoreCase))
            return "installer_data";
        if (normalized.Equals("@game/DATA/ICONS/ICON.SYS",
                StringComparison.OrdinalIgnoreCase))
            return "ps2_icon_metadata";
        if (normalized.StartsWith("@game/MODULES/", StringComparison.OrdinalIgnoreCase) &&
            extension.Equals(".img", StringComparison.OrdinalIgnoreCase) &&
            Path.GetFileName(normalized).StartsWith("IOPRP", StringComparison.OrdinalIgnoreCase))
            return "ps2_iop_image";
        return null;
    }

    private static string GetFamily(string path)
    {
        string normalized = path.Replace('\\', '/').TrimStart('/');
        int separator = normalized.IndexOf('/');
        return separator < 0 ? "." : normalized[..separator];
    }
}
