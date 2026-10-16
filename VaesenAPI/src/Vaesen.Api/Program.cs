using Vaesen.Api.Modules.Billing;
using Vaesen.Api.Modules.Dev;
using Vaesen.Api.Modules.Identity;
using Vaesen.Api.Modules.Players;
using Vaesen.Api.Modules.Sessions;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddOpenApi();
builder.Services.AddHealthChecks();

builder.Services
    .AddIdentityModule(builder.Configuration)
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

app.MapHealthChecks("/health");

var api = app.MapGroup("/api/v1");
api.MapIdentityEndpoints();
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
