<#
Runs rt_boot.exe headless with a wall-clock kill. Game data and BIOS are only read.
Log: recomp\rt\_out\<Name>.log (gitignored).
#>
param(
  [string]$Name = 'run',
  [string]$Roms = '<home>\star-cean-second\roms',
  [string]$Exe  = "$PSScriptRoot\..\host\_out\proj\disc\SCUS_944.21",
  [int]$Vblanks = 3000, [int]$Seconds = 20, [int]$Verbose = 0
)
$out = "$PSScriptRoot\_out"
$bin = Get-ChildItem "$Roms\disc1\*.bin" | Select-Object -First 1
$a = @('--bios', "$Roms\scph5501.bin", '--exe', (Resolve-Path $Exe).Path, '--cue-bin', "`"$($bin.FullName)`"",
       '--seconds', $Seconds, '--vblanks', $Vblanks, '--verbose', $Verbose)
$p = Start-Process -FilePath "$out\rt_boot.exe" -ArgumentList $a -NoNewWindow -PassThru `
     -RedirectStandardOutput "$out\$Name.log" -RedirectStandardError "$out\$Name.err"
if (-not $p.WaitForExit(($Seconds + 30) * 1000)) { $p.Kill(); "killed after timeout" }
"exit $($p.ExitCode)"
Get-Content "$out\$Name.err" -TotalCount 5
Get-Content "$out\$Name.log" -Tail 25
