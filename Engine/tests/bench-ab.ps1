<#
.SYNOPSIS
    Automated A/B and A/A benchmark runner for Battle with GPU enforcement,
    worktree isolation for main, ABBA sampling, cold start discard, and statistical summary.
#>
param(
    [ValidateSet('high', 'low')][string]$Gpu = 'high',
    [ValidateSet('AB', 'AA')][string]$Mode = 'AB',
    [int]$Runs = 10,
    [int]$Units = 30000,
    [int]$Frames = 600,
    [int]$Warmup = 150,
    [switch]$NoBuild,
    [string]$MainRef = 'main'
)

$ErrorActionPreference = 'Stop'

function Get-Median {
    param([double[]]$Values)
    if ($Values.Count -eq 0) { return 0.0 }
    $sorted = $Values | Sort-Object
    $count = $sorted.Count
    if ($count % 2 -eq 1) {
        return $sorted[[math]::Floor($count / 2)]
    }
    $mid = $count / 2
    return ($sorted[$mid - 1] + $sorted[$mid]) / 2.0
}

function Get-Stats {
    param([double[]]$Values)
    if ($Values.Count -eq 0) {
        return [PSCustomObject]@{ Count = 0; Mean = 0.0; Median = 0.0; Min = 0.0; Max = 0.0; StDev = 0.0; StdErr = 0.0 }
    }
    $count = $Values.Count
    $sum = 0.0
    foreach ($v in $Values) { $sum += $v }
    $mean = $sum / $count

    $sqSum = 0.0
    foreach ($v in $Values) { $sqSum += [math]::Pow($v - $mean, 2) }
    $variance = if ($count -gt 1) { $sqSum / ($count - 1) } else { 0.0 }
    $stdev = [math]::Sqrt($variance)
    $stderr = if ($count -gt 0) { $stdev / [math]::Sqrt($count) } else { 0.0 }

    $sorted = $Values | Sort-Object
    $min = $sorted[0]
    $max = $sorted[-1]
    $median = Get-Median $Values

    return [PSCustomObject]@{
        Count  = $count
        Mean   = [math]::Round($mean, 3)
        Median = [math]::Round($median, 3)
        Min    = [math]::Round($min, 3)
        Max    = [math]::Round($max, 3)
        StDev  = [math]::Round($stdev, 3)
        StdErr = [math]::Round($stderr, 3)
    }
}

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$WorktreeDir = Join-Path $RepoRoot "Engine\build\worktree-main"
$BranchExe = Join-Path $RepoRoot "Game\build\x64\Release\Battle\Battle.exe"

Write-Host "=== Battle Benchmark Runner ===" -ForegroundColor Cyan
Write-Host "Mode: $Mode | GPU Preference: $Gpu | Runs: $Runs per side | Units: $Units | Frames: $Frames | Warmup: $Warmup"

# 1. Build Branch B if requested
if (!$NoBuild) {
    Write-Host "[Build] Building current branch Release Battle.exe..." -ForegroundColor Yellow
    & (Join-Path $RepoRoot "Engine\BuildSupport\Invoke-MSBuild.ps1") -Project (Join-Path $RepoRoot "Game\Battle\Battle.vcxproj") -Configuration Release
}

if (!(Test-Path $BranchExe)) {
    throw "Branch executable not found: $BranchExe"
}

$SideBSha = (& git -C $RepoRoot rev-parse --short HEAD).Trim()
$SideBExe = $BranchExe
$SideALabel = "A"
$SideBLabel = "B"

