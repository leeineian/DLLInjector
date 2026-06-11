#pragma once
#include <string>
#include <windows.h>

struct AppConfig {
    std::wstring procName = L"minecraft.windows.exe";
    std::wstring dllPath = L"";
    int delay = 5;
    bool customProcName = false;
};

class ConfigManager {
public:
    static AppConfig Load();
    static void Save(const AppConfig& config);
};
