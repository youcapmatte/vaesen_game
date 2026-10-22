using System.Diagnostics;
using System.Text;
using Anthropic;
using Anthropic.Exceptions;
using Anthropic.Models.Beta.Messages;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Mystery.Ai;

/// <summary>
/// <see cref="IMysteryModel"/> on the Anthropic API. Each call streams (case steps can run to tens of thousands of
/// tokens), constrains the answer to the output type's JSON schema, caches the system prompt and the case context,
/// and lets the API fall back to another model on a refusal (server-side fallbacks, "default" routing). If the API
/// finds a schema too complex to compile, the call is repeated with the schema in the prompt instead.
/// </summary>
public sealed class AnthropicMysteryModel : IMysteryModel
{
    private const string FallbackBeta = "server-side-fallback-2026-07-01";

    private readonly AnthropicClient _client;
    private readonly ILogger<AnthropicMysteryModel> _logger;

    public AnthropicMysteryModel(IOptions<MysteryOptions> options, ILogger<AnthropicMysteryModel> logger)
    {
        _logger = logger;
        var key = options.Value.AnthropicApiKey;
        _client = string.IsNullOrWhiteSpace(key) ? new AnthropicClient() : new AnthropicClient { ApiKey = key };
    }

    /// <summary>For tests: a client whose HTTP traffic the test controls.</summary>
    internal AnthropicMysteryModel(AnthropicClient client, ILogger<AnthropicMysteryModel> logger)
    {
        _client = client;
        _logger = logger;
    }

    public async Task<ResultBase<ModelReply<T>>> GenerateAsync<T>(ModelRequest request, CancellationToken cancellationToken = default)
        where T : class
    {
        var ret = new ResultBase<ModelReply<T>>();
        try
        {
            Answer answer;
            try
            {
                answer = await StreamAsync<T>(request, structured: true, cancellationToken);
            }
            catch (AnthropicApiException ex) when (ex.Message.Contains("too complex", StringComparison.OrdinalIgnoreCase))
            {
                _logger.LogWarning("The {Step} schema is too complex for structured outputs; asking for JSON in the prompt instead", request.Step);
                answer = await StreamAsync<T>(request, structured: false, cancellationToken);
            }

            var stop = (answer.StopReason ?? "").Replace("_", "", StringComparison.Ordinal);
            if (stop.Contains("refusal", StringComparison.OrdinalIgnoreCase))
            {
                return ret.Fail($"The model declined the {request.Step} step.", ModelErrorCodes.Refused);
            }

            if (stop.Contains("maxtokens", StringComparison.OrdinalIgnoreCase))
            {
                return ret.Fail($"The {request.Step} step ran out of output tokens ({request.MaxTokens}).", ModelErrorCodes.Truncated);
            }

            var value = JsonConvert.DeserializeObject<T>(answer.Json);
            if (value is null)
            {
                return ret.Fail($"The {request.Step} step returned no JSON.", ModelErrorCodes.BadOutput);
            }

            ret.Ok(new ModelReply<T>
            {
                Value = value,
                RawJson = answer.Json,
                Model = answer.Model,
                StopReason = answer.StopReason,
                InputTokens = answer.Input,
                OutputTokens = answer.Output,
                CacheReadTokens = answer.CacheRead,
                CacheWriteTokens = answer.CacheWrite,
                LatencyMs = answer.LatencyMs,
            });
        }
        catch (JsonException ex)
        {
            ret.Fail($"The {request.Step} step returned JSON that does not fit: {ex.Message}", ModelErrorCodes.BadOutput);
        }
        catch (AnthropicRateLimitException ex)
        {
            ret.Fail($"Rate limited by the Anthropic API: {ex.Message}", ModelErrorCodes.RateLimited);
        }
        catch (Anthropic5xxException ex)
        {
            ret.Fail($"The Anthropic API failed: {ex.Message}", ModelErrorCodes.Unavailable);
        }
        catch (AnthropicApiException ex) when (ex.Message.Contains("authentication_error", StringComparison.Ordinal))
        {
            ret.Fail("The Anthropic API key is missing or was not accepted. Set Mystery:AnthropicApiKey (or ANTHROPIC_API_KEY) for the API.", ModelErrorCodes.Rejected);
        }
        catch (AnthropicApiException ex)
        {
            ret.Fail($"The Anthropic API rejected the {request.Step} request: {ex.Message}", ModelErrorCodes.Rejected);
        }
        catch (AnthropicException ex)
        {
            ret.Fail($"Could not reach the Anthropic API: {ex.Message}", ModelErrorCodes.Unavailable);
        }

        return ret;
    }

