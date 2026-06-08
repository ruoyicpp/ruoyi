#requires -Version 5.1
<#
.SYNOPSIS
  Downloads SQLite3 Multiple Ciphers amalgamation to src/third_party/sqlite3mc/

.DESCRIPTION
  Downloads the official amalgamation zip package from GitHub Releases,
  verifies its SHA256 checksum, and extracts it to the correct path.
#>

[CmdletBinding()]
param(
    [string] $Version = "2.1.0",
    [string] $SqliteVer = "3.49.1",
    [string] $ExpectedSha256 = "375aa1837c4f2067159e86c7403ec88f841b089e7fc2ecb05b1e6a1701b2eb56"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$mcDir = Join-Path $root "src\third_party\sqlite3mc"

if (!(Test-Path $mcDir)) {
    New-Item -ItemType Directory -Path $mcDir -Force | Out-Null
}

$amaC = Join-Path $mcDir "sqlite3mc_amalgamation.c"
$amaH = Join-Path $mcDir "sqlite3mc_amalgamation.h"
if ((Test-Path $amaC) -and (Test-Path $amaH)) {
    $amaCSize = [math]::Round((Get-Item $amaC).Length/1MB,1)
    $amaHSize = [math]::Round((Get-Item $amaH).Length/1MB,2)
    Write-Host "[OK] sqlite3mc amalgamation already exists, skipping download" -ForegroundColor Green
    Write-Host "     $amaC ($amaCSize MB)"
    Write-Host "     $amaH ($amaHSize MB)"
    exit 0
}

$zipName = "sqlite3mc-$Version-sqlite-$SqliteVer-amalgamation.zip"
$url = "https://github.com/utelle/SQLite3MultipleCiphers/releases/download/v$Version/$zipName"
$zip = Join-Path $mcDir $zipName

Write-Host "[1/4] Downloading $zipName (~6 MB)" -ForegroundColor Cyan
Write-Host "      $url"
try {
    Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing -TimeoutSec 120
} catch {
    $errMsg = $_.Exception.Message
    Write-Host "[ERR] Download failed: $errMsg" -ForegroundColor Red
    exit 1
}
$sizeMB = [math]::Round((Get-Item $zip).Length / 1MB, 2)
Write-Host "      $sizeMB MB"

Write-Host "[2/4] Verifying SHA256 checksum" -ForegroundColor Cyan
$actual = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
Write-Host "      Expected: $ExpectedSha256"
Write-Host "      Actual:   $actual"
if ($actual -ne $ExpectedSha256.ToLower()) {
    Write-Host "[ERR] SHA256 mismatch! The downloaded file is corrupted." -ForegroundColor Red
    Remove-Item $zip -Force
    exit 2
}

Write-Host "[3/4] Extracting and moving files" -ForegroundColor Cyan
$extracted = Join-Path $mcDir "_extracted"
if (Test-Path $extracted) { Remove-Item $extracted -Recurse -Force }
Expand-Archive -Path $zip -DestinationPath $extracted -Force

Move-Item (Join-Path $extracted "sqlite3mc_amalgamation.c") $amaC -Force
Move-Item (Join-Path $extracted "sqlite3mc_amalgamation.h") $amaH -Force

Write-Host "[4/4] Cleaning up" -ForegroundColor Cyan
Remove-Item $extracted -Recurse -Force
Remove-Item $zip -Force

Write-Host ""
$finalCSize = [math]::Round((Get-Item $amaC).Length/1MB,1)
$finalHSize = [math]::Round((Get-Item $amaH).Length/1MB,2)
Write-Host "[SUCCESS] sqlite3mc $Version (SQLite $SqliteVer) is ready" -ForegroundColor Green
Write-Host "          $amaC ($finalCSize MB)"
Write-Host "          $amaH ($finalHSize MB)"
Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Yellow
Write-Host "       cmake -B build -G Ninja"
Write-Host "       cmake --build build --parallel"

