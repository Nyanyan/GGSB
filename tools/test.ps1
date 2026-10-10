param([string]$ExistingLog, [switch]$Network)
$ErrorActionPreference = 'Stop'
$workspacePath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Push-Location -LiteralPath $workspacePath
try {
    New-Item -ItemType Directory -Force build | Out-Null
    & g++ -std=c++20 -O2 -Wall -Wextra -pedantic tests/core_tests.cpp -o build/core_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'コアテストのコンパイルに失敗しました。' }
    if ($ExistingLog) { & ./build/core_tests.exe $ExistingLog } else { & ./build/core_tests.exe }
    if ($LASTEXITCODE -ne 0) { throw 'コアテストに失敗しました。' }
    & g++ -std=c++20 -O2 -Wall -Wextra -Wno-unknown-pragmas tests/account_tests.cpp -lcrypt32 -o build/account_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'アカウント保存テストのコンパイルに失敗しました。' }
    & ./build/account_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'アカウント保存テストに失敗しました。' }
    if ($Network) {
        & g++ -std=c++20 -O2 -Wall -Wextra -Wno-unknown-pragmas tests/network_tests.cpp -lws2_32 -o build/network_tests.exe
        if ($LASTEXITCODE -ne 0) { throw '通信テストのコンパイルに失敗しました。' }
        & ./build/network_tests.exe
        if ($LASTEXITCODE -ne 0) { throw '通信テストに失敗しました。ローカル TCP 通信を許可する環境で実行してください。' }
    }
} finally { Pop-Location }
