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
| A1 | Get an account | `GetItem PK={accountId}, SK=ITEM` |
| A2 | Find an account by username (case-insensitive) | `GetItem PK=USERNAME#{lowercase}, SK=USERNAME` -> `OwnerId` -> A1 |
| A3 | Find an account by Auth0 user | `GetItem PK=AUTH0#{auth0UserId}, SK=AUTH0` -> `OwnerId` -> A1 |
| C1 | Get a character | `GetItem PK={characterId}, SK=ITEM` |
| C2 | List an account's characters (character select) | `Query GSI1 GSI1PK=ACCOUNT#{accountId}, GSI1SK begins_with CHAR#` |
| C3 | Is a character name free? | `GetItem PK=CHARNAME#{lowercase name}, SK=CHARNAME` |

### Keys

Table `vaesen-{env}`:

- `PK` (String) - partition key
- `SK` (String) - sort key
- `GSI1PK` / `GSI1SK` (String) - keys of index **`GSI1`** (projection: ALL)

Every model is stored by the generic `BaseRepository<T>` as **its JSON document** (the same camelCase JSON the API returns) plus key attributes. The id already says what it is (`account-...`), so it's the partition key as is:

| Item | PK | SK | GSI1PK | GSI1SK | Other attributes |
|---|---|---|---|---|---|
| Model (Account, Character, ...) | `{id}`, e.g. `account-3f2b...` | `ITEM` | set by the repository, if indexed | | the model's JSON fields (`username`, `subscription`, `createdAt`, `version`, ...), `Type` |
| Character | `character-...` | `ITEM` | `ACCOUNT#{accountId}` | `CHAR#{name}` | `accountId`, `name`, `guildId`, ... |
| Reservation (unique value) | `USERNAME#{lowercase}`, `AUTH0#{auth0UserId}`, `CHARNAME#{lowercase}` | `USERNAME`, `AUTH0`, `CHARNAME` | | | `OwnerId` (the id holding it), `Type = Reservation` |

Every item has a `Type` attribute (`Account`, `Character`, `Reservation`). It isn't needed by the API, but it makes the console readable and helps if you ever export data.

Conventions:

- Ids are `{class name in lowercase}-{lowercase guid}`, e.g. `account-3f2b8c1e-1d2c-4b5a-9e8f-0123456789ab`. A model gets its id automatically (`ObjectCore.Id`).
- Every model derives from `BaseCore`: `createdAt`, `updatedAt`, `isDeleted` (soft delete: deleted items are hidden, not removed) and `version` (updates based on an older version are rejected).
- An account's Auth0 identity is stored as `auth0UserId`: Auth0's `user_id`, which is the `sub` claim in its tokens (`auth0|...`, `google-oauth2|...`).
- Uniqueness (usernames, character names, Auth0 users) uses reservation items written in the same transaction as the model, each with `attribute_not_exists(PK)`.
- An account's guilds are stored on the account as `guildIds` (a list), so loading the account gives its guilds in one read. The other direction ("who is in guild X") isn't covered by that list: when a guild member list is needed, add a membership item per member (`PK = {guildId}`, `SK = MEMBER#{accountId}`) written in the same transaction as the account's `guildIds`.
- New models usually need no new table design: derive from `BaseCore`, add a repository deriving from `BaseRepository<T>`. Only new queries need a GSI key or a reservation.

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
"DataService": {
  "TableName": "vaesen-local",
  "ServiceUrl": "http://localhost:8000",
  "CreateTableIfMissing": true
}
```

The table is then created automatically after startup. If DynamoDB Local isn't running, the API still starts: storage just doesn't work, `/health/ready` reports Unhealthy, and the log says how to start it.

### 3.2 Check it works

With the AWS CLI installed (any dummy credentials work against DynamoDB Local):

```bash
aws dynamodb list-tables --endpoint-url http://localhost:8000 --region eu-central-1
```

The API creates the local table itself after startup, so there's nothing else to set up.

### 3.3 Browse it

Install **NoSQL Workbench** (free, from AWS). *Operation builder → Add connection → DynamoDB local*, port `8000`. You can browse, edit and query items there.

---

## 4. AWS setup

Pick one region for the API, DynamoDB and the GameLift home region. Vaesen uses **`eu-central-1` (Frankfurt)**: Stockholm (`eu-north-1`) can't be a GameLift home region, but it can be added later as a GameLift fleet location to run game servers close to Nordic players. IAM Identity Center being configured in Stockholm doesn't matter: its permissions apply in every region.

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

## 5. How the API is wired

The solution follows the Redkeep platform layout: framework libraries hold models, repositories, services and controllers; the API project only starts them. See `VaesenAPI/README.md`.

### 5.1 Files

```
Framework/Vaesen.Framework.Core/
  Models/BaseCore.cs              ObjectCore (typed ids) and BaseCore (timestamps, soft delete, version)
  Models/ResultBase.cs            the { success, result, errorMessage, errorCode } envelope
  Data/BaseRepository.cs          generic DynamoDB storage (JSON document + keys), reservations, GSI1 queries
  Data/DataServiceOptions.cs      "DataService" config section
  Data/DynamoDbTable.cs           the table schema (used for DynamoDB Local and tests)
  Data/DynamoDbTableInitializer.cs creates the table in DynamoDB Local, in the background
  Data/DynamoDbHealthCheck.cs     "is the table reachable?" for /health/ready
  Services/CoreServiceExtensions.cs AddCoreServices(): AWS, DynamoDB, Auth0, policies, health
