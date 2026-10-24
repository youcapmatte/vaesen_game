using Newtonsoft.Json;

namespace Vaesen.Framework.Mystery.Models;

/// <summary>
/// Where a level is (fixes 4, section 13): the real place it is modelled on, its parish and county, what an educated
/// person of the period knows of it, the towns around it and where its authorities sit.
/// </summary>
public sealed class LevelGeography
{
    /// <summary>The real place it is modelled on, to ground the model in real history and routes. Never shown to players, never named in a case.</summary>
    [JsonProperty("realAnalogue")]
    public string RealAnalogue { get; set; } = "";

    [JsonProperty("fictional")]
    public bool Fictional { get; set; } = true;

    /// <summary>The real analogue's position (latitude, longitude), so code can check what the neighbours say of distance and direction (review 3, item 7).</summary>
    [JsonProperty("analogueAt", NullValueHandling = NullValueHandling.Ignore)]
    public GeoPoint? AnalogueAt { get; set; }

    [JsonProperty("province")]
    public string Province { get; set; } = "";

    [JsonProperty("county")]
    public string County { get; set; } = "";

    [JsonProperty("parish")]
    public string Parish { get; set; } = "";

    /// <summary>What an educated Swede of the period knows of the place before arriving.</summary>
    [JsonProperty("knownFor")]
    public List<string> KnownFor { get; set; } = [];

    /// <summary>The towns around it: the only towns a case on the level names, besides headquarters and the capital.</summary>
    [JsonProperty("neighbours")]
    public List<LevelNeighbour> Neighbours { get; set; } = [];

    [JsonProperty("authorities")]
    public LevelAuthorities Authorities { get; set; } = new();

    [JsonIgnore]
    [System.Text.Json.Serialization.JsonIgnore]
    public bool IsEmpty => Neighbours.Count == 0 && KnownFor.Count == 0 && string.IsNullOrWhiteSpace(Province);
}

public sealed class LevelNeighbour
{
    [JsonProperty("name")]
    public string Name { get; set; } = "";

    /// <summary>"town", "county seat", "village", "works"...</summary>
    [JsonProperty("kind")]
    public string Kind { get; set; } = "";

    /// <summary>A rough travel distance ("about 45 km south-east by road").</summary>
    [JsonProperty("distance")]
    public string Distance { get; set; } = "";

    [JsonProperty("offers")]
    public List<string> Offers { get; set; } = [];

    /// <summary>The town's real position, if known.</summary>
    [JsonProperty("at", NullValueHandling = NullValueHandling.Ignore)]
    public GeoPoint? At { get; set; }
}

/// <summary>A real position in degrees.</summary>
public sealed class GeoPoint
{
    [JsonProperty("lat")]
    public double Lat { get; set; }

    [JsonProperty("lon")]
    public double Lon { get; set; }

    /// <summary>The great-circle distance in km and the compass bearing in degrees from here to <paramref name="to"/>.</summary>
    public (double Km, double Bearing) To(GeoPoint to)
    {
        const double R = 6371;
        double Rad(double d) => d * Math.PI / 180;
        var (f1, f2, dl) = (Rad(Lat), Rad(to.Lat), Rad(to.Lon - Lon));
        var a = Math.Pow(Math.Sin((f2 - f1) / 2), 2) + Math.Cos(f1) * Math.Cos(f2) * Math.Pow(Math.Sin(dl / 2), 2);
        var km = 2 * R * Math.Asin(Math.Sqrt(a));
        var bearing = Math.Atan2(Math.Sin(dl) * Math.Cos(f2), Math.Cos(f1) * Math.Sin(f2) - Math.Sin(f1) * Math.Cos(f2) * Math.Cos(dl)) * 180 / Math.PI;
        return (km, (bearing + 360) % 360);
    }
}

/// <summary>Where the level's authorities are and how far; empty means the level has none of that kind within reach.</summary>
public sealed class LevelAuthorities
{
    /// <summary>The crown sheriff (kronolänsman) or police: who, where, how far.</summary>
    [JsonProperty("police")]
    public string Police { get; set; } = "";

    /// <summary>The district court (häradsrätt): where it sits, and when.</summary>
    [JsonProperty("court")]
    public string Court { get; set; } = "";

    /// <summary>The provincial doctor (provinsialläkare).</summary>
    [JsonProperty("doctor")]
    public string Doctor { get; set; } = "";

    [JsonProperty("post")]
    public string Post { get; set; } = "";

    [JsonProperty("telegraph")]
    public string Telegraph { get; set; } = "";

    [JsonProperty("bank")]
    public string Bank { get; set; } = "";

    [JsonProperty("newspaper")]
    public string Newspaper { get; set; } = "";

    /// <summary>Each authority with the words a case uses for it, so a case can be checked against what the level has.</summary>
    public IEnumerable<(string Kind, string Where, string[] Words)> All() =>
    [
        ("police", Police, ["länsman", "sheriff", "constable"]),
        ("court", Court, ["häradsrätt", "district court", "assizes"]),
        ("doctor", Doctor, ["provincial doctor", "provinsialläkare", "district doctor"]),
        ("post", Post, ["post office"]),
        ("telegraph", Telegraph, ["telegraph", "telegram"]),
        ("bank", Bank, ["savings bank", "sparbank"]),
        ("newspaper", Newspaper, ["newspaper"]),
    ];
}

