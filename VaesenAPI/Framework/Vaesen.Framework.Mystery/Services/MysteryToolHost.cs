using System.Reflection;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace Vaesen.Framework.Mystery.Services;

/// <summary>
/// The service setup the command-line tools (CaseLab, ContentTool) share: configuration from appsettings.json, user
/// secrets and environment variables (Mystery__CreativeModel, ANTHROPIC_API_KEY...), console logging, the engine.
/// </summary>
public static class MysteryToolHost
{
    public static ServiceProvider Build(Action<MysteryOptions>? configure = null, LogLevel logLevel = LogLevel.Warning)
    {
        var configuration = new ConfigurationBuilder()
            .SetBasePath(AppContext.BaseDirectory)
            .AddJsonFile("appsettings.json", optional: true)
            .AddUserSecrets(Assembly.GetEntryAssembly() ?? typeof(MysteryToolHost).Assembly, optional: true)
            .AddEnvironmentVariables()
            .Build();

        var services = new ServiceCollection();
        services.AddLogging(b => b.AddSimpleConsole(o => o.SingleLine = true).SetMinimumLevel(logLevel));
        services.AddMysteryServices(configuration);
        if (configure is not null)
        {
            services.PostConfigure(configure);
        }

        return services.BuildServiceProvider();
    }

    /// <summary>
    /// The content folder in the source tree (so the ContentTool edits the files in git, not the copy in bin): walks up
    /// from the working directory and the tool's directory looking for Vaesen.Framework.Mystery/Content.
    /// </summary>
    public static string? FindSourceContent()
    {
        foreach (var start in new[] { Directory.GetCurrentDirectory(), AppContext.BaseDirectory })
        {
            for (var dir = new DirectoryInfo(start); dir is not null; dir = dir.Parent)
            {
                foreach (var candidate in new[]
                {
                    Path.Combine(dir.FullName, "Framework", "Vaesen.Framework.Mystery", "Content"),
                    Path.Combine(dir.FullName, "VaesenAPI", "Framework", "Vaesen.Framework.Mystery", "Content"),
                })
                {
                    if (Directory.Exists(candidate))
                    {
                        return candidate;
                    }
                }
            }
        }

        return null;
    }
}

/// <summary>Minimal "--name value" and "--flag" argument parsing for the tools.</summary>
public sealed class ToolArgs
{
    private readonly Dictionary<string, string?> _options = new(StringComparer.OrdinalIgnoreCase);

    public ToolArgs(IEnumerable<string> args)
    {
        string? pendingName = null;
        foreach (var arg in args)
        {
            if (arg.StartsWith("--", StringComparison.Ordinal))
            {
                if (pendingName is not null)
                {
                    _options[pendingName] = null;
                }

                pendingName = arg[2..];
            }
            else if (pendingName is not null)
            {
                _options[pendingName] = arg;
                pendingName = null;
            }
            else
            {
                Positional.Add(arg);
            }
        }

        if (pendingName is not null)
        {
            _options[pendingName] = null;
        }
    }

    public List<string> Positional { get; } = [];

    public bool Has(string name) => _options.ContainsKey(name);

    public string? Get(string name) => _options.GetValueOrDefault(name);

    public int? GetInt(string name) => int.TryParse(Get(name), out var value) ? value : null;
}
