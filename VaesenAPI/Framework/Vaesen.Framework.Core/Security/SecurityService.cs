using Microsoft.AspNetCore.Http;

namespace Vaesen.Framework.Core.Security;

/// <summary>Who is calling the current request. Services use this instead of reading HttpContext themselves.</summary>
public interface ISecurityService
{
    bool IsAuthenticated { get; }

    /// <summary>The caller's token subject ("sub"), or null when anonymous.</summary>
    string? CurrentSubject { get; }
}

public sealed class SecurityService : ISecurityService
{
    private readonly IHttpContextAccessor _httpContextAccessor;

    public SecurityService(IHttpContextAccessor httpContextAccessor)
    {
        _httpContextAccessor = httpContextAccessor;
    }

    public bool IsAuthenticated => _httpContextAccessor.HttpContext?.User.Identity?.IsAuthenticated == true;

    public string? CurrentSubject => IsAuthenticated ? _httpContextAccessor.HttpContext!.User.FindFirst("sub")?.Value : null;
}
