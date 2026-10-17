using System.Net;
using System.Net.Http.Json;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Common.Services;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Tests;

/// <summary>Guild endpoints (open in Development, which is what the test host runs as).</summary>
public sealed class GuildsTests : IAsyncLifetime
{
    private const string Guilds = "/api/guilds";

    private VaesenApiFactory _factory = null!;
    private HttpClient _client = null!;

    public Task InitializeAsync()
    {
        _factory = new VaesenApiFactory();
        _client = _factory.CreateClient();
        return Task.CompletedTask;
    }

    public async Task DisposeAsync() => await _factory.DisposeAsync();

    private async Task<Account> CreateAccountAsync(string username) =>
        (await (await _client.PostAsJsonAsync("/api/accounts", new { username })).ReadResultAsync<Account>()).Result!;

    private async Task<Account> GetAccountAsync(string accountId) =>
        (await (await _client.GetAsync($"/api/accounts/{accountId}")).ReadResultAsync<Account>()).Result!;

    private async Task<Guild> GetGuildAsync(string guildId) =>
        (await (await _client.GetAsync($"{Guilds}/{guildId}")).ReadResultAsync<Guild>()).Result!;

    private async Task<Guild> CreateGuildAsync(string name, Account founder) =>
        (await (await _client.PostAsJsonAsync(Guilds, new { name, createdByAccountId = founder.Id })).ReadResultAsync<Guild>()).Result!;

    private async Task<Character> CreateCharacterAsync(Guild guild, string firstName, Account creator) =>
        (await (await _client.PostAsJsonAsync($"{Guilds}/{guild.Id}/characters", new { firstName, lastName = "Falk", createdByAccountId = creator.Id }))
            .ReadResultAsync<Character>()).Result!;

    private Task<HttpResponseMessage> AssignAsync(Guild guild, Character character, Account player) =>
        _client.PutAsJsonAsync($"{Guilds}/{guild.Id}/characters/{character.Id}/player", new { accountId = player.Id });

    [Fact]
    public async Task Create_MakesFounderAMember_OnBothSides()
    {
        var founder = await CreateAccountAsync("Ingrid");

        var response = await _client.PostAsJsonAsync(Guilds, new { name = "The Society of Upsala", createdByAccountId = founder.Id });

        Assert.Equal(HttpStatusCode.Created, response.StatusCode);
        var guild = (await response.ReadResultAsync<Guild>()).Result!;
        Assert.True(ObjectCore.IsValidId<Guild>(guild.Id));
        Assert.Equal("The Society of Upsala", guild.Name);
        Assert.Equal(founder.Id, guild.CreatedBy);
        Assert.Equal(founder.Id, guild.GuildMasterAccountId);
        Assert.Equal([founder.Id], guild.MemberAccountIds);
        Assert.Equal([guild.Id], (await GetAccountAsync(founder.Id)).GuildIds);
    }

    [Fact]
    public async Task Create_WithTakenName_DifferentCase_IsConflict()
    {
        var founder = await CreateAccountAsync("Ingrid");
        await CreateGuildAsync("Lantern Watch", founder);

        var response = await _client.PostAsJsonAsync(Guilds, new { name = "LANTERN WATCH", createdByAccountId = founder.Id });

        Assert.Equal(HttpStatusCode.Conflict, response.StatusCode);
        Assert.Equal(GuildRepository.NameTaken, (await response.ReadResultAsync<Guild>()).ErrorCode);
    }

    [Theory]
    [InlineData("ab")]
    [InlineData("Bad<Name>")]
    public async Task Create_WithInvalidName_IsBadRequest(string name)
    {
        var founder = await CreateAccountAsync("Ingrid");

        var response = await _client.PostAsJsonAsync(Guilds, new { name, createdByAccountId = founder.Id });

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
    }

