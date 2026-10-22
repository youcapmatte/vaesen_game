using Amazon.CDK;
using Amazon.CDK.AWS.CertificateManager;
using Amazon.CDK.AWS.DynamoDB;
using Amazon.CDK.AWS.EC2;
using Amazon.CDK.AWS.ECS;
using Amazon.CDK.AWS.ElasticLoadBalancingV2;
using Amazon.CDK.AWS.Logs;
using Amazon.CDK.AWS.Route53;
using Amazon.CDK.AWS.Route53.Targets;
using Amazon.CDK.AWS.SecretsManager;
using Constructs;
using DynamoAttribute = Amazon.CDK.AWS.DynamoDB.Attribute;
using DynamoTable = Amazon.CDK.AWS.DynamoDB.Table;
using EcsSecret = Amazon.CDK.AWS.ECS.Secret;
using TargetHealthCheck = Amazon.CDK.AWS.ElasticLoadBalancingV2.HealthCheck;

namespace Nexus.Table.Infra;

/// <summary>
/// Everything that runs the Nexus Table:
/// <list type="bullet">
///   <item>the DynamoDB table (kept if the stack is deleted)</item>
///   <item>a VPC with public subnets only, so no NAT gateway to pay for; tasks get public IPs for their outbound calls</item>
///   <item>one HTTPS load balancer: api.{domain} goes to the API, everything else to the web app</item>
///   <item>three Fargate services built from the Dockerfiles: the API, the web app, and the Gamemaster agent (outbound only)</item>
/// </list>
/// </summary>
public sealed class AppStack : Stack
{
    // The secret every task reads its keys from; deploy.ps1 fills it from secrets.prod.json.
    private const string PlaceholderSecretArn = "arn:aws:secretsmanager:eu-central-1:000000000000:secret:nexus-table/prod-AAAAAA";

    private readonly Cluster _cluster;

