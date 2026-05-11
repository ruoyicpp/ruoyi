#requires -Version 5.1
<#
.SYNOPSIS
  下载 SQLite3 Multiple Ciphers amalgamation 到 src/third_party/sqlite3mc/

.DESCRIPTION
  ruoyi-cpp 启用页级 SQLite 加密时需要 sqlite3mc 源码（12MB），不入 git 仓库。
  本脚本从 GitHub Releases 下载官方包并校验 SHA256，解压到正确路径。

  执行后会得到：
    src/third_party/sqlite3mc/
      ├── sqlite3.h                    (已在 git 仓库中：shim 头)
      ├── sqlite3mc_amalgamation.c     (本脚本下载，~12 MB)
      └── sqlite3mc_amalgamation.h     (本脚本下载，~650 KB)

  之后重新 cmake configure + build 即可启用页级加密。

.EXAMPLE
  .\scripts\download_sqlite3mc.ps1
#>

[CmdletBinding()]
param(
    [string] $Version = "2.1.0",
    [string] $SqliteVer = "3.49.1",
    [string] $ExpectedSha256 = "375aa1837c4f2067159e86c7403ec88f841b089e7fc2ecb05b1e6a1701b2eb56"
)

$ErrorActionPreference = "Stop"

# 项目根 = 此脚本所在目录的父目录
$root = Split-Path -Parent $PSScriptRoot
$mcDir = Join-Path $root "src\third_party\sqlite3mc"

if (!(Test-Path $mcDir)) {
    New-Item -ItemType Directory -Path $mcDir -Force | Out-Null
}

$amaC = Join-Path $mcDir "sqlite3mc_amalgamation.c"
$amaH = Join-Path $mcDir "sqlite3mc_amalgamation.h"
if ((Test-Path $amaC) -and (Test-Path $amaH)) {
    Write-Host "[OK] sqlite3mc amalgamation 已存在，跳过下载" -ForegroundColor Green
    Write-Host "     $amaC ($([math]::Round((Get-Item $amaC).Length/1MB,1)) MB)"
    Write-Host "     $amaH ($([math]::Round((Get-Item $amaH).Length/1MB,2)) MB)"
    exit 0
}

$zipName = "sqlite3mc-$Version-sqlite-$SqliteVer-amalgamation.zip"
$url = "https://github.com/utelle/SQLite3MultipleCiphers/releases/download/v$Version/$zipName"
$zip = Join-Path $mcDir $zipName

Write-Host "[1/4] 下载 $zipName (~6 MB)" -ForegroundColor Cyan
Write-Host "      $url"
try {
    Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing -TimeoutSec 120
} catch {
    Write-Host "[ERR] 下载失败：$($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
$sizeMB = [math]::Round((Get-Item $zip).Length / 1MB, 2)
Write-Host "      $sizeMB MB"

Write-Host "[2/4] SHA256 校验" -ForegroundColor Cyan
$actual = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
Write-Host "      期望: $ExpectedSha256"
Write-Host "      实际: $actual"
if ($actual -ne $ExpectedSha256.ToLower()) {
    Write-Host "[ERR] SHA256 不匹配，下载文件已损坏或被篡改" -ForegroundColor Red
    Remove-Item $zip -Force
    exit 2
}

Write-Host "[3/4] 解压 + 移动" -ForegroundColor Cyan
$extracted = Join-Path $mcDir "_extracted"
if (Test-Path $extracted) { Remove-Item $extracted -Recurse -Force }
Expand-Archive -Path $zip -DestinationPath $extracted -Force

Move-Item (Join-Path $extracted "sqlite3mc_amalgamation.c") $amaC -Force
Move-Item (Join-Path $extracted "sqlite3mc_amalgamation.h") $amaH -Force

Write-Host "[4/4] 清理" -ForegroundColor Cyan
Remove-Item $extracted -Recurse -Force
Remove-Item $zip -Force

Write-Host ""
Write-Host "[完成] sqlite3mc $Version (SQLite $SqliteVer) 已就绪" -ForegroundColor Green
Write-Host "       $amaC ($([math]::Round((Get-Item $amaC).Length/1MB,1)) MB)"
Write-Host "       $amaH ($([math]::Round((Get-Item $amaH).Length/1MB,2)) MB)"
Write-Host ""
Write-Host "下一步：" -ForegroundColor Yellow
Write-Host "       cmake -B build -G Ninja"
Write-Host "       cmake --build build --parallel"
