using System.Security.Cryptography;

namespace Nexus.Table.Services;

/// <summary>Six-sided dice. Rolled on the server only; tests swap in fixed dice.</summary>
public interface IDiceRoller
{
    int D6();
}

public sealed class CryptoDiceRoller : IDiceRoller
{
    public int D6() => RandomNumberGenerator.GetInt32(1, 7);
}