    [Fact]
    public async Task Create_WithUnknownFounder_IsNotFound()
    {
        var response = await _client.PostAsJsonAsync(Guilds, new { name = "Nobody's Guild", createdByAccountId = new Account().Id });

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    [Fact]
    public async Task Get_ByNameAndByAccount()
    {
        var founder = await CreateAccountAsync("Ingrid");
        var guild = await CreateGuildAsync("Lantern Watch", founder);

        var byName = (await (await _client.GetAsync($"{Guilds}/by-name/lantern%20watch")).ReadResultAsync<Guild>()).Result!;
        var byAccount = (await (await _client.GetAsync($"{Guilds}/by-account/{founder.Id}")).ReadResultAsync<List<Guild>>()).Result!;

        Assert.Equal(guild.Id, byName.Id);
        Assert.Equal(guild.Id, Assert.Single(byAccount).Id);
    }

    [Fact]
    public async Task AddMember_UpdatesGuildAndAccount_AndIsIdempotent()
    {
        var founder = await CreateAccountAsync("Ingrid");
        var member = await CreateAccountAsync("Johan");
        var guild = await CreateGuildAsync("Lantern Watch", founder);

        var first = await _client.PostAsJsonAsync($"{Guilds}/{guild.Id}/members", new { accountId = member.Id });
        var again = await _client.PostAsJsonAsync($"{Guilds}/{guild.Id}/members", new { accountId = member.Id });

        Assert.Equal(HttpStatusCode.OK, first.StatusCode);
        Assert.Equal(HttpStatusCode.OK, again.StatusCode);
        Assert.Equal([founder.Id, member.Id], (await GetGuildAsync(guild.Id)).MemberAccountIds);
        Assert.Equal([guild.Id], (await GetAccountAsync(member.Id)).GuildIds);
    }

    [Fact]
    public async Task CreateCharacter_BelongsToGuild_AndRequiresAMember()
    {
        var founder = await CreateAccountAsync("Ingrid");
        var outsider = await CreateAccountAsync("Sigrid");
        var guild = await CreateGuildAsync("Lantern Watch", founder);

        var response = await _client.PostAsJsonAsync($"{Guilds}/{guild.Id}/characters", new { firstName = "Astrid", lastName = "Falk", createdByAccountId = founder.Id });
        var byOutsider = await _client.PostAsJsonAsync($"{Guilds}/{guild.Id}/characters", new { firstName = "Erik", lastName = "Lund", createdByAccountId = outsider.Id });

        Assert.Equal(HttpStatusCode.Created, response.StatusCode);
        var character = (await response.ReadResultAsync<Character>()).Result!;
        Assert.True(ObjectCore.IsValidId<Character>(character.Id));
        Assert.Equal(guild.Id, character.GuildId);
        Assert.Equal([character.Id], (await GetGuildAsync(guild.Id)).CharacterIds);
        Assert.Equal(HttpStatusCode.BadRequest, byOutsider.StatusCode);
        Assert.Equal(GuildService.NotAMember, (await byOutsider.ReadResultAsync<Character>()).ErrorCode);

        var listed = (await (await _client.GetAsync($"{Guilds}/{guild.Id}/characters")).ReadResultAsync<List<Character>>()).Result!;
        Assert.Equal(character.Id, Assert.Single(listed).Id);
    }

    [Fact]
    public async Task CreateCharacter_WithTakenName_IsConflict()
    {
        var founder = await CreateAccountAsync("Ingrid");
        var guild = await CreateGuildAsync("Lantern Watch", founder);
        await CreateCharacterAsync(guild, "Astrid", founder);

        var response = await _client.PostAsJsonAsync($"{Guilds}/{guild.Id}/characters", new { firstName = "astrid", lastName = "FALK", createdByAccountId = founder.Id });

        Assert.Equal(HttpStatusCode.Conflict, response.StatusCode);
        Assert.Equal(CharacterRepository.NameTaken, (await response.ReadResultAsync<Character>()).ErrorCode);
    }

    [Fact]
    public async Task AssignPlayer_RecordsWhoPlaysTheCharacter()
    {
        var founder = await CreateAccountAsync("Ingrid");
        var guild = await CreateGuildAsync("Lantern Watch", founder);
        var character = await CreateCharacterAsync(guild, "Astrid", founder);

        var response = await AssignAsync(guild, character, founder);

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        var updated = (await response.ReadResultAsync<Guild>()).Result!;
        Assert.Equal(founder.Id, updated.GetPlayerOf(character.Id));
        Assert.Equal(character.Id, updated.GetCharacterOf(founder.Id));
    }

    [Fact]
    public async Task AssignPlayer_ToAnotherMember_TakesTheCharacterOver()
    {
        var x = await CreateAccountAsync("PlayerX");
        var k = await CreateAccountAsync("PlayerK");
        var guild = await CreateGuildAsync("Lantern Watch", x);
        await _client.PostAsJsonAsync($"{Guilds}/{guild.Id}/members", new { accountId = k.Id });
        var y = await CreateCharacterAsync(guild, "Ylva", x);
        var i = await CreateCharacterAsync(guild, "Ivar", x);

        await AssignAsync(guild, y, x);    // X plays Y.
        await AssignAsync(guild, i, x);    // Next time X plays I instead: Y becomes free.
        await AssignAsync(guild, y, k);    // K plays Y.
        var final = (await (await AssignAsync(guild, i, k)).ReadResultAsync<Guild>()).Result!; // K takes I over from X.

        Assert.Equal(k.Id, final.GetPlayerOf(i.Id));
        Assert.Null(final.GetPlayerOf(y.Id));          // K moved from Y to I.
        Assert.Null(final.GetCharacterOf(x.Id));       // X lost I to K.
        Assert.Single(final.CharacterAssignments);
    }

    [Fact]
    public async Task AssignPlayer_RequiresMemberAndGuildsCharacter()
    {
        var founder = await CreateAccountAsync("Ingrid");
        var outsider = await CreateAccountAsync("Sigrid");
        var guild = await CreateGuildAsync("Lantern Watch", founder);
        var otherGuild = await CreateGuildAsync("Other Guild", outsider);
        var character = await CreateCharacterAsync(guild, "Astrid", founder);
        var otherCharacter = await CreateCharacterAsync(otherGuild, "Erik", outsider);

        var byOutsider = await AssignAsync(guild, character, outsider);
        var wrongGuild = await AssignAsync(guild, otherCharacter, founder);

        Assert.Equal(GuildService.NotAMember, (await byOutsider.ReadResultAsync<Guild>()).ErrorCode);
        Assert.Equal(GuildService.CharacterNotInGuild, (await wrongGuild.ReadResultAsync<Guild>()).ErrorCode);
    }

    [Fact]
    public async Task ReleasePlayer_FreesTheCharacter()
    {
        var founder = await CreateAccountAsync("Ingrid");
        var guild = await CreateGuildAsync("Lantern Watch", founder);
        var character = await CreateCharacterAsync(guild, "Astrid", founder);
        await AssignAsync(guild, character, founder);

        var response = await _client.DeleteAsync($"{Guilds}/{guild.Id}/characters/{character.Id}/player");

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        Assert.Empty((await GetGuildAsync(guild.Id)).CharacterAssignments);
    }

    [Fact]
    public async Task RemoveMember_UpdatesBothSides_AndFreesTheirCharacter()
    {
        var founder = await CreateAccountAsync("Ingrid");
        var member = await CreateAccountAsync("Johan");
        var guild = await CreateGuildAsync("Lantern Watch", founder);
        await _client.PostAsJsonAsync($"{Guilds}/{guild.Id}/members", new { accountId = member.Id });
        var character = await CreateCharacterAsync(guild, "Astrid", founder);
        await AssignAsync(guild, character, member);

        var response = await _client.DeleteAsync($"{Guilds}/{guild.Id}/members/{member.Id}");

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        var updated = await GetGuildAsync(guild.Id);
        Assert.Equal([founder.Id], updated.MemberAccountIds);
        Assert.Null(updated.GetPlayerOf(character.Id));
        Assert.Empty((await GetAccountAsync(member.Id)).GuildIds);
        Assert.Equal([character.Id], updated.CharacterIds); // The character stays with the guild.
    }

    [Fact]
    public async Task RemoveMember_GuildMaster_IsRefused()
    {
        var founder = await CreateAccountAsync("Ingrid");
        var guild = await CreateGuildAsync("Lantern Watch", founder);

        var response = await _client.DeleteAsync($"{Guilds}/{guild.Id}/members/{founder.Id}");

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
        Assert.Equal(GuildService.GuildMasterCannotLeave, (await response.ReadResultAsync<Guild>()).ErrorCode);
        Assert.Equal([founder.Id], (await GetGuildAsync(guild.Id)).MemberAccountIds);
    }

    [Fact]
    public async Task Get_UnknownOrMalformedId()
    {
        Assert.Equal(HttpStatusCode.NotFound, (await _client.GetAsync($"{Guilds}/{new Guild().Id}")).StatusCode);
        Assert.Equal(HttpStatusCode.BadRequest, (await _client.GetAsync($"{Guilds}/account-{Guid.NewGuid()}")).StatusCode);
    }
}
