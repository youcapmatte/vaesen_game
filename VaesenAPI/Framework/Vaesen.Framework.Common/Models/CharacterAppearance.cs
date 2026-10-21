using Newtonsoft.Json;

namespace Vaesen.Framework.Common.Models;

/// <summary>
/// How a character looks, as data: preset IDs and normalised numbers, never meshes or engine values. Any client
/// rebuilds the character from this object alone (the game's ApplyAppearance). Stored as a nested map on the
/// character item. Fields left out use their defaults; IDs are checked against <see cref="AppearanceOptions"/>.
/// </summary>
public sealed class CharacterAppearance
{
    /// <summary>The version this record was written for; lets the game migrate old records.</summary>
    public const int CurrentVersion = 1;

    [JsonProperty("appearanceVersion")]
    public int AppearanceVersion { get; set; } = CurrentVersion;

    /// <summary>"female" or "male". Filters which body, face and hair presets are allowed.</summary>
    [JsonProperty("sex")]
    public string? Sex { get; set; }

    [JsonProperty("bodyPresetId")]
    public string? BodyPresetId { get; set; }

    [JsonProperty("skinTone")]
    public SkinTone SkinTone { get; set; } = new();

    /// <summary>A base face; its asset never changes once shipped. Uniqueness comes from the sliders.</summary>
    [JsonProperty("facePresetId")]
    public string? FacePresetId { get; set; }

    /// <summary>Sculpt sliders by name (e.g. "jawWidth"), each from -1 to 1. Sliders left out are 0.</summary>
    [JsonProperty("faceSliders")]
    public Dictionary<string, float> FaceSliders { get; set; } = new(StringComparer.Ordinal);

    /// <summary>0 = young adult, 1 = elderly.</summary>
    [JsonProperty("age")]
    public float Age { get; set; }

    [JsonProperty("eyeColorId")]
    public string? EyeColorId { get; set; }

    [JsonProperty("hairStyleId")]
    public string? HairStyleId { get; set; }

    [JsonProperty("hairColor")]
    public HairColor HairColor { get; set; } = new();

    [JsonProperty("eyebrowStyleId")]
    public string? EyebrowStyleId { get; set; }

    /// <summary>Null: none.</summary>
    [JsonProperty("facialHairId")]
    public string? FacialHairId { get; set; }
}

/// <summary>MetaHuman skin tone: a point (U, V) in its tone model, each 0 to 1.</summary>
public sealed class SkinTone
{
    [JsonProperty("u")]
    public float U { get; set; } = 0.5f;

    [JsonProperty("v")]
    public float V { get; set; } = 0.5f;
}

/// <summary>Natural hair colour (melanin, redness), greying and an artificial dye amount, each 0 to 1.</summary>
public sealed class HairColor
{
    [JsonProperty("melanin")]
    public float Melanin { get; set; } = 0.5f;

    [JsonProperty("redness")]
    public float Redness { get; set; }

    /// <summary>How much of the hair is white (salt and pepper); 1 is white.</summary>
    [JsonProperty("grey")]
    public float Grey { get; set; }

    [JsonProperty("dye")]
    public float Dye { get; set; }
}
