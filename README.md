# RustMusic True Portable Builder v1.0

**Windows x64 • GitHub Actions • ZIP-only • PortableApps.com Menu metadata • Original icon**

This is an **independent builder repository**, not a fork of RustMusic. Each manual GitHub Actions run fetches an official tagged RustMusic source release, applies focused portability changes, compiles on a Windows runner, and packages `RustMusicPortable/`. The portable application is **not included in this builder kit**; GitHub Actions creates it.

The initial target is **RustMusic v0.3.1** (`https://github.com/larevuegeek/rustmusic-desktop/releases/tag/v0.3.1`). Source: https://github.com/larevuegeek/rustmusic-desktop. License: GPL-3.0-only.

## How to run (no local coding or compiler needed)

1. Download and **extract** this builder kit. It contains a `.github` directory, `launcher`, `scripts`, and this README.
2. In https://github.com/new create a **new empty repository**, for example `RustMusic-True-Portable-Builder`. Choose **Private** or Public. Do not add a README or license during creation.
3. Select **uploading an existing file** on the empty repository page. Drag and drop the **contents of the extracted kit** into the GitHub upload page, preserving the folders. Verify the file `.github/workflows/build.yml` appears with its exact path. If the browser does not preserve that folder, use **Add file → Create new file**, name it `.github/workflows/build.yml`, and paste in the workflow from your extracted folder. Commit the files. (Repeat upload for other missing folders/files.)
4. Open your repository's **Actions** tab. Select **Build RustMusic True Portable** in the left column.
5. Click **Run workflow**, leave the input at `v0.3.1`, and click the green **Run workflow** button.
6. Once the run is complete and green, click the successful run and download the artifact named **RustMusicPortable-v0.3.1-win64**.
7. Extract the downloaded artifact ZIP. It contains one **RustMusicPortable** folder. Move that folder wherever you like, e.g., `D:\PortableApps\RustMusicPortable`.
8. Double-click **RustMusicPortable.exe** at the root of that folder. Do **not** run `App\RustMusic.exe` directly.

Artifacts expire after **1 day**, so download the output after your run.

### Expected portable app structure

```text
RustMusicPortable/
  RustMusicPortable.exe     <-- start here; original official icon
  App/
    RustMusic.exe           <-- source-built player executable
    AppInfo/
      appinfo.ini           <-- PortableApps Menu detection
      appicon.ico           <-- original official icon
      appicon_32.png
  Data/                     <-- ALWAYS keep this folder across upgrades
    Tauri/                  <-- SQLite, settings, WebView2 and state
    TraceGuard/             <-- TraceGuard.log and recovered traces
  Other/
    LICENSE-RustMusic.txt
    README-PORTABLE.txt
    Source-Patches/
```

## Updating the portable app

Trigger a new build for an available upstream version. If the official source layout changed, the build **intentionally fails** until source patches have been reviewed (instead of silently producing a non-portable app). To update, close RustMusic and replace `App/` and `RustMusicPortable.exe` from the new artifact while **keeping `Data/` unchanged**. AppInfo metadata/icons may also be updated. Do not unpack a new artifact over an irreplaceable Data folder without making a backup first.

## What makes it portable

- Official source build; no installer, `.paf.exe`, or Windows application registration.
- Tauri app directories and WebView2 state use `App/RustMusic.exe`-relative `../Data/Tauri` (`appDirectoriesOverride`).
- Direct Rust SQLite storage, app logs, and `.rust-music` settings are patched to respect `RUSTMUSIC_PORTABLE_DATA`, provided only by the portable launcher.
- Windows autostart and in-app installer-style updates are disabled in the compiled edition. Use the portable ZIP for future updates.
- A native C++17 Windows launcher uses the **upstream icon** and creates `Data` automatically.
- TraceGuard logs before/after status for a deliberately restricted list of RustMusic-specific paths plus dynamically named app-prefixed folders and registry entries under known roots. Newly created, app-named host folders are copied into `Data/TraceGuard/Recovered`, verified, and removed **only if verification succeeds**. Pre-existing host data is never deleted or overwritten. Shared caches/registry keys are never removed. TraceGuard cannot guarantee zero traces.

## Important limits

- This builder has been authored from inspected public RustMusic source files; **the Windows build and first-run cleanup have not yet been executed/validated in this environment**. Please use the GitHub Actions logs to identify build issues and test TraceGuard on your PC. A successful CI compilation is not proof of full portability.
- Windows 10/11 needs the **Microsoft Edge WebView2 Runtime** (usually installed already on Windows 11). Media controls, network discovery, system audio, Windows Defender, file dialogs, and Windows itself may create shared logs/caches. Those should never be cleaned aggressively.
- If RustMusic is already installed normally, its **pre-existing settings are intentionally left alone**. They are *not* automatically imported into `Data`.
- Music libraries might save absolute paths. If you move between drives/PCs, you may need to relink folders in the player.
- Do not run the portable launcher as Administrator. Do not enable OS autostart or permanent file associations for strict portability.
- Tauri and RustMusic may introduce other platform integration paths in future versions. Source patch verification and real-world TraceGuard testing are required after upgrades.

## Diagnostics

If the build fails, in GitHub Actions open the failed workflow run, click the failed step, and download the `RustMusic-build-diagnostics` artifact if present. The last 40-60 lines of the error log are generally enough to identify the issue. For runtime issues, close the app and send `Data/TraceGuard/TraceGuard.log` with a description of the issue; do **not** send your music database or personal files unless necessary.

## Security and licensing

This independent builder fetches the source from the project's official GitHub tags. A tag is used for reproducibility but it is not a signature verification. Source code from upstream remains under **GPL-3.0-only**; launcher source, source modifications, and license are included in the output's `Other/` folder. Redistribution must comply with the GPL, including corresponding-source obligations and appropriate attribution. This is not an official distribution or a signed application; SmartScreen might warn on the community-built unsigned launcher.
