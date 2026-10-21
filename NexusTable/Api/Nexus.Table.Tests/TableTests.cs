using System.Net;
using System.Net.Http.Json;
using Nexus.Table.Models;

namespace Nexus.Table.Tests;

public sealed class TableTests : IDisposable
{
    private readonly NexusApiFactory _factory = new();

    public void Dispose() => _factory.Dispose();

    [Fact]
    public async Task First_sign_in_creates_an_account_in_the_society()
    {
        var host = await _factory.ClientForAsync("mattias");
        var player = await _factory.ClientForAsync("ingrid");

        var me = await NexusApiFactory.ResultAsync<Account>(await host.GetAsync("/api/me"));
        await player.GetAsync("/api/me");
        var society = await NexusApiFactory.ResultAsync<SocietyView>(await player.GetAsync("/api/society"));

        Assert.Equal(AccountRole.Host, me.Role);
        Assert.Equal("Mattias", me.DisplayName);
        Assert.Equal(["Mattias", "Ingrid"], society.Members.Select(m => m.DisplayName));
    }

    [Fact]
    public async Task Only_the_host_renames_the_society()
    {
        var player = await _factory.ClientForAsync("ingrid");
        var host = await _factory.ClientForAsync("mattias");

        var refused = await player.PatchAsJsonAsync("/api/society", new { name = "Ingrid's Club" });
        var renamed = await NexusApiFactory.ResultAsync<Society>(await host.PatchAsJsonAsync("/api/society", new { name = "The Lantern Society" }));

        Assert.Equal(HttpStatusCode.Forbidden, refused.StatusCode);
        Assert.Equal("The Lantern Society", renamed.Name);
    }

    [Fact]
    public async Task A_new_character_becomes_the_one_you_play_and_archetypes_are_unique()
    {
        var ingrid = await _factory.ClientForAsync("ingrid");
        var johan = await _factory.ClientForAsync("johan");

        var character = await NexusApiFactory.ResultAsync<Character>(await ingrid.PostAsJsonAsync("/api/characters", Requests.Academic()));
        var me = await NexusApiFactory.ResultAsync<Account>(await ingrid.GetAsync("/api/me"));
        var second = await johan.PostAsJsonAsync("/api/characters", Requests.Academic("Albert Brugge"));

        Assert.Equal(character.Id, me.ActiveCharacterId);
        Assert.Equal(12, character.Skills.Count);
        Assert.Equal(HttpStatusCode.Conflict, second.StatusCode);
        Assert.Equal("archetype_taken", (await NexusApiFactory.ReadAsync(second))["errorCode"]!.ToString());
    }

    [Fact]
    public async Task A_roll_counts_sixes_and_conditions_cost_dice()
    {
        var ingrid = await _factory.ClientForAsync("ingrid");
        var character = await NexusApiFactory.ResultAsync<Character>(await ingrid.PostAsJsonAsync("/api/characters", Requests.Academic()));
        await ingrid.PutAsJsonAsync($"/api/characters/{character.Id}/conditions", new { condition = "frightened", active = true });

        // Logic 5 + Learning 3 + difficulty -1 - Frightened = 6 dice.
        _factory.Dice.Queue(6, 2, 6, 3, 1, 4);
        var outcome = await NexusApiFactory.ResultAsync<RollOutcome>(
            await ingrid.PostAsJsonAsync("/api/rolls", new { characterId = character.Id, skill = "learning", difficulty = -1 }));

        Assert.Equal(6, outcome.Roll.PoolSize);
        Assert.Equal(2, outcome.Roll.Successes);
        Assert.Equal(["Logic", "Learning", "Difficulty", "Frightened"], outcome.Roll.Pool.Select(p => p.Label));
    }

    [Fact]
    public async Task Pushing_a_failed_roll_rerolls_and_costs_a_condition_once()
    {
        var ingrid = await _factory.ClientForAsync("ingrid");
        var character = await NexusApiFactory.ResultAsync<Character>(await ingrid.PostAsJsonAsync("/api/characters", Requests.Academic()));

        // Physique 3 + Agility 0: three failures, then the push turns one into a six.
        _factory.Dice.Queue(1, 2, 3, 6, 4, 5);
        var roll = await NexusApiFactory.ResultAsync<RollOutcome>(
            await ingrid.PostAsJsonAsync("/api/rolls", new { characterId = character.Id, skill = "agility" }));
        var mental = await ingrid.PostAsJsonAsync($"/api/rolls/{roll.Roll.Id}/push", new { condition = "angry" });
        var pushed = await NexusApiFactory.ResultAsync<RollOutcome>(
            await ingrid.PostAsJsonAsync($"/api/rolls/{roll.Roll.Id}/push", new { condition = "exhausted" }));
        var again = await ingrid.PostAsJsonAsync($"/api/rolls/{roll.Roll.Id}/push", new { condition = "battered" });

        Assert.Equal(0, roll.Roll.Successes);
        Assert.Equal(HttpStatusCode.BadRequest, mental.StatusCode);
        Assert.Equal([6, 4, 5], pushed.Roll.PushedDice);
        Assert.Equal(1, pushed.Roll.Successes);
        Assert.Equal(["exhausted"], pushed.Character.Conditions);
        Assert.Equal("already_pushed", (await NexusApiFactory.ReadAsync(again))["errorCode"]!.ToString());
    }

    [Fact]
    public async Task Players_cannot_roll_for_someone_else()
    {
        var ingrid = await _factory.ClientForAsync("ingrid");
        var johan = await _factory.ClientForAsync("johan");
        var character = await NexusApiFactory.ResultAsync<Character>(await ingrid.PostAsJsonAsync("/api/characters", Requests.Academic()));

        var response = await johan.PostAsJsonAsync("/api/rolls", new { characterId = character.Id, skill = "learning" });

        Assert.Equal(HttpStatusCode.Forbidden, response.StatusCode);
    }
}