Framework/Vaesen.Framework.Common/
  Models/Account.cs, Character.cs
  Repositorys/AccountRepository.cs   IAccountStore: username and Auth0 lookups via reservations
  Repositorys/CharacterRepository.cs ICharacterStore: characters per account via GSI1, unique names
  Services/AccountService.cs        validation + ResultBase
  Api/AccountController.cs          api/accounts
```

Services depend on the store interfaces (`IAccountStore`, ...), never on DynamoDB types, so they're tested with in-memory fakes.

### 5.2 Configuration

| Setting | `appsettings.json` | `appsettings.Development.json` |
|---|---|---|
| `DataService:TableName` | `vaesen-dev` | *(inherited: `vaesen-dev`)* |
| `DataService:ServiceUrl` | *(empty = real AWS)* | *(empty; set it only for DynamoDB Local, section 3)* |
| `AWS:Region` | `eu-central-1` | *(inherited)* |
| `AWS:Profile` | | `vaesen`: your SSO login |

Production overrides values with environment variables on the ECS task, e.g. `DataService__TableName=vaesen-prod`. **No credentials appear in configuration:** locally they come from your SSO profile, in AWS from the task role.

If the API logs an SSO or token error, your session has expired: run `aws sso login --profile vaesen` and restart the API.

### 5.3 Client registration

`CoreServiceExtensions.AddCoreServices`:

- **`ServiceUrl` set (DynamoDB Local):** a client with dummy credentials, a 5-second timeout and one retry, so a stopped DynamoDB Local fails fast instead of stalling requests.
- **`ServiceUrl` empty (AWS):** `AddAWSService<IAmazonDynamoDB>()`. Region and credentials come from the shared AWS options (`AddDefaultAWSOptions` in `AddCoreServices`), which the GameLift client uses too.
- `TableName` is validated at startup (`ValidateOnStart`), so a missing setting fails immediately with a clear message.

### 5.4 Health checks

| Endpoint | Checks | Use |
|---|---|---|
| `/health` | the process is up (no dependencies) | load balancer liveness check |
| `/health/ready` | also DynamoDB (`DescribeTable` on the configured table) | deployment readiness, monitoring, "is my setup right?" |

## 6. Repositories

### 6.1 Example: characters

`CharacterRepository : BaseRepository<Character>, ICharacterStore`:

- **Get**: inherited `GetItemAsync(id)`.
- **List an account's characters**: overrides `GetIndexKeys` to put each character in GSI1 under `ACCOUNT#{accountId}`, then `QueryIndexAsync` (paginated).
- **Create**: `CreateItemAsync(character, reservations)` writes the character and its `CHARNAME#` reservation in one transaction. A taken name (in any casing) cancels both and returns the reservation's error code (`character_name_taken`).

### 6.2 Updates without lost writes (optimistic concurrency)

`BaseRepository<T>.UpdateItemAsync` only writes if the stored `version` still equals the item's version, then increments it. If two writers (e.g. two game servers) read the same item, the second update fails with error code `conflict`: re-read and retry, or report it.

Reservations aren't updated automatically: changing a username or character name needs a dedicated repository method that moves the reservation in the same transaction.

### 6.3 Rules of thumb

- **Never `Scan`** in the API. If you need it, a key or GSI is missing.
- **Always paginate** `Query` (the `LastEvaluatedKey` loop); a single response is at most 1 MB.
- **Use transactions** whenever two items must change together (character + name, guild + membership).
- **AWS SDK v4 returns `null` for empty collections** (`response.Items`, `response.Item`, `LastEvaluatedKey`), not empty ones. Always null-check.
- **Keep items small.** Big or fast-changing data (e.g. inventories) can get its own items under the same PK: `PK=CHAR#123, SK=ITEM#{itemId}`.
- **Store timestamps as ISO-8601 strings** (`2026-10-17T09:00:00Z`) so they sort correctly in sort keys.

## 7. Testing

- **Endpoint tests** (`VaesenApiFactory`) use in-memory stores (`Tests/.../Fakes`) and skip the table initializer. Fast, no Docker.
- **Mapping tests** (`BaseRepositoryMappingTests`) check how models become DynamoDB items and back, without a database.
- **Store contract tests** (`Tests/Vaesen.Framework.Tests/Persistence/`) define the behaviour every `IAccountStore` / `ICharacterStore` must have once and run it against:
  - the in-memory fakes: always.
  - the real repositories against DynamoDB Local in a throwaway container (Testcontainers): only when Docker is running; otherwise these tests are reported as **Skipped**.

That way the in-memory fake can't drift from the real behaviour (e.g. case-insensitive unique names).

## 8. Running in AWS (ECS)

When the API is deployed to ECS Fargate:

1. Create an IAM role for the task (trusted by `ecs-tasks.amazonaws.com`) with the policy from 4.3 (plus GameLift permissions for the Sessions module).
2. Set it as the **task role** in the task definition. The AWS SDK picks it up automatically; nothing to configure in code.
3. Set environment variables on the task:
   - `ASPNETCORE_ENVIRONMENT=Production`
   - `AWS__Region=eu-central-1`
   - `DataService__TableName=vaesen-prod`
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
- [ ] `dotnet test`: the `AccountRepositoryTests` / `CharacterRepositoryTests` run instead of being skipped (needs Docker either way)
