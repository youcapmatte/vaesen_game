using System.Net.Http.Headers;
using System.Net.Http.Json;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc.Testing;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.DependencyInjection.Extensions;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Nexus.Table.Models;
using Nexus.Table.Services;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Ai;

namespace Nexus.Table.Tests;

/// <summary>The API in Development with LocalFile storage in a fresh folder and dice the test decides.</summary>
public sealed class NexusApiFactory : WebApplicationFactory<Program>
{
    private readonly string _dataDirectory = Path.Combine(Path.GetTempPath(), "nexus-table-tests", Guid.NewGuid().ToString("N"));

    public const string AgentKey = "test-agent-key";

    public FixedDice Dice { get; } = new();

    /// <summary>Stands in for LiveKit's server API; records what the API sent.</summary>
    public FakeLiveKit LiveKit { get; } = new();

    /// <summary>Set before the first request to run without LiveKit settings.</summary>
    public bool WithoutLiveKit { get; init; }

    protected override void ConfigureWebHost(IWebHostBuilder builder)
    {
        builder.UseEnvironment("Development");
        builder.UseSetting("DataService:Provider", NexusServiceExtensions.LocalFileProvider);
        builder.UseSetting("LocalData:Directory", _dataDirectory);
        builder.UseSetting("Mystery:CasePacketPath", Path.Combine(_dataDirectory, "cases"));
        if (!WithoutLiveKit)
        {
            builder.UseSetting("LiveKit:Url", "wss://nexus-test.livekit.cloud");
            builder.UseSetting("LiveKit:ApiKey", "APItestkey");
            builder.UseSetting("LiveKit:ApiSecret", "a-livekit-api-secret-that-is-long-enough-for-hs256");
            builder.UseSetting("LiveKit:AgentKey", AgentKey);
        }

        builder.ConfigureServices(services =>
        {
            services.Replace(ServiceDescriptor.Singleton<IDiceRoller>(Dice));
            services.Replace(ServiceDescriptor.Singleton<IMysteryModel>(new UnavailableModel()));
            services.AddHttpClient(CallService.HttpClientName).ConfigurePrimaryHttpMessageHandler(() => LiveKit);
        });
    }

    /// <summary>A client signed in as a dev player ("mattias" is the host).</summary>
    public async Task<HttpClient> ClientForAsync(string playerId)
    {
        var client = CreateClient();
        var login = await client.PostAsJsonAsync("/api/dev/login", new { playerId });
        var token = (await ReadAsync(login))["result"]!["accessToken"]!.ToString();
        client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", token);
        return client;
    }

    public static async Task<JObject> ReadAsync(HttpResponseMessage response) => JObject.Parse(await response.Content.ReadAsStringAsync());

    public static async Task<T> ResultAsync<T>(HttpResponseMessage response)
    {
        var body = await ReadAsync(response);
        Assert.True(body["success"]!.Value<bool>(), body.ToString(Formatting.None));
        return body["result"]!.ToObject<T>()!;
    }

    protected override void Dispose(bool disposing)
    {
        base.Dispose(disposing);
        if (disposing && Directory.Exists(_dataDirectory))
        {
            Directory.Delete(_dataDirectory, recursive: true);
        }
    }
}

/// <summary>Answers every LiveKit server API call with 200 and keeps the requests.</summary>
public sealed class FakeLiveKit : HttpMessageHandler
{
    public List<(Uri Uri, string? Authorization, string Body)> Requests { get; } = [];

    protected override async Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
    {
        var body = request.Content is null ? "" : await request.Content.ReadAsStringAsync(cancellationToken);
        Requests.Add((request.RequestUri!, request.Headers.Authorization?.Parameter, body));
        return new HttpResponseMessage(System.Net.HttpStatusCode.OK) { Content = new StringContent("{}") };
    }
}

/// <summary>Dice that come up as queued; 1 when nothing is queued.</summary>
public sealed class FixedDice : IDiceRoller
{
    private readonly Queue<int> _faces = new();

    public void Queue(params int[] faces)
    {
        foreach (var face in faces)
        {
            _faces.Enqueue(face);
        }
    }

    public int D6() => _faces.TryDequeue(out var face) ? face : 1;
}

public static class Requests
{
    /// <summary>A young Academic that follows every creation rule.</summary>
    public static CreateCharacterRequest Academic(string name = "Elin Gregorius") => new()
    {
        Name = name,
        Archetype = "academic",
        AgeGroup = "young",
        Age = 24,
        Attributes = new() { ["physique"] = 3, ["precision"] = 3, ["logic"] = 5, ["empathy"] = 4 },
        Skills = new() { ["learning"] = 3, ["investigation"] = 2, ["observation"] = 2, ["vigilance"] = 2, ["inspiration"] = 1 },
        Resources = 4,
        Talent = "bookworm",
        Motivation = "To read what no one else dares to",
        Trauma = "Saw the reading room's shadows move",
        DarkSecret = "Burned a rival's thesis",
        Memento = "Dried red rose",
        Equipment = ["Book collection", "Writing utensils", "Liquor"],
    };
}

/// <summary>Stands in for the Anthropic API in tests: every call fails as if the API were down, so no test reaches the network.</summary>
public sealed class UnavailableModel : IMysteryModel
{
    public Task<ResultBase<ModelReply<T>>> GenerateAsync<T>(ModelRequest request, CancellationToken cancellationToken = default) where T : class =>
        Task.FromResult(new ResultBase<ModelReply<T>>().Fail("The model is not available in tests.", ModelErrorCodes.Unavailable));
}
