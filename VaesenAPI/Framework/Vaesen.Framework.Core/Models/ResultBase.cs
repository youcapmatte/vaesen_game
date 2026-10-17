using Newtonsoft.Json;

namespace Vaesen.Framework.Core.Models;

/// <summary>
/// The envelope every service and endpoint returns: <c>{ "success": true, "result": {...} }</c> or
/// <c>{ "success": false, "errorMessage": "...", "errorCode": "..." }</c>. HTTP status codes still apply on top.
/// </summary>
public class ResultBase
{
    [JsonProperty("success")]
    public bool Success { get; set; }

    /// <summary>Readable error, safe to show to the user.</summary>
    [JsonProperty("errorMessage")]
    public string ErrorMessage { get; set; } = string.Empty;

    /// <summary>Optional machine-readable error, e.g. <c>username_taken</c>. See <see cref="ErrorCodes"/>.</summary>
    [JsonProperty("errorCode")]
    public string? ErrorCode { get; set; }

    public ResultBase Fail(string errorMessage, string? errorCode = null)
    {
        Success = false;
        ErrorMessage = errorMessage;
        ErrorCode = errorCode;
        return this;
    }

    public ResultBase Ok()
    {
        Success = true;
        ErrorMessage = string.Empty;
        ErrorCode = null;
        return this;
    }
}

public class ResultBase<T> : ResultBase
{
    [JsonProperty("result")]
    public T? Result { get; set; }

    public new ResultBase<T> Fail(string errorMessage, string? errorCode = null)
    {
        base.Fail(errorMessage, errorCode);
        return this;
    }

    public ResultBase<T> Ok(T result)
    {
        base.Ok();
        Result = result;
        return this;
    }
}

/// <summary>Error codes shared by services, so endpoints can map them to HTTP status codes.</summary>
public static class ErrorCodes
{
    public const string NotFound = "not_found";
    public const string Invalid = "invalid";
    public const string Conflict = "conflict";
    public const string Forbidden = "forbidden";
}
