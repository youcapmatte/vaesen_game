using Microsoft.AspNetCore.Diagnostics.HealthChecks;
using Vaesen.Framework.Common.Services;
using Vaesen.Framework.Core.Middleware;
using Vaesen.Framework.Core.Services;
using Vaesen.Framework.GameLift.Services;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddControllers()
    .AddNewtonsoftJson()
    .AddApplicationPart(typeof(Vaesen.Framework.Common.Api.AccountController).Assembly)
    .AddApplicationPart(typeof(Vaesen.Framework.GameLift.Api.SessionController).Assembly);

builder.Services.AddCoreServices(builder.Configuration, builder.Environment);
builder.Services.AddCommonServices(builder.Configuration);
builder.Services.AddGameLiftServices(builder.Configuration);

if (builder.Environment.IsDevelopment())
{
    builder.Services.AddDevServices(builder.Configuration);
}

builder.Services.AddOpenApi();

var app = builder.Build();

app.UseMiddleware<ResultExceptionMiddleware>();

if (app.Environment.IsDevelopment())
{
    app.MapOpenApi();
}
else
{
    app.UseHttpsRedirection();
}

app.UseAuthentication();
app.UseAuthorization();

// Liveness: the process is up. Readiness: its dependencies (DynamoDB, ...) are reachable too.
app.MapHealthChecks("/health", new HealthCheckOptions { Predicate = check => !check.Tags.Contains(CoreServiceExtensions.ReadyHealthTag) });
app.MapHealthChecks("/health/ready");

app.MapControllers();

app.Run();

// Exposes Program to WebApplicationFactory in the integration tests.
public partial class Program;
