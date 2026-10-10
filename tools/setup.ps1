param([string]$TemplateZip)
$ErrorActionPreference = 'Stop'
$workspacePath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $TemplateZip) {
    $documentsPath = [Environment]::GetFolderPath('MyDocuments')
    $candidates = @(
        (Join-Path $documentsPath 'Visual Studio 18\Templates\ProjectTemplates\OpenSiv3D_0.6.16.zip'),
        (Join-Path $documentsPath 'Visual Studio 2022\Templates\ProjectTemplates\OpenSiv3D_0.6.16.zip')
    )
    $TemplateZip = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $TemplateZip) { throw 'Siv3D 0.6.16 のプロジェクトテンプレートが見つかりません。-TemplateZip で ZIP のパスを指定してください。' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($TemplateZip)
try {
    foreach ($entry in $archive.Entries) {
        if ($entry.FullName.StartsWith('App/engine/') -or $entry.FullName.StartsWith('App/dll/') -or $entry.FullName -eq 'App/Resource.rc' -or $entry.FullName -eq 'App/icon.ico') {
            $destinationPath = [IO.Path]::GetFullPath((Join-Path $workspacePath $entry.FullName))
            if (-not $destinationPath.StartsWith($workspacePath + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'ZIP 内に不正なパスが含まれています。' }
            if ($entry.Name) {
                New-Item -ItemType Directory -Force -Path (Split-Path $destinationPath) | Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $destinationPath, $true)
            }
        }
    }
} finally { $archive.Dispose() }
if (Test-Path -LiteralPath (Join-Path $workspacePath 'assets\icon.ico')) { Copy-Item -LiteralPath (Join-Path $workspacePath 'assets\icon.ico') -Destination (Join-Path $workspacePath 'App\icon.ico') -Force }
Copy-Item -LiteralPath (Join-Path $workspacePath 'assets\languages') -Destination (Join-Path $workspacePath 'App') -Recurse -Force
Write-Output 'Siv3D のアプリ用リソースを App/ に配置しました。'
