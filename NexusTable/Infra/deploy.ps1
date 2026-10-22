<#
.SYNOPSIS
    Deploys the Nexus Table to AWS (see README.md in this folder).

.DESCRIPTION
    -Bootstrap   Once per account and region: prepares CDK (an S3 bucket and an ECR repository for the images).
    -Dns         Creates the nexus.norse.games hosted zone and prints the four name servers for Namecheap.
    (no switch)  Uploads secrets.prod.json to Secrets Manager, builds the three images, deploys everything,
                 and restarts the services so they read the current secret values.

    Needs the AWS CLI (profile "vaesen"), Node (for npx aws-cdk), the .NET 10 SDK, and Docker Desktop running.
#>
param(
    [switch]$Bootstrap,
    [switch]$Dns,
    [string]$AwsProfile = "vaesen"
)

$ErrorActionPreference = "Stop"
$Region = "eu-central-1"
$SecretName = "nexus-table/prod"
$Cluster = "nexus-table"
$Services = @("nexus-table-api", "nexus-table-web", "nexus-gm")
$Cdk = "aws-cdk@2"

Set-Location $PSScriptRoot

function Invoke-Checked([string]$What, [scriptblock]$Command) {
    & $Command
    if ($LASTEXITCODE -ne 0) { throw "$What failed (exit code $LASTEXITCODE)." }
}

function New-RandomHex {
    $bytes = New-Object byte[] 32
    [Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes)
    ($bytes | ForEach-Object { $_.ToString("x2") }) -join ""
}

$account = (aws sts get-caller-identity --profile $AwsProfile --query Account --output text)
if ($LASTEXITCODE -ne 0) { throw "The AWS profile '$AwsProfile' isn't signed in. Run: aws sso login --profile $AwsProfile" }
$env:CDK_DEFAULT_ACCOUNT = $account
$env:CDK_DEFAULT_REGION = $Region
Write-Host "AWS account $account, region $Region"

if ($Bootstrap) {
    Invoke-Checked "cdk bootstrap" { npx --yes $Cdk bootstrap "aws://$account/$Region" --profile $AwsProfile }
    return
}

if ($Dns) {
    Invoke-Checked "cdk deploy NexusTableDns" { npx --yes $Cdk deploy NexusTableDns --profile $AwsProfile --require-approval never }
    $servers = aws cloudformation describe-stacks --stack-name NexusTableDns --profile $AwsProfile --region $Region `
        --query "Stacks[0].Outputs[?OutputKey=='NameServers'].OutputValue" --output text
    Write-Host ""
    Write-Host "At Namecheap (norse.games > Advanced DNS), add one NS record per name server, host 'nexus':"
    $servers -split " " | ForEach-Object { Write-Host "  NS  nexus  $_" }
    Write-Host "Then run .\deploy.ps1 once 'Resolve-DnsName nexus.norse.games -Type NS' answers with them."
    return
}

# ---- 1. The keys: secrets.prod.json -> Secrets Manager ----

$secretsFile = Join-Path $PSScriptRoot "secrets.prod.json"
if (-not (Test-Path $secretsFile)) {
    throw "Copy secrets.example.json to secrets.prod.json and fill it in (it is ignored by git)."
}
$secrets = Get-Content $secretsFile -Raw | ConvertFrom-Json

# The two values nobody hands you are made here once and kept in the file, so they stay the same between deploys.
$generated = $false
foreach ($name in @("NEXUS_AGENT_KEY", "AUTH0_SECRET")) {
    if (-not $secrets.$name) { $secrets.$name = New-RandomHex; $generated = $true }
}
if ($generated) { $secrets | ConvertTo-Json | Set-Content $secretsFile -Encoding UTF8 }

$missing = $secrets.PSObject.Properties | Where-Object { -not $_.Value -or $_.Value -like "*your-project*" } | ForEach-Object { $_.Name }
if ($missing) { throw "secrets.prod.json still needs: $($missing -join ', ')" }

# Passed as a file so no key ends up on a command line.
$secretString = Join-Path ([IO.Path]::GetTempPath()) "nexus-table-secret-$([guid]::NewGuid()).json"
try {
    $secrets | ConvertTo-Json -Compress | Set-Content $secretString -Encoding ASCII -NoNewline
    $exists = aws secretsmanager list-secrets --profile $AwsProfile --region $Region `
        --filters "Key=name,Values=$SecretName" --query "SecretList[?Name=='$SecretName'] | length(@)" --output text
    if ($exists -eq "0") {
        Invoke-Checked "create-secret" { aws secretsmanager create-secret --name $SecretName --profile $AwsProfile --region $Region `
            --description "Nexus Table keys (from NexusTable/Infra/secrets.prod.json)" --secret-string "file://$secretString" | Out-Null }
        Write-Host "Created the secret $SecretName"
    }
    else {
        Invoke-Checked "put-secret-value" { aws secretsmanager put-secret-value --secret-id $SecretName --profile $AwsProfile --region $Region `
            --secret-string "file://$secretString" | Out-Null }
        Write-Host "Updated the secret $SecretName"
    }
}
finally {
    Remove-Item $secretString -ErrorAction SilentlyContinue
}
$secretArn = aws secretsmanager describe-secret --secret-id $SecretName --profile $AwsProfile --region $Region --query ARN --output text

# ---- 2. The delegation has to work, or the certificate waits for DNS validation for hours ----

$zoneServers = aws cloudformation describe-stacks --stack-name NexusTableDns --profile $AwsProfile --region $Region `
    --query "Stacks[0].Outputs[?OutputKey=='NameServers'].OutputValue" --output text 2>$null
if (-not $zoneServers) { throw "Run .\deploy.ps1 -Dns first." }
$published = @(Resolve-DnsName nexus.norse.games -Type NS -Server 1.1.1.1 -ErrorAction SilentlyContinue |
    Where-Object { $_.Type -eq "NS" } | ForEach-Object { $_.NameHost.TrimEnd(".") })
$expected = $zoneServers -split " " | ForEach-Object { $_.TrimEnd(".") }
if (-not ($published | Where-Object { $expected -contains $_ })) {
    throw "nexus.norse.games isn't delegated to Route 53 yet. Add the NS records at Namecheap (.\deploy.ps1 -Dns lists them) and wait a few minutes."
}

# ---- 3. Build the images and deploy ----

Invoke-Checked "docker" { docker info --format "{{.ServerVersion}}" | Out-Null }
Invoke-Checked "cdk deploy" { npx --yes $Cdk deploy NexusTableDns NexusTable --profile $AwsProfile -c "keysSecretArn=$secretArn" }

# ---- 4. Restart, so the tasks read the secret values just uploaded ----

foreach ($service in $Services) {
    Invoke-Checked "restart $service" { aws ecs update-service --cluster $Cluster --service $service --force-new-deployment `
        --profile $AwsProfile --region $Region --query "service.serviceName" --output text }
}
Write-Host ""
Write-Host "Deployed. The services restart over the next few minutes: https://nexus.norse.games"
