param([ValidateSet('M0','M1','M2','M3','M4','M5','M5-stress','M6','M7','M7-test','M8','M8-test','M8-audit')][string]$Stage='M8')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if ($Stage -like 'M8*') {
    $config = Get-Content -LiteralPath "$projectRoot/App/app_config.h" -Raw
    $test = $config -match '(?m)^#define APP_M8_TEST_ENABLED 1\s*$'
    $audit = $config -match '(?m)^#define APP_CRITICAL_AUDIT 1\s*$'
    if ($test -ne ($Stage -eq 'M8-test') -or $audit -ne ($Stage -eq 'M8-audit') -or
        $config -match '(?m)^#define APP_(M7_TEST|M5_STRESS|STORAGE_STRESS|WATCHDOG_TEST)_ENABLED 1\s*$') {
        throw 'M8 build flags do not match Stage; unrelated bench flags must be off.'
    }
}
if ($Stage -in @('M7','M7-test')) {
    $config = Get-Content -LiteralPath "$projectRoot/App/app_config.h" -Raw
    $enabled = $config -match '(?m)^#define APP_M7_TEST_ENABLED 1\s*$'
    if ($enabled -ne ($Stage -eq 'M7-test')) { throw 'APP_M7_TEST_ENABLED must be 0 for M7, 1 for M7-test.' }
}
if ($Stage -in @('M5','M5-stress')) {
    $config = Get-Content -LiteralPath "$projectRoot/App/app_config.h" -Raw
    $enabled = $config -match '(?m)^#define APP_M5_STRESS_ENABLED 1\s*$'
    if ($enabled -ne ($Stage -eq 'M5-stress')) { throw 'APP_M5_STRESS_ENABLED must be 0 for M5, 1 for M5-stress.' }
}
$fso = New-Object -ComObject Scripting.FileSystemObject
$shortRoot = $fso.GetFolder($projectRoot).ShortPath
$arguments = "-r $shortRoot/MDK-ARM/sms-hub.uvprojx -j0 -o $shortRoot/Docs/$Stage-build.log"
$process = Start-Process 'D:/Keil_v5/UV4/UV4.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
$process.WaitForExit()
$log = Get-Content -LiteralPath "$projectRoot/Docs/$Stage-build.log" -Raw
if ($log -notmatch '0 Error\(s\), 0 Warning\(s\)') { throw $log }
if ($log -notmatch 'Code=(\d+) RO-data=(\d+) RW-data=(\d+) ZI-data=(\d+)') { throw 'Missing program-size evidence.' }
$flashBytes = [int]$Matches[1] + [int]$Matches[2] + [int]$Matches[3]
$ramBytes = [int]$Matches[3] + [int]$Matches[4]
if ($flashBytes -gt 61440 -or $ramBytes -gt 20480) { throw "Budget exceeded: Flash=$flashBytes RAM=$ramBytes." }
Copy-Item -LiteralPath "$projectRoot/MDK-ARM/Objects/sms-hub.map" -Destination "$projectRoot/Docs/$Stage-map.txt"
Write-Output ($log -split "`n" | Select-Object -Last 5)
Write-Output "Flash=$flashBytes/61440B RAM=$ramBytes/20480B"
