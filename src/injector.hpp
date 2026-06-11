#pragma once
#include <string>
#include <windows.h>

DWORD GetProcId(const std::wstring& procName);
bool SetAccessControl(const std::wstring& filePath);
bool PerformInjection(DWORD procId, const std::wstring& dllPath);