    private async Task<Answer> StreamAsync<T>(ModelRequest request, bool structured, CancellationToken cancellationToken)
    {
        var stopwatch = Stopwatch.StartNew();
        var schema = StructuredSchema.For<T>();

        var content = new List<BetaContentBlockParam>();
        if (!string.IsNullOrWhiteSpace(request.Context))
        {
            content.Add(new BetaTextBlockParam { Text = request.Context, CacheControl = new BetaCacheControlEphemeral() });
        }

        var instruction = structured
            ? request.Instruction
            : request.Instruction + "\n\nAnswer with one JSON object that fits this JSON schema exactly, and nothing else:\n" + schema.ToJsonString();
        content.Add(new BetaTextBlockParam { Text = instruction });

        var output = structured
            ? new BetaOutputConfig
            {
                Effort = request.Effort,
                Format = new BetaJsonOutputFormat
                {
                    Schema = schema.ToDictionary(p => p.Key, p => System.Text.Json.JsonSerializer.SerializeToElement(p.Value)),
                },
            }
            : new BetaOutputConfig { Effort = request.Effort };

        var parameters = new MessageCreateParams
        {
            Model = request.Model,
            MaxTokens = request.MaxTokens,
            Betas = [FallbackBeta],
            Fallbacks = new Default(),
            System = new List<BetaTextBlockParam>
            {
                new() { Text = request.System, CacheControl = new BetaCacheControlEphemeral() },
            },
            OutputConfig = output,
            Messages = [new BetaUserMessageParam { Content = content }],
        };

        var text = new StringBuilder();
        var answer = new Answer { Model = request.Model };
        await foreach (var streamEvent in _client.Beta.Messages.CreateStreaming(parameters, cancellationToken))
        {
            if (streamEvent.TryPickStart(out var start))
            {
                answer.Model = start.Message.Model;
                answer.Input = start.Message.Usage.InputTokens;
                answer.CacheRead = start.Message.Usage.CacheReadInputTokens ?? 0;
                answer.CacheWrite = start.Message.Usage.CacheCreationInputTokens ?? 0;
            }
            else if (streamEvent.TryPickContentBlockDelta(out var delta) && delta.Delta.TryPickText(out var chunk))
            {
                text.Append(chunk.Text);
            }
            else if (streamEvent.TryPickDelta(out var messageDelta))
            {
                answer.StopReason = messageDelta.Delta.StopReason?.ToString();
                answer.Output = messageDelta.Usage.OutputTokens;
            }
        }

        stopwatch.Stop();
        answer.LatencyMs = stopwatch.ElapsedMilliseconds;
        answer.Json = structured ? text.ToString() : OutermostObject(text.ToString());
        _logger.LogInformation(
            "Mystery step {Step} on {Model}: {Input} in ({CacheRead} cached, {CacheWrite} written), {Output} out, {Ms} ms, stop {Stop}",
            request.Step, answer.Model, answer.Input, answer.CacheRead, answer.CacheWrite, answer.Output, answer.LatencyMs, answer.StopReason);
        return answer;
    }

    /// <summary>Without structured outputs the model may wrap the JSON in prose or a code fence; keep the object.</summary>
    private static string OutermostObject(string text)
    {
        var start = text.IndexOf('{', StringComparison.Ordinal);
        var end = text.LastIndexOf('}');
        return start >= 0 && end > start ? text[start..(end + 1)] : text;
    }

    private sealed class Answer
    {
        public string Json { get; set; } = "";

        public string Model { get; set; } = "";

        public string? StopReason { get; set; }

        public long Input { get; set; }

        public long Output { get; set; }

        public long CacheRead { get; set; }

        public long CacheWrite { get; set; }

        public long LatencyMs { get; set; }
    }
}

public static class ModelErrorCodes
{
    public const string Refused = "model_refused";
    public const string Truncated = "model_truncated";
    public const string BadOutput = "model_bad_output";
    public const string RateLimited = "model_rate_limited";
    public const string Unavailable = "model_unavailable";
    public const string Rejected = "model_rejected";
}
