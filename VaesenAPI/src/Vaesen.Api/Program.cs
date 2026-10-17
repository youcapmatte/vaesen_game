using Microsoft.AspNetCore.Diagnostics.HealthChecks;
using Vaesen.Api.Infrastructure.Persistence;
using Vaesen.Api.Modules.Accounts;
using Vaesen.Api.Modules.Billing;
using Vaesen.Api.Modules.Dev;
using Vaesen.Api.Modules.Identity;
using Vaesen.Api.Modules.Players;
using Vaesen.Api.Modules.Sessions;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddOpenApi();
builder.Services.AddHealthChecks();
builder.Services.AddDefaultAWSOptions(builder.Configuration.GetAWSOptions());

builder.Services
    .AddPersistence(builder.Configuration)
    .AddIdentityModule(builder.Configuration)
    .AddAccountsModule(builder.Configuration)
    .AddBillingModule(builder.Configuration)
    .AddSessionsModule(builder.Configuration)
    .AddPlayersModule(builder.Configuration);

if (builder.Environment.IsDevelopment())
{
    builder.Services.AddDevModule(builder.Configuration);
}

var app = builder.Build();

if (app.Environment.IsDevelopment())
{
    app.MapOpenApi();
}

app.UseHttpsRedirection();

// Liveness: the process is up. Readiness: its dependencies (DynamoDB, ...) are reachable too.
app.MapHealthChecks("/health", new HealthCheckOptions { Predicate = check => !check.Tags.Contains(PersistenceModule.ReadyTag) });
app.MapHealthChecks("/health/ready");

var api = app.MapGroup("/api/v1");
api.MapIdentityEndpoints();
api.MapAccountsEndpoints();
api.MapBillingEndpoints();
api.MapSessionsEndpoints();
api.MapPlayersEndpoints();

if (app.Environment.IsDevelopment())
{
    api.MapDevEndpoints();
}

app.Run();

// Exposes Program to WebApplicationFactory in the integration tests.
public partial class Program;
