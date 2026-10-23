using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Mystery.Ai;

/// <summary>
/// The language model as the generation pipeline sees it: a request whose answer must be JSON of type
/// <typeparamref name="T"/>. The Anthropic implementation constrains the output to <typeparamref name="T"/>'s schema;
/// tests use a scripted fake.
/// </summary>
public interface IMysteryModel
{
    Task<ResultBase<ModelReply<T>>> GenerateAsync<T>(ModelRequest request, CancellationToken cancellationToken = default) where T : class;
}

/// <summary>
/// One structured call. The prompt is split by how stable it is, so prompt caching works: <see cref="System"/> is the
/// same for every case, <see cref="Context"/> for every step of one case, <see cref="Instruction"/> changes per call.
/// </summary>
public sealed record ModelRequest
{
    /// <summary>Which step is asking (for logs): "conflict", "escalation", "humans", "facts", "minds", "arc", "hook", "critic".</summary>
    public required string Step { get; init; }

    public required string Model { get; init; }

    public string Effort { get; init; } = "high";

    public int MaxTokens { get; init; } = 64000;

    public required string System { get; init; }

    public string Context { get; init; } = "";

    public required string Instruction { get; init; }

    /// <summary>
    /// Mark the system prompt and context for the prompt cache. A different output schema invalidates the whole
    /// cached prefix, so the cache only pays for a call that repeats one with the same schema: a retry, a patch, the
    /// critic's second look. A first call leaves it off and saves the write premium.
    /// </summary>
    public bool Cache { get; init; } = true;

    /// <summary>The case's ids by <see cref="IdKinds"/> kind: the output schema takes them as enums.</summary>
    public IReadOnlyDictionary<string, IReadOnlyList<string>>? Ids { get; init; }
}

public sealed class ModelReply<T>
{
    public required T Value { get; init; }

    /// <summary>The JSON exactly as the model returned it.</summary>
    public string RawJson { get; init; } = "";

    public string Model { get; init; } = "";

    public string? StopReason { get; init; }

    public long InputTokens { get; init; }

    public long OutputTokens { get; init; }

    public long CacheReadTokens { get; init; }

    public long CacheWriteTokens { get; init; }

    public long LatencyMs { get; init; }

    /// <summary>A hash of what precedes the cache breakpoint (model, schema, system, context), to see why a cache missed.</summary>
    public string PrefixHash { get; init; } = "";
}
