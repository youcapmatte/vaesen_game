# Phase 0 spike (P0-10): runs the packaged benchmark (PackageSpikeBenchmark.bat) once per detail setting and collects
# the results. Each run: 1080p window, the 24 MetaHumans of L_SpikeBenchmark, 5 s warm-up, 30 s measured, then quits.
# Results: the "[P0-10]" and "[P0-4]" log lines, and Saved\Spike\CrowdBenchmark.txt inside the packaged build.
# -Standalone runs the uncooked game from the editor build instead (UnrealEditor.exe -game): no packaging needed,
# slightly pessimistic numbers.
param(
    [int]$Seconds = 30,
    [int[]]$ForcedLODs = @(-1, 1, 2, 3),
    [switch]$Standalone,
    [string]$Map = '/Game/Spike/Maps/L_SpikeBenchmark'
)
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
if ($Standalone) {
    $exe = 'F:\UE_SOURCE\UE5_8\Engine\Binaries\Win64\UnrealEditor.exe'
    $prefix = @("`"$(Join-Path $root 'VaesenGame\VaesenGame.uproject')`"")
    $extra = @('-game')
    $logDir = Join-Path $root 'VaesenGame\Saved\Logs'
} else {
    $build = Join-Path $root 'Builds\Spike\Win64\Development'
    $exe = Join-Path $build 'VaesenGameClient.exe'
    if (-not (Test-Path $exe)) { throw "No packaged benchmark at $exe; run PackageSpikeBenchmark.bat first." }
    $prefix = @()
    $extra = @()
    $logDir = Join-Path $build 'VaesenGame\Saved\Logs'
}

foreach ($lod in $ForcedLODs) {
    $log = "SpikeBenchmark_$(Split-Path $Map -Leaf)_LOD$lod.log"
    $arguments = $prefix + @(
        $Map,
        '-windowed', '-ResX=1920', '-ResY=1080', '-nosound', '-NoVerifyGC',
        '-ini:Engine:[SystemSettings]:r.WarnOfBadDrivers=0',
        "-ExecCmds=`"t.MaxFPS 0, r.VSync 0, Vaesen.Spike.Crowd 0 none $Seconds $lod 1`"",
        "-log=$log"
    ) + $extra
    $p = Start-Process -FilePath $exe -ArgumentList $arguments -PassThru
    if (-not $p.WaitForExit(($Seconds + 180) * 1000)) { $p.Kill(); Write-Warning "LOD $lod run timed out" }
    # Let the GPU driver release the previous run's memory before the next one starts.
    Start-Sleep -Seconds 15
    Select-String -Path (Join-Path $logDir $log) -Pattern '\[P0-10\]|\[P0-4\]|\[Spike\]|Fatal|Ensure' | ForEach-Object { $_.Line }
}
