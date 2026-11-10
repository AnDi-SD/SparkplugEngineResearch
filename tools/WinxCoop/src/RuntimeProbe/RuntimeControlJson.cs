using System.Reflection;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace WinxCoop.RuntimeProbe;

// Local typed control records only, never a network codec. NET6's serializer
// has no UnmappedMemberHandling, so validate member names before using the same
// serializer for values, constructors, numeric ranges and null handling.
internal static class RuntimeControlJson
{
    internal static JsonSerializerOptions CreateOptions(int maximumDepth) => new()
    {
        MaxDepth = maximumDepth,
#if !NET6_0
        UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow
#endif
    };

    internal static T? Deserialize<T>(byte[] bytes, JsonSerializerOptions options)
    {
#if NET6_0
        using JsonDocument document = JsonDocument.Parse(bytes, new JsonDocumentOptions { MaxDepth = options.MaxDepth });
        ValidateMembers(document.RootElement, typeof(T), options);
#endif
        return JsonSerializer.Deserialize<T>(bytes, options);
    }

#if NET6_0
    private static void ValidateMembers(JsonElement element, Type declaredType, JsonSerializerOptions options)
    {
        Type type = Nullable.GetUnderlyingType(declaredType) ?? declaredType;
        if (element.ValueKind is JsonValueKind.Null || type.IsPrimitive || type.IsEnum ||
            type == typeof(string) || type == typeof(decimal)) return;
        if (element.ValueKind != JsonValueKind.Object)
            throw new JsonException("Expected a typed local control object.");

        var properties = new Dictionary<string, Type>(options.PropertyNameCaseInsensitive
            ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
        foreach (PropertyInfo property in type.GetProperties(BindingFlags.Instance | BindingFlags.Public))
        {
            if (property.GetIndexParameters().Length != 0 || property.GetMethod is null ||
                property.GetCustomAttribute<JsonIgnoreAttribute>()?.Condition == JsonIgnoreCondition.Always) continue;
            if (property.GetCustomAttribute<JsonExtensionDataAttribute>() is not null)
                throw new NotSupportedException("Extension data is not admitted for local control records.");
            string name = property.GetCustomAttribute<JsonPropertyNameAttribute>()?.Name ??
                options.PropertyNamingPolicy?.ConvertName(property.Name) ?? property.Name;
            properties.Add(name, property.PropertyType);
        }
        foreach (JsonProperty property in element.EnumerateObject())
        {
            if (!properties.TryGetValue(property.Name, out Type? propertyType))
                throw new JsonException("Unmapped local control member: " + property.Name);
            ValidateMembers(property.Value, propertyType, options);
        }
    }
#endif
}
