# DynamoDB for Vaesen.Api

How Vaesen.Api stores its data in Amazon DynamoDB: local development, the AWS setup, and how the API talks to it.

Contents

1. [How DynamoDB thinks](#1-how-dynamodb-thinks)
2. [The Vaesen table design](#2-the-vaesen-table-design)
3. [Local development with DynamoDB Local](#3-local-development-with-dynamodb-local)
4. [AWS setup](#4-aws-setup)
5. [Connecting the API](#5-connecting-the-api)
6. [Writing a repository](#6-writing-a-repository)
7. [Testing](#7-testing)
8. [Running in AWS (ECS)](#8-running-in-aws-ecs)
9. [Looking at and editing data](#9-looking-at-and-editing-data)
10. [Checklist](#10-checklist)

---

## 1. How DynamoDB thinks

DynamoDB is a key-value / document database. Everything you need to know to use it well:

| Concept | Meaning |
|---|---|
| **Table** | Holds items. We use **one table per environment** (`vaesen-dev`, `vaesen-prod`) for all entity types ("single-table design"). |
| **Item** | One record: a JSON-like map of attributes (max 400 KB). |
| **Partition key (PK)** | Decides where an item lives. Every read by key needs it. |
| **Sort key (SK)** | Orders items that share a PK. `PK` + `SK` together are the item's unique key. |
| **Query** | Reads all items with one PK, optionally filtered by SK (`begins_with`, ranges). Cheap and fast. |
| **Scan** | Reads the whole table. Slow and expensive; never use it in the API. |
| **GSI (global secondary index)** | A second PK/SK on other attributes, so you can query items "the other way round". |
| **Conditional write** | A write that only happens if a condition holds (`attribute_not_exists(PK)`). Used for uniqueness and concurrency. |
| **Transaction** | Up to 100 writes/condition checks that all succeed or all fail. |

**The rule that matters:** design keys from your *access patterns* (the questions the API asks), not from the shape of the entities. There are no joins and no ad-hoc queries; if you need to ask a question, the keys must make it a `GetItem` or a `Query`.

---

## 2. The Vaesen table design

### Access patterns (first version)

| # | Question the API asks | Operation |
|---|---|---|
| A1 | Get an account | `GetItem PK=ACCOUNT#{accountId}, SK=PROFILE` |
| C1 | Get a character | `GetItem PK=CHAR#{characterId}, SK=PROFILE` |
| C2 | List an account's characters (character select) | `Query GSI1 GSI1PK=ACCOUNT#{accountId}, GSI1SK begins_with CHAR#` |
| C3 | Is a character name free? | `GetItem PK=CHARNAME#{lowercase name}` |
| G1 | Get a guild | `GetItem PK=GUILD#{guildId}, SK=PROFILE` |
| G2 | List a guild's members | `Query PK=GUILD#{guildId}, SK begins_with MEMBER#` |
| G3 | Which guild is a character in? | `Query GSI1 GSI1PK=CHAR#{characterId}, GSI1SK begins_with GUILD#` |

### Keys

Table `vaesen-{env}`:

- `PK` (String) - partition key
- `SK` (String) - sort key
- `GSI1PK` / `GSI1SK` (String) - keys of index **`GSI1`** (projection: ALL)

| Item | PK | SK | GSI1PK | GSI1SK | Other attributes |
|---|---|---|---|---|---|
| Account | `ACCOUNT#{accountId}` | `PROFILE` | | | `Email`, `CreatedAt` |
| Character | `CHAR#{characterId}` | `PROFILE` | `ACCOUNT#{accountId}` | `CHAR#{name}` | `Name`, `AccountId`, `GuildId`, `Version` |
| Character name reservation | `CHARNAME#{lowercase name}` | `CHARNAME` | | | `CharacterId` |
| Guild | `GUILD#{guildId}` | `PROFILE` | | | `Name`, `CreatedAt` |
| Guild membership | `GUILD#{guildId}` | `MEMBER#{characterId}` | `CHAR#{characterId}` | `GUILD#{guildId}` | `Rank`, `JoinedAt` |

Every item also has a `Type` attribute (`Account`, `Character`, ...). It isn't needed by the API, but it makes the console readable and helps if you ever export data.

Conventions:

- Ids are opaque strings (ULIDs or GUIDs). Auth0 subject ids (`auth0|...`) become the `accountId`.
- Prefixes (`CHAR#`, `GUILD#`) keep entity types apart and make `begins_with` queries possible.
- New access patterns usually mean a new item type or a new GSI, not a new table.

---

## 3. Local development with DynamoDB Local

AWS ships a local DynamoDB that speaks the same API. The API uses it in Development, exactly like `Sessions:Provider=Local` stands in for GameLift.

### 3.1 Run it with Docker

Create `VaesenAPI/docker-compose.yml`:

```yaml
services:
  dynamodb:
    image: amazon/dynamodb-local:latest
    container_name: vaesen-dynamodb
    # -sharedDb: one database regardless of credentials/region. -dbPath: keep data between restarts.
    command: "-jar DynamoDBLocal.jar -sharedDb -dbPath /home/dynamodblocal/data"
    working_dir: /home/dynamodblocal
    # The image's default user can't write to a fresh named volume.
    user: root
    ports:
      - "8000:8000"
    volumes:
      - dynamodb-data:/home/dynamodblocal/data

volumes:
  dynamodb-data:
```

Start / stop:

```bash
docker compose -f VaesenAPI/docker-compose.yml up -d
docker compose -f VaesenAPI/docker-compose.yml down
```

`down -v` also deletes the data volume (a clean database).

### 3.2 Check it works

With the AWS CLI installed (any dummy credentials work against DynamoDB Local):

```bash
aws dynamodb list-tables --endpoint-url http://localhost:8000 --region eu-central-1
```

The API creates the local table itself on startup (section 5.4), so there's nothing else to set up.

### 3.3 Browse it

Install **NoSQL Workbench** (free, from AWS). *Operation builder → Add connection → DynamoDB local*, port `8000`. You can browse, edit and query items there.

---

## 4. AWS setup

Pick one region for everything (GameLift, DynamoDB, the API). This guide uses `eu-central-1` (Frankfurt), the API's current default; `eu-north-1` (Stockholm) is also fine.

### 4.1 Your own access: IAM Identity Center (no access keys)

Don't create IAM users with long-lived access keys for yourself.

1. AWS Console → **IAM Identity Center** → *Enable*.
2. *Users* → create yourself. *Permission sets* → create one, e.g. `AdministratorAccess` while you're the only developer.
3. *AWS accounts* → assign your user + permission set to the account.
4. On your PC:
   ```bash
   aws configure sso --profile vaesen
   ```
   Follow the browser prompts. Then `aws sso login --profile vaesen` whenever the session expires.

The API, the AWS CLI and `Scripts/*.bat` all use this profile (`AWS_PROFILE=vaesen`, or `"AWS": { "Profile": "vaesen" }` in `appsettings.Development.json`).

### 4.2 Create the table

Console → **DynamoDB** → *Create table*:

| Setting | Value |
|---|---|
| Table name | `vaesen-dev` (later `vaesen-prod`) |
| Partition key | `PK` (String) |
| Sort key | `SK` (String) |
| Table settings | *Customize settings* |
| Capacity mode | **On-demand** (pay per request; no capacity planning) |
| Secondary indexes | *Create global index*: partition key `GSI1PK` (String), sort key `GSI1SK` (String), name `GSI1`, projection **All** |
| Encryption | AWS owned key (default) |
| Deletion protection | **On** |

After it's created, under *Backups*: turn on **Point-in-time recovery** (lets you restore the table to any second in the last 35 days). Do this at least for `vaesen-prod`.

Same thing with the CLI:

```bash
aws dynamodb create-table --profile vaesen --region eu-central-1 \
  --table-name vaesen-dev \
  --billing-mode PAY_PER_REQUEST \
  --attribute-definitions AttributeName=PK,AttributeType=S AttributeName=SK,AttributeType=S \
                          AttributeName=GSI1PK,AttributeType=S AttributeName=GSI1SK,AttributeType=S \
  --key-schema AttributeName=PK,KeyType=HASH AttributeName=SK,KeyType=RANGE \
  --global-secondary-indexes "IndexName=GSI1,KeySchema=[{AttributeName=GSI1PK,KeyType=HASH},{AttributeName=GSI1SK,KeyType=RANGE}],Projection={ProjectionType=ALL}" \
  --deletion-protection-enabled

aws dynamodb update-continuous-backups --profile vaesen --region eu-central-1 \
  --table-name vaesen-dev \
  --point-in-time-recovery-specification PointInTimeRecoveryEnabled=true
```

Later this belongs in infrastructure-as-code (AWS CDK in C#), so dev and prod are guaranteed identical.

### 4.3 The API's permissions (least privilege)

The deployed API gets this policy through its ECS **task role** (section 8). Replace the account id and region:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "VaesenTableAccess",
      "Effect": "Allow",
      "Action": [
        "dynamodb:GetItem",
        "dynamodb:BatchGetItem",
        "dynamodb:Query",
        "dynamodb:PutItem",
        "dynamodb:UpdateItem",
        "dynamodb:DeleteItem",
        "dynamodb:BatchWriteItem",
        "dynamodb:ConditionCheckItem",
        "dynamodb:DescribeTable"
      ],
      "Resource": [
        "arn:aws:dynamodb:eu-central-1:123456789012:table/vaesen-dev",
        "arn:aws:dynamodb:eu-central-1:123456789012:table/vaesen-dev/index/*"
      ]
    }
  ]
}
```

Notes:

- There is no separate IAM action for transactions; `TransactWriteItems` needs the underlying actions above.
- `Scan` and `DeleteTable` are deliberately missing.
- `DescribeTable` is only for the health check.

---

## 5. Connecting the API

### 5.1 Package

```bash
cd VaesenAPI
dotnet add src/Vaesen.Api package AWSSDK.DynamoDBv2
```

### 5.2 Where the code goes

Storage is shared infrastructure; each module owns its own repositories:

```
src/Vaesen.Api/
  Infrastructure/
    Persistence/
      PersistenceModule.cs        AddPersistence(): client, options, health check, local table init
      DynamoDbOptions.cs
      DynamoDbTableInitializer.cs creates the table in Development (DynamoDB Local only)
      DynamoDbHealthCheck.cs
      VaesenKeys.cs               the PK/SK/GSI key builders (one place for "CHAR#", "GUILD#", ...)
  Modules/
    Players/
      ICharacterRepository.cs     what the module needs (no DynamoDB types)
      DynamoDbCharacterRepository.cs
      InMemoryCharacterRepository.cs   for tests
```

Modules depend on their repository interfaces only. Endpoints and services never see `IAmazonDynamoDB`.

### 5.3 Configuration

`appsettings.json` (all environments):

```json
"DynamoDb": {
  "TableName": "vaesen-dev"
}
```

`appsettings.Development.json`:

```json
"DynamoDb": {
  "TableName": "vaesen-local",
  "ServiceUrl": "http://localhost:8000",
  "CreateTableIfMissing": true
}
```

Production overrides `TableName` (`vaesen-prod`) through an environment variable on the ECS task: `DynamoDb__TableName=vaesen-prod`. No credentials appear anywhere in configuration: locally they come from your SSO profile, in AWS from the task role.

### 5.4 Registration

`Infrastructure/Persistence/DynamoDbOptions.cs`:

```csharp
namespace Vaesen.Api.Infrastructure.Persistence;

public sealed class DynamoDbOptions
{
    public const string SectionName = "DynamoDb";

    public string TableName { get; set; } = "";

    /// <summary>Set to use DynamoDB Local (e.g. http://localhost:8000). Empty means real AWS.</summary>
    public string? ServiceUrl { get; set; }

    /// <summary>Create the table on startup if it's missing. Only for DynamoDB Local.</summary>
    public bool CreateTableIfMissing { get; set; }

    public const string Gsi1 = "GSI1";
}
```

`Infrastructure/Persistence/PersistenceModule.cs`:

```csharp
using Amazon.DynamoDBv2;
using Amazon.Runtime;

namespace Vaesen.Api.Infrastructure.Persistence;

public static class PersistenceModule
{
    public static IServiceCollection AddPersistence(this IServiceCollection services, IConfiguration configuration)
    {
        var section = configuration.GetSection(DynamoDbOptions.SectionName);
        services.AddOptions<DynamoDbOptions>()
            .Bind(section)
            .Validate(o => !string.IsNullOrWhiteSpace(o.TableName), "DynamoDb:TableName is required.")
            .ValidateOnStart();

        var options = section.Get<DynamoDbOptions>() ?? new DynamoDbOptions();

        if (!string.IsNullOrWhiteSpace(options.ServiceUrl))
        {
            // DynamoDB Local accepts any credentials.
            services.AddSingleton<IAmazonDynamoDB>(_ => new AmazonDynamoDBClient(
                new BasicAWSCredentials("local", "local"),
                new AmazonDynamoDBConfig { ServiceURL = options.ServiceUrl }));

            if (options.CreateTableIfMissing)
            {
                services.AddHostedService<DynamoDbTableInitializer>();
            }
        }
        else
        {
            // Region and credentials from the "AWS" config section / profile / ECS task role.
            services.AddAWSService<IAmazonDynamoDB>();
        }

        services.AddHealthChecks().AddCheck<DynamoDbHealthCheck>("dynamodb");
        return services;
    }
}
```

`AddDefaultAWSOptions(configuration.GetAWSOptions())` must run once before any `AddAWSService` call. It currently lives inside `SessionsModule`; move it to `Program.cs` so both modules share it:

```csharp
builder.Services.AddDefaultAWSOptions(builder.Configuration.GetAWSOptions());
builder.Services
    .AddPersistence(builder.Configuration)
    .AddIdentityModule(builder.Configuration)
    ...
```

`Infrastructure/Persistence/DynamoDbTableInitializer.cs` (creates the same table as section 4.2, locally):

```csharp
using Amazon.DynamoDBv2;
using Amazon.DynamoDBv2.Model;
using Microsoft.Extensions.Options;

namespace Vaesen.Api.Infrastructure.Persistence;

public sealed class DynamoDbTableInitializer(
    IAmazonDynamoDB dynamoDb,
    IOptions<DynamoDbOptions> options,
    ILogger<DynamoDbTableInitializer> logger) : IHostedService
{
    public async Task StartAsync(CancellationToken cancellationToken)
    {
        var tableName = options.Value.TableName;
        try
        {
            await dynamoDb.DescribeTableAsync(tableName, cancellationToken);
            return;
        }
        catch (ResourceNotFoundException)
        {
        }

        logger.LogInformation("Creating local DynamoDB table {Table}", tableName);
        await dynamoDb.CreateTableAsync(new CreateTableRequest
        {
            TableName = tableName,
            BillingMode = BillingMode.PAY_PER_REQUEST,
            AttributeDefinitions =
            [
                new AttributeDefinition("PK", ScalarAttributeType.S),
                new AttributeDefinition("SK", ScalarAttributeType.S),
                new AttributeDefinition("GSI1PK", ScalarAttributeType.S),
                new AttributeDefinition("GSI1SK", ScalarAttributeType.S),
            ],
            KeySchema =
            [
                new KeySchemaElement("PK", KeyType.HASH),
                new KeySchemaElement("SK", KeyType.RANGE),
            ],
            GlobalSecondaryIndexes =
            [
                new GlobalSecondaryIndex
                {
                    IndexName = DynamoDbOptions.Gsi1,
                    KeySchema =
                    [
                        new KeySchemaElement("GSI1PK", KeyType.HASH),
                        new KeySchemaElement("GSI1SK", KeyType.RANGE),
                    ],
                    Projection = new Projection { ProjectionType = ProjectionType.ALL },
                },
            ],
        }, cancellationToken);
    }

    public Task StopAsync(CancellationToken cancellationToken) => Task.CompletedTask;
}
```

`Infrastructure/Persistence/DynamoDbHealthCheck.cs`:

```csharp
using Amazon.DynamoDBv2;
using Microsoft.Extensions.Diagnostics.HealthChecks;
using Microsoft.Extensions.Options;

namespace Vaesen.Api.Infrastructure.Persistence;

public sealed class DynamoDbHealthCheck(IAmazonDynamoDB dynamoDb, IOptions<DynamoDbOptions> options) : IHealthCheck
{
    public async Task<HealthCheckResult> CheckHealthAsync(HealthCheckContext context, CancellationToken cancellationToken = default)
    {
        try
        {
            await dynamoDb.DescribeTableAsync(options.Value.TableName, cancellationToken);
            return HealthCheckResult.Healthy();
        }
        catch (Exception ex)
        {
            return HealthCheckResult.Unhealthy("DynamoDB table not reachable.", ex);
        }
    }
}
```

### 5.5 Key builders

Keep every key format in one place so typos can't create "ghost" items:

```csharp
namespace Vaesen.Api.Infrastructure.Persistence;

public static class VaesenKeys
{
    public const string Profile = "PROFILE";

    public static string Account(string accountId) => $"ACCOUNT#{accountId}";
    public static string Character(string characterId) => $"CHAR#{characterId}";
    public static string CharacterName(string name) => $"CHARNAME#{name.Trim().ToLowerInvariant()}";
    public static string Guild(string guildId) => $"GUILD#{guildId}";
    public static string GuildMember(string characterId) => $"MEMBER#{characterId}";
}
```

---

## 6. Writing a repository

Example: characters (access patterns C1-C3).

### 6.1 The module's view

`Modules/Players/ICharacterRepository.cs`:

```csharp
namespace Vaesen.Api.Modules.Players;

public sealed record Character(string Id, string AccountId, string Name, string? GuildId, int Version);

public interface ICharacterRepository
{
    Task<Character?> GetAsync(string characterId, CancellationToken cancellationToken);

    Task<IReadOnlyList<Character>> ListByAccountAsync(string accountId, CancellationToken cancellationToken);

    /// <summary>Creates the character and reserves its name. Returns false if the name is taken.</summary>
    Task<bool> TryCreateAsync(Character character, CancellationToken cancellationToken);
}
```

### 6.2 The DynamoDB implementation

The low-level client with explicit mapping is the clearest choice for single-table design: you see exactly which attributes are written.

`Modules/Players/DynamoDbCharacterRepository.cs`:

```csharp
using Amazon.DynamoDBv2;
using Amazon.DynamoDBv2.Model;
using Microsoft.Extensions.Options;
using Vaesen.Api.Infrastructure.Persistence;

namespace Vaesen.Api.Modules.Players;

public sealed class DynamoDbCharacterRepository(IAmazonDynamoDB dynamoDb, IOptions<DynamoDbOptions> options)
    : ICharacterRepository
{
    private string Table => options.Value.TableName;

    public async Task<Character?> GetAsync(string characterId, CancellationToken cancellationToken)
    {
        var response = await dynamoDb.GetItemAsync(new GetItemRequest
        {
            TableName = Table,
            Key = Key(VaesenKeys.Character(characterId), VaesenKeys.Profile),
        }, cancellationToken);

        return response.Item is { Count: > 0 } ? FromItem(response.Item) : null;
    }

    public async Task<IReadOnlyList<Character>> ListByAccountAsync(string accountId, CancellationToken cancellationToken)
    {
        var characters = new List<Character>();
        Dictionary<string, AttributeValue>? startKey = null;
        do
        {
            var response = await dynamoDb.QueryAsync(new QueryRequest
            {
                TableName = Table,
                IndexName = DynamoDbOptions.Gsi1,
                KeyConditionExpression = "GSI1PK = :pk AND begins_with(GSI1SK, :prefix)",
                ExpressionAttributeValues = new()
                {
                    [":pk"] = new AttributeValue(VaesenKeys.Account(accountId)),
                    [":prefix"] = new AttributeValue("CHAR#"),
                },
                ExclusiveStartKey = startKey,
            }, cancellationToken);

            characters.AddRange((response.Items ?? []).Select(FromItem));
            startKey = response.LastEvaluatedKey is { Count: > 0 } ? response.LastEvaluatedKey : null;
        } while (startKey is not null);

        return characters;
    }

    public async Task<bool> TryCreateAsync(Character character, CancellationToken cancellationToken)
    {
        // Both writes succeed or neither does: no character without its name, no name without its character.
        try
        {
            await dynamoDb.TransactWriteItemsAsync(new TransactWriteItemsRequest
            {
                TransactItems =
                [
                    new TransactWriteItem
                    {
                        Put = new Put
                        {
                            TableName = Table,
                            Item = ToItem(character),
                            ConditionExpression = "attribute_not_exists(PK)",
                        },
                    },
                    new TransactWriteItem
                    {
                        Put = new Put
                        {
                            TableName = Table,
                            Item = new Dictionary<string, AttributeValue>
                            {
                                ["PK"] = new(VaesenKeys.CharacterName(character.Name)),
                                ["SK"] = new("CHARNAME"),
                                ["Type"] = new("CharacterName"),
                                ["CharacterId"] = new(character.Id),
                            },
                            ConditionExpression = "attribute_not_exists(PK)",
                        },
                    },
                ],
            }, cancellationToken);
            return true;
        }
        catch (TransactionCanceledException)
        {
            return false;
        }
    }

    private static Dictionary<string, AttributeValue> Key(string pk, string sk) => new()
    {
        ["PK"] = new AttributeValue(pk),
        ["SK"] = new AttributeValue(sk),
    };

    private static Dictionary<string, AttributeValue> ToItem(Character character)
    {
        var item = new Dictionary<string, AttributeValue>
        {
            ["PK"] = new(VaesenKeys.Character(character.Id)),
            ["SK"] = new(VaesenKeys.Profile),
            ["GSI1PK"] = new(VaesenKeys.Account(character.AccountId)),
            ["GSI1SK"] = new($"CHAR#{character.Name}"),
            ["Type"] = new("Character"),
            ["Id"] = new(character.Id),
            ["AccountId"] = new(character.AccountId),
            ["Name"] = new(character.Name),
            ["Version"] = new() { N = character.Version.ToString() },
        };
        if (character.GuildId is not null)
        {
            item["GuildId"] = new(character.GuildId);
        }
        return item;
    }

    private static Character FromItem(Dictionary<string, AttributeValue> item) => new(
        item["Id"].S,
        item["AccountId"].S,
        item["Name"].S,
        item.TryGetValue("GuildId", out var guild) ? guild.S : null,
        int.Parse(item["Version"].N));
}
```

Register it in `PlayersModule.AddPlayersModule`:

```csharp
services.AddSingleton<ICharacterRepository, DynamoDbCharacterRepository>();
```

### 6.3 Updates without lost writes (optimistic concurrency)

Two writers (e.g. two game servers) can read the same item and overwrite each other. Guard updates with the `Version` attribute:

```csharp
await dynamoDb.UpdateItemAsync(new UpdateItemRequest
{
    TableName = Table,
    Key = Key(VaesenKeys.Character(character.Id), VaesenKeys.Profile),
    UpdateExpression = "SET GuildId = :guild, Version = :next",
    ConditionExpression = "Version = :expected",
    ExpressionAttributeValues = new()
    {
        [":guild"] = new AttributeValue(guildId),
        [":expected"] = new AttributeValue { N = character.Version.ToString() },
        [":next"] = new AttributeValue { N = (character.Version + 1).ToString() },
    },
}, cancellationToken);
// ConditionalCheckFailedException => someone else changed it first: re-read and retry, or report a conflict.
```

### 6.4 Rules of thumb

- **AWS SDK v4 returns `null` for empty collections** (`response.Items`, `response.Item`, `LastEvaluatedKey`), not empty ones. Always null-check, as in the code above.
- **Never `Scan`** in the API. If you need it, a key or GSI is missing.
- **Always paginate** `Query` (the `LastEvaluatedKey` loop above); a single response is at most 1 MB.
- **Use transactions** whenever two items must change together (character + name, guild + membership).
- **Keep items small.** Big or fast-changing data (e.g. full inventories) can get its own items under the same PK: `PK=CHAR#123, SK=ITEM#{itemId}`.
- **Store timestamps as ISO-8601 strings** (`2026-10-17T09:00:00Z`) so they sort correctly in sort keys.

---

## 7. Testing

Two layers, matching how `IGameLiftGateway` is tested today:

1. **Endpoint and service tests** replace the repository with an in-memory implementation in `VaesenApiFactory.ConfigureTestServices`:
   ```csharp
   services.AddSingleton<ICharacterRepository, InMemoryCharacterRepository>();
   ```
   They're fast, need no Docker, and run everywhere.

2. **Repository tests** run the real `DynamoDbCharacterRepository` against DynamoDB Local in a throwaway container, using Testcontainers:
   ```bash
   dotnet add tests/Vaesen.Api.Tests package Testcontainers.DynamoDb
   ```
   ```csharp
   public sealed class DynamoDbFixture : IAsyncLifetime
   {
       private readonly DynamoDbContainer _container = new DynamoDbBuilder().Build();

       public IAmazonDynamoDB Client { get; private set; } = null!;

       public async Task InitializeAsync()
       {
           await _container.StartAsync();
           Client = new AmazonDynamoDBClient(
               new BasicAWSCredentials("test", "test"),
               new AmazonDynamoDBConfig { ServiceURL = _container.GetConnectionString() });
           // Create the table here (reuse DynamoDbTableInitializer).
       }

       public Task DisposeAsync() => _container.DisposeAsync().AsTask();
   }
   ```
   These catch mistakes in key expressions and conditions that an in-memory fake can't.

---

## 8. Running in AWS (ECS)

When the API is deployed to ECS Fargate:

1. Create an IAM role for the task (trusted by `ecs-tasks.amazonaws.com`) with the policy from 4.3 (plus GameLift permissions for the Sessions module).
2. Set it as the **task role** in the task definition. The AWS SDK picks it up automatically; nothing to configure in code.
3. Set environment variables on the task:
   - `ASPNETCORE_ENVIRONMENT=Production`
   - `AWS__Region=eu-central-1`
   - `DynamoDb__TableName=vaesen-prod`
4. Point the load balancer health check at `/health`; it now includes DynamoDB.

---

## 9. Looking at and editing data

| Tool | Use it for |
|---|---|
| **AWS Console → DynamoDB → Explore items** | Browse items, query by PK, edit an item as a form or raw JSON, create/delete items. |
| **Console → PartiQL editor** | SQL-like one-offs: `SELECT * FROM "vaesen-dev" WHERE PK = 'CHAR#123'`, `UPDATE "vaesen-dev" SET Name = 'Ingrid' WHERE PK = 'CHAR#123' AND SK = 'PROFILE'` |
| **NoSQL Workbench** | Same browsing/editing on your desktop, against AWS or DynamoDB Local. Also visualizes the table design. |

Careful in production: the console bypasses the API's rules. Editing `Name` on a character doesn't update its `CHARNAME#` reservation or its `GSI1SK`. For anything beyond fixing a single value, add an admin endpoint that does it properly.

---

## 10. Checklist

Local:

- [ ] Docker Desktop installed; `docker compose -f VaesenAPI/docker-compose.yml up -d`
- [ ] `AWSSDK.DynamoDBv2` added; `AddPersistence` registered in `Program.cs`
- [ ] `appsettings.Development.json` has `DynamoDb:ServiceUrl` and `CreateTableIfMissing`
- [ ] API starts; `/health` reports Healthy; table visible in NoSQL Workbench

AWS:

- [ ] IAM Identity Center user + `aws configure sso --profile vaesen`
- [ ] Table `vaesen-dev` (on-demand, `GSI1`, deletion protection, PITR)
- [ ] Task role with the least-privilege policy from 4.3
- [ ] Same region as GameLift and the API
