using Amazon.DynamoDBv2;
using Amazon.Runtime;
using Microsoft.Extensions.Options;
using Testcontainers.DynamoDb;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Core.Data;
using Vaesen.Framework.Tests.Fakes;

namespace Vaesen.Framework.Tests.Persistence;

public class InMemoryStoreTests : StoreContract
{
    private readonly InMemoryDatabase _database = new();

    protected override IAccountStore Accounts => _database;

    protected override IGuildStore Guilds => _database;

    protected override ICharacterStore Characters => _database;
}

/// <summary>The real repositories against DynamoDB Local. Skipped when Docker isn't available.</summary>
public class DynamoDbStoreTests(DynamoDbFixture fixture) : StoreContract, IClassFixture<DynamoDbFixture>
{
    protected override IAccountStore Accounts => fixture.Accounts;

    protected override IGuildStore Guilds => fixture.Guilds;

    protected override ICharacterStore Characters => fixture.Characters;

    protected override void SkipIfUnavailable() => DynamoDbFixture.SkipIfDockerUnavailable();
}

/// <summary>DynamoDB Local in a throwaway Docker container, with the Vaesen table and the repositories on it.</summary>
public sealed class DynamoDbFixture : IAsyncLifetime
{
    private const string TableName = "vaesen-test";

    private DynamoDbContainer? _container;
    private IAmazonDynamoDB? _client;

    /// <summary>Docker Desktop on Windows exposes a named pipe; Linux/macOS a socket.</summary>
    public static bool IsDockerAvailable { get; } =
        File.Exists(OperatingSystem.IsWindows() ? @"\\.\pipe\docker_engine" : "/var/run/docker.sock");

    public AccountRepository Accounts { get; private set; } = null!;

    public CharacterRepository Characters { get; private set; } = null!;

    public GuildRepository Guilds { get; private set; } = null!;

    public static void SkipIfDockerUnavailable() =>
        Skip.IfNot(IsDockerAvailable, "Docker is not available; install Docker Desktop to run DynamoDB tests.");

    public async Task InitializeAsync()
    {
        if (!IsDockerAvailable)
        {
            return;
        }

        _container = new DynamoDbBuilder("amazon/dynamodb-local:latest").Build();
        await _container.StartAsync();

        _client = new AmazonDynamoDBClient(
            new BasicAWSCredentials("test", "test"),
            new AmazonDynamoDBConfig { ServiceURL = _container.GetConnectionString() });
        await DynamoDbTable.CreateAsync(_client, TableName, CancellationToken.None);

        var options = Options.Create(new DataServiceOptions { TableName = TableName });
        Accounts = new AccountRepository(_client, options, TimeProvider.System);
        Characters = new CharacterRepository(_client, options, TimeProvider.System);
        Guilds = new GuildRepository(_client, options, TimeProvider.System, Accounts, Characters);
    }

    public async Task DisposeAsync()
    {
        _client?.Dispose();
        if (_container is not null)
        {
            await _container.DisposeAsync();
        }
    }
}
