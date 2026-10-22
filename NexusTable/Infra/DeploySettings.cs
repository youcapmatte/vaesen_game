using Amazon.CDK;

namespace Nexus.Table.Infra;

/// <summary>
/// The deployment's settings, from the "context" in cdk.json. Secrets are not here: they live in one
/// Secrets Manager secret that deploy.ps1 fills from secrets.prod.json and passes in as keysSecretArn.
/// </summary>
public sealed record DeploySettings(
    string Region,
    string Domain,
    string ApiDomain,
    string TableName,
    string Auth0Domain,
    string Auth0ClientId,
    string Auth0Audience,
    string[] HostSubjects,
    string? KeysSecretArn,
    string? NarratorVoice,
    string? NpcVoice)
{
    public string WebUrl => $"https://{Domain}";

    public string ApiUrl => $"https://{ApiDomain}";

    public static DeploySettings From(App app)
    {
        string Required(string key) => Optional(key) ?? throw new InvalidOperationException($"cdk.json is missing the context value \"{key}\".");
        string? Optional(string key) => app.Node.TryGetContext(key) is string value && value.Length > 0 ? value : null;

        return new DeploySettings(
            Region: Required("region"),
            Domain: Required("domain"),
            ApiDomain: Required("apiDomain"),
            TableName: Required("tableName"),
            Auth0Domain: Required("auth0Domain"),
            Auth0ClientId: Required("auth0ClientId"),
            Auth0Audience: Required("auth0Audience"),
            // Several hosts are separated by commas.
            HostSubjects: Required("hostSubjects").Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries),
            KeysSecretArn: Optional("keysSecretArn"),
            NarratorVoice: Optional("narratorVoice"),
            NpcVoice: Optional("npcVoice"));
    }
}
