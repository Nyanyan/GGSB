param([ValidateSet('Release','Debug')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$workspacePath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not (Test-Path -LiteralPath (Join-Path $workspacePath 'App\Resource.rc'))) { & (Join-Path $PSScriptRoot 'setup.ps1') }
if (Test-Path -LiteralPath (Join-Path $workspacePath 'assets\icon.ico')) { Copy-Item -LiteralPath (Join-Path $workspacePath 'assets\icon.ico') -Destination (Join-Path $workspacePath 'App\icon.ico') -Force }
if (-not $env:SIV3D_0_6_16) { throw '環境変数 SIV3D_0_6_16 を Siv3D 0.6.16 SDK の場所に設定してください。' }
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswherePath)) { throw 'Visual Studio Installer の vswhere.exe が見つかりません。' }
$installationPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installationPath) { throw 'Visual Studio の C++ 開発環境が見つかりません。' }
$msbuildPath = Join-Path $installationPath 'MSBuild\Current\Bin\MSBuild.exe'
$toolsPath = Join-Path $installationPath 'VC\Tools\MSVC'
$toolVersion = Get-ChildItem -LiteralPath $toolsPath -Directory | Where-Object { $_.Name -like '14.4*' } | Sort-Object Name -Descending | Select-Object -First 1
if (-not $toolVersion) { throw 'v143 ツールセットが必要です。Visual Studio Installer で MSVC v143 を追加してください。' }
$originalPath = $env:PATH
try {
    $env:PATH = (Join-Path $toolVersion.FullName 'bin\Hostx64\x64') + ';' + $originalPath
    & $msbuildPath (Join-Path $workspacePath 'GGSB.sln') /m "/p:Configuration=$Configuration" /p:Platform=x64 "/p:VCToolsVersion=$($toolVersion.Name)" /p:PreferredToolArchitecture=x64 /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { throw 'ビルドに失敗しました。' }
} finally { $env:PATH = $originalPath }
Write-Output (Join-Path $workspacePath 'App\GGSB.exe')