# 2. Setup Side A
if ($Mode -eq 'AA') {
    $SideAExe = $BranchExe
    $SideASha = $SideBSha
    $SideALabel = "A1 ($SideASha)"
    $SideBLabel = "A2 ($SideBSha)"
    Write-Host "[Mode AA] Running self-test against current branch: $SideBSha" -ForegroundColor Green
} else {
    Write-Host "[Mode AB] Preparing main worktree at $WorktreeDir..." -ForegroundColor Yellow
    if (!(Test-Path (Split-Path $WorktreeDir -Parent))) {
        New-Item -ItemType Directory -Path (Split-Path $WorktreeDir -Parent) -Force | Out-Null
    }

    $needsInit = $true
    if (Test-Path (Join-Path $WorktreeDir ".git")) {
        $currentWorktreeHead = (& git -C $WorktreeDir rev-parse --short HEAD).Trim()
        Write-Host "Existing worktree found at $currentWorktreeHead"
        $needsInit = $false
    }

    if ($needsInit) {
        if (Test-Path $WorktreeDir) {
            & git -C $RepoRoot worktree remove $WorktreeDir --force 2>$null
            if (Test-Path $WorktreeDir) { Remove-Item -Path $WorktreeDir -Recurse -Force }
        }
        Write-Host "Creating git worktree for $MainRef..."
        & git -C $RepoRoot worktree add $WorktreeDir $MainRef --detach
        if ($LASTEXITCODE -ne 0) { throw "git worktree add failed" }

        # Cherry-pick benchmark harness commits onto main
        Write-Host "Cherry-picking benchmark harness onto main..."
        $commitsToPick = @("af995a8", "2c2091a", "f1a1b06", "61e96ba")
        $origEA = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            foreach ($c in $commitsToPick) {
                Write-Host "Cherry-picking $c..."
                $cpOut = & git -C $WorktreeDir cherry-pick $c 2>&1
                if ($LASTEXITCODE -ne 0) {
                    # Check for 034-PLAN.md conflict
                    if (Test-Path (Join-Path $WorktreeDir "Agent/Tasks/034-PLAN.md")) {
                        & git -C $WorktreeDir rm -f Agent/Tasks/034-PLAN.md 2>$null | Out-Null
                        & git -C $WorktreeDir cherry-pick --continue --no-edit 2>$null | Out-Null
                    } else {
                        throw "Cherry-pick $c failed: $cpOut"
                    }
                }
            }
        } finally {
            $ErrorActionPreference = $origEA
        }
    }

    if (!$NoBuild) {
        Write-Host "[Build] Building main worktree Release Battle.exe..." -ForegroundColor Yellow
        & (Join-Path $RepoRoot "Engine\BuildSupport\Invoke-MSBuild.ps1") -Project (Join-Path $WorktreeDir "Game\Battle\Battle.vcxproj") -Configuration Release
    }

    $SideAExe = Join-Path $WorktreeDir "Game\build\x64\Release\Battle\Battle.exe"
    if (!(Test-Path $SideAExe)) { throw "Main executable not found: $SideAExe" }
    $SideASha = (& git -C $WorktreeDir rev-parse --short HEAD).Trim()
    $SideALabel = "main ($SideASha)"
    $SideBLabel = "branch ($SideBSha)"
}

# 3. Registry GPU preference management
$regPath = "HKCU:\Software\Microsoft\DirectX\UserGpuPreferences"
if (!(Test-Path $regPath)) {
    New-Item -Path $regPath -Force | Out-Null
}

$targetGpuVal = if ($Gpu -eq 'high') { "GpuPreference=2;" } else { "GpuPreference=1;" }
$distinctExes = @($SideAExe, $SideBExe) | Select-Object -Unique

$originalPrefs = @{}
foreach ($exe in $distinctExes) {
    $resolved = (Resolve-Path $exe).Path
    $props = Get-ItemProperty -Path $regPath -ErrorAction SilentlyContinue
    if ($null -ne $props -and $null -ne $props.$resolved) {
        $originalPrefs[$resolved] = $props.$resolved
    } else {
        $originalPrefs[$resolved] = $null
    }
}

Write-Host "[GPU] Setting UserGpuPreferences to '$targetGpuVal' for:" -ForegroundColor Yellow
foreach ($resolved in $originalPrefs.Keys) {
    Write-Host "  $resolved"
    Set-ItemProperty -Path $regPath -Name $resolved -Value $targetGpuVal
}

