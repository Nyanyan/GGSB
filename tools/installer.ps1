param(
    [string]$Version,
    [string]$CompilerPath,
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$workspacePath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))

if (-not $CompilerPath) {
    $compilerCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    $compilerCandidates = @(
        $(if ($compilerCommand) { $compilerCommand.Source }),
        (Join-Path $workspacePath 'build\tooling\inno-setup\ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 7\ISCC.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 7\ISCC.exe'),
        (Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Programs\Inno Setup 7\ISCC.exe')
    )
    $CompilerPath = $compilerCandidates | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -First 1
}
if (-not $CompilerPath -or -not (Test-Path -LiteralPath $CompilerPath -PathType Leaf)) {
    throw 'Install Inno Setup 7, add ISCC.exe to PATH, or specify -CompilerPath.'
}
$CompilerPath = (Resolve-Path -LiteralPath $CompilerPath).Path
$compilerVersion = & $CompilerPath '--version'
if ($LASTEXITCODE -ne 0 -or $compilerVersion -notmatch '^\d+\.\d+(?:\.\d+)*$' -or ([version]$compilerVersion).Major -lt 7) {
    throw 'Inno Setup 7 or newer is required.'
}

if (-not $Version) {
    $sourceDate = & git -C $workspacePath log -1 --format=%cs -- Main.cpp stdafx.cpp stdafx.h GGSB.vcxproj src assets
    if ($LASTEXITCODE -ne 0 -or $sourceDate -notmatch '^\d{4}-\d{2}-\d{2}$') { throw 'Cannot determine the source date. Specify -Version.' }
    $Version = ($sourceDate -split '-' | ForEach-Object { [int]$_ }) -join '.'
}
if ($Version -notmatch '^\d{1,5}(\.\d{1,5}){1,3}$' -or @($Version -split '\.' | Where-Object { [int]$_ -gt 65535 }).Count) {
    throw 'Version must contain two to four numeric components, each from 0 to 65535.'
}
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Configuration Release }
& (Join-Path $PSScriptRoot 'package.ps1')

# Extract into a fresh directory so files left by prior packaging cannot leak in.
$payloadName = 'installer-payload-' + [guid]::NewGuid().ToString('N')
$payloadRelativePath = 'build\' + $payloadName
$payloadPath = Join-Path $workspacePath $payloadRelativePath
New-Item -ItemType Directory -Path $payloadPath | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $workspacePath 'build\GGSB-windows-x64.zip'))
try {
    foreach ($entry in $archive.Entries) {
        $name = $entry.FullName.Replace('\', '/')
        if ($name -notmatch '^(GGSB\.exe|THIRD_PARTY_LICENSES\.txt|README\.md|(?:dll|engine|languages|docs)/.*)$' -or
            $name -match '(?i)(?:^|/)(?:settings\.ini|accounts\.dat(?:\.[^/]*)?|logs|records|Screenshot)(?:/|$)' -or
            $name -match '(?i)\.(?:pdb|obj|log|ggf)$') {
            throw 'The distribution ZIP contains a non-distribution file.'
        }
        $destinationPath = [IO.Path]::GetFullPath((Join-Path $payloadPath $name))
        if (-not $destinationPath.StartsWith($payloadPath + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe ZIP entry path.' }
        if ($entry.Name) {
            New-Item -ItemType Directory -Force -Path (Split-Path $destinationPath) | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $destinationPath, $false)
        }
    }
} finally { $archive.Dispose() }

& $CompilerPath '/Qp' ("/DAppVersion=$Version") ("/DPayloadDir=$payloadRelativePath") (Join-Path $PSScriptRoot 'installer.iss')
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
$installerPath = Join-Path $workspacePath ("build\GGSB-$Version-windows-x64-setup.exe")
if (-not (Test-Path -LiteralPath $installerPath -PathType Leaf)) { throw 'The compiler did not produce the installer.' }
Write-Output $installerPath
