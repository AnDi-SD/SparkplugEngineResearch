using System.Globalization;
using System.Text.Json;

namespace SmoLVLcreator.Core;

/// <summary>Editor messages, independent of the culture used for numbers and file formats.</summary>
public static class LevelText
{
    public const string Auto = "Auto";
    public const string Russian = "Russian";
    public const string English = "English";
    public const string WorkerLanguageVariable = "SMOLVLCREATOR_WORKER_LANGUAGE";
    private const string LabelPrefix = "\u001fSMOLVL:";

    private static readonly Lazy<Dictionary<string, string>> En = new(() => Read("en"));
    private static readonly Lazy<Dictionary<string, string>> Ru = new(() => Read("ru"));

    public static string Preference { get; private set; } = Auto;
    public static string Language { get; private set; } = English;
    public static IEnumerable<string> Keys => En.Value.Keys;

    public static string Normalize(string? preference) => preference switch
    {
        Russian => Russian,
        English => English,
        _ => Auto
    };

    public static string ResolveLanguage(string? preference, CultureInfo systemUiCulture) =>
        Normalize(preference) switch
        {
            Auto => systemUiCulture.TwoLetterISOLanguageName == "ru" ? Russian : English,
            string explicitLanguage => explicitLanguage
        };

    public static void Initialize(string? preference = null)
    {
        Preference = Normalize(preference);
        Language = ResolveLanguage(Preference, CultureInfo.CurrentUICulture);
    }

    public static string Get(string key, params object?[] args)
    {
        string template = Language == Russian && Ru.Value.TryGetValue(key, out string? translated)
            ? translated
            : En.Value.GetValueOrDefault(key, key);
        return args.Length == 0 ? template : string.Format(CultureInfo.CurrentCulture, template, args);
    }

    // Session journals keep string labels for existing Core consumers. Store the key and
    // snapshot of its arguments so earlier operations are displayed in the active language.
    public static string Label(string key, params object?[] args) =>
        LabelPrefix + JsonSerializer.Serialize(new DeferredLabel(key, args));

    public static string Display(string? label)
    {
        if (label is null)
            return string.Empty;
        if (!label.StartsWith(LabelPrefix, StringComparison.Ordinal))
            return label;

        try
        {
            DeferredLabel? value = JsonSerializer.Deserialize<DeferredLabel>(label[LabelPrefix.Length..]);
            return value is null ? label : Get(value.Key, value.Args.Select(static item => item is JsonElement json
                ? ReadArgument(json)
                : item).ToArray());
        }
        catch (JsonException)
        {
            return label;
        }
    }

    // Shared components and Windows can return free-form technical diagnostics. Preserve
    // that detail verbatim; localize structured editor messages at their display boundary.
    public static string Error(string? message) => Display(message);

    private static object? ReadArgument(JsonElement item) => item.ValueKind switch
    {
        JsonValueKind.Number when item.TryGetInt64(out long integer) => integer,
        JsonValueKind.Number => item.GetDouble(),
        JsonValueKind.String => item.GetString(),
        JsonValueKind.Null => null,
        _ => item.ToString()
    };

    private static Dictionary<string, string> Read(string language)
    {
        string name = $"SmoLVLcreator.Strings.{language}.json";
        using Stream stream = typeof(LevelText).Assembly.GetManifestResourceStream(name)
            ?? throw new InvalidOperationException(name);
        return JsonSerializer.Deserialize<Dictionary<string, string>>(stream)
            ?? throw new InvalidDataException(name);
    }

    private sealed record DeferredLabel(string Key, object?[] Args);
}
