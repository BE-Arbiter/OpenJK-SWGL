# Caps the CPU clock for a test, then restores the power plan values.
# Usage: .\cpu_throttle.ps1 -Mhz 2500 [-Cores 12] [-Exe path\SWGL_SP.exe] [-ExeArgs "+load x"]
# Without -Exe, the cap stays until you press Enter.
param(
    [int]$Mhz = 2500,
    [int]$Cores = 0,
    [string]$Exe = "",
    [string]$ExeArgs = ""
)

$ref = (Get-CimInstance Win32_Processor | Select-Object -First 1).MaxClockSpeed
$pct = [math]::Max(5, [math]::Min(100, [int][math]::Round($Mhz * 100.0 / $ref)))

# The AC index is the second-to-last "0x" line (the last one is DC; the first ones are min/max/step).
$line = powercfg /query SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMAX | Select-String "0x" | Select-Object -Last 2 | Select-Object -First 1
$oldMax = [Convert]::ToInt32(($line.ToString() -split "0x")[-1].Trim(), 16)
Write-Host "Reference clock $ref MHz -> cap $pct% (~$([int]($ref * $pct / 100)) MHz). Saved max=$oldMax"

try {
    powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMAX $pct
    powercfg /setactive SCHEME_CURRENT

    if ($Exe) {
        $p = Start-Process $Exe -ArgumentList $ExeArgs -PassThru -WorkingDirectory (Split-Path $Exe)
        if ($Cores -gt 0) { $p.ProcessorAffinity = [IntPtr][long]([Math]::Pow(2, $Cores) - 1) }
        $p.WaitForExit()
    } else {
        Read-Host "Cap active. Press Enter to restore"
    }
}
finally {
    powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMAX $oldMax
    powercfg /setactive SCHEME_CURRENT
    Write-Host "Power plan restored."
}
