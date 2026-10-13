param(
    [string]$OutputPath = "",
    [switch]$FullValidation
)
$ErrorActionPreference = 'Stop'
$projectPath = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$binaryPath = Join-Path $projectPath 'build-msvc/Release/navier_storage_benchmark.exe'
$cliPath = Join-Path $projectPath 'build-msvc/Release/navier.exe'
if (!(Test-Path -LiteralPath $binaryPath)) { throw 'Compile Release primeiro.' }
if (!$OutputPath) {
    $runPath = Join-Path $projectPath ('runs/benchmark-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 6))
    New-Item -ItemType Directory -Path $runPath | Out-Null
    $OutputPath = Join-Path $runPath 'benchmark10g.data'
}
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
if (Test-Path -LiteralPath $OutputPath) { throw 'Arquivo existente: escolha outro caminho.' }
if (!(Test-Path -LiteralPath (Split-Path $OutputPath -Parent))) { throw 'Pasta de saída não existe.' }
& $binaryPath $OutputPath 2560 1024 512 2 | Tee-Object -FilePath ($OutputPath + '.csv')
if ($LASTEXITCODE -ne 0) { throw 'Geração falhou.' }
& $cliPath info $OutputPath
if ($LASTEXITCODE -ne 0) { throw 'Inspeção falhou.' }
if ($FullValidation) {
    & $cliPath validate $OutputPath full
    if ($LASTEXITCODE -ne 0) { throw 'Validação falhou.' }
}
Write-Output "Dataset: $OutputPath"
