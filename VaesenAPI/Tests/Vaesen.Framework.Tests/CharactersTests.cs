using System.Net;
using System.Net.Http.Json;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;
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

    private async Task<Character> CreateCharacterAsync(string firstName = "Astrid", string lastName = "Falk") =>
        (await (await CreateAsync(new { firstName, lastName, @class = "Occultist", healthPoints = 42, createdByAccountId = _founder.Id }))
            .ReadResultAsync<Character>()).Result!;

    private Task<HttpResponseMessage> PatchAsync(string characterId, object body) =>
        _client.PatchAsJsonAsync($"{Characters}/{characterId}", body);

    [Fact]
    public async Task Create_StoresAllFields_AndNameIsFirstPlusLastName()
    {
        var response = await CreateAsync(new
        {
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
        Assert.Equal("Astrid Falk", character.Name);
        Assert.Equal("Astrid", character.FirstName);
        Assert.Equal("Falk", character.LastName);
        Assert.Equal("Occultist", character.Class);
        Assert.Equal(42, character.HealthPoints);
        Assert.Equal(_founder.Id, character.CreatedBy);
    }

    [Fact]
    public async Task Create_WithoutOptionalFields_UsesDefaults()
    {
        var character = (await (await CreateAsync(new { firstName = "Erik", lastName = "Lund", createdByAccountId = _founder.Id }))
            .ReadResultAsync<Character>()).Result!;

        Assert.Equal("Erik Lund", character.Name);
        Assert.Null(character.Class);
        Assert.Equal(Character.DefaultHealthPoints, character.HealthPoints);
    }

    [Theory]
    [InlineData(null, "Falk", null, null)]
    [InlineData("Astrid", null, null, null)]
    [InlineData("", "Falk", null, null)]
    [InlineData("Bad<Name>", "Falk", null, null)]
    [InlineData("Astrid", "Falk", "Class 123", null)]
    [InlineData("Astrid", "Falk", null, -1)]
    public async Task Create_WithMissingOrInvalidFields_IsBadRequest(string? firstName, string? lastName, string? characterClass, int? healthPoints)
    {
        var response = await CreateAsync(new { firstName, lastName, @class = characterClass, healthPoints, createdByAccountId = _founder.Id });

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
        Assert.Equal(ErrorCodes.Invalid, (await response.ReadResultAsync<Character>()).ErrorCode);
    }

    [Fact]
    public async Task Create_WithTakenFullName_DifferentCase_IsConflict()
    {
        await CreateCharacterAsync("Astrid", "Falk");

        var response = await CreateAsync(new { firstName = "ASTRID", lastName = "falk", createdByAccountId = _founder.Id });

        Assert.Equal(HttpStatusCode.Conflict, response.StatusCode);
        Assert.Equal(CharacterRepository.NameTaken, (await response.ReadResultAsync<Character>()).ErrorCode);
    }

    [Fact]
    public async Task Create_SameFirstName_DifferentLastName_IsFine()
    {
        await CreateCharacterAsync("Astrid", "Falk");

        var response = await CreateAsync(new { firstName = "Astrid", lastName = "Lund", createdByAccountId = _founder.Id });

        Assert.Equal(HttpStatusCode.Created, response.StatusCode);
    }

    [Fact]
    public async Task Get_ByIdAndByName()
    {
        var character = await CreateCharacterAsync();

        var byId = (await (await _client.GetAsync($"{Characters}/{character.Id}")).ReadResultAsync<Character>()).Result!;
        var byName = (await (await _client.GetAsync($"{Characters}/by-name/astrid%20falk")).ReadResultAsync<Character>()).Result!;

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
        Assert.Equal("Astrid Falk", updated.Name);
        Assert.Equal(_founder.Id, updated.UpdatedBy);
        Assert.Equal(2, updated.Version);
    }

    [Fact]
    public async Task Update_LastName_RenamesTheCharacter_AndFreesTheOldName()
    {
        var character = await CreateCharacterAsync();

        var updated = (await (await PatchAsync(character.Id, new { lastName = "Lund" })).ReadResultAsync<Character>()).Result!;

        Assert.Equal("Astrid Lund", updated.Name);
        Assert.Equal(character.Id, (await (await _client.GetAsync($"{Characters}/by-name/Astrid%20Lund")).ReadResultAsync<Character>()).Result!.Id);
        Assert.Equal(HttpStatusCode.NotFound, (await _client.GetAsync($"{Characters}/by-name/Astrid%20Falk")).StatusCode);
        Assert.Equal(HttpStatusCode.Created, (await CreateAsync(new { firstName = "Astrid", lastName = "Falk", createdByAccountId = _founder.Id })).StatusCode);
    }

    [Fact]
    public async Task Update_RenameToATakenName_IsConflict()
    {
        await CreateCharacterAsync("Astrid", "Lund");
        var character = await CreateCharacterAsync("Astrid", "Falk");

        var response = await PatchAsync(character.Id, new { lastName = "Lund" });

        Assert.Equal(HttpStatusCode.Conflict, response.StatusCode);
        Assert.Equal("Astrid Falk", (await (await _client.GetAsync($"{Characters}/{character.Id}")).ReadResultAsync<Character>()).Result!.Name);
    }

    [Fact]
    public async Task Update_CannotClearFirstOrLastName_ButCanClearClass()
    {
        var character = await CreateCharacterAsync();

        var clearName = await PatchAsync(character.Id, new { lastName = "" });
        var clearClass = (await (await PatchAsync(character.Id, new { @class = "" })).ReadResultAsync<Character>()).Result!;

        Assert.Equal(HttpStatusCode.BadRequest, clearName.StatusCode);
        Assert.Null(clearClass.Class);
        Assert.Equal("Falk", clearClass.LastName);
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
        Assert.Equal(HttpStatusCode.Created, (await CreateAsync(new { firstName = "Astrid", lastName = "Falk", createdByAccountId = _founder.Id })).StatusCode);
    }
}
