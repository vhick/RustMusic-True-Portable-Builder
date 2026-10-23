# Assemble the PortableApps.com Menu-compatible folder (ZIP created by GitHub Artifacts).
param(
    [Parameter(Mandatory=$true)][string]$Upstream,
    [Parameter(Mandatory=$true)][string]$Launcher,
    [Parameter(Mandatory=$true)][string]$BuilderRoot,
    [Parameter(Mandatory=$true)][string]$Version
)
$ErrorActionPreference='Stop'
$base=Join-Path $BuilderRoot 'dist/RustMusicPortable'
if(Test-Path $base){Remove-Item $base -Recurse -Force}
foreach($folder in @('App','App/AppInfo','Data','Data/Tauri','Data/TraceGuard','Other','Other/Source-Patches')) {
    New-Item -Path (Join-Path $base $folder) -ItemType Directory -Force | Out-Null
}
$release=Join-Path $Upstream 'src-tauri/target/release'
$exe=@('RustMusic.exe','rustmusic.exe') | ForEach-Object {Join-Path $release $_} | Where-Object {Test-Path $_} | Select-Object -First 1
if(!$exe){throw "No source-built RustMusic executable in $release. Refusing to package."}
Copy-Item $exe (Join-Path $base 'App/RustMusic.exe') -Force
Get-ChildItem $release -Filter '*.dll' -File | ForEach-Object {
    Copy-Item $_.FullName (Join-Path $base 'App') -Force
}
if(!(Test-Path $Launcher)){throw "Native launcher missing: $Launcher"}
Copy-Item $Launcher (Join-Path $base 'RustMusicPortable.exe') -Force
$icon=Join-Path $Upstream 'src-tauri/icons/icon.ico'
if(!(Test-Path $icon)){throw "Official application icon missing: $icon"}
Copy-Item $icon (Join-Path $base 'App/AppInfo/appicon.ico') -Force
# Use the original raster icon directly from the official source.
foreach($entry in @(@('32x32.png','appicon_32.png'),@('128x128.png','appicon_128.png'))){
  $sourceIcon=Join-Path $Upstream ('src-tauri/icons/'+$entry[0]);
  if(Test-Path $sourceIcon){Copy-Item $sourceIcon (Join-Path $base ('App/AppInfo/'+$entry[1])) -Force}
}
# GitHub Actions artifacts skip empty folders. Add harmless visible placeholders
# so App/Data/Other and both Data subfolders survive the generated ZIP.
[System.IO.File]::WriteAllText((Join-Path $base 'Data/Tauri/KEEP-PORTABLE-DATA.txt'), 'This folder preserves RustMusic settings, SQLite data and WebView2 state across upgrades.', [Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText((Join-Path $base 'Data/TraceGuard/KEEP-TRACEGUARD.txt'), 'Runtime logs and verified recovered app-specific traces are stored here.', [Text.UTF8Encoding]::new($false))
$v4=$Version+'.0' 
$ini=@"
[Format]
Type=PortableApps.comFormat
Version=3.8

[Details]
Name=RustMusic Portable
AppID=RustMusicPortable
Publisher=LaRevueGeeK & Community Portable Builder
Homepage=https://rustmusic.dev
Category=Music
Description=Open-source Hi-Res, DSD and bit-perfect audio player
Language=Multilingual

[License]
Shareable=true
OpenSource=true
Freeware=true
CommercialUse=true

[Version]
PackageVersion=$v4
DisplayVersion=$Version

[Control]
Icons=1
Start=RustMusicPortable.exe
"@
[System.IO.File]::WriteAllText((Join-Path $base 'App/AppInfo/appinfo.ini'),$ini,[Text.UTF8Encoding]::new($false))
Copy-Item (Join-Path $Upstream 'LICENSE') (Join-Path $base 'Other/LICENSE-RustMusic.txt') -Force
Copy-Item (Join-Path $Upstream 'PORTABLE_PATCH_SUMMARY.txt') (Join-Path $base 'Other/Source-Patches/PATCH-SUMMARY.txt') -Force
Copy-Item (Join-Path $Upstream 'PORTABLE_PATCH.diff') (Join-Path $base 'Other/Source-Patches/PORTABLE_PATCH.diff') -Force
Copy-Item (Join-Path $BuilderRoot 'scripts/patch_portable.py') (Join-Path $base 'Other/Source-Patches/patch_portable.py') -Force
Copy-Item (Join-Path $BuilderRoot 'launcher/RustMusicPortable.cpp') (Join-Path $base 'Other/Source-Patches/RustMusicPortable.cpp') -Force
$info=@"
RustMusic True Portable Builder 1.2
Upstream source: https://github.com/larevuegeek/rustmusic-desktop
Upstream release: https://github.com/larevuegeek/rustmusic-desktop/releases/tag/v$Version
Compiled locally by GitHub Actions from the tag above, with source changes in Other/Source-Patches/.
Source remains GPL-3.0-only. This is a community portable variant, not an official binary.

Move the entire RustMusicPortable folder between Windows computers.
Launch with RustMusicPortable.exe (NOT App/RustMusic.exe).
Keep Data intact when upgrading. Replace App/ and RustMusicPortable.exe only.
The app uses the system WebView2 Runtime (normally installed on Windows 11).
See Data/TraceGuard/TraceGuard.log for conservative trace cleanup logs.
Do not assume zero remnants without testing on your own PC.
"@
[System.IO.File]::WriteAllText((Join-Path $base 'Other/README-PORTABLE.txt'),$info,[Text.UTF8Encoding]::new($false))
# Validate the exact structure before uploading.
$required=@('RustMusicPortable.exe','App/RustMusic.exe','App/AppInfo/appicon.ico','App/AppInfo/appinfo.ini','Data/Tauri/KEEP-PORTABLE-DATA.txt','Data/TraceGuard/KEEP-TRACEGUARD.txt','Other/LICENSE-RustMusic.txt')
foreach($p in $required){if(!(Test-Path (Join-Path $base $p))){throw "Package validation FAILED: $p missing"}}
Write-Host 'Portable folder contents:'
Get-ChildItem $base -Recurse | Select-Object FullName,Length | Format-Table -AutoSize
Write-Host 'PACKAGE VALIDATION OK.'
