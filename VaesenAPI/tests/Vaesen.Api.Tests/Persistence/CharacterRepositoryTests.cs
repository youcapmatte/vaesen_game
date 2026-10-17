using Amazon.DynamoDBv2;
using Amazon.Runtime;
using Microsoft.Extensions.Options;
using Testcontainers.DynamoDb;
using Vaesen.Api.Infrastructure.Persistence;
using Vaesen.Api.Modules.Players;

namespace Vaesen.Api.Tests.Persistence;

public class InMemoryCharacterRepositoryTests : CharacterRepositoryContract
{
    protected override ICharacterRepository Repository { get; } = new InMemoryCharacterRepository();
}

/// <summary>
/// The real repository against DynamoDB Local in a throwaway Docker container.
/// Skipped when Docker isn't available on this machine.
/// </summary>
public class DynamoDbCharacterRepositoryTests(DynamoDbFixture fixture)
    : CharacterRepositoryContract, IClassFixture<DynamoDbFixture>
{
    protected override ICharacterRepository Repository => fixture.CreateCharacterRepository();

    protected override void SkipIfUnavailable() =>
        Skip.IfNot(DynamoDbFixture.IsDockerAvailable, "Docker is not available; install Docker Desktop to run DynamoDB tests.");
}

public sealed class DynamoDbFixture : IAsyncLifetime
{
    private const string TableName = "vaesen-test";

    private DynamoDbContainer? _container;
    private IAmazonDynamoDB? _client;

    /// <summary>Docker Desktop on Windows exposes a named pipe; Linux/macOS a socket.</summary>
    public static bool IsDockerAvailable { get; } =
        File.Exists(OperatingSystem.IsWindows() ? @"\\.\pipe\docker_engine" : "/var/run/docker.sock");

    public ICharacterRepository CreateCharacterRepository() =>
        new DynamoDbCharacterRepository(_client!, Options.Create(new DynamoDbOptions { TableName = TableName }));

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