    public AppStack(Construct scope, string id, DeploySettings settings, IPublicHostedZone zone, IStackProps props)
        : base(scope, id, props)
    {
        var repositoryRoot = Path.GetFullPath(Path.Combine(Directory.GetCurrentDirectory(), "..", ".."));
        var nexusRoot = Path.Combine(repositoryRoot, "NexusTable");

        if (settings.KeysSecretArn is null)
        {
            Annotations.Of(this).AddError("No keysSecretArn: deploy with deploy.ps1, which creates the secret and passes its ARN.");
        }

        var keys = Amazon.CDK.AWS.SecretsManager.Secret.FromSecretCompleteArn(this, "Keys", settings.KeysSecretArn ?? PlaceholderSecretArn);

        // ---- Data ----

        var table = new DynamoTable(this, "Table", new TableProps
        {
            TableName = settings.TableName,
            PartitionKey = new DynamoAttribute { Name = "PK", Type = AttributeType.STRING },
            SortKey = new DynamoAttribute { Name = "SK", Type = AttributeType.STRING },
            BillingMode = BillingMode.PAY_PER_REQUEST,
            PointInTimeRecoverySpecification = new PointInTimeRecoverySpecification { PointInTimeRecoveryEnabled = true },
            RemovalPolicy = RemovalPolicy.RETAIN,
        });
        table.AddGlobalSecondaryIndex(new GlobalSecondaryIndexProps
        {
            IndexName = "GSI1",
            PartitionKey = new DynamoAttribute { Name = "GSI1PK", Type = AttributeType.STRING },
            SortKey = new DynamoAttribute { Name = "GSI1SK", Type = AttributeType.STRING },
            ProjectionType = ProjectionType.ALL,
        });

        // ---- Network, certificate and load balancer ----

        var vpc = new Vpc(this, "Vpc", new VpcProps
        {
            MaxAzs = 2,
            NatGateways = 0,
            SubnetConfiguration = [new SubnetConfiguration { Name = "public", SubnetType = SubnetType.PUBLIC, CidrMask = 24 }],
        });

        var certificate = new Certificate(this, "Certificate", new CertificateProps
        {
            DomainName = settings.Domain,
            SubjectAlternativeNames = [settings.ApiDomain],
            Validation = CertificateValidation.FromDns(zone),
        });

        var loadBalancer = new ApplicationLoadBalancer(this, "LoadBalancer", new ApplicationLoadBalancerProps
        {
            Vpc = vpc,
            InternetFacing = true,
            // SignalR keeps its WebSocket alive every 15 s; this only has to outlast that.
            IdleTimeout = Duration.Seconds(120),
        });
        loadBalancer.AddRedirect();
        var https = loadBalancer.AddListener("Https", new BaseApplicationListenerProps
        {
            Port = 443,
            Certificates = [ListenerCertificate.FromCertificateManager(certificate)],
            SslPolicy = SslPolicy.RECOMMENDED_TLS,
        });

        _cluster = new Cluster(this, "Cluster", new ClusterProps { Vpc = vpc, ClusterName = "nexus-table" });

        // ---- The API ----

        var apiImage = ContainerImage.FromAsset(repositoryRoot, new AssetImageProps
        {
            File = "NexusTable/Api/Dockerfile",
            // The context is the repository root (the API references VaesenAPI/Framework); send only the three projects.
            IgnoreMode = IgnoreMode.DOCKER,
            Exclude =
            [
                "*",
                "!NexusTable/Api/Dockerfile",
                "!NexusTable/Api/Nexus.Table/",
                "!NexusTable/Api/Nexus.Table.Api/",
                "!VaesenAPI/Framework/Vaesen.Framework.Core/",
                "**/bin/",
                "**/obj/",
                "**/.data/",
            ],
        });

        var apiEnvironment = new Dictionary<string, string>
        {
            ["AWS__Region"] = settings.Region,
            ["DataService__Provider"] = "DynamoDb",
            ["DataService__TableName"] = table.TableName,
            ["Auth0__Domain"] = settings.Auth0Domain,
            ["Auth0__Audience"] = settings.Auth0Audience,
            ["Cors__Origins__0"] = settings.WebUrl,
        };
        for (var i = 0; i < settings.HostSubjects.Length; i++)
        {
            apiEnvironment[$"Nexus__HostSubjects__{i}"] = settings.HostSubjects[i];
        }

        var api = AddService("Api", "nexus-table-api", cpu: 256, memoryMiB: 512, apiImage, port: 8080, apiEnvironment, new Dictionary<string, EcsSecret>
        {
            ["LiveKit__Url"] = EcsSecret.FromSecretsManager(keys, "LIVEKIT_URL"),
            ["LiveKit__ApiKey"] = EcsSecret.FromSecretsManager(keys, "LIVEKIT_API_KEY"),
            ["LiveKit__ApiSecret"] = EcsSecret.FromSecretsManager(keys, "LIVEKIT_API_SECRET"),
            ["LiveKit__AgentKey"] = EcsSecret.FromSecretsManager(keys, "NEXUS_AGENT_KEY"),
        });
        table.GrantReadWriteData(api.TaskDefinition.TaskRole);

        https.AddTargets("Api", new AddApplicationTargetsProps
        {
            Port = 8080,
            Protocol = ApplicationProtocol.HTTP,
            Targets = [api],
            Conditions = [ListenerCondition.HostHeaders([settings.ApiDomain])],
            Priority = 10,
            HealthCheck = new TargetHealthCheck { Path = "/health", HealthyHttpCodes = "200" },
            DeregistrationDelay = Duration.Seconds(15),
        });

        // ---- The web app ----

        var webImage = ContainerImage.FromAsset(Path.Combine(nexusRoot, "Web"), new AssetImageProps
        {
            BuildArgs = new Dictionary<string, string>
            {
                ["NEXT_PUBLIC_API_URL"] = settings.ApiUrl,
                ["NEXT_PUBLIC_DEV_LOGIN"] = "false",
            },
        });

        var web = AddService("Web", "nexus-table-web", cpu: 256, memoryMiB: 512, webImage, port: 3030, new Dictionary<string, string>
        {
            ["APP_BASE_URL"] = settings.WebUrl,
            ["AUTH0_DOMAIN"] = settings.Auth0Domain,
            ["AUTH0_CLIENT_ID"] = settings.Auth0ClientId,
            ["AUTH0_AUDIENCE"] = settings.Auth0Audience,
            ["AUTH0_CONNECTION"] = "discord",
        }, new Dictionary<string, EcsSecret>
        {
            ["AUTH0_CLIENT_SECRET"] = EcsSecret.FromSecretsManager(keys, "AUTH0_CLIENT_SECRET"),
            ["AUTH0_SECRET"] = EcsSecret.FromSecretsManager(keys, "AUTH0_SECRET"),
        });

        // Everything that isn't the API's host name.
        https.AddTargets("Web", new AddApplicationTargetsProps
        {
            Port = 3030,
            Protocol = ApplicationProtocol.HTTP,
            Targets = [web],
            HealthCheck = new TargetHealthCheck { Path = "/", HealthyHttpCodes = "200-399" },
            DeregistrationDelay = Duration.Seconds(15),
        });

        // ---- The Gamemaster agent: outbound only, to LiveKit, Deepgram, Anthropic, ElevenLabs and the API ----

        var agentEnvironment = new Dictionary<string, string> { ["NEXUS_API_URL"] = settings.ApiUrl };
        if (settings.NarratorVoice is not null)
        {
            agentEnvironment["NEXUS_NARRATOR_VOICE"] = settings.NarratorVoice;
        }
        if (settings.NpcVoice is not null)
        {
            agentEnvironment["NEXUS_NPC_VOICE"] = settings.NpcVoice;
        }

        AddService("Agent", "nexus-gm", cpu: 512, memoryMiB: 1024, ContainerImage.FromAsset(Path.Combine(nexusRoot, "GmAgent")), port: null, agentEnvironment, new Dictionary<string, EcsSecret>
        {
            ["LIVEKIT_URL"] = EcsSecret.FromSecretsManager(keys, "LIVEKIT_URL"),
            ["LIVEKIT_API_KEY"] = EcsSecret.FromSecretsManager(keys, "LIVEKIT_API_KEY"),
            ["LIVEKIT_API_SECRET"] = EcsSecret.FromSecretsManager(keys, "LIVEKIT_API_SECRET"),
            ["NEXUS_AGENT_KEY"] = EcsSecret.FromSecretsManager(keys, "NEXUS_AGENT_KEY"),
            ["DEEPGRAM_API_KEY"] = EcsSecret.FromSecretsManager(keys, "DEEPGRAM_API_KEY"),
            ["ANTHROPIC_API_KEY"] = EcsSecret.FromSecretsManager(keys, "ANTHROPIC_API_KEY"),
            ["ELEVEN_API_KEY"] = EcsSecret.FromSecretsManager(keys, "ELEVEN_API_KEY"),
        });

        // ---- DNS ----

        var target = RecordTarget.FromAlias(new LoadBalancerTarget(loadBalancer));
        new ARecord(this, "WebRecord", new ARecordProps { Zone = zone, RecordName = settings.Domain, Target = target });
        new ARecord(this, "ApiRecord", new ARecordProps { Zone = zone, RecordName = settings.ApiDomain, Target = target });

        new CfnOutput(this, "WebUrl", new CfnOutputProps { Value = settings.WebUrl });
        new CfnOutput(this, "ApiUrl", new CfnOutputProps { Value = settings.ApiUrl });
    }