function Invoke-Run {
    param(
        [string]$ExePath,
        [string]$Sha,
        [string]$Label,
        [int]$Iteration,
        [bool]$IsCold = $false
    )

    $env:GIT_COMMIT = $Sha
    $output = & $ExePath --bench $Frames --warmup $Warmup --units $Units 2>&1
    $exitCode = $LASTEXITCODE
    Remove-Item Env:GIT_COMMIT -ErrorAction SilentlyContinue

    if ($exitCode -ne 0) {
        Write-Host $output
        throw "Run failed with exit code $exitCode for $Label"
    }

    # Verify GPU adapter
    $gpuLine = $output | Where-Object { $_ -match "^GPU:\s*(.+)$" } | Select-Object -First 1
    if (!$gpuLine) {
        throw "Could not find GPU header line in benchmark output for $Label"
    }
    $actualGpu = $Matches[1].Trim()

    if ($Gpu -eq 'high') {
        if ($actualGpu -match "Radeon.*610M" -or $actualGpu -match "Microsoft Basic") {
            throw "GPU validation failed: requested 'high' (discrete), but got '$actualGpu'!"
        }
    } elseif ($Gpu -eq 'low') {
        if ($actualGpu -match "RTX|GeForce|NVIDIA") {
            throw "GPU validation failed: requested 'low' (integrated), but got '$actualGpu'!"
        }
    }

    # Parse battle-bench-result line
    $resultLine = $output | Where-Object { $_ -match "^battle-bench-result\s+" } | Select-Object -First 1
    if (!$resultLine) {
        throw "Could not find battle-bench-result line in output for $Label"
    }

    $pattern = 'battle-bench-result git=(?<git>\S+) config=(?<config>\S+) units=(?<units>\d+) frames=(?<frames>\d+) warmup=(?<warmup>\d+) cpu_median=(?<cpu_med>[\d\.]+) cpu_p95=(?<cpu_p95>[\d\.]+) cpu_p99=(?<cpu_p99>[\d\.]+) p99_frames=(?<p99_frames>\d+) gpu_median=(?<gpu_med>[\d\.]+) gpu_p95=(?<gpu_p95>[\d\.]+) gpu_p99=(?<gpu_p99>[\d\.]+) gpu_ready=(?<gpu_ready>\d+) gpu_expected=(?<gpu_expected>\d+) gpu_status=(?<gpu_status>\S+)'
    if (!($resultLine -match $pattern)) {
        throw "Failed to parse result line: $resultLine"
    }

    $m = $Matches
    $item = [PSCustomObject]@{
        Label        = $Label
        Iteration    = $Iteration
        IsCold       = $IsCold
        GpuName      = $actualGpu
        Git          = $m['git']
        CpuMedian    = [double]$m['cpu_med']
        CpuP95       = [double]$m['cpu_p95']
        CpuP99       = [double]$m['cpu_p99']
        P99Frames    = [int]$m['p99_frames']
        GpuMedian    = [double]$m['gpu_med']
        GpuP95       = [double]$m['gpu_p95']
        GpuP99       = [double]$m['gpu_p99']
        GpuReady     = [int]$m['gpu_ready']
        GpuExpected  = [int]$m['gpu_expected']
        GpuStatus    = $m['gpu_status']
    }

    $tag = if ($IsCold) { "[COLD]" } else { "[$Iteration]" }
    Write-Host ("  {0,6} {1,-14} | CPU: med={2,5:F2} p95={3,5:F2} p99={4,5:F2} ms (p99={5} frames) | GPU: med={6,5:F2} p95={7,5:F2} ms | GPU-ready={8}/{9} ({10})" -f `
        $tag, $Label, $item.CpuMedian, $item.CpuP95, $item.CpuP99, $item.P99Frames, $item.GpuMedian, $item.GpuP95, $item.GpuReady, $item.GpuExpected, $item.GpuStatus)

    return $item
}

$resultsA = [System.Collections.Generic.List[PSCustomObject]]::new()
$resultsB = [System.Collections.Generic.List[PSCustomObject]]::new()

try {
    # 4. Cold run (discarded)
    Write-Host "`n--- Cold Start Runs (discarded) ---" -ForegroundColor Magenta
    $null = Invoke-Run -ExePath $SideAExe -Sha $SideASha -Label $SideALabel -Iteration 0 -IsCold $true
    $null = Invoke-Run -ExePath $SideBExe -Sha $SideBSha -Label $SideBLabel -Iteration 0 -IsCold $true

    # 5. ABBA Measurement Runs
    Write-Host "`n--- ABBA Measurement Runs ($Runs per side) ---" -ForegroundColor Cyan
    $pairs = [math]::Ceiling($Runs / 2.0)
    $runA = 0
    $runB = 0

    for ($cycle = 0; $cycle -lt $pairs; ++$cycle) {
        # ABBA block: A, B, B, A
        $order = @("A", "B", "B", "A")
        foreach ($side in $order) {
            if ($side -eq "A" -and $runA -lt $Runs) {
                $runA++
                $res = Invoke-Run -ExePath $SideAExe -Sha $SideASha -Label $SideALabel -Iteration $runA -IsCold $false
                $resultsA.Add($res)
            } elseif ($side -eq "B" -and $runB -lt $Runs) {
                $runB++
                $res = Invoke-Run -ExePath $SideBExe -Sha $SideBSha -Label $SideBLabel -Iteration $runB -IsCold $false
                $resultsB.Add($res)
            }
        }
    }

    # 6. Statistical Analysis
    Write-Host "`n=== Statistical Summary ($Gpu GPU) ===" -ForegroundColor Cyan
    $cpuMedA = Get-Stats ($resultsA | ForEach-Object { $_.CpuMedian })
    $cpuMedB = Get-Stats ($resultsB | ForEach-Object { $_.CpuMedian })

    $cpuP95A = Get-Stats ($resultsA | ForEach-Object { $_.CpuP95 })
    $cpuP95B = Get-Stats ($resultsB | ForEach-Object { $_.CpuP95 })

    $cpuP99A = Get-Stats ($resultsA | ForEach-Object { $_.CpuP99 })
    $cpuP99B = Get-Stats ($resultsB | ForEach-Object { $_.CpuP99 })

    $gpuMedA = Get-Stats ($resultsA | ForEach-Object { $_.GpuMedian })
    $gpuMedB = Get-Stats ($resultsB | ForEach-Object { $_.GpuMedian })

    $gpuP95A = Get-Stats ($resultsA | ForEach-Object { $_.GpuP95 })
    $gpuP95B = Get-Stats ($resultsB | ForEach-Object { $_.GpuP95 })

    function Format-Row {
        param([string]$Metric, $statA, $statB)
        $diff = [math]::Round($statB.Median - $statA.Median, 3)
        $pct = if ($statA.Median -ne 0) { [math]::Round(($diff / $statA.Median) * 100.0, 2) } else { 0.0 }
        $sign = if ($diff -gt 0) { "+$diff" } else { "$diff" }
        $signPct = if ($pct -gt 0) { "+$pct%" } else { "$pct%" }
        return ("{0,-16} | A: {1,5:F2} ms (sd={2,4:F2}) | B: {3,5:F2} ms (sd={4,4:F2}) | Delta: {5,6} ms ({6,6})" -f `
            $Metric, $statA.Median, $statA.StDev, $statB.Median, $statB.StDev, $sign, $signPct)
    }

    Write-Host (Format-Row "CPU Median" $cpuMedA $cpuMedB)
    Write-Host (Format-Row "CPU P95" $cpuP95A $cpuP95B)
    Write-Host (Format-Row "CPU P99" $cpuP99A $cpuP99B)
    Write-Host (Format-Row "GPU Median" $gpuMedA $gpuMedB)
    Write-Host (Format-Row "GPU P95" $gpuP95A $gpuP95B)

    # Noise Threshold Evaluation
    $noiseFloor = [math]::Max($cpuMedA.StDev, $cpuMedB.StDev)
    $cpuDelta = [math]::Abs($cpuMedB.Median - $cpuMedA.Median)

    Write-Host "`n--- Verdict ---" -ForegroundColor Yellow
    Write-Host ("Detected GPU: {0}" -f $resultsA[0].GpuName)
    Write-Host ("CPU Median A: {0:F2} ms (stderr={1:F3} ms) | B: {2:F2} ms (stderr={3:F3} ms)" -f $cpuMedA.Median, $cpuMedA.StdErr, $cpuMedB.Median, $cpuMedB.StdErr)
    Write-Host ("Observed |Delta|: {0:F3} ms | Baseline StDev (Noise Floor): {1:F3} ms" -f $cpuDelta, $noiseFloor)

    if ($Mode -eq 'AA') {
        Write-Host "A/A Mode: Inherent measurement noise floor is $noiseFloor ms ($([math]::Round(($noiseFloor / $cpuMedA.Median) * 100.0, 2))%)." -ForegroundColor Green
    } else {
        if ($cpuDelta -le $noiseFloor) {
            Write-Host "VERDICT: PASS (No regression). Difference ($cpuDelta ms) is within intrinsic noise floor ($noiseFloor ms)." -ForegroundColor Green
        } else {
            Write-Host "VERDICT: NOTICE. Difference ($cpuDelta ms) exceeds standard deviation ($noiseFloor ms)." -ForegroundColor Yellow
        }
    }

} finally {
    # 7. Restore original registry values
    Write-Host "`n[Cleanup] Restoring original UserGpuPreferences..." -ForegroundColor Gray
    foreach ($resolved in $originalPrefs.Keys) {
        $orig = $originalPrefs[$resolved]
        if ($null -ne $orig) {
            Set-ItemProperty -Path $regPath -Name $resolved -Value $orig
        } else {
            Remove-ItemProperty -Path $regPath -Name $resolved -ErrorAction SilentlyContinue
        }
    }

    if (Test-Path $WorktreeDir) {
        Write-Host "[Cleanup] Removing worktree..." -ForegroundColor Gray
        & git -C $RepoRoot worktree remove $WorktreeDir --force 2>$null
    }
}
