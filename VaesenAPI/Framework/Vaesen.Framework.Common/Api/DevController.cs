using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Services;
using Vaesen.Framework.Core.Api;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Common.Api;

/// <summary>Development-only login with seed guilds and players. Answers 404 in every other environment.</summary>
[DevelopmentOnly]
[AllowAnonymous]
[ApiController]
[Route("api/dev")]
public sealed class DevController : ControllerBase
{
    private readonly IDevLoginService _devLogin;

    public DevController(IDevLoginService devLogin)
    {
        _devLogin = devLogin;
    }

    [HttpGet("guilds")]
    public ActionResult<ResultBase<IReadOnlyList<DevGuild>>> Guilds() =>
        Ok(new ResultBase<IReadOnlyList<DevGuild>>().Ok(_devLogin.GetGuilds()));

    [HttpPost("login")]
    public ActionResult<ResultBase<DevLoginResult>> Login([FromBody] DevLoginRequest? request) =>
        this.ToActionResult(_devLogin.Login(request ?? new DevLoginRequest()));
}
