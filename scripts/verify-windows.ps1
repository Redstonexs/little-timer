param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$CoreTests
)
$ErrorActionPreference = 'Stop'
if ($CoreTests) {
    & $CoreTests
    if ($LASTEXITCODE -ne 0) { throw 'Windows core tests failed.' }
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$qaProcess = Start-Process -FilePath $Executable -ArgumentList @('--self-test', ('"' + $OutputDirectory + '"')) -PassThru
if (-not $qaProcess.WaitForExit(45000)) {
    $qaProcess.Kill()
    throw 'Native UI verification exceeded 45 seconds.'
}
$qaProcess.Refresh()
Get-Content -LiteralPath (Join-Path $OutputDirectory 'windows-report.txt') -Encoding UTF8
if ($qaProcess.ExitCode -ne 0) { throw ('Native UI verification failed: ' + $qaProcess.ExitCode) }
