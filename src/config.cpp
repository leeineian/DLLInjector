#include "config.hpp"
#include <shlwapi.h>

static std::wstring GetConfigFilePath() {
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(nullptr, szPath, MAX_PATH);
    PathRemoveFileSpecW(szPath);
    PathAppendW(szPath, L"config.ini");
    return szPath;
}

AppConfig ConfigManager::Load() {
    AppConfig config;
    std::wstring path = GetConfigFilePath();

    wchar_t szBuf[MAX_PATH];
    GetPrivateProfileStringW(L"Settings", L"ProcessName", L"minecraft.windows.exe", szBuf, MAX_PATH, path.c_str());
    config.procName = szBuf;

    GetPrivateProfileStringW(L"Settings", L"DLLPath", L"", szBuf, MAX_PATH, path.c_str());
    config.dllPath = szBuf;

    config.delay = GetPrivateProfileIntW(L"Settings", L"Delay", 5, path.c_str());
    if (config.delay < 1) config.delay = 5;

    config.customProcName = GetPrivateProfileIntW(L"Settings", L"CustomProcess", 0, path.c_str()) != 0;

    return config;
}

void ConfigManager::Save(const AppConfig& config) {
    std::wstring path = GetConfigFilePath();

    WritePrivateProfileStringW(L"Settings", L"ProcessName", config.procName.c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"DLLPath", config.dllPath.c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"Delay", std::to_wstring(config.delay).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"CustomProcess", config.customProcName ? L"1" : L"0", path.c_str());
}
