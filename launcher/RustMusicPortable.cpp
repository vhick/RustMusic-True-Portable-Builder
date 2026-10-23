// RustMusicPortable native Windows launcher (C++17, WinAPI, no .NET requirement).
// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <system_error>
#include <algorithm>
#include <set>
#include <cwctype>
namespace fs = std::filesystem;

static std::string utf8(const std::wstring& w) {
  if(w.empty())return {};
  int len=WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),nullptr,0,nullptr,nullptr);
  if(len<=0)return "<unicode-error>";
  std::string s(len,'\0');
  WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),s.data(),len,nullptr,nullptr);
  return s;
}
static std::wstring env(const wchar_t* key) {
  DWORD n=GetEnvironmentVariableW(key,nullptr,0);
  if(!n)return {};
  std::wstring value(n,L'\0');
  DWORD actual=GetEnvironmentVariableW(key,value.data(),n);
  if(!actual || actual>=n)return {};
  value.resize(actual); return value;
}
static std::wstring dateStamp() {
  SYSTEMTIME t; GetLocalTime(&t);
  wchar_t b[64]; swprintf_s(b,L"%04d%02d%02d-%02d%02d%02d-%03d",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);
  return b;
}
static fs::path pathEnv(const wchar_t* key,const wchar_t* append) {
  auto e=env(key); return e.empty()? fs::path{} : fs::path(e)/append;
}
class Logger {
  std::ofstream file;
public:
  explicit Logger(const fs::path& p) : file(p,std::ios::app|std::ios::binary) {}
  void line(const std::string& s) {if(file) {file<<dateStampUtf8()<<" | "<<s<<"\r\n";file.flush();}}
  static std::string dateStampUtf8() {return utf8(dateStamp());}
};
static bool pathExists(const fs::path& p) {std::error_code e;return fs::exists(p,e);}
// Dynamic TraceGuard: enumerate only app-prefixed direct children of well-known
// profile/ProgramData roots, never shared browser or Windows folders.
static std::wstring lower(std::wstring s) {
  for(auto& c:s)c=static_cast<wchar_t>(std::towlower(c));return s;
}
static bool appPrefix(const std::wstring& name) {
  const auto s=lower(name);
  return s.rfind(L"rustmusic",0)==0 || s.rfind(L"com.larevuegeek.rustmusic",0)==0 ||
         s.rfind(L".rust-music",0)==0;
}
static std::vector<fs::path> dynamicAppDirs() {
  std::vector<fs::path> out;
  for(const wchar_t* variable:{L"APPDATA",L"LOCALAPPDATA",L"PROGRAMDATA"}) {
    const auto base=env(variable);
    if(base.empty())continue;
    std::error_code ec;
    for(fs::directory_iterator i(fs::path(base),fs::directory_options::skip_permission_denied,ec),end;
        i!=end && !ec;i.increment(ec)) {
      if(i->is_directory(ec) && appPrefix(i->path().filename().wstring()))out.push_back(i->path());
      if(ec)break;
    }
  }
  return out;
}
static std::set<std::wstring> dynamicAppRegistryKeys() {
  std::set<std::wstring> names;
  HKEY h=nullptr;
  if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software",0,KEY_ENUMERATE_SUB_KEYS,&h)!=ERROR_SUCCESS)return names;
  DWORD index=0;
  for(;;) {
    wchar_t name[1024];DWORD n=1024;
    const LONG result=RegEnumKeyExW(h,index++,name,&n,nullptr,nullptr,nullptr,nullptr);
    if(result==ERROR_NO_MORE_ITEMS)break;
    if(result!=ERROR_SUCCESS)continue;
    if(appPrefix(std::wstring(name,n)))names.insert(std::wstring(name,n));
  }
  RegCloseKey(h);return names;
}
static std::set<std::wstring> dynamicAppRunValues() {
  std::set<std::wstring> names;
  HKEY h=nullptr;
  if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&h)!=ERROR_SUCCESS)return names;
  DWORD i=0;
  for(;;){
    wchar_t name[1024];DWORD n=1024;
    LONG r=RegEnumValueW(h,i++,name,&n,nullptr,nullptr,nullptr,nullptr);
    if(r==ERROR_NO_MORE_ITEMS)break;
    if(r!=ERROR_SUCCESS)continue;
    if(appPrefix(std::wstring(name,n)))names.insert(std::wstring(name,n));
  }
  RegCloseKey(h);return names;
}
static bool registryKeyExists(const wchar_t* sub) {
  HKEY h=nullptr; LONG r=RegOpenKeyExW(HKEY_CURRENT_USER,sub,0,KEY_READ,&h);
  if(r==ERROR_SUCCESS)RegCloseKey(h);
  return r==ERROR_SUCCESS;
}
static bool runValueExists(const wchar_t* name) {
  HKEY h=nullptr; if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&h)!=ERROR_SUCCESS)return false;
  LONG r=RegQueryValueExW(h,name,nullptr,nullptr,nullptr,nullptr);RegCloseKey(h);return r==ERROR_SUCCESS;
}
static void cleanNewRegistryKey(const wchar_t* sub,bool existed,Logger& log) {
  if(existed)return;
  if(!registryKeyExists(sub))return;
  const LONG r=RegDeleteTreeW(HKEY_CURRENT_USER,sub);
  log.line(std::string(r==ERROR_SUCCESS?"REMOVED app-only NEW registry key: ":"WARNING registry cleanup failed: ")+utf8(sub)+" rc="+std::to_string(r));
}
static void cleanNewRunValue(const wchar_t* name,bool existed,Logger& log) {
  if(existed||!runValueExists(name))return;
  HKEY h=nullptr; LONG r=RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_SET_VALUE,&h);
  if(r==ERROR_SUCCESS){r=RegDeleteValueW(h,name);RegCloseKey(h);}
  log.line(std::string(r==ERROR_SUCCESS?"REMOVED NEW Run startup value: ":"WARNING Run-value cleanup failed: ")+utf8(name));
}
static bool noSymlinks(const fs::path& p) {
  std::error_code ec;
  if(fs::is_symlink(fs::symlink_status(p,ec)))return false;
  for(fs::recursive_directory_iterator it(p,fs::directory_options::skip_permission_denied,ec),end;it!=end && !ec;it.increment(ec)) {
    if(fs::is_symlink(it->symlink_status(ec)))return false;
  }
  return !ec;
}
struct Footprint{uintmax_t files=0,bytes=0;bool valid=true;};
static Footprint measure(const fs::path& p) {
  Footprint o;std::error_code ec;
  for(fs::recursive_directory_iterator it(p,fs::directory_options::skip_permission_denied,ec),end;it!=end && !ec;it.increment(ec)) {
    if(it->is_regular_file(ec)){++o.files; o.bytes+=it->file_size(ec);}
    if(ec)break;
  }
  o.valid=!ec;return o;
}
static void quarantineNewDirectory(const fs::path& p, bool previouslyExisted,const fs::path& quarantine,Logger& log) {
  if(p.empty())return;
  if(previouslyExisted) {log.line("PRESERVED PREEXISTING: "+utf8(p.wstring()));return;}
  if(!pathExists(p))return;
  std::error_code ec;
  if(!fs::is_directory(p,ec)||ec||!noSymlinks(p)) {
    log.line("WARNING cannot safely quarantine (not directory/reparse/permission): "+utf8(p.wstring()));return;
  }
  fs::create_directories(quarantine,ec);
  if(ec) {log.line("WARNING quarantine destination unavailable: "+utf8(p.wstring()));return;}
  // Name based on exact well-known root, never attempt to clean arbitrary user paths.
  std::string tag=utf8(p.filename().wstring());
  if(tag.empty())tag="app-only";
  for(char& c:tag)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))c='_';
  fs::path dst=quarantine/(fs::path(dateStamp()+L"-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::wstring(tag.begin(),tag.end())));
  Footprint before=measure(p);
  if(!before.valid){log.line("WARNING footprint measurement failed, host preserved: "+utf8(p.wstring()));return;}
  fs::copy(p,dst,fs::copy_options::recursive,ec);
  if(ec){log.line("WARNING copy failed, original preserved: "+utf8(p.wstring())+" error="+ec.message());return;}
  Footprint after=measure(dst);
  if(!after.valid||after.files!=before.files||after.bytes!=before.bytes) {
    log.line("WARNING quarantine verification failed, original preserved: "+utf8(p.wstring()));return;
  }
  // Verified copy is in Data. Host source can now be removed.
  fs::remove_all(p,ec);
  log.line(std::string(ec?"WARNING removal failed (copy retained): ":"RECOVERED newly created trace to Data and cleaned: ")+utf8(p.wstring())+" -> "+utf8(dst.wstring()));
}
static void errorPopup(const wchar_t* s){MessageBoxW(nullptr,s,L"RustMusic Portable",MB_OK|MB_ICONERROR);}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int) {
  HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\RustMusicPortableLauncher-05c4fa19");
  if(mutex&&GetLastError()==ERROR_ALREADY_EXISTS){
    MessageBoxW(nullptr,L"RustMusic Portable is already running. Close it before starting another copy.",L"RustMusic Portable",MB_OK|MB_ICONINFORMATION);
    CloseHandle(mutex);return 0;
  }
  wchar_t buf[32768]={};
  DWORD n=GetModuleFileNameW(nullptr,buf,32768);
  if(n==0||n>=32768){errorPopup(L"Unable to determine the portable folder.");if(mutex)CloseHandle(mutex);return 1;}
  const fs::path root=fs::path(buf).parent_path();
  const fs::path exe=root/L"App"/L"RustMusic.exe";
  const fs::path portable=root/L"Data"/L"Tauri";
  const fs::path trace=root/L"Data"/L"TraceGuard";
  const fs::path webview=portable/L"WebView2";
  std::error_code ec;
  for(auto& p:{portable,trace,webview}){fs::create_directories(p,ec);if(ec){errorPopup(L"Cannot create Data folder. Check write permissions.");if(mutex)CloseHandle(mutex);return 2;}}
  Logger log(trace/L"TraceGuard.log");
  log.line("===== portable launch =====");
  log.line("Root: "+utf8(root.wstring()));
  if(!pathExists(exe)){log.line("ERROR missing app executable: "+utf8(exe.wstring()));errorPopup(L"App\\RustMusic.exe not found. Re-extract the complete portable ZIP.");if(mutex)CloseHandle(mutex);return 3;}
  // All known app-specific native Rust and Tauri paths are patched to use this root.
  SetEnvironmentVariableW(L"RUSTMUSIC_PORTABLE",L"1");
  SetEnvironmentVariableW(L"RUSTMUSIC_PORTABLE_DATA",portable.c_str());
  SetEnvironmentVariableW(L"WEBVIEW2_USER_DATA_FOLDER",webview.c_str());
  // Do NOT override HOME, USERPROFILE, APPDATA or LOCALAPPDATA: that affects OS helpers.
  const std::vector<fs::path> candidates={
    pathEnv(L"USERPROFILE",L".rust-music"),
    pathEnv(L"APPDATA",L"com.larevuegeek.rustmusic"),
    pathEnv(L"LOCALAPPDATA",L"com.larevuegeek.rustmusic"),
    pathEnv(L"APPDATA",L"RustMusic"),
    pathEnv(L"LOCALAPPDATA",L"RustMusic")
  };
  std::vector<bool> present;for(auto& p:candidates)present.push_back(pathExists(p));
  const wchar_t* keys[]={L"Software\\RustMusic",L"Software\\com.larevuegeek.rustmusic",L"Software\\LaRevueGeeK\\RustMusic"};
  std::vector<bool> hadKeys;for(auto key:keys)hadKeys.push_back(registryKeyExists(key));
  bool hadRun=runValueExists(L"RustMusic"), hadPortableRun=runValueExists(L"RustMusicPortable");
  // Capture dynamic (app-prefixed) names before the player starts.
  const auto initialDynamicDirs=dynamicAppDirs();
  std::set<std::wstring> initialDynamicPaths;
  for(const auto& p:initialDynamicDirs)initialDynamicPaths.insert(lower(p.wstring()));
  const auto initialDynamicKeys=dynamicAppRegistryKeys();
  const auto initialDynamicRun=dynamicAppRunValues();
  std::wstring command=L"\""+exe.wstring()+L"\"";
  STARTUPINFOW si{};si.cb=sizeof(si);
  PROCESS_INFORMATION pi{};
  std::vector<wchar_t> commandBuffer(command.begin(),command.end());commandBuffer.push_back(0);
  BOOL ok=CreateProcessW(exe.c_str(),commandBuffer.data(),nullptr,nullptr,FALSE,0,nullptr,(root/L"App").c_str(),&si,&pi);
  if(!ok) {
    DWORD e=GetLastError();log.line("ERROR CreateProcessW="+std::to_string(e));
    errorPopup(L"RustMusic failed to start. Check Data\\TraceGuard\\TraceGuard.log.");
    if(mutex)CloseHandle(mutex);return 4;
  }
  CloseHandle(pi.hThread);
  WaitForSingleObject(pi.hProcess,INFINITE);
  DWORD code=0;GetExitCodeProcess(pi.hProcess,&code);
  CloseHandle(pi.hProcess);
  log.line("RustMusic exited; exit code="+std::to_string(code));
  // Do not touch data that existed before launch, even if the app changed it.
  const fs::path quarantine=trace/L"Recovered";
  for(size_t i=0;i<candidates.size();++i)quarantineNewDirectory(candidates[i],present[i],quarantine,log);
  for(size_t i=0;i<hadKeys.size();++i)cleanNewRegistryKey(keys[i],hadKeys[i],log);
  cleanNewRunValue(L"RustMusic",hadRun,log);
  cleanNewRunValue(L"RustMusicPortable",hadPortableRun,log);
  // Only names created this run and starting with RustMusic/com.larevuegeek.rustmusic.
  // A separate app's profiles, generic browser caches and Windows registry are never scanned/deleted.
  for(const auto& path:dynamicAppDirs()) {
    if(initialDynamicPaths.count(lower(path.wstring())))continue;
    bool fixedCandidate=false;
    for(const auto& q:candidates)if(lower(path.wstring())==lower(q.wstring()))fixedCandidate=true;
    if(!fixedCandidate)quarantineNewDirectory(path,false,quarantine,log);
  }
  for(const auto& key:dynamicAppRegistryKeys()) {
    if(initialDynamicKeys.count(key))continue;
    const std::wstring full=L"Software\\"+key;
    cleanNewRegistryKey(full.c_str(),false,log);
  }
  for(const auto& run:dynamicAppRunValues()) {
    if(!initialDynamicRun.count(run))cleanNewRunValue(run.c_str(),false,log);
  }
  log.line("TraceGuard finished. Only app-named paths were checked; shared Windows caches are NEVER deleted.");
  if(mutex)CloseHandle(mutex);
  return static_cast<int>(code);
}
