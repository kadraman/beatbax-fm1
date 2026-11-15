# SPDX-License-Identifier: GPL-3.0-or-later
# Windows helper: run build.sh via Git Bash or WSL when JIELI_TOOLCHAIN is set.
$ErrorActionPreference = "Stop"
if (-not $env:JIELI_TOOLCHAIN) {
    Write-Error "Set JIELI_TOOLCHAIN to the JieLi pi32 toolchain root (see firmware/README.md)."
}
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $here

$bash = Get-Command bash -ErrorAction SilentlyContinue
if ($bash) {
    & bash ./build.sh
    exit $LASTEXITCODE
}

$wsl = Get-Command wsl -ErrorAction SilentlyContinue
if ($wsl) {
    $unix = (wsl wslpath -a $here).Trim()
    wsl bash -lc "cd '$unix' && export JIELI_TOOLCHAIN=`$(wslpath -a '$env:JIELI_TOOLCHAIN') && sh ./build.sh"
    exit $LASTEXITCODE
}

Write-Error "Need Git Bash (bash) or WSL to run build.sh on Windows."
