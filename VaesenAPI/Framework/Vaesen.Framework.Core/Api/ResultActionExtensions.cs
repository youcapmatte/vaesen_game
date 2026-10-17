using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Core.Api;

public static class ResultActionExtensions
{
    /// <summary>
    /// Returns the result with the HTTP status that matches it: 200 on success, otherwise from its
    /// <see cref="ResultBase.ErrorCode"/> (not_found 404, invalid 400, conflict 409, forbidden 403; anything else 400).
    /// </summary>
    public static ActionResult<ResultBase<T>> ToActionResult<T>(this ControllerBase controller, ResultBase<T> result)
    {
        if (result.Success)
        {
            return controller.Ok(result);
        }

        var status = result.ErrorCode switch
        {
            ErrorCodes.NotFound => StatusCodes.Status404NotFound,
            ErrorCodes.Conflict => StatusCodes.Status409Conflict,
            ErrorCodes.Forbidden => StatusCodes.Status403Forbidden,
            _ when result.ErrorCode?.EndsWith("_taken", StringComparison.Ordinal) == true => StatusCodes.Status409Conflict,
            _ => StatusCodes.Status400BadRequest,
        };
        return controller.StatusCode(status, result);
    }
}
