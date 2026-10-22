using System.Net;
using System.Text;
using Anthropic;
using Microsoft.Extensions.Logging.Abstractions;
using Newtonsoft.Json.Linq;
using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Generation;

namespace Vaesen.Framework.Mystery.Tests;

/// <summary>The Anthropic client against a fake HTTP server: what goes on the wire, and how the stream is read.</summary>
public sealed class AnthropicModelTests
{
    private static ModelRequest Request => new()
    {
        Step = "escalation",
        Model = "claude-opus-5-5",
        Effort = "high",
        MaxTokens = 64000,
        System = "You design mysteries.",
        Context = "# The level\n...",
        Instruction = "Write the events.",
    };

    private static (AnthropicMysteryModel model, FakeApi api) Build(params string[] answers)
    {
        var api = new FakeApi(answers);
        var client = new AnthropicClient { ApiKey = "sk-ant-test", HttpClient = new HttpClient(api), MaxRetries = 0 };
        return (new AnthropicMysteryModel(client, NullLogger<AnthropicMysteryModel>.Instance), api);
    }

    [Fact]
    public async Task A_step_streams_with_its_schema_caching_and_fallbacks()
    {
        var (model, api) = Build(FakeApi.Stream("""{"events":[{"id":"e1","description":"The wheel stops.","when":"nightly","placeId":"mill","register":"physical","opening":true}]}"""));

        var reply = await model.GenerateAsync<EscalationOutput>(Request);

        Assert.True(reply.Success, reply.ErrorMessage);
        Assert.Equal("e1", Assert.Single(reply.Result!.Value.Events).Id);
        Assert.Equal(1200, reply.Result.InputTokens);
        Assert.Equal(1000, reply.Result.CacheReadTokens);
        Assert.Equal(55, reply.Result.OutputTokens);

        var body = Assert.Single(api.Bodies);
        Assert.Equal("claude-opus-5-5", body["model"]!.ToString());
        Assert.True(body["stream"]!.Value<bool>());
        Assert.Equal("default", body["fallbacks"]!.ToString());
        Assert.Equal("high", body["output_config"]!["effort"]!.ToString());
        Assert.Equal("json_schema", body["output_config"]!["format"]!["type"]!.ToString());
        Assert.False(body["output_config"]!["format"]!["schema"]!["additionalProperties"]!.Value<bool>());
        Assert.Equal("ephemeral", body["system"]![0]!["cache_control"]!["type"]!.ToString());
        Assert.Equal("ephemeral", body["messages"]![0]!["content"]![0]!["cache_control"]!["type"]!.ToString());
        Assert.Null(body["messages"]![0]!["content"]![1]!["cache_control"]);
        Assert.Contains("server-side-fallback-2026-07-01", api.Betas.Single(), StringComparison.Ordinal);
    }

    [Fact]
    public async Task Running_out_of_tokens_is_reported_as_truncated()
    {
        var (model, _) = Build(FakeApi.Stream("""{"events":[""", stopReason: "max_tokens"));

        var reply = await model.GenerateAsync<EscalationOutput>(Request);

        Assert.False(reply.Success);
        Assert.Equal(ModelErrorCodes.Truncated, reply.ErrorCode);
    }

    [Fact]
    public async Task A_schema_too_complex_to_compile_is_asked_for_in_the_prompt_instead()
    {
        var (model, api) = Build(
            FakeApi.Error(400, "Schema is too complex for compilation."),
            FakeApi.Stream("Here it is:\n```json\n{\"events\":[]}\n```"));

        var reply = await model.GenerateAsync<EscalationOutput>(Request);

        Assert.True(reply.Success, reply.ErrorMessage);
        Assert.Empty(reply.Result!.Value.Events);
        Assert.Equal(2, api.Bodies.Count);
        Assert.Null(api.Bodies[1]["output_config"]!["format"]);
        Assert.Contains("JSON schema", api.Bodies[1]["messages"]![0]!["content"]![1]!["text"]!.ToString(), StringComparison.Ordinal);
    }

    [Fact]
    public async Task A_rejected_request_says_so()
    {
        var (model, _) = Build(FakeApi.Error(401, "invalid x-api-key"));

        var reply = await model.GenerateAsync<EscalationOutput>(Request);

        Assert.False(reply.Success);
        Assert.Equal(ModelErrorCodes.Rejected, reply.ErrorCode);
    }

    private sealed class FakeApi(string[] answers) : HttpMessageHandler
    {
        private int _next;

        public List<JObject> Bodies { get; } = [];

        public List<string> Betas { get; } = [];

        public static string Stream(string text, string stopReason = "end_turn")
        {
            var json = new JValue(text).ToString(Newtonsoft.Json.Formatting.None);
            return "SSE\n"
                + "event: message_start\ndata: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_1\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"claude-opus-5-5\",\"content\":[],\"stop_reason\":null,\"stop_sequence\":null,\"usage\":{\"input_tokens\":1200,\"output_tokens\":1,\"cache_read_input_tokens\":1000,\"cache_creation_input_tokens\":0}}}\n\n"
                + "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
                + $"event: content_block_delta\ndata: {{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{{\"type\":\"text_delta\",\"text\":{json}}}}}\n\n"
                + "event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                + $"event: message_delta\ndata: {{\"type\":\"message_delta\",\"delta\":{{\"stop_reason\":\"{stopReason}\",\"stop_sequence\":null}},\"usage\":{{\"output_tokens\":55}}}}\n\n"
                + "event: message_stop\ndata: {\"type\":\"message_stop\"}\n\n";
        }

        public static string Error(int status, string message) =>
            $"ERR{status}\n{{\"type\":\"error\",\"error\":{{\"type\":\"invalid_request_error\",\"message\":\"{message}\"}}}}";

        protected override async Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
        {
            Bodies.Add(JObject.Parse(await request.Content!.ReadAsStringAsync(cancellationToken)));
            Betas.Add(string.Join(",", request.Headers.TryGetValues("anthropic-beta", out var values) ? values : []));

            var answer = answers[Math.Min(_next++, answers.Length - 1)];
            var newline = answer.IndexOf('\n', StringComparison.Ordinal);
            var kind = answer[..newline];
            var payload = answer[(newline + 1)..];
            if (kind.StartsWith("ERR", StringComparison.Ordinal))
            {
                return new HttpResponseMessage((HttpStatusCode)int.Parse(kind[3..], System.Globalization.CultureInfo.InvariantCulture))
                {
                    Content = new StringContent(payload, Encoding.UTF8, "application/json"),
                };
            }

            return new HttpResponseMessage(HttpStatusCode.OK) { Content = new StringContent(payload, Encoding.UTF8, "text/event-stream") };
        }
    }
}
