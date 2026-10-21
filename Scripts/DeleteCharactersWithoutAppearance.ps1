<#
.SYNOPSIS
    Deletes the characters made before the character creator: the ones with no appearance.

.DESCRIPTION
    Goes through the development seed guilds (api/dev/guilds, found by name: api/guilds/by-name/{name}),
    lists their characters, and picks those without an appearance. Without -Delete it only lists them. With -Delete it
    deletes them through the API (DELETE api/characters/{id}: the character leaves its guild and its name is free again),
    but only if there are at most -Expected of them (default 1), so a mistake can't wipe a guild.

    The API must be running locally (dotnet run in VaesenAPI/Api/Vaesen.Api, Development).

.EXAMPLE
    Scripts\DeleteCharactersWithoutAppearance.ps1
    Scripts\DeleteCharactersWithoutAppearance.ps1 -Delete
#>
param(
    [string]$ApiUrl = "http://localhost:5230",
    [switch]$Delete,
    [int]$Expected = 1
)

$ErrorActionPreference = "Stop"
$seeds = (Invoke-RestMethod -Uri "$ApiUrl/api/dev/guilds").result
$found = @()
foreach ($seed in $seeds) {
    try {
        $guild = (Invoke-RestMethod -Uri "$ApiUrl/api/guilds/by-name/$([uri]::EscapeDataString($seed.name))").result
    } catch {
        Write-Host "$($seed.name): no guild yet"
        continue
    }
    $response = Invoke-RestMethod -Uri "$ApiUrl/api/guilds/$($guild.id)/characters"
    if (-not $response.success) {
        throw "Listing $($guild.name) failed: $($response.errorMessage)"
    }
    foreach ($character in $response.result) {
        $hasAppearance = $null -ne $character.PSObject.Properties["appearance"] -and $null -ne $character.appearance
        Write-Host ("{0,-16} {1,-28} {2,-40} {3}" -f $seed.id, $character.name, $character.id, $(if ($hasAppearance) { "appearance" } else { "NO APPEARANCE" }))
        if (-not $hasAppearance) {
            $found += [pscustomobject]@{ Guild = $seed.id; Id = $character.id; Name = $character.name }
        }
    }
}

Write-Host ""
Write-Host "$($found.Count) character(s) without an appearance."
if (-not $Delete -or $found.Count -eq 0) {
    if ($found.Count -gt 0) { Write-Host "Run again with -Delete to delete them." }
    return
}
if ($found.Count -gt $Expected) {
    throw "Expected at most $Expected, found $($found.Count): not deleting. Check the list, then pass -Expected $($found.Count) if they should all go."
}
foreach ($character in $found) {
    $response = Invoke-RestMethod -Method Delete -Uri "$ApiUrl/api/characters/$($character.Id)"
    if (-not $response.success) {
        throw "Deleting $($character.Name) failed: $($response.errorMessage)"
    }
    Write-Host "Deleted $($character.Name) ($($character.Id)) from $($character.Guild)."
}
