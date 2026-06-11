#include "injector.hpp"
#include <tlhelp32.h>
#include <sddl.h>
#include <aclapi.h>

DWORD GetProcId(const std::wstring& procName) {
    DWORD procId = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, procName.c_str()) == 0) {
                    procId = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
    return procId;
}

bool SetAccessControl(const std::wstring& filePath) {
    PACL pOldDACL = nullptr, pNewDACL = nullptr;
    PSECURITY_DESCRIPTOR pSD = nullptr;
    EXPLICIT_ACCESSW ea;

    if (GetNamedSecurityInfoW(filePath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                              nullptr, nullptr, &pOldDACL, nullptr, &pSD) != ERROR_SUCCESS) {
        return false;
    }

    PSID pSid = nullptr;
    // ALL_APP_PACKAGES SID string: "S-1-15-2-1"
    if (!ConvertStringSidToSidW(L"S-1-15-2-1", &pSid)) {
        LocalFree(pSD);
        return false;
    }

    ZeroMemory(&ea, sizeof(EXPLICIT_ACCESSW));
    ea.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea.Trustee.ptstrName = (LPWCH)pSid;

    bool success = false;
    if (SetEntriesInAclW(1, &ea, pOldDACL, &pNewDACL) == ERROR_SUCCESS) {
        if (SetNamedSecurityInfoW((LPWCH)filePath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                  nullptr, nullptr, pNewDACL, nullptr) == ERROR_SUCCESS) {
            success = true;
        }
        LocalFree(pNewDACL);
    }

    FreeSid(pSid);
    LocalFree(pSD);
    return success;
}

bool PerformInjection(DWORD procId, const std::wstring& dllPath) {
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);
    if (!hProc) return false;

    size_t size = (dllPath.size() + 1) * sizeof(wchar_t);
    void* loc = VirtualAllocEx(hProc, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!loc) {
        CloseHandle(hProc);
        return false;
    }

    if (!WriteProcessMemory(hProc, loc, dllPath.c_str(), size, nullptr)) {
        VirtualFreeEx(hProc, loc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0,
                                        (LPTHREAD_START_ROUTINE)LoadLibraryW, loc, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProc, loc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    VirtualFreeEx(hProc, loc, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProc);
    return true;
}
