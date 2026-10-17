<#
.SYNOPSIS
    Creates the Vaesen DynamoDB table in AWS (VaesenAPI/docs/DynamoDB.md, section 4.2).

.DESCRIPTION
    On-demand billing, PK/SK keys, the GSI1 index, deletion protection and point-in-time recovery.
    Safe to re-run: an existing table is left as it is (only point-in-time recovery is switched on).
    Requires the AWS CLI and a signed-in profile:  aws sso login --profile vaesen

.EXAMPLE
    .\Scripts\Aws\CreateDynamoDbTable.ps1
    .\Scripts\Aws\CreateDynamoDbTable.ps1 -TableName vaesen-prod
#>
param(
    [string]$TableName = "vaesen-dev",
    [string]$Region = "eu-central-1",
    [string]$Profile = "vaesen"
)

$ErrorActionPreference = "Stop"
$aws = @("--profile", $Profile, "--region", $Region)

function Invoke-Aws {
    & aws @args @aws
    if ($LASTEXITCODE -ne 0) { throw "aws $($args -join ' ') failed (exit code $LASTEXITCODE)." }
}

Write-Host "Account:" -NoNewline
Invoke-Aws sts get-caller-identity --query Account --output text

# "Not found" is expected here; don't let its stderr output stop the script (Windows PowerShell treats it as an error).
$ErrorActionPreference = "Continue"
& aws dynamodb describe-table --table-name $TableName @aws --query "Table.TableStatus" --output text 2>&1 | Out-Null
$exists = $LASTEXITCODE -eq 0
$ErrorActionPreference = "Stop"

if ($exists) {
    Write-Host "Table '$TableName' already exists in $Region; leaving it unchanged."
}
else {
    $definition = @{
        TableName                 = $TableName
        BillingMode               = "PAY_PER_REQUEST"
        AttributeDefinitions      = @(
            @{ AttributeName = "PK"; AttributeType = "S" },
            @{ AttributeName = "SK"; AttributeType = "S" },
            @{ AttributeName = "GSI1PK"; AttributeType = "S" },
            @{ AttributeName = "GSI1SK"; AttributeType = "S" }
        )
        KeySchema                 = @(
            @{ AttributeName = "PK"; KeyType = "HASH" },
            @{ AttributeName = "SK"; KeyType = "RANGE" }
        )
        GlobalSecondaryIndexes    = @(
            @{
                IndexName  = "GSI1"
                KeySchema  = @(
                    @{ AttributeName = "GSI1PK"; KeyType = "HASH" },
                    @{ AttributeName = "GSI1SK"; KeyType = "RANGE" }
                )
                Projection = @{ ProjectionType = "ALL" }
            }
        )
        DeletionProtectionEnabled = $true
    }

    # Pass the definition as a JSON file: avoids PowerShell mangling commas and braces in CLI arguments.
    $file = New-TemporaryFile
    try {
        $definition | ConvertTo-Json -Depth 6 | Set-Content -Path $file.FullName -Encoding ascii
        Write-Host "Creating table '$TableName' in $Region..."
        Invoke-Aws dynamodb create-table --cli-input-json "file://$($file.FullName)" --query "TableDescription.TableArn" --output text
    }
    finally {
        Remove-Item $file.FullName -ErrorAction SilentlyContinue
    }

    Invoke-Aws dynamodb wait table-exists --table-name $TableName
    Write-Host "Table is active."
}

Write-Host "Point-in-time recovery:" -NoNewline
Invoke-Aws dynamodb update-continuous-backups --table-name $TableName `
    --point-in-time-recovery-specification PointInTimeRecoveryEnabled=true `
    --query "ContinuousBackupsDescription.PointInTimeRecoveryDescription.PointInTimeRecoveryStatus" --output text

Write-Host "Done. Browse it: https://$Region.console.aws.amazon.com/dynamodbv2/home?region=$Region#item-explorer?table=$TableName"
