param(
    [Parameter(Mandatory)][string]$Port,
    [ValidateRange(1,172800)][int]$Seconds = 86400,
    [Parameter(Mandatory)][string]$Output
)
$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath $Output) { throw "Output already exists: $Output" }
$serial = [System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$serial.Encoding = [System.Text.UTF8Encoding]::new($false)
$serial.ReadTimeout = 250
$serial.DtrEnable = $false
$serial.RtsEnable = $false
$writer = $null
$clock = [System.Diagnostics.Stopwatch]::new()
try {
    $serial.Open()
    $writer = [System.IO.StreamWriter]::new($Output,$false,[System.Text.UTF8Encoding]::new($false))
    $writer.AutoFlush = $true
    $writer.WriteLine("CAPTURE start=$([DateTime]::UtcNow.ToString('o')) target_seconds=$Seconds")
    $clock.Start()
    while ($clock.Elapsed.TotalSeconds -lt $Seconds) {
        try { $writer.WriteLine($serial.ReadLine().TrimEnd("`r")) }
        catch [System.TimeoutException] { }
    }
}
finally {
    if ($writer) {
        $writer.WriteLine("CAPTURE elapsed_seconds=$($clock.Elapsed.TotalSeconds.ToString('F3',[Globalization.CultureInfo]::InvariantCulture))")
        $writer.Dispose()
    }
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
