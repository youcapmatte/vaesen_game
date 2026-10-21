using System.Net;
using System.Net.Http.Json;
using Microsoft.Extensions.Configuration;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Services;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Tests;

/// <summary>A character's appearance: stored with the character when created, and checked against the allowed presets.</summary>
public sealed class AppearanceTests : IAsyncLifetime
{
    private VaesenApiFactory _factory = null!;
    private HttpClient _client = null!;
    private Account _founder = null!;
    private Guild _guild = null!;

    public async Task InitializeAsync()
    {
        _factory = new VaesenApiFactory();
        _client = _factory.CreateClient();
        _founder = (await (await _client.PostAsJsonAsync("/api/accounts", new { username = "Ingrid" })).ReadResultAsync<Account>()).Result!;
        _guild = (await (await _client.PostAsJsonAsync("/api/guilds", new { name = "Lantern Watch", createdByAccountId = _founder.Id }))
            .ReadResultAsync<Guild>()).Result!;
    }

    public async Task DisposeAsync() => await _factory.DisposeAsync();

    /// <summary>A valid female appearance, as the creator would send it; <paramref name="change"/> alters one field.</summary>
    private static Dictionary<string, object?> Appearance(Action<Dictionary<string, object?>>? change = null)
    {
        var appearance = new Dictionary<string, object?>
        {
            ["appearanceVersion"] = 1,
            ["sex"] = "female",
            ["bodyPresetId"] = "body_f_slim",
            ["skinTone"] = new { u = 0.42, v = 0.3 },
            ["facePresetId"] = "face_f_07",
            ["faceSliders"] = new Dictionary<string, double> { ["jawWidth"] = 0.2, ["cheekboneHeight"] = -0.3 },
            ["age"] = 0.35,
            ["eyeColorId"] = "eye_grey_blue",
            ["hairStyleId"] = "hair_f_01",
            ["hairColor"] = new { melanin = 0.7, redness = 0.3, dye = 0.0 },
            ["eyebrowStyleId"] = "brow_f_03",
            ["facialHairId"] = null,
        };
        change?.Invoke(appearance);
        return appearance;
    }

    private Task<HttpResponseMessage> CreateAsync(object? appearance) =>
        _client.PostAsJsonAsync($"/api/guilds/{_guild.Id}/characters",
            new { firstName = "Astrid", lastName = "Falk", @class = "Occultist", appearance, createdByAccountId = _founder.Id });

    [Fact]
    public async Task Create_WithAppearance_StoresIt_AndItLoadsBackTheSame()
    {
        var created = await CreateAsync(Appearance());

        Assert.Equal(HttpStatusCode.Created, created.StatusCode);
        var id = (await created.ReadResultAsync<Character>()).Result!.Id;
        var loaded = (await (await _client.GetAsync($"/api/characters/{id}")).ReadResultAsync<Character>()).Result!.Appearance!;
        Assert.Equal(1, loaded.AppearanceVersion);
        Assert.Equal("female", loaded.Sex);
        Assert.Equal("body_f_slim", loaded.BodyPresetId);
        Assert.Equal("face_f_07", loaded.FacePresetId);
        Assert.Equal(0.42f, loaded.SkinTone.U, 3);
        Assert.Equal(0.2f, loaded.FaceSliders["jawWidth"], 3);
        Assert.Equal(-0.3f, loaded.FaceSliders["cheekboneHeight"], 3);
        Assert.Equal(0.35f, loaded.Age, 3);
        Assert.Equal("eye_grey_blue", loaded.EyeColorId);
        Assert.Equal(0.7f, loaded.HairColor.Melanin, 3);
        Assert.Null(loaded.FacialHairId);
    }

    [Fact]
    public async Task Create_WithoutAppearance_IsStillAllowed()
    {
        var created = await CreateAsync(null);

        Assert.Equal(HttpStatusCode.Created, created.StatusCode);
        Assert.Null((await created.ReadResultAsync<Character>()).Result!.Appearance);
    }

    [Fact]
    public async Task Create_TidiesSexIdsAndSliderNames()
    {
        var created = await CreateAsync(Appearance(a =>
        {
            a["sex"] = " Female ";
            a["facePresetId"] = " face_f_07 ";
            a["faceSliders"] = new Dictionary<string, double> { ["JAWWIDTH"] = 0.5 };
        }));

        var appearance = (await created.ReadResultAsync<Character>()).Result!.Appearance!;
        Assert.Equal("female", appearance.Sex);
        Assert.Equal("face_f_07", appearance.FacePresetId);
        Assert.Equal(0.5f, appearance.FaceSliders["jawWidth"], 3);
    }

