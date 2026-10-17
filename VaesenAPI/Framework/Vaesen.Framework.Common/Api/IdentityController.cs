using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Core.Security;

namespace Vaesen.Framework.Common.Api;

[Authorize(Policy = Policies.Player)]
[ApiController]
[Route("api/identity")]
public sealed class IdentityController : ControllerBase
{
    /// <summary>Lets the game client confirm its token works and learn its own subject.</summary>
    [HttpGet("me")]
    public ActionResult<ResultBase<MeResult>> Me() =>
        Ok(new ResultBase<MeResult>().Ok(new MeResult { PlayerId = User.GetSubject() }));
}

public sealed class MeResult
{
    [JsonProperty("playerId")]
    public string PlayerId { get; set; } = "";
}