    /// <summary>One container per task, one task per service, logs kept for a month.</summary>
    private FargateService AddService(
        string id,
        string serviceName,
        int cpu,
        int memoryMiB,
        ContainerImage image,
        int? port,
        Dictionary<string, string> environment,
        Dictionary<string, EcsSecret> secrets)
    {
        var task = new FargateTaskDefinition(this, $"{id}Task", new FargateTaskDefinitionProps { Cpu = cpu, MemoryLimitMiB = memoryMiB });

        var logs = new LogGroup(this, $"{id}Logs", new LogGroupProps
        {
            LogGroupName = $"/nexus-table/{serviceName}",
            Retention = RetentionDays.ONE_MONTH,
            RemovalPolicy = RemovalPolicy.DESTROY,
        });

        task.AddContainer(id, new ContainerDefinitionOptions
        {
            ContainerName = serviceName,
            Image = image,
            Environment = environment,
            Secrets = secrets,
            Logging = LogDrivers.AwsLogs(new AwsLogDriverProps { LogGroup = logs, StreamPrefix = serviceName }),
            PortMappings = port is { } containerPort ? [new PortMapping { ContainerPort = containerPort }] : null,
        });

        return new FargateService(this, $"{id}Service", new FargateServiceProps
        {
            Cluster = _cluster,
            ServiceName = serviceName,
            TaskDefinition = task,
            DesiredCount = 1,
            AssignPublicIp = true,
            VpcSubnets = new SubnetSelection { SubnetType = SubnetType.PUBLIC },
            // Start the new task before stopping the old one, and roll back a deployment that never gets healthy.
            MinHealthyPercent = 100,
            MaxHealthyPercent = 200,
            CircuitBreaker = new DeploymentCircuitBreaker { Rollback = true },
        });
    }
}
