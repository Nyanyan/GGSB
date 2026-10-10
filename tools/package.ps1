$ErrorActionPreference = 'Stop'
$workspacePath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$packagePath = Join-Path $workspacePath 'build\GGSB-windows-x64'
New-Item -ItemType Directory -Force -Path $packagePath | Out-Null
Copy-Item -LiteralPath (Join-Path $workspacePath 'App\GGSB.exe') -Destination $packagePath
& (Join-Path $PSScriptRoot 'sanitize-binary.ps1') -Path (Join-Path $packagePath 'GGSB.exe')
if (-not (Test-Path -LiteralPath (Join-Path $workspacePath 'App\THIRD_PARTY_LICENSES.txt'))) {
    $licenseProcess = Start-Process -FilePath (Join-Path $workspacePath 'App\GGSB.exe') -WorkingDirectory (Join-Path $workspacePath 'App') -WindowStyle Hidden -ArgumentList '--mute --quit-after 0' -PassThru
    $licenseProcess.WaitForExit()
}
Copy-Item -LiteralPath (Join-Path $workspacePath 'App\THIRD_PARTY_LICENSES.txt') -Destination $packagePath
Copy-Item -LiteralPath (Join-Path $workspacePath 'App\dll') -Destination $packagePath -Recurse -Force
Copy-Item -LiteralPath (Join-Path $workspacePath 'App\engine') -Destination $packagePath -Recurse -Force
Copy-Item -LiteralPath (Join-Path $workspacePath 'assets\languages') -Destination $packagePath -Recurse -Force
Copy-Item -LiteralPath (Join-Path $workspacePath 'docs') -Destination $packagePath -Recurse -Force
Copy-Item -LiteralPath (Join-Path $workspacePath 'README.md') -Destination $packagePath
$packageItems = @('GGSB.exe','THIRD_PARTY_LICENSES.txt','dll','engine','languages','docs','README.md') | ForEach-Object { Join-Path $packagePath $_ }
Compress-Archive -LiteralPath $packageItems -DestinationPath (Join-Path $workspacePath 'build\GGSB-windows-x64.zip') -Force
Write-Output (Join-Path $workspacePath 'build\GGSB-windows-x64.zip')
