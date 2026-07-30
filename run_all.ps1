# Auto SW Academy — 전체 빌드 + 전체 테스트 + 통합 데모 (Windows PowerShell)
# Usage: .\run_all.ps1

Write-Host "==> 빌드" -ForegroundColor Cyan
& "C:\msys64\usr\bin\bash.exe" -l -c "cd '$PWD' && make -j4"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`n==> 전체 테스트" -ForegroundColor Cyan
& "C:\msys64\usr\bin\bash.exe" -l -c "cd '$PWD' && make check"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`n==> 통합 데모 (capstone)" -ForegroundColor Cyan
$env:MSYSTEM = "MINGW64"
$env:CHERE_INVOKING = "yes"
& "C:\msys64\usr\bin\bash.exe" -l -c "cd '$PWD' && mkdir -p logs && ./build/capstone_demo --trace logs/capstone_trace.log 2>&1" | Tee-Object -FilePath "capstone_output.txt"

Write-Host "`n==> 트레이스 DBC 디코드 (미니 CANoe 워크플로)" -ForegroundColor Cyan
& "C:\msys64\usr\bin\bash.exe" -l -c "cd '$PWD' && make trace-check"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`n모든 단계 성공적으로 완료!" -ForegroundColor Green
