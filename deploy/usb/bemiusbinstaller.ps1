# BEMI BIOS USB Installer (Windows PowerShell)
param(
    [string]$Device = "",
    [string]$Firmware = "Build\RELEASE_GCC5\X64\BemiBiosCore.efi"
)

if (-not $Device) {
    Write-Host "Usage: .\BemiUsbInstaller.ps1 -Device <DRIVE_LETTER> [-Firmware <path>]"
    Get-WmiObject Win32_LogicalDisk | Where-Object { $_.DriveType -eq 2 } | ForEach-Object {
        Write-Host "  $($_.DeviceID) ($($_.Size / 1GB) GB)"
    }
    exit 1
}

Write-Host "WARNING: $Device will be modified!"
$resp = Read-Host "Continue? [y/N]"
if ($resp -ne "y") { exit 0 }

Write-Host "[INSTALL] Copying firmware..."
$espPath = "$Device\EFI\BOOT"
New-Item -ItemType Directory -Path $espPath -Force | Out-Null
Copy-Item -Path $Firmware -Destination "$espPath\BOOTX64.EFI" -Force

$nshContent = @"
@echo -off
echo BEMI BIOS v7.2 - UEFI Shell Auto-Run
\EFI\BOOT\BOOTX64.EFI
"@
Set-Content -Path "$Device\startup.nsh" -Value $nshContent

Write-Host "[OK] BEMI BIOS installed to $Device"
