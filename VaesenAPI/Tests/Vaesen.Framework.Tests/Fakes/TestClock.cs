namespace Vaesen.Framework.Tests.Fakes;

/// <summary>A clock tests can move forward, e.g. to let a character claim time out.</summary>
public sealed class TestClock : TimeProvider
{
    private DateTimeOffset _now = DateTimeOffset.UtcNow;

    public override DateTimeOffset GetUtcNow() => _now;

    public void Advance(TimeSpan by) => _now += by;
}