/// <summary>
/// How the Society gets to a level (fixes 4, section 14): routes made of legs, each leg with the months it runs and,
/// for railways, canals and steamers, the line it uses from the setting's <see cref="Setting.PeriodTransport"/>.
/// </summary>
public sealed class LevelJourney
{
    [JsonProperty("fromHeadquarters")]
    public string FromHeadquarters { get; set; } = "Uppsala";

    [JsonProperty("routes")]
    public List<TravelRoute> Routes { get; set; } = [];

    /// <summary>A short sensory description of the journey, for the GM to narrate on the way.</summary>
    [JsonProperty("journeyScene")]
    public string JourneyScene { get; set; } = "";
}

public sealed class TravelRoute
{
    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("legs")]
    public List<TravelLeg> Legs { get; set; } = [];
}

public sealed class TravelLeg
{
    [JsonProperty("from")]
    public string From { get; set; } = "";

    [JsonProperty("to")]
    public string To { get; set; } = "";

    /// <summary>One of <see cref="TravelModes"/>.</summary>
    [OneOf(typeof(TravelModes))]
    [JsonProperty("mode")]
    public string Mode { get; set; } = TravelModes.Coach;

    /// <summary>For a railway, canal or steamer leg: the id of the line in the setting's period transport.</summary>
    [JsonProperty("lineId")]
    public string? LineId { get; set; }

    [JsonProperty("duration")]
    public string Duration { get; set; } = "";

    [JsonProperty("cost")]
    public string Cost { get; set; } = "";

    [JsonProperty("schedule")]
    public string Schedule { get; set; } = "";

    /// <summary>The months it runs (1-12); empty: all year.</summary>
    [JsonProperty("months")]
    public List<int> Months { get; set; } = [];

    [JsonProperty("notes")]
    public string Notes { get; set; } = "";

    public bool RunsIn(int month) => Months.Count == 0 || Months.Contains(month);
}

public static class TravelModes
{
    public const string Railway = "railway";
    public const string Steamer = "steamer";
    public const string CanalBoat = "canalBoat";
    public const string Coach = "coach";
    public const string HiredCarriage = "hiredCarriage";
    public const string Cart = "cart";
    public const string Sleigh = "sleigh";
    public const string Boat = "boat";
    public const string OnFoot = "onFoot";

    public static readonly IReadOnlyList<string> All = [Railway, Steamer, CanalBoat, Coach, HiredCarriage, Cart, Sleigh, Boat, OnFoot];

    /// <summary>Modes that run on a line that opened in a given year, so the leg must name it.</summary>
    public static bool NeedsLine(string mode) => mode is Railway or Steamer or CanalBoat;
}

/// <summary>
/// A railway, canal, steamer route or telegraph line of the period (fixes 4, section 15), with the year it opened and
/// the months it runs, so a level or case never puts a train where there was none.
/// </summary>
public sealed class PeriodTransportLine
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    /// <summary>One of <see cref="TransportKinds"/>.</summary>
    [OneOf(typeof(TransportKinds))]
    [JsonProperty("kind")]
    public string Kind { get; set; } = TransportKinds.Railway;

    [JsonProperty("from")]
    public string From { get; set; } = "";

    [JsonProperty("to")]
    public string To { get; set; } = "";

    [JsonProperty("via")]
    public List<string> Via { get; set; } = [];

    /// <summary>The year it opened (the whole line, or the stretch the endpoints name).</summary>
    [JsonProperty("opened")]
    public int Opened { get; set; }

    /// <summary>The months it runs; empty: all year. Canals and lake steamers stop when the ice comes.</summary>
    [JsonProperty("months")]
    public List<int> Months { get; set; } = [];

    [JsonProperty("region")]
    public string Region { get; set; } = "";

    [JsonProperty("notes")]
    public string Notes { get; set; } = "";

    public bool OpenIn(int year, int month) => Opened <= year && (Months.Count == 0 || Months.Contains(month));

    [JsonIgnore]
    [System.Text.Json.Serialization.JsonIgnore]
    public IEnumerable<string> Places => Via.Prepend(From).Append(To);
}

public static class TransportKinds
{
    public const string Railway = "railway";
    public const string Canal = "canal";
    public const string Steamer = "steamer";
    public const string Telegraph = "telegraph";

    public static readonly IReadOnlyList<string> All = [Railway, Canal, Steamer, Telegraph];

    /// <summary>The kind of line a travel mode needs.</summary>
    public static string? For(string mode) => mode switch
    {
        TravelModes.Railway => Railway,
        TravelModes.CanalBoat => Canal,
        TravelModes.Steamer => Steamer,
        _ => null,
    };
}

/// <summary>Where a region's authorities sit (fixes 4, section 15): county seats, district courts, doctors, newspapers.</summary>
public sealed class RegionalInstitution
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    /// <summary>"countySeat", "districtCourt", "crownSheriff", "provincialDoctor", "newspaper", "bank"...</summary>
    [JsonProperty("kind")]
    public string Kind { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    /// <summary>The town it sits in.</summary>
    [JsonProperty("seat")]
    public string Seat { get; set; } = "";

    [JsonProperty("region")]
    public string Region { get; set; } = "";

    [JsonProperty("notes")]
    public string Notes { get; set; } = "";
}
