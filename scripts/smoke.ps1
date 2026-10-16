# End-to-end smoke test: two Python HTTP backends behind the load balancer, queried with curl.
# Usage: powershell -File scripts\smoke.ps1 [-Preset dev]
param([string]$Preset = "dev")

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "build\$Preset\src\opencpp_lb.exe"
if (-not (Test-Path $exe)) { throw "Build first: $exe not found" }

$procs = @()
try {
    foreach ($port in 9001, 9002) {
        $procs += Start-Process python -ArgumentList "-m", "http.server", $port, "--bind", "127.0.0.1" `
            -WorkingDirectory $root -WindowStyle Hidden -PassThru
    }
    $procs += Start-Process $exe -ArgumentList "-l", "127.0.0.1:8080", "-b", "127.0.0.1:9001", "-b", "127.0.0.1:9002" `
        -NoNewWindow -PassThru
    Start-Sleep -Seconds 1

    $failures = 0
    foreach ($i in 1..4) {
        $status = curl.exe -s -o NUL -w "%{http_code}" --max-time 3 http://127.0.0.1:8080/
        Write-Host "[SMOKE] request $i -> HTTP $status"
        if ($status -ne "200") { $failures++ }
    }
    if ($failures -gt 0) { throw "$failures request(s) failed" }
    Write-Host "[SMOKE] PASS"
} finally {
    $procs | ForEach-Object { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue }
}
