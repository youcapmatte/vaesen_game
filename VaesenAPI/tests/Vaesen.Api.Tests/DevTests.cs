using System.Net;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using Vaesen.Api.Modules.Dev;
using Vaesen.Api.Modules.Identity;

namespace Vaesen.Api.Tests;

public class DevTests
{
    [Fact]
    public async Task Guilds_ListsSeedGuildsWithPlayers()
    {
        await using var factory = new VaesenApiFactory(useTestAuth: false);

        var guilds = await factory.CreateClient().GetFromJsonAsync<DevGuild[]>("/api/v1/dev/guilds");

        var guild = Assert.Single(guilds!, g => g.Id == "upsala-society");
        Assert.Contains(guild.Players, p => p.Id == "ingrid");
    }

    [Fact]
    public async Task Login_UnknownPlayer_IsNotFound()
    {
        await using var factory = new VaesenApiFactory(useTestAuth: false);

        var response = await factory.CreateClient().PostAsJsonAsync("/api/v1/dev/login", new DevLoginRequest("nobody"));

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    [Fact]
    public async Task Login_TokenIsAcceptedByPlayerEndpoints()
    {
        await using var factory = new VaesenApiFactory(useTestAuth: false);
        var client = factory.CreateClient();

        var login = await (await client.PostAsJsonAsync("/api/v1/dev/login", new DevLoginRequest("ingrid")))
            .Content.ReadFromJsonAsync<DevLoginResponse>();
        client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", login!.AccessToken);
        var me = await client.GetFromJsonAsync<MeResponse>("/api/v1/identity/me");

        Assert.Equal("dev|ingrid", login.PlayerId);
        Assert.Equal("upsala-society", login.GuildId);
        Assert.Equal("dev|ingrid", me!.PlayerId);
    }

    [Fact]
    public async Task ForgedToken_IsRejected()
    {
        await using var factory = new VaesenApiFactory(useTestAuth: false);
        var client = factory.CreateClient();
        // Issuer says "vaesen-dev" but the signature is garbage.
        client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer",
            "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJ2YWVzZW4tZGV2Iiwic3ViIjoiZGV2fGluZ3JpZCJ9.bad-signature");

        var response = await client.GetAsync("/api/v1/identity/me");

        Assert.Equal(HttpStatusCode.Unauthorized, response.StatusCode);
    }
}
