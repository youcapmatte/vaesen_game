namespace Vaesen.Framework.Mystery.Ai;

/// <summary>
/// List prices in USD per million tokens, for the cost line of the generation report. From Anthropic's pricing page
/// (platform.claude.com/docs/en/about-claude/pricing), October 2026. Unknown models cost nothing in the report.
/// </summary>
public static class ModelPrices
{
    private sealed record Price(double Input, double CacheWrite, double CacheRead, double Output);

    private static readonly (string Prefix, Price Price)[] Prices =
    [
        ("claude-opus-5-5", new Price(4, 5, 0.20, 20)),
        ("claude-sonnet-5-5", new Price(2, 2.50, 0.10, 10)),
        ("claude-haiku-5-5", new Price(0.10, 0.125, 0.01, 0.50)),
    ];

    /// <summary>The cost of one call. <paramref name="input"/> is the uncached input, as the API reports it.</summary>
    public static double Cost(string? model, long input, long cacheWrite, long cacheRead, long output)
    {
        var price = Prices.FirstOrDefault(p => model?.StartsWith(p.Prefix, StringComparison.Ordinal) == true).Price;
        return price is null
            ? 0
            : (input * price.Input + cacheWrite * price.CacheWrite + cacheRead * price.CacheRead + output * price.Output) / 1_000_000;
    }
}
