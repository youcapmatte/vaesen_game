namespace Vaesen.Framework.Common.Models;

/// <summary>
/// What a valid <see cref="CharacterAppearance"/> may contain: the "Characters:Appearance" configuration section.
/// The game's preset data table (DT_AppearancePresets) is the source of these lists; until its export exists, the
/// defaults below match the spec's placeholder IDs. A list left empty in configuration uses its default.
/// </summary>
public sealed class AppearanceOptions
{
    public static readonly IReadOnlyList<string> Sexes = ["female", "male"];

    /// <summary>Per sex ("female", "male"): the allowed body, face, hair, eyebrow and facial hair preset IDs.</summary>
    public Dictionary<string, SexPresets> PresetsBySex { get; set; } = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Eye colours, for either sex.</summary>
    public List<string> EyeColors { get; set; } = [];

    /// <summary>The sculpt slider names.</summary>
    public List<string> FaceSliders { get; set; } = [];

    /// <summary>The allowed skin tone region (MetaHuman's tone model has invalid corners).</summary>
    public float SkinToneMinU { get; set; } = 0f;
    public float SkinToneMaxU { get; set; } = 0.85f;
    public float SkinToneMinV { get; set; } = 0f;
    public float SkinToneMaxV { get; set; } = 1f;

    public SexPresets EffectivePresets(string sex) =>
        PresetsBySex.TryGetValue(sex, out var configured) ? configured.WithDefaults(DefaultPresets(sex)) : DefaultPresets(sex);

    public IReadOnlyList<string> EffectiveEyeColors => EyeColors.Count > 0 ? EyeColors : DefaultEyeColors;

    public IReadOnlyList<string> EffectiveFaceSliders => FaceSliders.Count > 0 ? FaceSliders : DefaultFaceSliders;

    public static readonly IReadOnlyList<string> DefaultEyeColors =
        ["eye_blue", "eye_grey_blue", "eye_grey", "eye_green", "eye_grey_green", "eye_hazel", "eye_light_brown", "eye_brown", "eye_dark_brown", "eye_amber"];

    public static readonly IReadOnlyList<string> DefaultFaceSliders =
        ["jawWidth", "chinLength", "cheekboneHeight", "cheekboneWidth", "noseWidth", "noseLength", "browHeight", "eyeSpacing", "lipFullness"];

    public static SexPresets DefaultPresets(string sex)
    {
        var s = string.Equals(sex, "male", StringComparison.OrdinalIgnoreCase) ? "m" : "f";
        return new SexPresets
        {
            Bodies = [$"body_{s}_slim", $"body_{s}_average", $"body_{s}_heavy"],
            Faces = Enumerable.Range(1, 15).Select(i => $"face_{s}_{i:00}").ToList(),
            HairStyles = Enumerable.Range(1, 8).Select(i => $"hair_{s}_{i:00}").ToList(),
            EyebrowStyles = Enumerable.Range(1, 5).Select(i => $"brow_{s}_{i:00}").ToList(),
            FacialHair = s == "m" ? Enumerable.Range(1, 6).Select(i => $"beard_m_{i:00}").ToList() : [],
        };
    }
}

/// <summary>The preset IDs allowed for one sex.</summary>
public sealed class SexPresets
{
    public List<string> Bodies { get; set; } = [];
    public List<string> Faces { get; set; } = [];
    public List<string> HairStyles { get; set; } = [];
    public List<string> EyebrowStyles { get; set; } = [];
    public List<string> FacialHair { get; set; } = [];

    /// <summary>These lists, with any empty one taken from <paramref name="defaults"/>. (Facial hair may stay empty.)</summary>
    public SexPresets WithDefaults(SexPresets defaults) => new()
    {
        Bodies = Bodies.Count > 0 ? Bodies : defaults.Bodies,
        Faces = Faces.Count > 0 ? Faces : defaults.Faces,
        HairStyles = HairStyles.Count > 0 ? HairStyles : defaults.HairStyles,
        EyebrowStyles = EyebrowStyles.Count > 0 ? EyebrowStyles : defaults.EyebrowStyles,
        FacialHair = FacialHair,
    };
}
