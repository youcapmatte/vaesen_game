using System.Net;
using System.Net.Http.Json;
using Microsoft.Extensions.DependencyInjection;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Common.Services;
using Vaesen.Framework.GameLift.Models;

namespace Vaesen.Framework.Tests;

/// <summary>Entering the world after Play: guild hubs, returning to the last instance, and what game servers report.</summary>
public sealed class EnterWorldTests : IAsyncLifetime
{
    private const string Enter = "/api/sessions/enter";

    private VaesenApiFactory _factory = null!;
    private HttpClient _admin = null!;

    private Account _player = null!;
    private Guild _guild = null!;
    private Character _character = null!;
    private HttpClient _client = null!;

    private string HubSession => $"gyllencreutz-{_guild.Id}";

    public async Task InitializeAsync()
    {
        _factory = new VaesenApiFactory();
        _admin = _factory.CreateClient();

        _player = (await (await _admin.PostAsJsonAsync("/api/accounts", new { username = "Astrid" })).ReadResultAsync<Account>()).Result!;
        _guild = (await (await _admin.PostAsJsonAsync("/api/guilds", new { name = "Lantern Watch", createdByAccountId = _player.Id })).ReadResultAsync<Guild>()).Result!;
        _character = (await (await _admin.PostAsJsonAsync($"/api/guilds/{_guild.Id}/characters",
            new { firstName = "Ylva", lastName = "Falk", createdByAccountId = _player.Id })).ReadResultAsync<Character>()).Result!;
        await ClaimAsync(_player);

        // What the dev login gives the game: subject "dev|{accountId}".
        _client = _factory.CreatePlayerClient($"dev|{_player.Id}");
    }

    public async Task DisposeAsync() => await _factory.DisposeAsync();

    private Task<HttpResponseMessage> ClaimAsync(Account account) =>
        _admin.PutAsJsonAsync($"/api/guilds/{_guild.Id}/characters/{_character.Id}/player", new { accountId = account.Id });

    private Task<HttpResponseMessage> EnterAsync() => _client.PostAsJsonAsync(Enter, new { characterId = _character.Id });

    private async Task<Character> GetCharacterAsync() =>
        (await (await _admin.GetAsync($"/api/characters/{_character.Id}")).ReadResultAsync<Character>()).Result!;

    private async Task SetLocationAsync(CharacterLocation location)
    {
        var store = _factory.Services.GetRequiredService<ICharacterStore>();
        var character = (await store.GetAsync(_character.Id))!;
        character.LastLocation = location;
        await store.UpdateAsync(character, character.Name);
    }

    [Fact]
    public async Task Enter_FirstTime_StartsTheGuildsHub()
    {
        var response = await EnterAsync();

        Assert.Equal(HttpStatusCode.Accepted, response.StatusCode);
        Assert.Equal(JoinWorldResponse.Starting, (await response.ReadResultAsync<JoinWorldResponse>()).Result!.Status);
        Assert.Equal([HubSession], _factory.GameLift.CreatedSessionNames);
    }

    [Fact]
    public async Task Enter_HubRunning_ReturnsConnectUrlAndRecordsLocation()
    {
        _factory.GameLift.Activate(HubSession);

        var response = await EnterAsync();

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        var join = (await response.ReadResultAsync<JoinWorldResponse>()).Result!;
        Assert.Equal("gyllencreutz", join.WorldId);
        Assert.Equal(HubSession, join.Instance);
        Assert.Equal("/Game/Vaesen/Maps/L_Gyllencreutz", join.Map);
        Assert.Equal($"10.0.0.5:7777?PlayerSessionId=psess-{_character.Id}", join.ConnectUrl);

        var location = (await GetCharacterAsync()).LastLocation!;
        Assert.Equal(HubSession, location.SessionName);
        Assert.Null(location.LeftAt);
    }

