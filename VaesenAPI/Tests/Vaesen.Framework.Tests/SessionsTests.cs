using System.Net;
using Vaesen.Framework.Common.Api;
using Vaesen.Framework.Core.Security;
using Vaesen.Framework.GameLift.Models;
using Vaesen.Framework.GameLift.Services;

namespace Vaesen.Framework.Tests;

public class SessionsTests
{
    private const string JoinTown = "/api/sessions/worlds/town/join";

    [Fact]
    public async Task Join_WithoutToken_IsUnauthorized()
    {
        await using var factory = new VaesenApiFactory();

        var response = await factory.CreateClient().PostAsync(JoinTown, null);

        Assert.Equal(HttpStatusCode.Unauthorized, response.StatusCode);
    }

    [Fact]
    public async Task Join_WithGameServerToken_IsForbidden()
    {
        await using var factory = new VaesenApiFactory();

        var response = await factory.CreateMachineClient(Auth0Options.GameServerScope).PostAsync(JoinTown, null);

        Assert.Equal(HttpStatusCode.Forbidden, response.StatusCode);
    }

    [Fact]
    public async Task Join_UnknownWorld_IsNotFound()
    {
        await using var factory = new VaesenApiFactory();

        var response = await factory.CreatePlayerClient().PostAsync("/api/sessions/worlds/nowhere/join", null);

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    [Fact]
    public async Task Join_WorldWithNoSession_StartsWorldAndAsksClientToRetry()
    {
        await using var factory = new VaesenApiFactory();

        var response = await factory.CreatePlayerClient().PostAsync(JoinTown, null);

        Assert.Equal(HttpStatusCode.Accepted, response.StatusCode);
        Assert.Equal("2", response.Headers.GetValues("Retry-After").Single());
        var result = await response.ReadResultAsync<JoinWorldResponse>();
        Assert.Equal(JoinWorldResponse.Starting, result.Result!.Status);
        Assert.Equal("town", Assert.Single(factory.GameLift.CreatedWorlds).Id);
    }

    [Fact]
    public async Task Join_WhileWorldIsActivating_DoesNotStartASecondSession()
    {
        await using var factory = new VaesenApiFactory();
        var client = factory.CreatePlayerClient();

        await client.PostAsync(JoinTown, null);
        await client.PostAsync(JoinTown, null);

        Assert.Single(factory.GameLift.CreatedWorlds);
    }

    [Fact]
    public async Task Join_ActiveWorld_ReturnsConnectUrlWithPlayerSession()
    {
        await using var factory = new VaesenApiFactory();
        factory.GameLift.SessionsByWorld["town"] =
            new GameSessionInfo("gsess-town", GameSessionState.Active, "10.0.0.5", null, 7777);

        var response = await factory.CreatePlayerClient("auth0|abc").PostAsync(JoinTown, null);

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        var join = (await response.ReadResultAsync<JoinWorldResponse>()).Result!;
        Assert.Equal(JoinWorldResponse.Ready, join.Status);
        Assert.Equal("psess-auth0|abc", join.PlayerSessionId);
        Assert.Equal("10.0.0.5:7777?PlayerSessionId=psess-auth0|abc", join.ConnectUrl);
    }

    [Fact]
    public async Task Join_FullWorld_IsConflict()
    {
        await using var factory = new VaesenApiFactory();
        factory.GameLift.SessionsByWorld["town"] =
            new GameSessionInfo("gsess-town", GameSessionState.Active, "10.0.0.5", null, 7777);
        factory.GameLift.FullSessions.Add("gsess-town");

        var response = await factory.CreatePlayerClient().PostAsync(JoinTown, null);

        Assert.Equal(HttpStatusCode.Conflict, response.StatusCode);
    }

    [Fact]
    public async Task Worlds_ListsConfiguredWorlds()
    {
        await using var factory = new VaesenApiFactory();

        var result = await (await factory.CreatePlayerClient().GetAsync("/api/sessions/worlds")).ReadResultAsync<List<WorldSummary>>();

        Assert.Contains(result.Result!, w => w.Id == "town");
    }

    [Fact]
    public async Task Me_ReturnsSubjectFromToken()
    {
        await using var factory = new VaesenApiFactory();

        var result = await (await factory.CreatePlayerClient("auth0|abc").GetAsync("/api/identity/me")).ReadResultAsync<MeResult>();

        Assert.Equal("auth0|abc", result.Result!.PlayerId);
    }
}
