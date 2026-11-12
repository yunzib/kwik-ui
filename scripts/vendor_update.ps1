# scripts/vendor_update.ps1 — 三方快照库统一升级脚本（维护者工具）
# 用法:  .\vendor_update.ps1 -Lib harfbuzz -Ref v9.1.0
# 流程:  下载上游 archive → 按 $Rules 提取子集覆盖 third_party/<Lib>
#        → git apply --3way 重放补丁 → 提示更新 README Version 行
# 纪律:  $Rules 与 third_party/README.md 的 Files extracted 一一对应，
#        两处必须同步修改；本脚本是"同口径对比"的执行载体。

param(
    [Parameter(Mandatory)][string]$Lib,   # 库名（$Rules 的键）
    [Parameter(Mandatory)][string]$Ref    # 上游 tag 或 SHA
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent   # 仓库根
$Dst  = Join-Path $Root "third_party/$Lib"

# ── 提取规则表（与 third_party/README.md 一一对应）──────────────
# Keep = 从上游 archive 中保留的相对路径；未列出的全部丢弃
$Rules = @{
    'quickjs'  = @{ Url='https://github.com/quickjs-ng/quickjs/archive/refs/tags/{REF}.tar.gz'; Keep=@('*') }
    'freetype' = @{ Url='https://github.com/freetype/freetype/archive/refs/tags/{REF}.tar.gz'; Keep=@('*') }
    'harfbuzz' = @{ Url='https://github.com/harfbuzz/harfbuzz/archive/refs/tags/{REF}.tar.gz'; Keep=@('*') }
    'stb'      = @{ Url='https://github.com/nothings/stb/archive/{REF}.tar.gz'; Keep=@('*') }
    'nanosvg'  = @{ Url='https://github.com/memononen/nanosvg/archive/refs/tags/{REF}.tar.gz'; Keep=@('*') }
    'simdjson' = @{ Url='https://github.com/simdjson/simdjson/archive/refs/tags/{REF}.tar.gz'
                    Keep=@('LICENSE','singleheader'); PostMove=@{'singleheader/*'='.'} }
    'FFmpeg'   = @{ Url='https://github.com/FFmpeg/FFmpeg/archive/refs/tags/{REF}.tar.gz'; Keep=@('*') }
    'fastgltf' = @{ Url='https://github.com/spnda/fastgltf/archive/refs/tags/{REF}.tar.gz'; Keep=@('*') }
    'msdfgen'  = @{ Url='https://github.com/Chlumsky/msdfgen/archive/refs/tags/{REF}.tar.gz'; Keep=@('*') }
}
if (-not $Rules.ContainsKey($Lib)) { throw "未知库名 '$Lib'，可用: $($Rules.Keys -join ', ')" }
$R = $Rules[$Lib]

# ── ① 下载上游 ─────────────────────────────────────────────────
$Tmp = Join-Path $env:TEMP "vendor_update_$Lib"
if (Test-Path $Tmp) { Remove-Item $Tmp -Recurse -Force }
New-Item -ItemType Directory $Tmp | Out-Null
$Url = $R.Url -replace '\{REF\}', $Ref
$Archive = Join-Path $Tmp ("{0}.download" -f $Lib)
Write-Host ">> 下载 $Url"
Invoke-WebRequest -Uri $Url -OutFile $Archive

# ── ② 解压（tar.gz 走 tar.exe，Win10+ 自带）────────────────────
if ($R.ContainsKey('Mode') -and $R['Mode'] -eq 'Raw') {
    # Raw 模式：直接抓取文件（stb 按文件 pin）
    foreach ($f in $R.RawFiles) {
        $u = $Url -replace [regex]::Escape((Split-Path $Url -Leaf)), $f
        Invoke-WebRequest -Uri $u -OutFile (Join-Path $Tmp $f)
    }
    $SrcDir = $Tmp
} else {
    tar -xzf $Archive -C $Tmp
    $SrcDir = (Get-ChildItem $Tmp -Directory | Select-Object -First 1).FullName  # archive 单顶层目录
}

# ── ③ 重建目标子集（保留 README 登记之外的本地无物——全新覆盖）──
if (Test-Path $Dst) { Remove-Item $Dst -Recurse -Force }
New-Item -ItemType Directory $Dst | Out-Null
foreach ($k in $R.Keep) {
    $src = Join-Path $SrcDir $k
    if (Test-Path $src) {
        Copy-Item $src -Destination (Join-Path $Dst $k) -Recurse -Force
    } else { Write-Warning "上游缺失 $k —— 提取规则需与上游结构核对（README 同步更新）" }
}
# PostCopy：许可等需从被排除目录捞出拷到库根的文件
foreach ($p in $R.PostCopy) {
    Copy-Item (Join-Path $SrcDir $p) -Destination $Dst -Force
}
# PostMove：目录内文件上提（simdjson singleheader → 库根）
if ($R.PostMove) { foreach ($k in $R.PostMove.Keys) {
    Move-Item (Join-Path $Dst $k) -Destination $Dst -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $Dst $k.TrimEnd('/*')) -ErrorAction SilentlyContinue
} }

# ── ④ 重放补丁（冲突则人工合并后更新 patches/<Lib>/*.patch）────
$PatchDir = Join-Path $Root "patches/$Lib"
if (Test-Path $PatchDir) {
    git -C $Dst apply --3way (Get-ChildItem $PatchDir -Filter *.patch | % FullName)
}

# ── ⑤ FFmpeg 特例：清构建缓存，强制下次重新 configure ──────────
if ($Lib -eq 'FFmpeg' -and (Test-Path (Join-Path $Root 'build/ffmpeg-build'))) {
    Remove-Item (Join-Path $Root 'build/ffmpeg-build') -Recurse -Force
    Write-Host '>> 已清除 build/ffmpeg-build，下次构建将重新 configure'
}

Write-Host @"

>> 完成。请人工完成:
   1. third_party/README.md 中 $Lib 的 Version 行更新为: $Ref
   2. docs/CHANGELOG.md 顶部加升级条目
   3. 跑验证门禁: cmake --build build -j && ctest --test-dir build && smoke 38/38
"@ -ForegroundColor Yellow