    public static TheoryData<string, object?> InvalidFields => new()
    {
        { "appearanceVersion", 99 },
        { "sex", "unknown" },
        { "sex", null },
        { "facePresetId", "face_m_01" },     // a male face on a female character
        { "facePresetId", "face_f_99" },
        { "facePresetId", null },
        { "bodyPresetId", "body_f_giant" },
        { "eyeColorId", "eye_red" },
        { "hairStyleId", "hair_m_01" },
        { "facialHairId", "beard_m_01" },    // no facial hair for female characters by default
        { "age", 1.5 },
        { "age", -0.1 },
        { "skinTone", new { u = 0.95, v = 0.5 } },  // outside MetaHuman's valid tone region
        { "hairColor", new { melanin = 2.0, redness = 0.0, dye = 0.0 } },
        { "faceSliders", new Dictionary<string, double> { ["jawWidth"] = 1.2 } },
        { "faceSliders", new Dictionary<string, double> { ["earSize"] = 0.1 } },
    };

    [Theory]
    [MemberData(nameof(InvalidFields))]
    public async Task Create_WithInvalidAppearance_IsBadRequest(string field, object? value)
    {
        var response = await CreateAsync(Appearance(a => a[field] = value));

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
        Assert.Equal(ErrorCodes.Invalid, (await response.ReadResultAsync<Character>()).ErrorCode);
    }

    [Fact]
    public void Validate_MaleWithFacialHair_IsValid()
    {
        var appearance = new CharacterAppearance
        {
            Sex = "male",
            BodyPresetId = "body_m_heavy",
            FacePresetId = "face_m_03",
            EyeColorId = "eye_brown",
            HairStyleId = "hair_m_02",
            EyebrowStyleId = "brow_m_01",
            FacialHairId = "beard_m_04",
        };

        Assert.Null(AppearanceRules.Validate(appearance, new AppearanceOptions()));
    }

    [Fact]
    public void ExportedPresetsFile_BindsToAppearanceOptions()
    {
        // The shape UVaesenAppearanceLibrary::ToApiOptionsJson writes (the game's preset table export).
        const string exported = """
            {"Characters":{"Appearance":{
              "PresetsBySex":{
                "female":{"Bodies":["body_f_spike"],"Faces":["face_f_spike_a","face_f_spike_b"],"HairStyles":["hair_f_updo_buns"],"EyebrowStyles":["brow_f_default"],"FacialHair":[]},
                "male":{"Bodies":[],"Faces":[],"HairStyles":[],"EyebrowStyles":[],"FacialHair":[]}},
              "EyeColors":["eye_brown"],
              "FaceSliders":["jawWidth","noseWidth","cheekboneHeight","age"]}}}
            """;
        var configuration = new ConfigurationBuilder()
            .AddJsonStream(new MemoryStream(System.Text.Encoding.UTF8.GetBytes(exported)))
            .Build();
        var options = new CharacterOptions();
        configuration.GetSection(CharacterOptions.SectionName).Bind(options);

        var female = options.Appearance.EffectivePresets("female");
        Assert.Equal(["face_f_spike_a", "face_f_spike_b"], female.Faces);
        Assert.Equal(["body_f_spike"], female.Bodies);
        Assert.Empty(female.FacialHair);
        Assert.Equal(["eye_brown"], options.Appearance.EffectiveEyeColors);
        Assert.Contains("age", options.Appearance.EffectiveFaceSliders);
        // A sex with nothing exported yet keeps the placeholder defaults.
        Assert.Contains("face_m_01", options.Appearance.EffectivePresets("male").Faces);

        var appearance = new CharacterAppearance
        {
            Sex = "female", BodyPresetId = "body_f_spike", FacePresetId = "face_f_spike_a", EyeColorId = "eye_brown",
            HairStyleId = "hair_f_updo_buns", EyebrowStyleId = "brow_f_default", FaceSliders = { ["jawWidth"] = 0.5f },
        };
        Assert.Null(AppearanceRules.Validate(appearance, options.Appearance));
    }

    [Fact]
    public void Validate_UsesConfiguredPresets_InsteadOfDefaults()
    {
        var options = new AppearanceOptions
        {
            PresetsBySex = { ["female"] = new SexPresets { Faces = ["face_f_spike_a"] } },
        };
        var appearance = new CharacterAppearance
        {
            Sex = "female",
            BodyPresetId = "body_f_slim",
            FacePresetId = "face_f_07",
            EyeColorId = "eye_brown",
            HairStyleId = "hair_f_01",
            EyebrowStyleId = "brow_f_01",
        };

        Assert.NotNull(AppearanceRules.Validate(appearance, options));
        appearance.FacePresetId = "face_f_spike_a";
        Assert.Null(AppearanceRules.Validate(appearance, options));
    }
}