    [Fact]
    public async Task Enter_EachGuildGetsItsOwnHub()
    {
        var other = (await (await _admin.PostAsJsonAsync("/api/accounts", new { username = "Erik" })).ReadResultAsync<Account>()).Result!;
        var otherGuild = (await (await _admin.PostAsJsonAsync("/api/guilds", new { name = "Other Guild", createdByAccountId = other.Id })).ReadResultAsync<Guild>()).Result!;
        var otherCharacter = (await (await _admin.PostAsJsonAsync($"/api/guilds/{otherGuild.Id}/characters",
            new { firstName = "Erik", lastName = "Lind", createdByAccountId = other.Id })).ReadResultAsync<Character>()).Result!;
        await _admin.PutAsJsonAsync($"/api/guilds/{otherGuild.Id}/characters/{otherCharacter.Id}/player", new { accountId = other.Id });

        await EnterAsync();
        await _factory.CreatePlayerClient($"dev|{other.Id}").PostAsJsonAsync(Enter, new { characterId = otherCharacter.Id });

        Assert.Equal([HubSession, $"gyllencreutz-{otherGuild.Id}"], _factory.GameLift.CreatedSessionNames);
    }

    [Fact]
    public async Task Enter_WithoutTheClaim_IsForbidden()
    {
        await _admin.DeleteAsync($"/api/guilds/{_guild.Id}/characters/{_character.Id}/player");

        var response = await EnterAsync();

        Assert.Equal(HttpStatusCode.Forbidden, response.StatusCode);
    }

    [Fact]
    public async Task Enter_JustLeftARunningInstance_GoesBackThere()
    {
        _factory.GameLift.Activate("town");
        await SetLocationAsync(new CharacterLocation { WorldId = "town", SessionName = "town", EnteredAt = DateTime.UtcNow, LeftAt = _factory.Clock.GetUtcNow().UtcDateTime });
        _factory.Clock.Advance(TimeSpan.FromMinutes(2));

        var join = (await (await EnterAsync()).ReadResultAsync<JoinWorldResponse>()).Result!;

        Assert.Equal("town", join.Instance);
    }

    [Fact]
    public async Task Enter_LeftTooLongAgo_GoesToTheHub()
    {
        _factory.GameLift.Activate("town");
        _factory.GameLift.Activate(HubSession);
        await SetLocationAsync(new CharacterLocation { WorldId = "town", SessionName = "town", EnteredAt = DateTime.UtcNow, LeftAt = _factory.Clock.GetUtcNow().UtcDateTime });
        _factory.Clock.Advance(new SessionsOptions().ReturnWindow + TimeSpan.FromMinutes(1));

        var join = (await (await EnterAsync()).ReadResultAsync<JoinWorldResponse>()).Result!;

        Assert.Equal(HubSession, join.Instance);
    }

    [Fact]
    public async Task Enter_LastInstanceClosed_GoesToTheHub()
    {
        _factory.GameLift.Activate(HubSession);
        await SetLocationAsync(new CharacterLocation { WorldId = "town", SessionName = "town", EnteredAt = DateTime.UtcNow, LeftAt = _factory.Clock.GetUtcNow().UtcDateTime });

        var join = (await (await EnterAsync()).ReadResultAsync<JoinWorldResponse>()).Result!;

        Assert.Equal(HubSession, join.Instance);
    }

    [Fact]
    public async Task CharacterLeft_RecordsWhenAndFreesTheCharacter()
    {
        _factory.GameLift.Activate(HubSession);
        await EnterAsync();

        var response = await _admin.PostAsync($"/api/sessions/server/characters/{_character.Id}/left", null);

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        Assert.NotNull((await GetCharacterAsync()).LastLocation!.LeftAt);
        var guild = (await (await _admin.GetAsync($"/api/guilds/{_guild.Id}")).ReadResultAsync<Guild>()).Result!;
        Assert.Null(guild.GetPlayerOf(_character.Id));
    }

    [Fact]
    public async Task Heartbeat_KeepsTheClaim()
    {
        var other = (await (await _admin.PostAsJsonAsync("/api/accounts", new { username = "Johan" })).ReadResultAsync<Account>()).Result!;
        await _admin.PostAsJsonAsync($"/api/guilds/{_guild.Id}/members", new { accountId = other.Id });
        var timeout = new GuildOptions().CharacterClaimTimeout;

        _factory.Clock.Advance(timeout - TimeSpan.FromMinutes(1));
        await _admin.PostAsJsonAsync("/api/sessions/server/heartbeat", new { characterIds = new[] { _character.Id } });
        _factory.Clock.Advance(TimeSpan.FromMinutes(2));

        var response = await ClaimAsync(other);

        Assert.Equal(HttpStatusCode.Conflict, response.StatusCode);
        Assert.Equal(GuildService.CharacterTaken, (await response.ReadResultAsync<Guild>()).ErrorCode);
    }
}
