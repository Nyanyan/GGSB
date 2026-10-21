param([switch]$CaptureThemes)
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
$audioCases = @(
    @{Name='arena'; Screen='arena'; Kind='move'; Volume=100; Extra=@()},
    @{Name='multi'; Screen='multi'; Kind='move'; Volume=100; Extra=@()},
    @{Name='chat'; Screen='arena'; Kind='chat'; Volume=100; Extra=@()},
    @{Name='chat-quiet'; Screen='arena'; Kind='chat'; Volume=25; Extra=@()},
    @{Name='chat-zero'; Screen='arena'; Kind='chat'; Volume=0; Extra=@()},
    @{Name='chat-muted'; Screen='arena'; Kind='chat'; Volume=100; Extra=@('--audio-chat-muted')},
    @{Name='chat-off'; Screen='arena'; Kind='chat'; Volume=100; Extra=@('--audio-chat-off')}
)
$audioPeaks = @{}
foreach ($case in $audioCases) {
    $audioResultPath = Join-Path $workspacePath ('build\audio-smoke-' + $case.Name + '.txt')
    if (Test-Path -LiteralPath $audioResultPath) { Remove-Item -LiteralPath $audioResultPath }
    $audioArguments = @('--verify-audio',('"' + $audioResultPath + '"'),'--screen',$case.Screen,'--audio-kind',$case.Kind,'--audio-volume',("" + $case.Volume),'--quit-after','1') + $case.Extra
    $audioProcess = Start-Process -FilePath $executablePath -WorkingDirectory $runtimePath -ArgumentList $audioArguments -WindowStyle Hidden -PassThru
    if (-not $audioProcess.WaitForExit(20000) -or $audioProcess.ExitCode -ne 0) { throw 'Audio test did not finish normally.' }
    $audioResult = Get-Content -LiteralPath $audioResultPath -Raw
    if (-not $audioResult.StartsWith('PASS:')) { throw $audioResult }
    if ($audioResult -match 'peak=([0-9.]+)') { $audioPeaks[$case.Name] = [double]::Parse($Matches[1], [Globalization.CultureInfo]::InvariantCulture) }
    Write-Output ($case.Name + ': ' + $audioResult.Trim())
}
if ($audioPeaks['chat-quiet'] -ge $audioPeaks['chat'] * 0.5 -or $audioPeaks['chat-quiet'] -le $audioPeaks['chat'] * 0.1) { throw 'Volume adjustment did not reduce the notification amplitude.' }
if ($CaptureThemes) {
    $previews = @(
        @{Theme='dark'; Screen='settings'; Language='ja'},
        @{Theme='light'; Screen='settings'; Language='ja'},
        @{Theme='light'; Screen='settings'; Language='en'},
        @{Theme='light'; Screen='connect'; Language='ja'},
        @{Theme='light'; Screen='connect'; Language='en'},
        @{Theme='light'; Screen='arena'; Language='en'},
        @{Theme='light'; Screen='multi'; Language='en'},
        @{Theme='light'; Screen='console'; Language='en'},
        @{Theme='light'; Screen='chat'; Language='en'},
        @{Theme='light'; Screen='challenge'; Language='ja'},
        @{Theme='light'; Screen='records'; Language='en'}
    )
    foreach ($preview in $previews) {
        $capturePath = Join-Path $workspacePath ('build\theme-' + $preview.Theme + '-' + $preview.Screen + '-' + $preview.Language + '.png')
        if (Test-Path -LiteralPath $capturePath) { Remove-Item -LiteralPath $capturePath }
        $fixtureArguments = if ($preview.Screen -eq 'console') { @('--console-preview') } else { @('--demo') }
        $captureArguments = @($fixtureArguments) + @('--mute','--theme',$preview.Theme,'--language',$preview.Language,'--screen',$preview.Screen,'--capture',('"' + $capturePath + '"'),'--capture-at','1','--quit-after','2')
        $previewProcess = Start-Process -FilePath $executablePath -WorkingDirectory $runtimePath -ArgumentList $captureArguments -WindowStyle Hidden -PassThru
        if (-not $previewProcess.WaitForExit(20000) -or $previewProcess.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $capturePath)) { throw ('Theme preview failed: ' + $capturePath) }
        Write-Output $capturePath
    }
}
$settingsHashAfter = if (Test-Path -LiteralPath $settingsPath) { (Get-FileHash -LiteralPath $settingsPath -Algorithm SHA256).Hash } else { $null }
if ($settingsHashAfter -ne $settingsHash) { throw 'GUIテストが普段の接続設定を変更しました。' }
Write-Output $result
Write-Output 'PASS: isolated GUI runtime, user settings unchanged'
