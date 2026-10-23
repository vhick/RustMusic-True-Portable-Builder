#!/usr/bin/env python3
"""Strict source transformations for RustMusic v0.3.1. Fail closed on upstream changes."""
import argparse, json, pathlib, re, sys

p=argparse.ArgumentParser()
p.add_argument('--source', required=True)
p.add_argument('--version', default='0.3.1')
a=p.parse_args()
root=pathlib.Path(a.source)
changes=[]

def replace(path, old, new, expected=1):
    file=root / path
    text=file.read_text(encoding='utf-8')
    count=text.count(old)
    if count != expected:
        raise SystemExit(f'PATCH FAILED {path}: expected {expected} occurrences, found {count}: {old[:90]}')
    file.write_text(text.replace(old,new), encoding='utf-8')
    changes.append(f'{path}: {old[:85]!r} -> {new[:90]!r}')

def replace_re(path, pat, new, expected=1):
    file=root / path
    text=file.read_text(encoding='utf-8')
    result, count=re.subn(pat,new,text,flags=re.MULTILINE)
    if count!=expected:
        raise SystemExit(f'PATCH FAILED {path}: regex expected {expected}, got {count}: {pat}')
    file.write_text(result, encoding='utf-8')
    changes.append(f'{path}: pattern {pat[:100]!r}')

pkg=json.loads((root/'package.json').read_text(encoding='utf-8'))
if pkg['version'] != a.version:
    raise SystemExit(f'Expected package version {a.version}, found {pkg["version"]}; review required.')

replace('src-tauri/src/helper/database/sqlite.rs',
    'let mut dir: PathBuf = dirs::data_dir().unwrap_or_else(|| PathBuf::from("."));',
    'let mut dir: PathBuf = std::env::var_os("RUSTMUSIC_PORTABLE_DATA")\n        .map(PathBuf::from)\n        .unwrap_or_else(|| dirs::data_dir().unwrap_or_else(|| PathBuf::from(".")));')
replace('src-tauri/src/core/settings_manager/settings_manager.rs',
    'let mut config_path = home_dir;',
    'let mut config_path = env::var_os("RUSTMUSIC_PORTABLE_DATA")\n            .map(PathBuf::from)\n            .unwrap_or(home_dir);')
replace('src-tauri/src/lib.rs',
    'let mut log_dir = dirs::data_dir().unwrap_or_else(|| std::path::PathBuf::from("."));',
    'let mut log_dir = std::env::var_os("RUSTMUSIC_PORTABLE_DATA")\n        .map(std::path::PathBuf::from)\n        .unwrap_or_else(|| dirs::data_dir().unwrap_or_else(|| std::path::PathBuf::from(".")));')

# Remove Windows startup registration at the source; no plugin can create Run entry.
replace_re('src-tauri/src/lib.rs',
    r'\s*\.plugin\(tauri_plugin_autostart::init\(\s*tauri_plugin_autostart::MacosLauncher::LaunchAgent,\s*None,\s*//[^\n]*\n\s*\)\)',
    '\n        // PORTABLE BUILD: autostart plugin disabled.')
# Never invoke Tauri's installer based updater.
replace('src-tauri/src/lib.rs',
    '.plugin(tauri_plugin_updater::Builder::new().build())',
    '/* PORTABLE BUILD: updater plugin disabled. Upgrade by replacing App/ only. */')

# Disable periodic frontend updater polling and its install banner.
replace('src/routes/+layout.svelte',
    "  const { initUpdaterAutoCheck } = await import('#lib/services/updater/updater.service');\n  initUpdaterAutoCheck(5000);",
    '  // PORTABLE BUILD: automatic updater disabled.')
replace('src/routes/+layout.svelte',
    'import UpdateBanner from "#lib/components/updater/UpdateBanner.svelte";',
    '// PORTABLE BUILD: no installer update banner.')
# Match the component itself, not indentation that can differ between revisions.
# Exactly one live UpdateBanner component must be present; otherwise fail safely.
replace_re('src/routes/+layout.svelte',
    r'^[ \t]*<UpdateBanner[ \t]*/>[ \t]*$',
    '  <!-- PORTABLE BUILD: updater disabled -->')

# The updater Rust plugin and auto-check/banner are removed. Any manual check
# in settings uses existing graceful error handling; it cannot download/install.

cfgpath=root/'src-tauri/tauri.conf.json'
cfg=json.loads(cfgpath.read_text(encoding='utf-8'))
if cfg.get('version')!=a.version: raise SystemExit('Tauri version mismatch. Not patching.')
if 'appDirectoriesOverride' in cfg.get('app', {}): raise SystemExit('Upstream has custom directories. Review first.')
# RustMusic.exe is inside App/, so relative ../Data/Tauri resolves beside App/.
cfg['app']['appDirectoriesOverride']='../Data/Tauri'
for window in cfg['app']['windows']:
    if window.get('dataDirectory'): raise SystemExit('Upstream custom webview data directory; review needed.')
cfg['bundle']['active']=False
cfg['bundle']['createUpdaterArtifacts']=False
cfgpath.write_text(json.dumps(cfg,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
changes.append('src-tauri/tauri.conf.json: appDirectoriesOverride=../Data/Tauri; no installer bundles')

cap=root/'src-tauri/capabilities/default.json'
c=json.loads(cap.read_text(encoding='utf-8'))
remove={'autostart:allow-enable','autostart:allow-disable','autostart:allow-is-enabled','updater:default','process:allow-restart'}
before=set(s for s in c['permissions'] if isinstance(s,str))
if not remove.issubset(before): raise SystemExit(f'Upstream capability set changed: missing {remove-before}')
c['permissions']=[s for s in c['permissions'] if not (isinstance(s,str) and s in remove)]
cap.write_text(json.dumps(c,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
changes.append('src-tauri/capabilities/default.json: updater/autostart/restart permissions removed')

for n in changes: print('PATCH OK:', n)
report=root/'PORTABLE_PATCH_SUMMARY.txt'
report.write_text('RustMusic '+a.version+' portable source adjustments\n\n'+'\n'.join(changes)+'\n',encoding='utf-8')
print('Portable source patch applied; run `git diff` for review.')
