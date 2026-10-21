using Amazon.DynamoDBv2;
using Amazon.DynamoDBv2.Model;
using Amazon.Runtime;
using Microsoft.Extensions.Options;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Core.Data;

namespace Vaesen.Framework.Tests.Persistence;

/// <summary>
/// How models are turned into DynamoDB items and back. Runs without DynamoDB (no calls are made),
/// so the mapping is covered even where the Docker-based repository tests are skipped.
/// </summary>
public class BaseRepositoryMappingTests
{
    [Fact]
    public void Account_RoundTripsThroughItem_WithKeysAndType()
    {
        var account = new Account
        {
            Username = "Ingrid",
            Email = "ingrid@example.com",
            Auth0UserId = "auth0|abc",
            Subscription = new AccountSubscription
            {
                Status = SubscriptionStatus.Active,
                Plan = "monthly",
                CurrentPeriodEnd = new DateTime(2030, 1, 1, 12, 30, 0, DateTimeKind.Utc),
            },
            GuildIds = ["guild-1a2b3c4d-0000-4000-8000-000000000001", "guild-1a2b3c4d-0000-4000-8000-000000000002"],
            CreatedAt = new DateTime(2026, 10, 17, 9, 0, 0, DateTimeKind.Utc),
            Version = 3,
        };
        var repository = new MappingProbe<Account>();

        var item = repository.ToItemPublic(account);
        var loaded = MappingProbe<Account>.FromItemPublic(item);

        Assert.Equal(account.Id, item["PK"].S);
        Assert.Equal(BaseRepository<Account>.ItemSortKey, item["SK"].S);
        Assert.Equal("Account", item["Type"].S);
        Assert.Equal("Active", item["subscription"].M["status"].S);
        Assert.Equal(account.Id, loaded.Id);
        Assert.Equal("Ingrid", loaded.Username);
        Assert.Equal("auth0|abc", loaded.Auth0UserId);
        Assert.Equal(SubscriptionStatus.Active, loaded.Subscription.Status);
        Assert.Equal(account.Subscription.CurrentPeriodEnd, loaded.Subscription.CurrentPeriodEnd);
        Assert.Equal(DateTimeKind.Utc, loaded.CreatedAt.Kind);
        Assert.Equal(account.CreatedAt, loaded.CreatedAt);
        Assert.Equal(3, loaded.Version);
        Assert.Equal(2, item["guildIds"].L.Count);
        Assert.Equal(account.GuildIds, loaded.GuildIds);
    }

    [Fact]
    public void EmptyGuildIds_RoundTripAsEmptyList()
    {
        var repository = new MappingProbe<Account>();

        var loaded = MappingProbe<Account>.FromItemPublic(repository.ToItemPublic(new Account { Username = "Johan" }));

        Assert.NotNull(loaded.GuildIds);
        Assert.Empty(loaded.GuildIds);
    }

    [Fact]
    public void NullValues_AreLeftOut()
    {
        var item = new MappingProbe<Account>().ToItemPublic(new Account { Username = "Johan" });

        Assert.False(item.ContainsKey("email"));
        Assert.False(item.ContainsKey("auth0UserId"));
    }

    [Fact]
    public void Character_Appearance_IsANestedMap_AndRoundTrips()
    {
        var character = new Character
        {
            GuildId = "guild-3f2b8c1e-1d2c-4b5a-9e8f-0123456789ab",
            Name = "Astrid Falk",
            Appearance = new CharacterAppearance
            {
                Sex = "female",
                FacePresetId = "face_f_07",
                SkinTone = new SkinTone { U = 0.42f, V = 0.3f },
                FaceSliders = { ["jawWidth"] = 0.2f },
                HairColor = new HairColor { Melanin = 0.7f },
            },
        };
        var repository = new MappingProbe<Character>();

        var item = repository.ToItemPublic(character);
        var loaded = MappingProbe<Character>.FromItemPublic(item);

        Assert.Equal("face_f_07", item["appearance"].M["facePresetId"].S);
        Assert.NotNull(item["appearance"].M["faceSliders"].M["jawWidth"].N);
        Assert.Equal("face_f_07", loaded.Appearance!.FacePresetId);
        Assert.Equal(0.42f, loaded.Appearance.SkinTone.U, 3);
        Assert.Equal(0.2f, loaded.Appearance.FaceSliders["jawWidth"], 3);
    }

    [Fact]
    public void Character_WithoutAppearance_HasNoAppearanceAttribute()
    {
        var item = new MappingProbe<Character>().ToItemPublic(new Character { GuildId = "guild-3f2b8c1e-1d2c-4b5a-9e8f-0123456789ab", Name = "Erik Lund" });

        Assert.False(item.ContainsKey("appearance"));
    }

    [Fact]
    public void Character_IsIndexedUnderItsGuild()
    {
        var character = new Character { GuildId = "guild-3f2b8c1e-1d2c-4b5a-9e8f-0123456789ab", Name = "Astrid" };
        var repository = new CharacterRepository(OfflineClient(), Options(), TimeProvider.System);

        var item = typeof(CharacterRepository).BaseType!
            .GetMethod("ToItem", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic)!
            .Invoke(repository, [character]) as Dictionary<string, AttributeValue>;

        Assert.Equal("GUILD#guild-3f2b8c1e-1d2c-4b5a-9e8f-0123456789ab", item!["GSI1PK"].S);
        Assert.Equal("CHAR#Astrid", item["GSI1SK"].S);
    }

    private static IAmazonDynamoDB OfflineClient() =>
        new AmazonDynamoDBClient(new BasicAWSCredentials("test", "test"), new AmazonDynamoDBConfig { ServiceURL = "http://127.0.0.1:1" });

    private static IOptions<DataServiceOptions> Options() =>
        Microsoft.Extensions.Options.Options.Create(new DataServiceOptions { TableName = "vaesen-test" });

    private sealed class MappingProbe<T>() : BaseRepository<T>(OfflineClient(), Options(), TimeProvider.System)
        where T : Vaesen.Framework.Core.Models.BaseCore
    {
        public Dictionary<string, AttributeValue> ToItemPublic(T item) => ToItem(item);

        public static T FromItemPublic(Dictionary<string, AttributeValue> item) => FromItem(item);
    }
}
