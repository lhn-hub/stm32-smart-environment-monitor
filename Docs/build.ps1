$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$fso = New-Object -ComObject Scripting.FileSystemObject
$shortRoot = $fso.GetFolder($projectRoot).ShortPath
$arguments = "-r $shortRoot/MDK-ARM/sms-hub.uvprojx -j0 -o $shortRoot/Docs/M0-build.log"
$process = Start-Process 'D:/Keil_v5/UV4/UV4.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
$process.WaitForExit()
$log = Get-Content -LiteralPath "$projectRoot/Docs/M0-build.log" -Raw
if ($log -notmatch '0 Error\(s\), 0 Warning\(s\)') { throw $log }
Copy-Item -LiteralPath "$projectRoot/MDK-ARM/Objects/sms-hub.map" -Destination "$projectRoot/Docs/M0-map.txt"
Write-Output ($log -split "`n" | Select-Object -Last 5)
