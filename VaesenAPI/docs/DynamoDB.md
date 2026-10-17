# DynamoDB for Vaesen.Api

How Vaesen.Api stores its data in Amazon DynamoDB: local development, the AWS setup, and how the API talks to it.

Contents

1. [How DynamoDB thinks](#1-how-dynamodb-thinks)
2. [The Vaesen table design](#2-the-vaesen-table-design)
3. [Optional: offline development with DynamoDB Local](#3-optional-offline-development-with-dynamodb-local)
4. [AWS setup](#4-aws-setup)
5. [How the API is wired](#5-how-the-api-is-wired)
6. [Repositories](#6-repositories)
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

## 3. Optional: offline development with DynamoDB Local

**By default, development uses the real `vaesen-dev` table in AWS** (section 4): no extra software, data browsable in the AWS console, identical to production. You only need `aws sso login --profile vaesen` when the session expires.

DynamoDB Local is the alternative for working offline, or for a database nobody else touches. AWS ships it as a Docker image that speaks the same API. The repository tests use it too (section 7).

### 3.1 Run it with Docker

Install **Docker Desktop** (docker.com) once. The compose file is in the repo: `VaesenAPI/docker-compose.yml`.

```bash
cd VaesenAPI
docker compose up -d      # start (data is kept in a Docker volume)
docker compose down       # stop
docker compose down -v    # stop and delete all local data
```

To point the API at it, replace the `AWS` section in `appsettings.Development.json` with:

```json
"DynamoDb": {
  "TableName": "vaesen-local",
  "ServiceUrl": "http://localhost:8000",
  "CreateTableIfMissing": true
}
```

The table is then created automatically after startup. If DynamoDB Local isn't running, the API still starts: storage just doesn't work, `/health/ready` reports Unhealthy, and the log says how to start it.

### 3.2 Check it works

With the AWS CLI installed (any dummy credentials work against DynamoDB Local):

```bash
aws dynamodb list-tables --endpoint-url http://localhost:8000 --region eu-north-1
```

The API creates the local table itself after startup, so there's nothing else to set up.

### 3.3 Browse it

Install **NoSQL Workbench** (free, from AWS). *Operation builder → Add connection → DynamoDB local*, port `8000`. You can browse, edit and query items there.

---

## 4. AWS setup

Pick one region for everything (GameLift, DynamoDB, the API). Vaesen uses **`eu-north-1` (Stockholm)**, the API's default and where IAM Identity Center is configured.

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

Or all of the above in one go from PowerShell (signed in with `aws sso login --profile vaesen`):

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\Aws\CreateDynamoDbTable.ps1
powershell -ExecutionPolicy Bypass -File .\Scripts\Aws\CreateDynamoDbTable.ps1 -TableName vaesen-prod
```

It's safe to re-run: an existing table is left unchanged.

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
        "arn:aws:dynamodb:eu-north-1:123456789012:table/vaesen-dev",
        "arn:aws:dynamodb:eu-north-1:123456789012:table/vaesen-dev/index/*"
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

## 5. How the API is wired

This is implemented; this section explains where things are and why.

### 5.1 Files

```
src/Vaesen.Api/
  Infrastructure/Persistence/
    PersistenceModule.cs         AddPersistence(): options, DynamoDB client, local table init, health check
    DynamoDbOptions.cs           "DynamoDb" config section
    DynamoDbTable.cs             the table schema (used for DynamoDB Local and tests)
    DynamoDbTableInitializer.cs  creates the table in DynamoDB Local, in the background
    DynamoDbHealthCheck.cs       "is the table reachable?" for /health/ready
    VaesenKeys.cs                every key format ("CHAR#", "GUILD#", ...) in one place
  Modules/Players/
    ICharacterRepository.cs      what the module needs; no DynamoDB types
    DynamoDbCharacterRepository.cs
    InMemoryCharacterRepository.cs   used by endpoint tests
```

Modules depend on their repository interfaces only. Endpoints and services never see `IAmazonDynamoDB`. New entities follow the same pattern: an interface in the module, a DynamoDB implementation next to it, key formats added to `VaesenKeys`.

### 5.2 Configuration

| Setting | `appsettings.json` | `appsettings.Development.json` |
|---|---|---|
| `DynamoDb:TableName` | `vaesen-dev` | *(inherited: `vaesen-dev`)* |
| `DynamoDb:ServiceUrl` | *(empty = real AWS)* | *(empty; set it only for DynamoDB Local, section 3)* |
| `AWS:Region` | `eu-north-1` | *(inherited)* |
| `AWS:Profile` | | `vaesen`: your SSO login |

Production overrides values with environment variables on the ECS task, e.g. `DynamoDb__TableName=vaesen-prod`. **No credentials appear in configuration:** locally they come from your SSO profile, in AWS from the task role.

If the API logs an SSO or token error, your session has expired: run `aws sso login --profile vaesen` and restart the API.

### 5.3 Client registration

`PersistenceModule.AddPersistence`:

- **`ServiceUrl` set (DynamoDB Local):** a client with dummy credentials, a 5-second timeout and one retry, so a stopped DynamoDB Local fails fast instead of stalling requests.
- **`ServiceUrl` empty (AWS):** `AddAWSService<IAmazonDynamoDB>()`. Region and credentials come from the shared AWS options registered once in `Program.cs` (`AddDefaultAWSOptions`), which the GameLift client in the Sessions module uses too.
- `TableName` is validated at startup (`ValidateOnStart`), so a missing setting fails immediately with a clear message.

### 5.4 Health checks

| Endpoint | Checks | Use |
|---|---|---|
| `/health` | the process is up (no dependencies) | load balancer liveness check |
| `/health/ready` | also DynamoDB (`DescribeTable` on the configured table) | deployment readiness, monitoring, "is my setup right?" |

## 6. Repositories

### 6.1 Example: characters

`DynamoDbCharacterRepository` covers access patterns C1-C3:

- **Get**: `GetItem CHAR#{id} / PROFILE`.
- **List an account's characters**: `Query` on `GSI1`, paginated with `LastEvaluatedKey`.
- **Create** in a **transaction**: the character item and its `CHARNAME#` reservation are written together, each with `attribute_not_exists(PK)`. A taken name (in any casing) cancels the whole transaction, so there's never a character without its name or a name without its character.

### 6.2 Updates without lost writes (optimistic concurrency)

Two writers (e.g. two game servers) can read the same item and overwrite each other. Characters carry a `Version`; updates should require the version they read:

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

### 6.3 Rules of thumb

- **Never `Scan`** in the API. If you need it, a key or GSI is missing.
- **Always paginate** `Query` (the `LastEvaluatedKey` loop); a single response is at most 1 MB.
- **Use transactions** whenever two items must change together (character + name, guild + membership).
- **AWS SDK v4 returns `null` for empty collections** (`response.Items`, `response.Item`, `LastEvaluatedKey`), not empty ones. Always null-check.
- **Keep items small.** Big or fast-changing data (e.g. inventories) can get its own items under the same PK: `PK=CHAR#123, SK=ITEM#{itemId}`.
- **Store timestamps as ISO-8601 strings** (`2026-10-17T09:00:00Z`) so they sort correctly in sort keys.

## 7. Testing

- **Endpoint tests** (`VaesenApiFactory`) use `InMemoryCharacterRepository` and skip the table initializer. Fast, no Docker.
- **Repository contract tests** (`tests/Vaesen.Api.Tests/Persistence/`) define the behaviour every `ICharacterRepository` must have once, in `CharacterRepositoryContract`, and run it against:
  - `InMemoryCharacterRepository`: always.
  - `DynamoDbCharacterRepository` against DynamoDB Local in a throwaway container (Testcontainers): only when Docker is running; otherwise these tests are reported as **Skipped**.

That way the in-memory fake can't drift from the real behaviour (e.g. case-insensitive unique names).

## 8. Running in AWS (ECS)

When the API is deployed to ECS Fargate:

1. Create an IAM role for the task (trusted by `ecs-tasks.amazonaws.com`) with the policy from 4.3 (plus GameLift permissions for the Sessions module).
2. Set it as the **task role** in the task definition. The AWS SDK picks it up automatically; nothing to configure in code.
3. Set environment variables on the task:
   - `ASPNETCORE_ENVIRONMENT=Production`
   - `AWS__Region=eu-north-1`
   - `DynamoDb__TableName=vaesen-prod`
4. Point the load balancer health check at `/health` (liveness). Use `/health/ready` to verify that a deployment can reach DynamoDB.

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

AWS (needed for development and production):

- [ ] IAM Identity Center user + `aws configure sso --profile vaesen`
- [ ] `aws sts get-caller-identity --profile vaesen` shows your account
- [ ] `powershell -ExecutionPolicy Bypass -File .\Scripts\Aws\CreateDynamoDbTable.ps1` (creates `vaesen-dev`)
- [ ] Start the API; `/health/ready` reports Healthy
- [ ] Before deploying: task role with the least-privilege policy from 4.3, same region as GameLift

Optional, offline:

- [ ] Docker Desktop installed; `docker compose up -d` in `VaesenAPI`; Development config switched (section 3.1)
- [ ] `dotnet test`: the `DynamoDbCharacterRepositoryTests` run instead of being skipped (needs Docker either way)
