# scripts/apply_patches.ps1 — 单库补丁重放（升级解冲突后使用）
# 用法:  .\apply_patches.ps1 -Lib harfbuzz
param([Parameter(Mandatory)][string]$Lib)
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
$Dir  = Join-Path $Root "third_party/$Lib"
$Pats = Get-ChildItem (Join-Path $Root "patches/$Lib") -Filter *.patch -ErrorAction SilentlyContinue
if (-not $Pats) { Write-Host "patches/$Lib 无补丁，跳过"; exit 0 }
foreach ($p in $Pats) {
    Write-Host ">> apply $($p.Name)"
    git -C $Dir apply --3way $p.FullName
    if ($LASTEXITCODE -ne 0) { throw "补丁应用失败: $($p.Name)（人工解决冲突后提交，并更新该 .patch）" }
}