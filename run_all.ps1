# Auto SW Academy — Windows 전체 파이프라인 (PowerShell)
# MSYS2 경로 자동 탐색 + 모든 단계 MINGW64 고정
$msysPaths = @(
    "C:\msys64",
    "C:\msys32",
    "$env:ProgramFiles\msys64",
    "$env:LOCALAPPDATA\msys64",
    "D:\msys64",
    "D:\msys32"
)
$msysRoot = $msysPaths | Where-Object { Test-Path "$_\usr\bin\bash.exe" } | Select-Object -First 1
if (-not $msysRoot) { Write-Error "MSYS2를 찾을 수 없습니다. 설치 경로를 확인하세요."; exit 1 }

$env:MSYSTEM = "MINGW64"
Set-Location $PSScriptRoot
$env:CHERE_INVOKING = "yes"
$bashExe = "$msysRoot\usr\bin\bash.exe"

Write-Host "MSYS2 found at: $msysRoot"

Write-Host "=== 빌드: make -j4 ==="
& $bashExe -l -c "make -j4"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "=== 테스트: make check ==="
& $bashExe -l -c "make check"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "=== 통합 데모: capstone_demo ==="
& $bashExe -l -c "mkdir -p logs && ./build/capstone_demo --trace logs/capstone_trace.log"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "=== DBC 트레이스 검증: make trace-check ==="
& $bashExe -l -c "make trace-check"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "전체 파이프라인 완료"
