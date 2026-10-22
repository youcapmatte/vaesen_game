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
public sealed class ModelRequest
{
    /// <summary>Which step is asking (for logs): "conflict", "escalation", "humans", "clues", "critic".</summary>
    public required string Step { get; init; }

    public required string Model { get; init; }

    public string Effort { get; init; } = "high";

    public int MaxTokens { get; init; } = 64000;

    public required string System { get; init; }

    public string Context { get; init; } = "";

    public required string Instruction { get; init; }
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
}
