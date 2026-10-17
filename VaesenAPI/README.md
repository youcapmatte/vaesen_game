# Vaesen API

Backend for the Vaesen game: accounts, characters, subscriptions and getting players into GameLift worlds.
Same layout as the Redkeep platform: framework libraries hold the code, the API project only starts it.

```
Vaesen.sln
Framework/
  Vaesen.Framework.Core/      Shared infrastructure
    Models/                   ObjectCore + BaseCore (typed ids "account-{guid}", timestamps, soft delete, version), ResultBase<T>
    Data/                     BaseRepository<T> on DynamoDB, DataServiceOptions, table schema, health check
    Security/                 Auth0 options, policies (Player, GameServer, Admin), ISecurityService
    Middleware/               ResultExceptionMiddleware (unhandled errors as ResultBase)
    Api/                      ToActionResult (ResultBase -> HTTP status), [DevelopmentOnly]
    Services/                 CoreServiceExtensions.AddCoreServices
  Vaesen.Framework.Common/    Accounts, guilds, characters, billing, dev login
    Models/ Repositorys/ Services/ Api/ (AccountController, GuildController, IdentityController, DevController)
    Services/CommonServiceExtensions.AddCommonServices (+ AddDevServices in Development)
  Vaesen.Framework.GameLift/  Worlds and joining them via GameLift
    Models/ Services/ Api/ (SessionController)
    Services/GameLiftServiceExtensions.AddGameLiftServices
Api/
  Vaesen.Api/                 Program.cs + appsettings: registers the framework libraries and their controllers
Tests/
  Vaesen.Framework.Tests/     Endpoint tests (in-memory stores), mapping tests, store contract tests
docs/
  DynamoDB.md                 Table design, AWS setup, repositories
```

## Conventions

- **Models** derive from `BaseCore` (`id`, `createdAt`, `createdBy`, `updatedAt`, `updatedBy`, `isDeleted`, `version`) and use Newtonsoft `[JsonProperty("camelCase")]`. Ids come from the class name: `new Account().Id` is `account-{lowercase guid}`, `new Guild().Id` is `guild-{lowercase guid}`.
- **Writes across models** (guild + member accounts, character + guild) go through one store method that commits them in a single DynamoDB transaction.
- **Repositories** derive from `BaseRepository<T>` and expose a store interface (`IAccountStore`) that services depend on.
- **Services** come as `IXService` / `XService` and return `ResultBase<T>`; failures carry an `errorCode` (`not_found`, `invalid`, `conflict`, `username_taken`, ...).
- **Controllers** live in the framework libraries (`[ApiController]`, `[Route("api/...")]`, `[Authorize(Policy = ...)]`) and return `ResultBase<T>` with a matching HTTP status (`this.ToActionResult(result)`).
- **Registration** is one extension method per library (`AddCoreServices`, `AddCommonServices`, `AddGameLiftServices`), called from `Api/Vaesen.Api/Program.cs`.

Every response has the same shape:

```json
{ "success": true, "result": { ... }, "errorMessage": "", "errorCode": null }
```

## Running

```powershell
aws sso login --profile vaesen
dotnet run --project .\Api\Vaesen.Api --launch-profile http
```

- http://localhost:5230/docs: interactive API docs (Scalar, Development only). Click **Authentication**, paste a token from `POST /api/dev/login` (`result.accessToken`), and try the player endpoints.
- http://localhost:5230/openapi/v1.json: the OpenAPI document (Development only).
- http://localhost:5230/health: the API is up.
- http://localhost:5230/health/ready: it can also reach DynamoDB.
- `Api/Vaesen.Api/Vaesen.Api.http`: ready-made requests.

```powershell
dotnet test
```
