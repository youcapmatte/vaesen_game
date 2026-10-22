using Amazon.CDK;
using Amazon.CDK.AWS.Route53;
using Constructs;

namespace Nexus.Table.Infra;

/// <summary>
/// The hosted zone for the Nexus Table's subdomain (nexus.norse.games). norse.games itself stays at Namecheap,
/// which delegates this subdomain with four NS records: the NameServers output below.
/// </summary>
public sealed class DnsStack : Stack
{
    public IPublicHostedZone Zone { get; }

    public DnsStack(Construct scope, string id, DeploySettings settings, IStackProps props)
        : base(scope, id, props)
    {
        var zone = new PublicHostedZone(this, "Zone", new PublicHostedZoneProps
        {
            ZoneName = settings.Domain,
            Comment = "Nexus Table. Delegated from Namecheap (norse.games) with NS records.",
        });

        // Deleting the zone would break the delegation at Namecheap, and a new zone gets new name servers.
        zone.ApplyRemovalPolicy(RemovalPolicy.RETAIN);
        Zone = zone;

        new CfnOutput(this, "NameServers", new CfnOutputProps
        {
            Value = Fn.Join(" ", zone.HostedZoneNameServers!),
            Description = $"Add each as an NS record for \"{settings.Domain.Split('.')[0]}\" at Namecheap.",
        });
    }
}
