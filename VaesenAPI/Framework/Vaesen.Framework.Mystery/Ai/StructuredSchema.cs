using System.Collections.Concurrent;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Schema;
using System.Text.Json.Serialization;

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
        var node = Options.GetJsonSchemaAsNode(type, new JsonSchemaExporterOptions { TreatNullObliviousAsNonNullable = true });
        var schema = node as JsonObject ?? throw new InvalidOperationException($"No object schema for {type.Name}.");
        Normalize(schema);
        return schema;
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
