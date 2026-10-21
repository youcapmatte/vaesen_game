using Newtonsoft.Json;
using Nexus.Table.Models;

namespace Nexus.Table.Services;

/// <summary>The Vaesen character rules, read once from the embedded Rules/vaesen-rules.json.</summary>
public interface IRulesService
{
    VaesenRules Rules { get; }

    /// <summary>A d66 roll on the memento table (first die tens, second ones).</summary>
    RuleMemento RollMemento();
}

public sealed class RulesService : IRulesService
{
    private const string ResourceName = "Nexus.Table.Rules.vaesen-rules.json";

    private readonly IDiceRoller _dice;

    public RulesService(IDiceRoller dice)
    {
        _dice = dice;
        using var stream = typeof(RulesService).Assembly.GetManifestResourceStream(ResourceName)
            ?? throw new InvalidOperationException($"Embedded resource {ResourceName} is missing.");
        using var reader = new StreamReader(stream);
        Rules = JsonConvert.DeserializeObject<VaesenRules>(reader.ReadToEnd())
            ?? throw new InvalidOperationException("vaesen-rules.json is empty.");
    }

    public VaesenRules Rules { get; }

    public RuleMemento RollMemento()
    {
        var roll = (_dice.D6() * 10) + _dice.D6();
        return Rules.Mementos.Find(m => m.Roll == roll) ?? new RuleMemento { Roll = roll, Item = "Something of your own choosing" };
    }
}
