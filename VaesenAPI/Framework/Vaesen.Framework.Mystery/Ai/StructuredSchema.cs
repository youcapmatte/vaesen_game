using System.Collections.Concurrent;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Schema;
using System.Text.Json.Serialization;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Ai;

/// <summary>
/// The JSON schema of a step's output type, in the form structured outputs accept: every object closed
/// (additionalProperties false) with every property required, nullable values as anyOf with null, and no keywords the
/// API does not support. Property names are the camelCase names the models' [JsonProperty] attributes use.
/// </summary>
public static class StructuredSchema
{
    private static readonly ConcurrentDictionary<Type, JsonObject> Cache = new();

    private static readonly JsonSerializerOptions Options = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        RespectNullableAnnotations = true,
        NumberHandling = JsonNumberHandling.Strict,
        TypeInfoResolver = new System.Text.Json.Serialization.Metadata.DefaultJsonTypeInfoResolver(),
    };

    private static readonly string[] Unsupported =
        ["minimum", "maximum", "exclusiveMinimum", "exclusiveMaximum", "multipleOf", "minLength", "maxLength", "pattern", "minItems", "maxItems", "uniqueItems", "default", "$schema"];

    public static JsonObject For<T>() => For(typeof(T));

    public static JsonObject For(Type type) => (JsonObject)Cache.GetOrAdd(type, Build).DeepClone();

    private static JsonObject Build(Type type)
    {
        var node = Options.GetJsonSchemaAsNode(type, new JsonSchemaExporterOptions
        {
            TreatNullObliviousAsNonNullable = true,
            TransformSchemaNode = AddAllowedValues,
        });
        var schema = node as JsonObject ?? throw new InvalidOperationException($"No object schema for {type.Name}.");
        HoistReferences(schema);
        Normalize(schema);
        return schema;
    }

    /// <summary>A property marked <see cref="OneOfAttribute"/> gets its values as an enum.</summary>
    private static JsonNode AddAllowedValues(JsonSchemaExporterContext context, JsonNode node)
    {
        var oneOf = context.PropertyInfo?.AttributeProvider?.GetCustomAttributes(typeof(OneOfAttribute), inherit: true).OfType<OneOfAttribute>().FirstOrDefault();
        if (oneOf is not null && node is JsonObject schema)
        {
            schema["enum"] = new JsonArray(oneOf.Values.Select(v => (JsonNode)JsonValue.Create(v)!).ToArray());
        }

        return node;
    }

    /// <summary>
    /// The exporter writes a type's second use as a $ref to its first, e.g. "#/properties/core/items". Structured outputs
    /// only take references into $defs, so every referenced schema moves there: its first use and every reference then
    /// point to "#/$defs/{name}". Done before <see cref="Normalize"/>, which moves nodes and would break the pointers.
    /// </summary>
    private static void HoistReferences(JsonObject schema)
    {
        var targets = new List<string>();
        CollectReferences(schema, targets);
        targets = targets.Where(t => t != "#" && !t.StartsWith("#/$defs/", StringComparison.Ordinal)).Distinct().ToList();
        if (targets.Count == 0)
        {
            return;
        }

        var names = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var target in targets)
        {
            var name = DefinitionName(target);
            for (var i = 2; names.ContainsValue(name); i++)
            {
                name = $"{DefinitionName(target)}{i}";
            }

            names[target] = name;
        }

        // Copy every target out of the untouched tree first; a target may sit inside another one.
        var definitions = targets.ToDictionary(t => t, t => Resolve(schema, t)?.DeepClone() ?? throw new InvalidOperationException($"Unresolvable $ref {t}."));

        var defs = schema["$defs"] as JsonObject ?? new JsonObject();
        foreach (var (target, definition) in definitions)
        {
            defs[names[target]] = Rewrite(definition, target, names, isDefinitionRoot: true);
        }

        var root = (JsonObject)Rewrite(schema, "#", names, isDefinitionRoot: true)!;
        schema.Clear();
        foreach (var (key, value) in root.ToList())
        {
            root.Remove(key);
            schema[key] = value;
        }

        schema["$defs"] = defs;
    }

    private static void CollectReferences(JsonNode? node, List<string> targets)
    {
        switch (node)
        {
            case JsonObject obj:
                if (obj["$ref"] is JsonValue reference && reference.GetValueKind() == JsonValueKind.String)
                {
                    targets.Add(reference.GetValue<string>());
                }

                foreach (var child in obj.Select(p => p.Value))
                {
                    CollectReferences(child, targets);
                }

                break;
            case JsonArray array:
                foreach (var child in array)
                {
                    CollectReferences(child, targets);
                }

                break;
        }
    }

    /// <summary>A copy of <paramref name="node"/> (found at <paramref name="pointer"/>) with referenced schemas and references pointing into $defs.</summary>
    private static JsonNode? Rewrite(JsonNode? node, string pointer, Dictionary<string, string> names, bool isDefinitionRoot = false)
    {
        if (!isDefinitionRoot && names.TryGetValue(pointer, out var defined))
        {
            return new JsonObject { ["$ref"] = $"#/$defs/{defined}" };
        }

        switch (node)
        {
            case JsonObject obj:
                var copy = new JsonObject();
                foreach (var (key, value) in obj)
                {
                    if (key == "$ref" && value is JsonValue reference && reference.GetValueKind() == JsonValueKind.String
                        && names.TryGetValue(reference.GetValue<string>(), out var name))
                    {
                        copy[key] = $"#/$defs/{name}";
                    }
                    else
                    {
                        copy[key] = Rewrite(value, $"{pointer}/{Escape(key)}", names);
                    }
                }

                return copy;
            case JsonArray array:
                var items = new JsonArray();
                for (var i = 0; i < array.Count; i++)
                {
                    items.Add(Rewrite(array[i], $"{pointer}/{i}", names));
                }

                return items;
            default:
                return node?.DeepClone();
        }
    }

    private static JsonNode? Resolve(JsonNode root, string pointer)
    {
        JsonNode? node = root;
        foreach (var segment in pointer.TrimStart('#').Split('/', StringSplitOptions.RemoveEmptyEntries))
        {
            var key = segment.Replace("~1", "/", StringComparison.Ordinal).Replace("~0", "~", StringComparison.Ordinal);
            node = node switch
            {
                JsonObject obj => obj[key],
                JsonArray array when int.TryParse(key, out var index) && index < array.Count => array[index],
                _ => null,
            };
        }

        return node;
    }

    private static string Escape(string key) => key.Replace("~", "~0", StringComparison.Ordinal).Replace("/", "~1", StringComparison.Ordinal);

    /// <summary>"#/properties/core/items" becomes "core_items".</summary>
    private static string DefinitionName(string pointer)
    {
        var parts = pointer.TrimStart('#').Split('/', StringSplitOptions.RemoveEmptyEntries)
            .Where(p => p is not ("properties" or "anyOf" or "oneOf" or "allOf") && !int.TryParse(p, out _))
            .Select(p => new string(p.Where(char.IsLetterOrDigit).ToArray()))
            .Where(p => p.Length > 0);
        var name = string.Join("_", parts);
        return name.Length > 0 ? name : "definition";
    }

    private static void Normalize(JsonNode? node)
    {
        switch (node)
        {
            case JsonObject obj:
                foreach (var key in Unsupported)
                {
                    obj.Remove(key);
                }

                // ["string", "null"] becomes anyOf [{type: string}, {type: null}].
                if (obj["type"] is JsonArray types)
                {
                    var names = types.Select(t => t!.GetValue<string>()).ToList();
                    obj.Remove("type");
                    if (names.Count == 1)
                    {
                        obj["type"] = names[0];
                    }
                    else
                    {
                        var copy = (JsonObject)obj.DeepClone();
                        foreach (var key in copy.Select(p => p.Key).ToList())
                        {
                            obj.Remove(key);
                        }

                        var options = new JsonArray();
                        foreach (var name in names)
                        {
                            var option = name == "null" ? new JsonObject() : (JsonObject)copy.DeepClone();
                            option["type"] = name;
                            options.Add(option);
                        }

                        obj["anyOf"] = options;
                    }
                }

                if (obj["properties"] is JsonObject properties)
                {
                    obj["additionalProperties"] = false;
                    obj["required"] = new JsonArray(properties.Select(p => (JsonNode)JsonValue.Create(p.Key)!).ToArray());
                }
                else if (obj["type"]?.GetValueKind() == JsonValueKind.String && obj["type"]!.GetValue<string>() == "object")
                {
                    if (obj["additionalProperties"] is not null and not JsonValue)
                    {
                        throw new InvalidOperationException("Structured outputs cannot hold dictionaries; use a list of objects.");
                    }

                    obj["additionalProperties"] = false;
                    obj["properties"] ??= new JsonObject();
                    obj["required"] ??= new JsonArray();
                }

                foreach (var child in obj.Select(p => p.Value).ToList())
                {
                    Normalize(child);
                }

                break;
            case JsonArray array:
                foreach (var child in array)
                {
                    Normalize(child);
                }

                break;
        }
    }
}
