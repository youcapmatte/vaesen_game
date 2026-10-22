using Newtonsoft.Json;

namespace Vaesen.Framework.Mystery.Models;

/// <summary>
/// Base for canon content: authored once (by the ContentTool or by hand), reviewed, then read by every Society.
/// Ids are readable slugs ("nack", "inn", "level-ostervik-october"), not guids. A record is never changed in place
/// once approved: a change is published as a new <see cref="Version"/>.
/// </summary>
public abstract class ContentRecord
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("version")]
    public int Version { get; set; } = 1;

    /// <summary><see cref="ContentStatus.Draft"/> until the owner has reviewed it; only approved records are used by generation.</summary>
    [JsonProperty("status")]
    public string Status { get; set; } = ContentStatus.Draft;

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonIgnore]
    [System.Text.Json.Serialization.JsonIgnore]
    public bool IsApproved => Status == ContentStatus.Approved;
}

public static class ContentStatus
{
    public const string Draft = "draft";
    public const string Approved = "approved";
    public const string Retired = "retired";
}
