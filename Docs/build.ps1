param([ValidateSet('M0','M1','M2','M3')][string]$Stage='M3')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$fso = New-Object -ComObject Scripting.FileSystemObject
$shortRoot = $fso.GetFolder($projectRoot).ShortPath
$arguments = "-r $shortRoot/MDK-ARM/sms-hub.uvprojx -j0 -o $shortRoot/Docs/$Stage-build.log"
$process = Start-Process 'D:/Keil_v5/UV4/UV4.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
$process.WaitForExit()
$log = Get-Content -LiteralPath "$projectRoot/Docs/$Stage-build.log" -Raw
if ($log -notmatch '0 Error\(s\), 0 Warning\(s\)') { throw $log }
Copy-Item -LiteralPath "$projectRoot/MDK-ARM/Objects/sms-hub.map" -Destination "$projectRoot/Docs/$Stage-map.txt"
Write-Output ($log -split "`n" | Select-Object -Last 5)
