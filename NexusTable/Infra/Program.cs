using Amazon.CDK;
using Nexus.Table.Infra;

// Two stacks: the DNS zone first (its name servers go in at Namecheap), then everything that needs it.
var app = new App();
var settings = DeploySettings.From(app);
var environment = new Amazon.CDK.Environment
{
    Account = System.Environment.GetEnvironmentVariable("CDK_DEFAULT_ACCOUNT"),
    Region = settings.Region,
};

var dns = new DnsStack(app, "NexusTableDns", settings, new StackProps
{
    Env = environment,
    Description = $"Nexus Table: the {settings.Domain} hosted zone, delegated from Namecheap.",
});

new AppStack(app, "NexusTable", settings, dns.Zone, new StackProps
{
    Env = environment,
    Description = "Nexus Table: DynamoDB, the API, the web app and the Gamemaster agent on ECS Fargate behind one load balancer.",
});

app.Synth();
