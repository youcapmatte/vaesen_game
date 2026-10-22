using System.Net;
using System.Net.Http.Json;
using Microsoft.IdentityModel.JsonWebTokens;
using Newtonsoft.Json.Linq;
using Nexus.Table.Models;
using Nexus.Table.Services;

namespace Nexus.Table.Tests;

public sealed class CallTests : IDisposable
{
    private readonly NexusApiFactory _factory = new();

    public void Dispose() => _factory.Dispose();

    [Fact]
    public async Task A_join_token_puts_the_player_in_the_society_room_as_their_character()
    {
        var ingrid = await _factory.ClientForAsync("ingrid");
        var character = await NexusApiFactory.ResultAsync<Character>(await ingrid.PostAsJsonAsync("/api/characters", Requests.Academic()));

        var join = await NexusApiFactory.ResultAsync<CallJoin>(await ingrid.PostAsync("/api/call/join", null));
        var token = new JsonWebTokenHandler().ReadJsonWebToken(join.Token);
        var video = JObject.Parse(token.GetPayloadValue<object>("video").ToString()!);
        var metadata = JObject.Parse(token.GetPayloadValue<string>("metadata"));

        Assert.Equal("wss://nexus-test.livekit.cloud", join.Url);
        Assert.Equal("APItestkey", token.Issuer);
        Assert.Equal(join.Identity, token.Subject);
        Assert.Equal("Elin Gregorius", token.GetPayloadValue<string>("name"));
        Assert.Equal(join.Room, video["room"]!.ToString());
        Assert.True(video["roomJoin"]!.Value<bool>());
        Assert.Null(video["roomAdmin"]);
        Assert.Equal(character.Id, metadata["characterId"]!.ToString());
        Assert.Equal("Player", metadata["role"]!.ToString());
    }

    [Fact]
    public async Task Only_the_host_brings_in_the_gamemaster()
    {
        var ingrid = await _factory.ClientForAsync("ingrid");
        var host = await _factory.ClientForAsync("mattias");

        var refused = await ingrid.PostAsync("/api/call/gamemaster", null);
        var started = await host.PostAsync("/api/call/gamemaster", null);

        Assert.Equal(HttpStatusCode.Forbidden, refused.StatusCode);
        Assert.Equal(HttpStatusCode.OK, started.StatusCode);
        var (uri, authorization, body) = Assert.Single(_factory.LiveKit.Requests);
        Assert.Equal("https://nexus-test.livekit.cloud/twirp/livekit.AgentDispatchService/CreateDispatch", uri.ToString());
        Assert.Equal("nexus-gm", JObject.Parse(body)["agent_name"]!.ToString());
        var admin = new JsonWebTokenHandler().ReadJsonWebToken(authorization);
        Assert.True(JObject.Parse(admin.GetPayloadValue<object>("video").ToString()!)["roomAdmin"]!.Value<bool>());
    }

    [Fact]
    public async Task Without_livekit_settings_the_call_says_so()
    {
        using var factory = new NexusApiFactory { WithoutLiveKit = true };
        var ingrid = await factory.ClientForAsync("ingrid");

        var response = await ingrid.PostAsync("/api/call/join", null);

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
        Assert.Equal(CallService.NotConfigured, (await NexusApiFactory.ReadAsync(response))["errorCode"]!.ToString());
    }

    [Fact]
    public async Task The_gm_agent_reads_every_sheet_with_its_secret_but_only_with_the_key()
    {
        var ingrid = await _factory.ClientForAsync("ingrid");
        await ingrid.PostAsJsonAsync("/api/characters", Requests.Academic());
        var agent = _factory.CreateClient();

        var anonymous = await agent.GetAsync("/api/agent/table");
        agent.DefaultRequestHeaders.Add("X-Agent-Key", NexusApiFactory.AgentKey);
        var table = await NexusApiFactory.ResultAsync<GmTable>(await agent.GetAsync("/api/agent/table"));

        Assert.Equal(HttpStatusCode.Unauthorized, anonymous.StatusCode);
        Assert.Equal("Burned a rival's thesis", table.Players.Single(p => p.Character is not null).Character!.DarkSecret);
    }
}
