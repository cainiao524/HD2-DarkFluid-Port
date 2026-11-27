# Partial download of an old HELLDIVERS 2 build (app 553850, depot 553853) with DepotDownloader.
# Run it yourself in a terminal: the Steam password and Steam Guard code are typed only there.
#
#   .\download.ps1 -Account <steam account> -Manifest 5604538564913335971 -Tag 20240601 -ListOnly   # file list only
#   .\download.ps1 -Account <steam account> -Manifest 5604538564913335971 -Tag 20240601             # files in probe.txt
#   .\download.ps1 -Account <steam account> -Manifest 5604538564913335971 -Tag 20240601 -FileList .\assets.txt
#   .\download.ps1 -Qr -Manifest 5604538564913335971 -Tag 20240601 -ListOnly                       # sign in by QR code
#
# Output: <OutRoot>\<Tag>\ (default F:\hd2_old). -Tool points at DepotDownloader.exe. Manifest ids: manifests.md.
param(
    [string]$Account = "",
    [Parameter(Mandatory)][string]$Manifest,
    [Parameter(Mandatory)][string]$Tag,
    [string]$FileList = "$PSScriptRoot\probe.txt",
    [string]$OutRoot = "F:\hd2_old",
    [string]$Tool = "F:\Game\A\HD2\A\Depotdownloader\DepotDownloader.exe",
    [switch]$Qr,
    [switch]$ListOnly
)
if (-not (Test-Path $Tool)) { Write-Error "DepotDownloader.exe not found at $Tool (use -Tool)"; exit 1 }
if (-not $Qr -and $Account -eq "") { Write-Error "Give -Account <steam account>, or -Qr to sign in with the Steam mobile app"; exit 1 }
$out = Join-Path $OutRoot $Tag
New-Item -ItemType Directory -Force $out | Out-Null
$common = @("-app", "553850", "-depot", "553853", "-manifest", $Manifest, "-dir", $out)
if ($Qr) { $common += "-qr" } else { $common += @("-username", $Account, "-remember-password") }
if ($ListOnly) { & $Tool @common -manifest-only }
else { & $Tool @common -filelist $FileList -validate }
Write-Host "exit code: $LASTEXITCODE  ->  $out"
