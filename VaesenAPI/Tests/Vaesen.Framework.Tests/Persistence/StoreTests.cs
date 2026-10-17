using Amazon.DynamoDBv2;
using Amazon.Runtime;
using Microsoft.Extensions.Options;
using Testcontainers.DynamoDb;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Core.Data;
using Vaesen.Framework.Tests.Fakes;

namespace Vaesen.Framework.Tests.Persistence;

public class InMemoryAccountStoreTests : AccountStoreContract
{
    protected override IAccountStore Store { get; } = new InMemoryAccountStore();
}

public class InMemoryCharacterStoreTests : CharacterStoreContract
{
    protected override ICharacterStore Store { get; } = new InMemoryCharacterStore();
}

/// <summary>The real AccountRepository against DynamoDB Local. Skipped when Docker isn't available.</summary>
public class AccountRepositoryTests(DynamoDbFixture fixture) : AccountStoreContract, IClassFixture<DynamoDbFixture>
{
    protected override IAccountStore Store => fixture.CreateAccountRepository();

    protected override void SkipIfUnavailable() => DynamoDbFixture.SkipIfDockerUnavailable();
}

/// <summary>The real CharacterRepository against DynamoDB Local. Skipped when Docker isn't available.</summary>
public class CharacterRepositoryTests(DynamoDbFixture fixture) : CharacterStoreContract, IClassFixture<DynamoDbFixture>
{
    protected override ICharacterStore Store => fixture.CreateCharacterRepository();

    protected override void SkipIfUnavailable() => DynamoDbFixture.SkipIfDockerUnavailable();
}

/// <summary>DynamoDB Local in a throwaway Docker container, with the Vaesen table created.</summary>
public sealed class DynamoDbFixture : IAsyncLifetime
{
    private const string TableName = "vaesen-test";

    private DynamoDbContainer? _container;
    private IAmazonDynamoDB? _client;

    /// <summary>Docker Desktop on Windows exposes a named pipe; Linux/macOS a socket.</summary>
    public static bool IsDockerAvailable { get; } =
        File.Exists(OperatingSystem.IsWindows() ? @"\\.\pipe\docker_engine" : "/var/run/docker.sock");

    private static IOptions<DataServiceOptions> TableOptions => Options.Create(new DataServiceOptions { TableName = TableName });

    public static void SkipIfDockerUnavailable() =>
        Skip.IfNot(IsDockerAvailable, "Docker is not available; install Docker Desktop to run DynamoDB tests.");

    public AccountRepository CreateAccountRepository() => new(_client!, TableOptions, TimeProvider.System);

    public CharacterRepository CreateCharacterRepository() => new(_client!, TableOptions, TimeProvider.System);

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
