$ErrorActionPreference = 'Stop'
$workspacePath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$resultPath = Join-Path $workspacePath 'build\gui-smoke.txt'
$runtimePath = Join-Path $workspacePath ('build\gui-smoke-runtime-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runtimePath | Out-Null
foreach ($name in @('GGSB.exe','THIRD_PARTY_LICENSES.txt','dll','engine','languages')) {
    Copy-Item -LiteralPath (Join-Path $workspacePath ('App\' + $name)) -Destination $runtimePath -Recurse -Force
}
$executablePath = Join-Path $runtimePath 'GGSB.exe'
$settingsPath = Join-Path $workspacePath 'App\settings.ini'
$settingsHash = if (Test-Path -LiteralPath $settingsPath) { (Get-FileHash -LiteralPath $settingsPath -Algorithm SHA256).Hash } else { $null }
if (Test-Path -LiteralPath $resultPath) { Remove-Item -LiteralPath $resultPath }
$process = Start-Process -FilePath $executablePath -WorkingDirectory $runtimePath -ArgumentList '--mute','--verify-offline',$resultPath,'--quit-after','1' -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(20000)) { throw 'GUIテストがタイムアウトしました。' }
if ($process.ExitCode -ne 0) { throw 'アプリが異常終了しました。' }
$result = Get-Content -LiteralPath $resultPath -Raw
if (-not $result.StartsWith('PASS:')) { throw $result }
foreach ($screen in @('arena','multi')) {
    $audioResultPath = Join-Path $workspacePath ('build\audio-smoke-' + $screen + '.txt')
    if (Test-Path -LiteralPath $audioResultPath) { Remove-Item -LiteralPath $audioResultPath }
    $audioProcess = Start-Process -FilePath $executablePath -WorkingDirectory $runtimePath -ArgumentList '--verify-audio',('"' + $audioResultPath + '"'),'--screen',$screen,'--quit-after','1' -WindowStyle Hidden -PassThru
    if (-not $audioProcess.WaitForExit(20000) -or $audioProcess.ExitCode -ne 0) { throw 'Audio test did not finish normally.' }
    $audioResult = Get-Content -LiteralPath $audioResultPath -Raw
    if (-not $audioResult.StartsWith('PASS:')) { throw $audioResult }
    Write-Output ($screen + ': ' + $audioResult.Trim())
}
$settingsHashAfter = if (Test-Path -LiteralPath $settingsPath) { (Get-FileHash -LiteralPath $settingsPath -Algorithm SHA256).Hash } else { $null }
if ($settingsHashAfter -ne $settingsHash) { throw 'GUIテストが普段の接続設定を変更しました。' }
Write-Output $result
Write-Output 'PASS: isolated GUI runtime, user settings unchanged'
