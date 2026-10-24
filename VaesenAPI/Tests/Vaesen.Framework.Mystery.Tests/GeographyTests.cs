using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Content;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Services;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Tests;

/// <summary>Fixes 4, part C: a level's geography, its journey as legs, and the period transport they are checked against.</summary>
public sealed class GeographyTests
{
    /// <summary>A pack whose Gruvbo has a road route, a rail route on a line not yet open, and a canal route in summer only.</summary>
    private static ContentPack PackWithJourney()
    {
        var pack = ContentTests.LoadPack();
        pack.Setting.PeriodTransport =
        [
            new PeriodTransportLine { Id = "stockholm-uppsala", Name = "Stockholm–Uppsala railway", Kind = TransportKinds.Railway, From = "Stockholm", To = "Uppsala", Opened = 1866 },
            new PeriodTransportLine { Id = "uppsala-sala", Name = "Uppsala–Sala railway", Kind = TransportKinds.Railway, From = "Uppsala", To = "Sala", Opened = 1873 },
            new PeriodTransportLine { Id = "stromsholm-canal", Name = "Strömsholm Canal", Kind = TransportKinds.Canal, From = "Strömsholm", To = "Smedjebacken", Via = ["Ängelsberg"], Opened = 1795, Months = [5, 6, 7, 8, 9, 10] },
        ];
        pack.Setting.RegionalInstitutions = [new RegionalInstitution { Id = "vasteras-seat", Kind = "countySeat", Name = "Västmanland county governor", Seat = "Västerås" }];
        var gruvbo = pack.FindLevel("level-gruvbo-october")!;
        gruvbo.Geography = new LevelGeography
        {
            RealAnalogue = "Norberg (northern Västmanland)",
            Province = "Västmanland",
            County = "Västmanlands län",
            KnownFor = ["one of the oldest iron-mining districts of Bergslagen"],
            Neighbours = [new LevelNeighbour { Name = "Sala", Kind = "town", Distance = "about 45 km south-east by road" }],
            Authorities = new LevelAuthorities { Police = "the crown sheriff at Sala, half a day away", Doctor = "the provincial doctor at Sala" },
        };
        gruvbo.Journey = new LevelJourney
        {
            Routes =
            [
                new TravelRoute
                {
                    Name = "By road via Sala",
                    Legs = [new TravelLeg { From = "Uppsala", To = "Sala", Mode = TravelModes.Coach, Duration = "most of a day" }, new TravelLeg { From = "Sala", To = "Gruvbo", Mode = TravelModes.HiredCarriage, Duration = "half a day" }],
                },
                new TravelRoute
                {
                    Name = "By rail via Sala",
                    Legs = [new TravelLeg { From = "Uppsala", To = "Sala", Mode = TravelModes.Railway, LineId = "uppsala-sala", Duration = "three hours" }],
                },
                new TravelRoute
                {
                    Name = "By the canal",
                    Legs = [new TravelLeg { From = "Strömsholm", To = "Ängelsberg", Mode = TravelModes.CanalBoat, LineId = "stromsholm-canal", Duration = "a day", Months = [4, 5, 6, 7, 8, 9, 10] }],
                },
            ],
        };
        return pack;
    }

    [Fact]
    public void A_leg_on_a_line_not_yet_open_is_an_error_and_a_canal_leg_in_the_ice_months_a_warning()
    {
        var issues = ContentValidator.Validate(PackWithJourney()).Where(i => i.Location.StartsWith("level:level-gruvbo-october", StringComparison.Ordinal)).ToList();

        Assert.Contains(issues, i => i.IsError && i.Explanation.Contains("opened in 1873", StringComparison.Ordinal));
        Assert.Contains(issues, i => !i.IsError && i.Explanation.Contains("months 4", StringComparison.Ordinal));
        Assert.DoesNotContain(issues, i => i.Explanation.Contains("has no geography", StringComparison.Ordinal));
        Assert.Contains(issues, i => !i.IsError && i.Explanation.Contains("does not say where its court is", StringComparison.Ordinal));
    }

    [Fact]
    public void A_rail_leg_without_its_line_is_flagged()
    {
        var pack = PackWithJourney();
        pack.FindLevel("level-gruvbo-october")!.Journey.Routes[1].Legs[0].LineId = null;

        Assert.Contains(ContentValidator.Validate(pack), i => !i.IsError && i.Explanation.Contains("should name its line", StringComparison.Ordinal));
    }

    [Fact]
    public void Only_routes_that_run_that_month_and_year_are_offered()
    {
        var pack = PackWithJourney();
        var gruvbo = pack.FindLevel("level-gruvbo-october")!;

        Assert.Equal(["By road via Sala", "By the canal"], gruvbo.RoutesFor(1872, 10, pack.Setting).Select(r => r.Name));
        Assert.Equal(["By road via Sala"], gruvbo.RoutesFor(1872, 1, pack.Setting).Select(r => r.Name));
        Assert.Equal(["By road via Sala", "By rail via Sala"], gruvbo.RoutesFor(1873, 1, pack.Setting).Select(r => r.Name));
        Assert.Equal(["By road via Sala", "By rail via Sala", "By the canal"], gruvbo.RoutesFor(1873, 6, pack.Setting).Select(r => r.Name));
    }

    [Fact]
    public void A_case_naming_an_authority_or_town_the_level_lacks_or_its_real_analogue_is_warned()
    {
        var pack = PackWithJourney();
        var packet = ValidatorTests.SampleCase();
        packet.Hook.Text += " The district court at Västerås will hear it; they say it is as bad as Norberg. The sheriff at Sala knows.";
        var context = new ValidationContext
        {
            Pack = pack,
            Level = pack.FindLevel("level-gruvbo-october")!,
            Vaesen = packet.Brief.VaesenIds.Select(id => pack.FindVaesen(id)!).ToList(),
            Conflict = pack.FindConflict(packet.Brief.ConflictArchetypeId),
        };

        var geography = new CaseValidator().Validate(packet, context, CheckScope.Hook).Where(i => i.Location == "geography").ToList();

        Assert.Contains(geography, i => i.Explanation.Contains("district court", StringComparison.Ordinal));
        Assert.Contains(geography, i => i.Explanation.Contains("Västerås", StringComparison.Ordinal));
        Assert.Contains(geography, i => i.Explanation.Contains("Norberg", StringComparison.Ordinal));
        Assert.DoesNotContain(geography, i => i.Explanation.Contains("Sala", StringComparison.Ordinal) || i.Explanation.Contains("sheriff", StringComparison.Ordinal));
        Assert.All(geography, i => Assert.False(i.IsError));
    }

    [Fact]
    public void The_content_tool_schemas_carry_only_what_the_model_writes()
    {
        var placement = StructuredSchema.For(typeof(LevelPlacement)).ToJsonString();
        var canon = StructuredSchema.For(typeof(PeriodCanon)).ToJsonString();

        Assert.Contains("\"journeyScene\"", placement, StringComparison.Ordinal);
        Assert.Contains("\"canalBoat\"", placement, StringComparison.Ordinal);
        Assert.DoesNotContain("IsEmpty", placement, StringComparison.OrdinalIgnoreCase);
        Assert.Contains("\"opened\"", canon, StringComparison.Ordinal);
        Assert.DoesNotContain("\"places\"", canon, StringComparison.OrdinalIgnoreCase);
    }
}
