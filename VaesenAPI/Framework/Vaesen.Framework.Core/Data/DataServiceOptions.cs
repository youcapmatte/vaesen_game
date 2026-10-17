namespace Vaesen.Framework.Core.Data;

/// <summary>The "DataService" configuration section: where repositories store their data.</summary>
public sealed class DataServiceOptions
{
    public const string SectionName = "DataService";

    /// <summary>Name of the GSI holding the inverted keys (GSI1PK / GSI1SK). See docs/DynamoDB.md.</summary>
    public const string Gsi1 = "GSI1";

    /// <summary>DynamoDB table holding all Vaesen data for this environment, e.g. "vaesen-dev".</summary>
    public string TableName { get; set; } = "";

    /// <summary>Set to use DynamoDB Local (e.g. http://localhost:8000). Empty means real AWS.</summary>
    public string? ServiceUrl { get; set; }

    /// <summary>Create the table on startup if it's missing. Only honoured with DynamoDB Local.</summary>
    public bool CreateTableIfMissing { get; set; }
}
