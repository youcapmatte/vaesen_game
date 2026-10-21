using System.Text.Json.Serialization;
using Microsoft.AspNetCore.Diagnostics.HealthChecks;
using Microsoft.OpenApi;
using Newtonsoft.Json.Converters;
using Nexus.Table.Hubs;
using Nexus.Table.Services;
using Scalar.AspNetCore;
using Vaesen.Framework.Core.Middleware;
using Vaesen.Framework.Core.Services;

const string WebAppCors = "WebApp";

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddControllers()
    .AddNewtonsoftJson(options => options.SerializerSettings.Converters.Add(new StringEnumConverter()))
    .AddApplicationPart(typeof(Nexus.Table.Api.MeController).Assembly);

builder.Services.AddSignalR()
    .AddNewtonsoftJsonProtocol(options => options.PayloadSerializerSettings.Converters.Add(new StringEnumConverter()));

builder.Services.AddCoreServices(builder.Configuration, builder.Environment);
builder.Services.AddNexusServices(builder.Configuration);

if (builder.Environment.IsDevelopment())
{
    builder.Services.AddNexusDevAuthentication(builder.Configuration);
}

// The web app calls the API from the browser; SignalR needs credentials allowed for its negotiate request.
builder.Services.AddCors(options => options.AddPolicy(WebAppCors, policy => policy
    .WithOrigins(builder.Configuration.GetSection("Cors:Origins").Get<string[]>() ?? [])
    .AllowAnyHeader()
    .AllowAnyMethod()
    .AllowCredentials()));

// The API serializes with Newtonsoft (enums as strings); describe enums the same way in the OpenAPI document.
builder.Services.ConfigureHttpJsonOptions(options => options.SerializerOptions.Converters.Add(new JsonStringEnumConverter()));

builder.Services.AddOpenApi("v1", options =>
{
    options.AddDocumentTransformer((document, _, _) =>
    {
        document.Info = new OpenApiInfo
        {
            Title = "Nexus Table API",
            Version = "v1",
            Description = "Accounts, the Society, Vaesen characters and dice for the AI game master tabletop. "
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
    // OpenAPI document at /openapi/v1.json, interactive docs at /docs (Scalar, as in Redkeep and Vaesen.Api).
    app.MapOpenApi();
    app.MapScalarApiReference("docs", options =>
    {
        options.Title = "Nexus Table API";
        options.Theme = ScalarTheme.DeepSpace;
        options.ShowDeveloperTools = DeveloperToolsVisibility.Always;
    });
}
else
{
    app.UseHttpsRedirection();
}

app.UseCors(WebAppCors);
app.UseAuthentication();
app.UseAuthorization();

// Liveness: the process is up. Readiness: its dependencies (DynamoDB, ...) are reachable too.
app.MapHealthChecks("/health", new HealthCheckOptions { Predicate = check => !check.Tags.Contains(CoreServiceExtensions.ReadyHealthTag) });
app.MapHealthChecks("/health/ready");

app.MapControllers();
app.MapHub<TableHub>(TableHub.Path);

app.Run();

// Exposes Program to WebApplicationFactory in the integration tests.
public partial class Program;
