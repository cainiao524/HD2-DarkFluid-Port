# For several later builds of depot 553853: the file list, plus bundles.nxa when the build is the slim kind (its archive
# table is the only way to see which resource archives a slim build holds). Small downloads only (about 7 MB each).
#
#   .\scan-versions.ps1 -Qr
#   .\scan-versions.ps1 -Account <steam account>
#
# Output: F:\hd2_old\scan\<date>\ (manifest_553853_*.txt, and data\bundles.nxa for slim builds).
param(
    [string]$Account = "",
    [switch]$Qr,
    [string]$Tool = "F:\Game\A\HD2\A\Depotdownloader\DepotDownloader.exe",
    [string]$OutRoot = "F:\hd2_old\scan"
)
$versions = [ordered]@{
    "20241218" = "8707272629965652805"
    "20250204" = "6498840053859909568"
    "20250404" = "894088980511742516"
    "20250627" = "9203183163189675220"
    "20250916" = "5599124225100708315"
}
if (-not (Test-Path $Tool)) { Write-Error "DepotDownloader.exe not found at $Tool (use -Tool)"; exit 1 }
if (-not $Qr -and $Account -eq "") { Write-Error "Give -Account <steam account>, or -Qr"; exit 1 }
$login = if ($Qr) { @("-qr") } else { @("-username", $Account, "-remember-password") }
$list = Join-Path $OutRoot "bundles-nxa.txt"
New-Item -ItemType Directory -Force $OutRoot | Out-Null
Set-Content -Path $list -Value 'regex:^data[\\/]bundles\.nxa$' -Encoding ascii
foreach ($date in $versions.Keys) {
    $out = Join-Path $OutRoot $date
    $id = $versions[$date]
    Write-Host "=== $date manifest $id"
    & $Tool -app 553850 -depot 553853 -manifest $id @login -dir $out -manifest-only
    $m = Get-ChildItem $out -Filter "manifest_553853_*.txt" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($m -and (Select-String -Path $m.FullName -SimpleMatch "bundles.nxa" -Quiet)) {
        Write-Host "    slim build: fetching data\bundles.nxa"
        & $Tool -app 553850 -depot 553853 -manifest $id @login -dir $out -filelist $list
    }
}
Write-Host "done -> $OutRoot"
