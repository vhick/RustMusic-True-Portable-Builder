# Changelog

## v1.2 — GitHub Actions packaging false-failure fix

- Diagnosed `logs_108122348725.zip`: upstream RustMusic v0.3.1 built successfully; C++ launcher compiled successfully; `package.ps1` reported `PACKAGE VALIDATION OK`.
- Removed the invalid `$LASTEXITCODE -ne 0` condition after invoking a PowerShell `.ps1` script. When no native command sets `$LASTEXITCODE`, a successful call can leave it `$null`, which compares unequal to `0`.
- Added explicit output validation before artifact upload, while retaining fail-fast terminating errors.
- Updated packaged portable README version to 1.2.
- The source modifications and launcher are unchanged from v1.1.

# Changelog

## v1.1 — October 23, 2026

- Fix strict `+layout.svelte` UpdateBanner patch failure from GitHub Actions run 108118474151.
- Prevent Windows C++17 launcher build failure due to ambiguous `exists(fs::path)` call.
- Add GitHub Desktop upgrade instructions.

## v1.0

- Initial standalone, manual GitHub Actions portable builder.
