using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Logging;
using Newtonsoft.Json;
using Newtonsoft.Json.Serialization;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Core.Middleware;

/// <summary>Turns unhandled exceptions into a 500 with a <see cref="ResultBase"/> body, so clients always get the same shape.</summary>
public sealed class ResultExceptionMiddleware : IMiddleware
{
    private static readonly JsonSerializerSettings SerializerSettings = new()
    {
        ContractResolver = new CamelCasePropertyNamesContractResolver(),
    };

    private readonly ILogger<ResultExceptionMiddleware> _logger;

    public ResultExceptionMiddleware(ILogger<ResultExceptionMiddleware> logger)
    {
        _logger = logger;
    }

    public async Task InvokeAsync(HttpContext context, RequestDelegate next)
    {
        try
        {
            await next(context).ConfigureAwait(false);
        }
        catch (Exception ex) when (!context.Response.HasStarted && !context.RequestAborted.IsCancellationRequested)
        {
            _logger.LogError(ex, "Unhandled exception for {Method} {Path}", context.Request.Method, context.Request.Path);

            var result = new ResultBase().Fail("Something went wrong on the server.", "server_error");
            context.Response.StatusCode = StatusCodes.Status500InternalServerError;
            context.Response.ContentType = "application/json";
            await context.Response.WriteAsync(JsonConvert.SerializeObject(result, SerializerSettings)).ConfigureAwait(false);
        }
    }
}
