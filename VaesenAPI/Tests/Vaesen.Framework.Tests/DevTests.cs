using System.Net;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using Vaesen.Framework.Common.Api;
using Vaesen.Framework.Common.Models;

namespace Vaesen.Framework.Tests;

public class DevTests
{
    [Fact]
    public async Task Guilds_ListsSeedGuildsWithPlayers()
    {
        await using var factory = new VaesenApiFactory(useTestAuth: false);

        var result = await (await factory.CreateClient().GetAsync("/api/dev/guilds")).ReadResultAsync<List<DevGuild>>();

        Assert.True(result.Success);
        var guild = Assert.Single(result.Result!, g => g.Id == "upsala-society");
        Assert.Contains(guild.Players, p => p.Id == "ingrid");
    }

    [Fact]
    public async Task Login_UnknownPlayer_IsNotFound()
    {
        await using var factory = new VaesenApiFactory(useTestAuth: false);

        var response = await factory.CreateClient().PostAsJsonAsync("/api/dev/login", new { playerId = "nobody" });

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
        Assert.False((await response.ReadResultAsync<DevLoginResult>()).Success);
    }

    [Fact]
    public async Task Login_TokenIsAcceptedByPlayerEndpoints()
    {
        await using var factory = new VaesenApiFactory(useTestAuth: false);
        var client = factory.CreateClient();

        var login = (await (await client.PostAsJsonAsync("/api/dev/login", new { playerId = "ingrid" })).ReadResultAsync<DevLoginResult>()).Result!;
        client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", login.AccessToken);
        var me = (await (await client.GetAsync("/api/identity/me")).ReadResultAsync<MeResult>()).Result!;

        Assert.Equal("dev|ingrid", login.PlayerId);
        Assert.Equal("upsala-society", login.GuildId);
        Assert.Equal("dev|ingrid", me.PlayerId);
    }

    [Fact]
    public async Task ForgedToken_IsRejected()
    {
        await using var factory = new VaesenApiFactory(useTestAuth: false);
        var client = factory.CreateClient();
        // Issuer says "vaesen-dev" but the signature is garbage.
        client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer",
            "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJ2YWVzZW4tZGV2Iiwic3ViIjoiZGV2fGluZ3JpZCJ9.bad-signature");

        var response = await client.GetAsync("/api/identity/me");

        Assert.Equal(HttpStatusCode.Unauthorized, response.StatusCode);
    }
}
