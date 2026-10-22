using Microsoft.Extensions.Options;
using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Data;

/// <summary>Each Society's world, one document per Society. Files today; DynamoDB (SOCIETY#{id}) later.</summary>
public interface IWorldStore
{
    /// <summary>The Society's world, or a not_found failure before it is seeded.</summary>
    Task<ResultBase<SocietyWorld>> GetAsync(string societyId, CancellationToken cancellationToken = default);

    /// <summary>Saves the world. Fails with conflict if it changed since <paramref name="world"/> was read.</summary>
    Task<ResultBase<SocietyWorld>> SaveAsync(SocietyWorld world, CancellationToken cancellationToken = default);
}

/// <summary>Worlds as JSON files: {WorldPath}/{societyId}.json.</summary>
public sealed class FileWorldStore(IOptions<MysteryOptions> options) : IWorldStore
{
    private static readonly SemaphoreSlim Gate = new(1, 1);

    public string Root => string.IsNullOrWhiteSpace(options.Value.WorldPath)
        ? Path.Combine(Directory.GetCurrentDirectory(), ".data", "worlds")
        : options.Value.WorldPath;

    public async Task<ResultBase<SocietyWorld>> GetAsync(string societyId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<SocietyWorld>();
        var path = PathFor(societyId);
        if (!File.Exists(path))
        {
            return ret.Fail("The Society's world has not been seeded yet.", ErrorCodes.NotFound);
        }

        try
        {
            var world = JsonConvert.DeserializeObject<SocietyWorld>(await File.ReadAllTextAsync(path, cancellationToken));
            return world is null ? ret.Fail("The world file is empty.", ErrorCodes.NotFound) : ret.Ok(world);
        }
        catch (Exception ex) when (ex is IOException or JsonException)
        {
            return ret.Fail($"Could not read the world: {ex.Message}");
        }
    }

    public async Task<ResultBase<SocietyWorld>> SaveAsync(SocietyWorld world, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<SocietyWorld>();
        await Gate.WaitAsync(cancellationToken);
        try
        {
            var path = PathFor(world.SocietyId);
            if (File.Exists(path))
            {
                var stored = JsonConvert.DeserializeObject<SocietyWorld>(await File.ReadAllTextAsync(path, cancellationToken));
                if (stored is not null && stored.Version != world.Version)
                {
                    return ret.Fail("The world changed while this was being saved; try again.", ErrorCodes.Conflict);
                }
            }

            Directory.CreateDirectory(Root);
            world.Version++;
            world.UpdatedAt = DateTime.UtcNow;
            await File.WriteAllTextAsync(path, JsonConvert.SerializeObject(world, Formatting.Indented), cancellationToken);
            return ret.Ok(world);
        }
        catch (IOException ex)
        {
            return ret.Fail($"Could not save the world: {ex.Message}");
        }
        finally
        {
            Gate.Release();
        }
    }

    private string PathFor(string societyId) =>
        Path.Combine(Root, new string(societyId.Select(c => char.IsLetterOrDigit(c) || c is '-' or '_' ? c : '_').ToArray()) + ".json");
}
