using System.Text.Json.Serialization;
using Microsoft.AspNetCore.Diagnostics.HealthChecks;
using Microsoft.OpenApi;
using Scalar.AspNetCore;
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

// The API serializes with Newtonsoft (enums as strings); describe enums the same way in the OpenAPI document.
builder.Services.ConfigureHttpJsonOptions(options => options.SerializerOptions.Converters.Add(new JsonStringEnumConverter()));

builder.Services.AddOpenApi("v1", options =>
{
    options.AddDocumentTransformer((document, _, _) =>
    {
        document.Info = new OpenApiInfo
        {
            Title = "Vaesen API",
            Version = "v1",
            Description = "Accounts, characters, subscriptions and GameLift worlds for Vaesen. "
                + "Every response is { success, result, errorMessage, errorCode }.",
        };

        // Lets the docs UI send a token. In Development: POST /api/dev/login, then paste result.accessToken.
        document.Components ??= new OpenApiComponents();
        document.Components.SecuritySchemes ??= new Dictionary<string, IOpenApiSecurityScheme>();
        document.Components.SecuritySchemes["Bearer"] = new OpenApiSecurityScheme
        {
            Type = SecuritySchemeType.Http,
            Scheme = "bearer",
            BearerFormat = "JWT",
            Description = "Auth0 access token, or in Development a dev login token (POST /api/dev/login).",
        };
        document.Security ??= [];
        document.Security.Add(new OpenApiSecurityRequirement { [new OpenApiSecuritySchemeReference("Bearer", document)] = [] });
        return Task.CompletedTask;
    });
});

var app = builder.Build();

app.UseMiddleware<ResultExceptionMiddleware>();

if (app.Environment.IsDevelopment())
{
    // OpenAPI document at /openapi/v1.json, interactive docs at /docs (Scalar, as in Redkeep).
    app.MapOpenApi();
    app.MapScalarApiReference("docs", options =>
    {
        options.Title = "Vaesen API";
        options.Theme = ScalarTheme.DeepSpace;
        options.ShowDeveloperTools = DeveloperToolsVisibility.Always;
    });
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
