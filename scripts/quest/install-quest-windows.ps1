<#
.SYNOPSIS
Install the prebuilt LBA2 Quest APK and copy the retail game data.

.DESCRIPTION
Downloads Android platform-tools into a temporary directory, fetches the
latest Quest APK from the fork's GitHub Releases, copies the folder containing
lba2.hqr to /sdcard/lba2cc/, and installs the APK.

.PARAMETER Assets
Folder containing lba2.hqr. Prompted if omitted.

.PARAMETER Apk
Use a local APK instead of downloading a release.

.PARAMETER Tag
Release tag to install. Default is the latest stable release; use "latest"
for the rolling pre-release.

.PARAMETER Clean
Uninstall the previous app before installing.

.PARAMETER NoLaunch
Do not launch the app after install.

.PARAMETER KeepTools
Keep the temporary platform-tools and APK.

.PARAMETER DryRun
Print the actions without touching the headset.

.EXAMPLE
.\install-quest-windows.ps1 -Assets "C:\Games\LBA2"

.EXAMPLE
.\install-quest-windows.ps1 -Tag latest -Clean
#>
[CmdletBinding()]
param(
    [string]$Assets = "",
    [string]$Apk = "",
    [string]$Tag = "",
    [switch]$Clean,
    [switch]$NoLaunch,
    [switch]$KeepTools,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$Repo = "s-gelazevicius/lba2-quest"
$Package = "org.lbalab.lba2cc"
$Activity = "org.libsdl.app.SDLActivity"
$Destination = "/sdcard/lba2cc"
$PlatformToolsUrl = "https://dl.google.com/android/repository/platform-tools-latest-windows.zip"

function Fail([string]$Message) {
    throw $Message
}

if ($env:OS -ne "Windows_NT") {
    Fail "This installer is for Windows. Use the Linux or macOS installer instead."
}

$Temp = Join-Path $env:TEMP ("lba2-quest-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $Temp -Force | Out-Null
$Serial = ""
$Adb = ""

function Cleanup {
    if (-not $KeepTools) {
        Remove-Item -LiteralPath $Temp -Recurse -Force -ErrorAction SilentlyContinue
    } else {
        Write-Host ""
        Write-Host "Kept temporary files in: $Temp"
    }
}

try {
    Write-Host "Downloading temporary Android platform-tools..."
    $toolsZip = Join-Path $Temp "platform-tools.zip"
    Invoke-WebRequest -Uri $PlatformToolsUrl -OutFile $toolsZip -UseBasicParsing
    Expand-Archive -LiteralPath $toolsZip -DestinationPath $Temp -Force
    $Adb = Join-Path (Join-Path $Temp "platform-tools") "adb.exe"
    if (-not (Test-Path -LiteralPath $Adb)) {
        Fail "adb.exe was not found after extracting platform-tools."
    }
    try {
        Get-ChildItem -LiteralPath (Split-Path -Parent $Adb) -File |
            Unblock-File -ErrorAction Stop
    } catch {
        # Unblock-File is Windows-only; ignore on non-Windows test hosts.
    }

    if ($Apk) {
        if (-not (Test-Path -LiteralPath $Apk)) {
            Fail "APK not found: $Apk"
        }
        $ApkPath = (Resolve-Path -LiteralPath $Apk).Path
        $ApkName = Split-Path -Leaf $ApkPath
    } else {
        $api = if ($Tag) {
            "https://api.github.com/repos/$Repo/releases/tags/$Tag"
        } else {
            "https://api.github.com/repos/$Repo/releases/latest"
        }
        $headers = @{
            "User-Agent" = "lba2-quest-installer"
            "Accept" = "application/vnd.github+json"
        }
        if ($env:GITHUB_TOKEN) {
            $headers["Authorization"] = "Bearer $env:GITHUB_TOKEN"
        }
        try {
            $release = Invoke-RestMethod -Uri $api -Headers $headers
        } catch {
            if (-not $env:GITHUB_TOKEN) {
                Fail "Could not read the GitHub release: $($_.Exception.Message) If the repository is private, set GITHUB_TOKEN and retry."
            }
            Fail "Could not read the GitHub release: $($_.Exception.Message)"
        }

        $asset = @($release.assets) |
            Where-Object { $_.name -like "*-quest-arm64-v8a.apk" } |
            Select-Object -First 1
        if (-not $asset) {
            Fail "Release $($release.tag_name) does not contain a Quest APK. Try -Tag latest or use -Apk."
        }

        $ApkName = $asset.name
        $ApkPath = Join-Path $Temp $ApkName
        Write-Host "Downloading $ApkName from release $($release.tag_name)..."
        $downloadHeaders = @{
            "User-Agent" = "lba2-quest-installer"
            "Accept" = "application/octet-stream"
        }
        if ($env:GITHUB_TOKEN) {
            $downloadHeaders["Authorization"] = "Bearer $env:GITHUB_TOKEN"
        }
        $downloadUrl = if ($asset.url) { $asset.url } else { $asset.browser_download_url }
        Invoke-WebRequest -Uri $downloadUrl -OutFile $ApkPath -Headers $downloadHeaders -UseBasicParsing
    }

    if (-not $Assets) {
        $Assets = Read-Host "Path to the folder containing lba2.hqr"
    }
    if ($Assets -eq "~") {
        $Assets = $HOME
    } elseif ($Assets -match '^~[\\/](.*)$') {
        $Assets = Join-Path $HOME $Matches[1]
    }
    if (-not (Test-Path -LiteralPath $Assets -PathType Container)) {
        Fail "Folder not found: $Assets"
    }
    $Assets = (Resolve-Path -LiteralPath $Assets).Path

    if (-not (Test-Path -LiteralPath (Join-Path $Assets "lba2.hqr"))) {
        foreach ($sub in @("Common", "CommonClassic")) {
            $candidate = Join-Path $Assets $sub
            if (Test-Path -LiteralPath (Join-Path $candidate "lba2.hqr")) {
                $Assets = $candidate
                break
            }
        }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $Assets "lba2.hqr"))) {
        Fail "lba2.hqr was not found in: $Assets"
    }

    if ($DryRun) {
        Write-Host "[dry-run] adb:    $Adb"
        Write-Host "[dry-run] APK:    $ApkPath"
        Write-Host "[dry-run] assets: $Assets"
        Write-Host "[dry-run] would run: adb shell mkdir -p $Destination"
        Write-Host "[dry-run] would run: adb push <assets> $Destination/"
        Write-Host "[dry-run] would run: adb install -r $ApkPath"
        if (-not $NoLaunch) {
            Write-Host "[dry-run] would run: adb shell am start -n $Package/$Activity"
        }
        return
    }

    function Invoke-Adb {
        if ($script:Serial) {
            & $script:Adb -s $script:Serial @args
        } else {
            & $script:Adb @args
        }
    }

    Write-Host "Checking for a connected Quest..."
    $devices = & $Adb devices 2>&1
    $online = @($devices |
        Where-Object { $_ -match "\tdevice\s*$" } |
        ForEach-Object { ($_ -split "\t")[0].Trim() })
    $unauthorized = @($devices |
        Where-Object { $_ -match "\tunauthorized\s*$" } |
        ForEach-Object { ($_ -split "\t")[0].Trim() })

    if ($online.Count -eq 0) {
        if ($unauthorized.Count -gt 0) {
            Fail "The Quest is connected but not authorized. Accept the USB debugging prompt in the headset, then retry."
        }
        $message = @"
No Quest found.

Checklist:
  - Developer Mode is enabled on the headset.
  - USB debugging is enabled in the headset developer settings.
  - The USB cable supports data, not just charging.
  - On Windows, install the Meta Quest ADB driver if the device is not listed.
  - Accept the USB debugging prompt in the headset.
"@
        Fail $message
    }

    if ($online.Count -gt 1) {
        Write-Host "Multiple devices connected:"
        $online | ForEach-Object { Write-Host "  $_" }
        $Serial = Read-Host "Enter the serial to use"
        if ($online -notcontains $Serial) {
            Fail "Unknown serial: $Serial"
        }
    } else {
        $Serial = $online[0]
    }

    Write-Host "Copying game data to $Destination ..."
    Invoke-Adb shell mkdir -p $Destination | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Fail "Could not create $Destination on the headset."
    }

    Get-ChildItem -LiteralPath $Assets -Force | ForEach-Object {
        Invoke-Adb push $_.FullName "$Destination/"
        if ($LASTEXITCODE -ne 0) {
            Fail "Failed to copy $($_.Name) to the headset."
        }
    }

    if ($Clean) {
        Write-Host "Removing the previous install (app-local saves may be lost)..."
        Invoke-Adb uninstall $Package 2>$null | Out-Null
    }

    $output = Invoke-Adb install -r $ApkPath 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        Write-Host $output
        if ($output -match "INSTALL_FAILED_UPDATE_INCOMPATIBLE|signatures do not match") {
            if (-not $Clean) {
                $answer = Read-Host "The existing app used a different debug key. Uninstall and reinstall? App-local saves may be lost. [y/N]"
                if ($answer -match "^(y|yes)$") {
                    Invoke-Adb uninstall $Package 2>$null | Out-Null
                    $output = Invoke-Adb install -r $ApkPath 2>&1 | Out-String
                    if ($LASTEXITCODE -ne 0) {
                        Write-Host $output
                        Fail "Install failed."
                    }
                } else {
                    Fail "Re-run with -Clean to replace the existing install."
                }
            } else {
                Fail "Install failed."
            }
        } else {
            Fail "Install failed."
        }
    } else {
        Write-Host "Installed $ApkName."
    }

    if (-not $NoLaunch) {
        Invoke-Adb shell am start -n "$Package/$Activity" | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Launched $Package."
        } else {
            Write-Host "Installed, but the app could not be launched automatically. Open it from the Quest library."
        }
    }
}
finally {
    Cleanup
}
