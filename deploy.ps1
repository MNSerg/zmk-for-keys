# Flash ZMK .uf2 to nice!nano bootloader drive (default F:)
#
# Usage:
#   .\deploy.ps1 left
#   .\deploy.ps1 right
#   .\deploy.ps1 right_bare
#   .\deploy.ps1 settings_reset
#   .\deploy.ps1 -Uf2 .\firmware\corne_v3_left-nice_nano.uf2
#   .\deploy.ps1 left -Drive E:
#
# 1) Double-tap RESET on the nice!nano → Windows mounts NICENANO (often F:)
# 2) Run this script → board reboots after copy

[CmdletBinding(DefaultParameterSetName = "ByTarget")]
param(
    [Parameter(Position = 0, ParameterSetName = "ByTarget")]
    [ValidateSet("left", "right", "right_bare", "left_bare", "settings_reset")]
    [string]$Target = "left",

    [Parameter(Mandatory = $true, ParameterSetName = "ByFile")]
    [string]$Uf2,

    [string]$Drive = "F:",

    [string]$FirmwareDir = ""
)

$ErrorActionPreference = "Stop"

function Resolve-DriveRoot([string]$d) {
    if ($d -notmatch '^[A-Za-z]:\\?$') {
        throw "Drive must look like F: or F:\ (got '$d')"
    }
    return ($d.TrimEnd('\') + '\')
}

function Find-Uf2([string]$target, [string]$dir) {
    $roots = @()
    if ($dir) { $roots += $dir }
    $roots += @(
        (Join-Path $PSScriptRoot "firmware")
        (Join-Path $PSScriptRoot ".")
        (Join-Path $env:USERPROFILE "Downloads")
        (Join-Path $env:USERPROFILE "Downloads\firmware")
    ) | Select-Object -Unique

    $filter = switch ($target) {
        "left" { { $_.Name -like "corne_v3_left*.uf2" -and $_.Name -notlike "corne_v3_left_bare*" } }
        "right" { { $_.Name -like "corne_v3_right*.uf2" -and $_.Name -notlike "corne_v3_right_bare*" } }
        "left_bare" { { $_.Name -like "corne_v3_left_bare*.uf2" } }
        "right_bare" { { $_.Name -like "corne_v3_right_bare*.uf2" } }
        "settings_reset" { { $_.Name -like "settings_reset*.uf2" } }
    }

    foreach ($root in $roots) {
        if (-not (Test-Path -LiteralPath $root)) { continue }
        $hit = Get-ChildItem -LiteralPath $root -Filter "*.uf2" -File -ErrorAction SilentlyContinue |
            Where-Object $filter |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    return $null
}

$driveRoot = Resolve-DriveRoot $Drive

if (-not (Test-Path -LiteralPath $driveRoot)) {
    Write-Host "Drive $Drive is not mounted." -ForegroundColor Yellow
    Write-Host "Double-tap RESET on the nice!nano, wait for NICENANO, then re-run."
    exit 1
}

if ($PSCmdlet.ParameterSetName -eq "ByFile") {
    $uf2Path = $Uf2
    if (-not [System.IO.Path]::IsPathRooted($uf2Path)) {
        $uf2Path = Join-Path (Get-Location) $uf2Path
    }
    if (-not (Test-Path -LiteralPath $uf2Path)) {
        throw "UF2 not found: $uf2Path"
    }
}
else {
    $uf2Path = Find-Uf2 $Target $FirmwareDir
    if (-not $uf2Path) {
        throw @"
No UF2 found for target '$Target'.
Unpack firmware.zip into .\firmware\ (or Downloads\), or pass -Uf2 path.
"@
    }
}

$destName = Split-Path $uf2Path -Leaf
$dest = Join-Path $driveRoot $destName

Write-Host "Source : $uf2Path"
Write-Host "Target : $dest"
Copy-Item -LiteralPath $uf2Path -Destination $dest -Force
Write-Host "Copied. Board should reboot; $Drive will disappear." -ForegroundColor Green
