using Vaesen.Framework.Common.Models;

namespace Vaesen.Framework.Common.Services;

/// <summary>
/// What a valid <see cref="CharacterAppearance"/> looks like, so a modified client can't store broken data. Every ID
/// must be an allowed preset for the character's sex, and every number a finite value within its range; anything else
/// is rejected (not clamped), so a bad client is noticed rather than silently corrected.
/// </summary>
public static class AppearanceRules
{
    /// <summary>The appearance tidied for storage (sex lower-case, IDs trimmed, slider names as configured), or null.</summary>
    public static CharacterAppearance? Normalize(CharacterAppearance? appearance, AppearanceOptions options)
    {
        if (appearance is null)
        {
            return null;
        }

        var sliderNames = options.EffectiveFaceSliders;
        return new CharacterAppearance
        {
            AppearanceVersion = appearance.AppearanceVersion,
            Sex = CharacterRules.Clean(appearance.Sex)?.ToLowerInvariant(),
            BodyPresetId = CharacterRules.Clean(appearance.BodyPresetId),
            SkinTone = appearance.SkinTone ?? new SkinTone(),
            FacePresetId = CharacterRules.Clean(appearance.FacePresetId),
            FaceSliders = (appearance.FaceSliders ?? [])
                .ToDictionary(p => sliderNames.FirstOrDefault(n => string.Equals(n, p.Key, StringComparison.OrdinalIgnoreCase)) ?? p.Key, p => p.Value, StringComparer.Ordinal),
            Age = appearance.Age,
            EyeColorId = CharacterRules.Clean(appearance.EyeColorId),
            HairStyleId = CharacterRules.Clean(appearance.HairStyleId),
            HairColor = appearance.HairColor ?? new HairColor(),
            EyebrowStyleId = CharacterRules.Clean(appearance.EyebrowStyleId),
            FacialHairId = CharacterRules.Clean(appearance.FacialHairId),
        };
    }

    /// <summary>Null if valid (or absent: older clients create characters without one), else what's wrong.</summary>
    public static string? Validate(CharacterAppearance? appearance, AppearanceOptions options)
    {
        if (appearance is null)
        {
            return null;
        }

        if (appearance.AppearanceVersion < 1 || appearance.AppearanceVersion > CharacterAppearance.CurrentVersion)
        {
            return $"Appearance version must be between 1 and {CharacterAppearance.CurrentVersion}.";
        }

        var sex = AppearanceOptions.Sexes.FirstOrDefault(s => string.Equals(s, appearance.Sex?.Trim(), StringComparison.OrdinalIgnoreCase));
        if (sex is null)
        {
            return $"Sex must be one of: {string.Join(", ", AppearanceOptions.Sexes)}.";
        }

        var presets = options.EffectivePresets(sex);
        return Required(appearance.BodyPresetId, presets.Bodies, "Body")
            ?? Required(appearance.FacePresetId, presets.Faces, "Face")
            ?? Required(appearance.EyeColorId, options.EffectiveEyeColors, "Eye colour")
            ?? Required(appearance.HairStyleId, presets.HairStyles, "Hairstyle")
            ?? Required(appearance.EyebrowStyleId, presets.EyebrowStyles, "Eyebrows")
            ?? Optional(appearance.FacialHairId, presets.FacialHair, "Facial hair")
            ?? Range(appearance.SkinTone?.U, options.SkinToneMinU, options.SkinToneMaxU, "Skin tone U")
            ?? Range(appearance.SkinTone?.V, options.SkinToneMinV, options.SkinToneMaxV, "Skin tone V")
            ?? Range(appearance.Age, 0f, 1f, "Age")
            ?? Range(appearance.HairColor?.Melanin, 0f, 1f, "Hair melanin")
            ?? Range(appearance.HairColor?.Redness, 0f, 1f, "Hair redness")
            ?? Range(appearance.HairColor?.Grey, 0f, 1f, "Hair grey")
            ?? Range(appearance.HairColor?.Dye, 0f, 1f, "Hair dye")
            ?? Sliders(appearance.FaceSliders, options.EffectiveFaceSliders);
    }

    private static string? Required(string? id, IReadOnlyList<string> allowed, string field) =>
        string.IsNullOrWhiteSpace(id) ? $"{field} is required." : Optional(id, allowed, field);

    private static string? Optional(string? id, IReadOnlyList<string> allowed, string field) =>
        string.IsNullOrWhiteSpace(id) || allowed.Contains(id.Trim(), StringComparer.OrdinalIgnoreCase)
            ? null
            : $"{field} '{id}' isn't available for this character.";

    private static string? Range(float? value, float min, float max, string field) =>
        value is null || (float.IsFinite(value.Value) && value >= min && value <= max)
            ? null
            : $"{field} must be between {min} and {max}.";

    private static string? Sliders(Dictionary<string, float>? sliders, IReadOnlyList<string> names)
    {
        foreach (var (name, value) in sliders ?? [])
        {
            if (!names.Contains(name, StringComparer.OrdinalIgnoreCase))
            {
                return $"Unknown face slider '{name}'.";
            }

            if (Range(value, -1f, 1f, $"Face slider '{name}'") is { } error)
            {
                return error;
            }
        }

        return null;
    }
}
