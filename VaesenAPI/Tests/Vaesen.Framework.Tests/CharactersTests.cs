using System.Net;
using System.Net.Http.Json;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Tests;

/// <summary>Character endpoints (open in Development, which is what the test host runs as).</summary>
public sealed class CharactersTests : IAsyncLifetime
{
    private const string Characters = "/api/characters";

    private VaesenApiFactory _factory = null!;
    private HttpClient _client = null!;
    private Account _founder = null!;
    private Guild _guild = null!;

    public async Task InitializeAsync()
    {
        _factory = new VaesenApiFactory();
        _client = _factory.CreateClient();
        _founder = (await (await _client.PostAsJsonAsync("/api/accounts", new { username = "Ingrid" })).ReadResultAsync<Account>()).Result!;
        _guild = (await (await _client.PostAsJsonAsync("/api/guilds", new { name = "Lantern Watch", createdByAccountId = _founder.Id }))
            .ReadResultAsync<Guild>()).Result!;
    }

    public async Task DisposeAsync() => await _factory.DisposeAsync();

    private Task<HttpResponseMessage> CreateAsync(object body) =>
        _client.PostAsJsonAsync($"/api/guilds/{_guild.Id}/characters", body);

    private async Task<Character> CreateCharacterAsync(string name = "Astrid") =>
        (await (await CreateAsync(new { name, firstName = "Astrid", lastName = "Falk", @class = "Occultist", healthPoints = 42, createdByAccountId = _founder.Id }))
            .ReadResultAsync<Character>()).Result!;

    private Task<HttpResponseMessage> PatchAsync(string characterId, object body) =>
        _client.PatchAsJsonAsync($"{Characters}/{characterId}", body);

    [Fact]
    public async Task Create_StoresAllFields()
    {
        var response = await CreateAsync(new
        {
            name = "Astrid",
            firstName = "Astrid",
            lastName = "Falk",
            @class = "Occultist",
            healthPoints = 42,
            createdByAccountId = _founder.Id,
        });

        Assert.Equal(HttpStatusCode.Created, response.StatusCode);
        var character = (await response.ReadResultAsync<Character>()).Result!;
        Assert.True(ObjectCore.IsValidId<Character>(character.Id));
        Assert.Equal(_guild.Id, character.GuildId);
        Assert.Equal("Astrid", character.Name);
        Assert.Equal("Astrid", character.FirstName);
        Assert.Equal("Falk", character.LastName);
        Assert.Equal("Occultist", character.Class);
        Assert.Equal(42, character.HealthPoints);
        Assert.Equal(_founder.Id, character.CreatedBy);
    }

    [Fact]
    public async Task Create_WithoutOptionalFields_UsesDefaults()
    {
        var character = (await (await CreateAsync(new { name = "Erik", createdByAccountId = _founder.Id })).ReadResultAsync<Character>()).Result!;

        Assert.Null(character.FirstName);
        Assert.Null(character.Class);
        Assert.Equal(Character.DefaultHealthPoints, character.HealthPoints);
    }

    [Theory]
    [InlineData("x", null, null, null)]
    [InlineData("Astrid", "Bad<Name>", null, null)]
    [InlineData("Astrid", null, "Class 123", null)]
    [InlineData("Astrid", null, null, -1)]
    public async Task Create_WithInvalidFields_IsBadRequest(string name, string? firstName, string? characterClass, int? healthPoints)
    {
        var response = await CreateAsync(new { name, firstName, @class = characterClass, healthPoints, createdByAccountId = _founder.Id });

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
        Assert.Equal(ErrorCodes.Invalid, (await response.ReadResultAsync<Character>()).ErrorCode);
    }

    [Fact]
    public async Task Get_ByIdAndByName()
    {
        var character = await CreateCharacterAsync();

        var byId = (await (await _client.GetAsync($"{Characters}/{character.Id}")).ReadResultAsync<Character>()).Result!;
        var byName = (await (await _client.GetAsync($"{Characters}/by-name/astrid")).ReadResultAsync<Character>()).Result!;

        Assert.Equal(character.Id, byId.Id);
        Assert.Equal(character.Id, byName.Id);
    }

    [Fact]
    public async Task Get_UnknownOrMalformedId()
    {
        Assert.Equal(HttpStatusCode.NotFound, (await _client.GetAsync($"{Characters}/{new Character().Id}")).StatusCode);
        Assert.Equal(HttpStatusCode.BadRequest, (await _client.GetAsync($"{Characters}/guild-{Guid.NewGuid()}")).StatusCode);
    }

    [Fact]
    public async Task Update_ChangesOnlyTheGivenFields()
    {
        var character = await CreateCharacterAsync();

        var response = await PatchAsync(character.Id, new { healthPoints = 7, @class = "Doctor", updatedByAccountId = _founder.Id });

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        var updated = (await response.ReadResultAsync<Character>()).Result!;
        Assert.Equal(7, updated.HealthPoints);
        Assert.Equal("Doctor", updated.Class);
        Assert.Equal("Astrid", updated.FirstName);
        Assert.Equal("Falk", updated.LastName);
        Assert.Equal("Astrid", updated.Name);
        Assert.Equal(_founder.Id, updated.UpdatedBy);
        Assert.Equal(2, updated.Version);
    }

    [Fact]
    public async Task Update_WithEmptyString_ClearsTheField()
    {
        var character = await CreateCharacterAsync();

        var updated = (await (await PatchAsync(character.Id, new { lastName = "" })).ReadResultAsync<Character>()).Result!;

        Assert.Null(updated.LastName);
        Assert.Equal("Astrid", updated.FirstName);
    }

    [Fact]
    public async Task Update_WithInvalidHealth_IsBadRequest()
    {
        var character = await CreateCharacterAsync();

        var response = await PatchAsync(character.Id, new { healthPoints = -5 });

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
    }

    [Fact]
    public async Task Delete_RemovesCharacterFromGuild_StopsItsPlayer_AndFreesTheName()
    {
        var character = await CreateCharacterAsync();
        await _client.PutAsJsonAsync($"/api/guilds/{_guild.Id}/characters/{character.Id}/player", new { accountId = _founder.Id });

        var response = await _client.DeleteAsync($"{Characters}/{character.Id}");

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        Assert.Equal(HttpStatusCode.NotFound, (await _client.GetAsync($"{Characters}/{character.Id}")).StatusCode);
        var guild = (await (await _client.GetAsync($"/api/guilds/{_guild.Id}")).ReadResultAsync<Guild>()).Result!;
        Assert.Empty(guild.CharacterIds);
        Assert.Empty(guild.CharacterAssignments);
        Assert.Equal(HttpStatusCode.Created, (await CreateAsync(new { name = "Astrid", createdByAccountId = _founder.Id })).StatusCode);
    }
}
