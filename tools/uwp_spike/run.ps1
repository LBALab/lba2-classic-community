# Registers the spike from this folder (Developer Mode, no signing), runs it
# for 8 seconds, prints its LocalState log, takes a screenshot and stops it.
# The folder needs uwp_spike.exe, AppxManifest.xml and Assets\*.png (see XBOX_PLAN.md).
# Remove with: Get-AppxPackage LBA2CC.UwpSpike | Remove-AppxPackage
$ErrorActionPreference = 'Continue'
"this shell: session " + (Get-Process -Id $PID).SessionId
"locked (LogonUI running): " + [bool](Get-Process -Name LogonUI -ErrorAction SilentlyContinue)
$dir = $PSScriptRoot
Add-AppxPackage -Register "$dir\AppxManifest.xml" -ForceApplicationShutdown
$pkg = Get-AppxPackage -Name LBA2CC.UwpSpike
"registered: " + $pkg.PackageFamilyName
$state = Join-Path $env:LOCALAPPDATA "Packages\$($pkg.PackageFamilyName)\LocalState"
Remove-Item "$state\uwp_spike.log" -ErrorAction SilentlyContinue
Start-Process "shell:AppsFolder\$($pkg.PackageFamilyName)!App"
Start-Sleep -Seconds 8
$proc = Get-Process -Name uwp_spike -ErrorAction SilentlyContinue
"process: " + ($proc | ForEach-Object { "pid $($_.Id) session $($_.SessionId) window '$($_.MainWindowTitle)'" })
"--- log"
Get-Content "$state\uwp_spike.log" -ErrorAction SilentlyContinue
"--- end log"
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
try { $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size); $bmp.Save("$dir\shot.png"); "screenshot: $($b.Width)x$($b.Height)" } catch { "screenshot failed: $_" }
if ($proc) { $proc | ForEach-Object { Stop-Process -Id $_.Id -Force }; "stopped" }
Get-WinEvent -LogName 'Microsoft-Windows-TWinUI/Operational' -MaxEvents 5 -ErrorAction SilentlyContinue | Where-Object { $_.TimeCreated -gt (Get-Date).AddMinutes(-2) } | ForEach-Object { "twinui: " + $_.Message }
