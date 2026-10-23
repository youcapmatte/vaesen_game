using System.Reflection;

namespace Vaesen.Framework.Mystery.Models;

/// <summary>
/// The property takes one of a fixed set of values: the static <c>All</c> list of <paramref name="valuesType"/>
/// (e.g. <see cref="FactKinds"/>). The model output schema carries it as an enum, so the model picks from the list
/// instead of inventing a near miss ("surface" for "crude") that the checks then reject.
/// </summary>
[AttributeUsage(AttributeTargets.Property)]
public sealed class OneOfAttribute(Type valuesType) : Attribute
{
    public IReadOnlyList<string> Values { get; } =
        valuesType.GetField("All", BindingFlags.Public | BindingFlags.Static)?.GetValue(null) as IReadOnlyList<string>
        ?? throw new ArgumentException($"{valuesType.Name} has no static All list of strings.", nameof(valuesType));
}
