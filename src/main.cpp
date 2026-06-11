#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <string>
#include "resource.h"
#include "config.h"
#include "injector.h"

// Link standard Windows libraries
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")

// Control & Timer IDs
#define IDC_BTN_INJECT       1001
#define IDC_BTN_HIDE         1002
#define IDC_BTN_SELECT       1003
#define IDC_EDIT_TARGET      1004
#define IDC_CHK_CUSTOM       1005
#define IDC_CHK_AUTO         1006
#define IDC_EDIT_DELAY       1007
#define IDC_EDIT_PATH        1008
#define IDC_STATUSBAR        1009

#define TIMER_AUTOINJECT     1
#define WM_TRAYICON          (WM_USER + 1)

// Tray Context Menu IDs
#define IDM_TRAY_OPEN        2001
#define IDM_TRAY_INJECT      2002
#define IDM_TRAY_CLOSE       2003

// Global handles
HWND g_btnInject = nullptr;
HWND g_btnHide = nullptr;
HWND g_btnSelect = nullptr;
HWND g_editTarget = nullptr;
HWND g_chkCustom = nullptr;
HWND g_chkAuto = nullptr;
HWND g_editDelay = nullptr;
HWND g_editPath = nullptr;
HWND g_statusBar = nullptr;
HFONT g_hFont = nullptr;

// State management
AppConfig g_config;
DWORD g_lastInjectedProcId = 0;
NOTIFYICONDATAW g_nid = { 0 };

#define Button_GetCheck(hwnd) SendMessageW(hwnd, BM_GETCHECK, 0, 0)
#define Button_SetCheck(hwnd, check) SendMessageW(hwnd, BM_SETCHECK, (WPARAM)(check), 0)

void UpdateControlsState(HWND hwnd) {
    BOOL autoChecked = Button_GetCheck(g_chkAuto) == BST_CHECKED;
    BOOL customChecked = Button_GetCheck(g_chkCustom) == BST_CHECKED;

    EnableWindow(g_chkCustom, !autoChecked);
    EnableWindow(g_editTarget, !autoChecked && customChecked);
    EnableWindow(g_btnSelect, !autoChecked);
    EnableWindow(g_editDelay, !autoChecked);
    EnableWindow(g_editPath, !autoChecked);
    EnableWindow(g_btnInject, !autoChecked);
}

void InitTrayIcon(HWND hwnd) {
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!g_nid.hIcon) {
        g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    wcscpy_s(g_nid.szTip, L"DLL Injector - Double-click to show");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

void HideToTray(HWND hwnd) {
    ShowWindow(hwnd, SW_HIDE);

    // Copy balloon notification details
    g_nid.uFlags |= NIF_INFO;
    wcscpy_s(g_nid.szInfoTitle, L"DLL Injector");
    wcscpy_s(g_nid.szInfo, L"DLL Injector is now running in your system tray.");
    g_nid.dwInfoFlags = NIIF_INFO;

    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void SelectDLL(HWND hwnd) {
    wchar_t szFile[MAX_PATH] = { 0 };
    if (!g_config.dllPath.empty()) {
        wcscpy_s(szFile, MAX_PATH, g_config.dllPath.c_str());
    }

    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Dynamic Link Library (*.dll)\0*.dll\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        g_config.dllPath = szFile;
        SetWindowTextW(g_editPath, szFile);
    }
}

void PerformManualInjection(HWND hwnd) {
    wchar_t szProcName[MAX_PATH];
    GetWindowTextW(g_editTarget, szProcName, MAX_PATH);
    g_config.procName = szProcName;

    wchar_t szPath[MAX_PATH];
    GetWindowTextW(g_editPath, szPath, MAX_PATH);
    g_config.dllPath = szPath;

    if (g_config.procName.empty()) {
        SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)L"Error: Process name cannot be empty!");
        return;
    }

    DWORD attribs = GetFileAttributesW(g_config.dllPath.c_str());
    if (attribs == INVALID_FILE_ATTRIBUTES || (attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)L"Error: Invalid DLL path!");
        return;
    }

    SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)L"Searching for process...");
    DWORD procId = GetProcId(g_config.procName);
    if (procId == 0) {
        std::wstring msg = L"Can't find process! | Name: " + g_config.procName;
        SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)msg.c_str());
        return;
    }

    SetAccessControl(g_config.dllPath);
    if (PerformInjection(procId, g_config.dllPath)) {
        std::wstring msg = L"Process found! | ID: " + std::to_wstring(procId) + L" | Injected!";
        SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)msg.c_str());
        ConfigManager::Save(g_config);
    } else {
        SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)L"Error: Injection failed!");
    }
}

void ToggleAutoInject(HWND hwnd) {
    BOOL autoChecked = Button_GetCheck(g_chkAuto) == BST_CHECKED;
    if (autoChecked) {
        wchar_t szDelay[16];
        GetWindowTextW(g_editDelay, szDelay, 16);
        g_config.delay = _wtoi(szDelay);
        if (g_config.delay < 1) {
            g_config.delay = 1;
            SetWindowTextW(g_editDelay, L"1");
        }

        wchar_t szProcName[MAX_PATH];
        GetWindowTextW(g_editTarget, szProcName, MAX_PATH);
        g_config.procName = szProcName;

        wchar_t szPath[MAX_PATH];
        GetWindowTextW(g_editPath, szPath, MAX_PATH);
        g_config.dllPath = szPath;

        g_config.customProcName = (Button_GetCheck(g_chkCustom) == BST_CHECKED);

        DWORD attribs = GetFileAttributesW(g_config.dllPath.c_str());
        if (attribs == INVALID_FILE_ATTRIBUTES || (attribs & FILE_ATTRIBUTE_DIRECTORY)) {
            SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)L"Error: Invalid DLL path!");
            Button_SetCheck(g_chkAuto, BST_UNCHECKED);
            return;
        }

        UpdateControlsState(hwnd);
        SetTimer(hwnd, TIMER_AUTOINJECT, g_config.delay * 1000, nullptr);

        std::wstring status = L"AutoInject: Enabled | polling every " + std::to_wstring(g_config.delay) + L"s";
        SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)status.c_str());
        ConfigManager::Save(g_config);
    } else {
        KillTimer(hwnd, TIMER_AUTOINJECT);
        g_lastInjectedProcId = 0;
        UpdateControlsState(hwnd);
        SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)L"AutoInject: Disabled");
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Retrieve system default UI/message font
            NONCLIENTMETRICSW ncm = { sizeof(NONCLIENTMETRICSW) };
            SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
            g_hFont = CreateFontIndirectW(&ncm.lfMessageFont);

            // Controls positioning:
            // Inject Button
            g_btnInject = CreateWindowExW(
                0, L"BUTTON", L"Inject",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                10, 10, 120, 35,
                hwnd, (HMENU)IDC_BTN_INJECT, ((LPCREATESTRUCTW)lParam)->hInstance, nullptr
            );

            // Hide Menu Button
            g_btnHide = CreateWindowExW(
                0, L"BUTTON", L"Hide Menu",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                10, 50, 120, 22,
                hwnd, (HMENU)IDC_BTN_HIDE, ((LPCREATESTRUCTW)lParam)->hInstance, nullptr
            );

            // Select DLL Button
            g_btnSelect = CreateWindowExW(
                0, L"BUTTON", L"Select",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                10, 77, 60, 22,
                hwnd, (HMENU)IDC_BTN_SELECT, ((LPCREATESTRUCTW)lParam)->hInstance, nullptr
            );

            // Target Process Name Edit
            g_editTarget = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"minecraft.windows.exe",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
                140, 10, 235, 22,
                hwnd, (HMENU)IDC_EDIT_TARGET, ((LPCREATESTRUCTW)lParam)->hInstance, nullptr
            );

            // Custom Target Checkbox
            g_chkCustom = CreateWindowExW(
                0, L"BUTTON", L"Custom Target",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                140, 35, 235, 20,
                hwnd, (HMENU)IDC_CHK_CUSTOM, ((LPCREATESTRUCTW)lParam)->hInstance, nullptr
            );

            // Auto Inject Checkbox
            g_chkAuto = CreateWindowExW(
                0, L"BUTTON", L"Auto Inject",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                140, 55, 110, 20,
                hwnd, (HMENU)IDC_CHK_AUTO, ((LPCREATESTRUCTW)lParam)->hInstance, nullptr
            );

            // Delay Edit
            g_editDelay = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"5",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | ES_NUMBER | ES_CENTER,
                250, 55, 30, 22,
                hwnd, (HMENU)IDC_EDIT_DELAY, ((LPCREATESTRUCTW)lParam)->hInstance, nullptr
            );
            SendMessageW(g_editDelay, EM_LIMITTEXT, 2, 0);

            // Delay Label
            HWND hDelayLabel = CreateWindowExW(
                0, L"STATIC", L"s delay",
                WS_VISIBLE | WS_CHILD,
                285, 57, 90, 20,
                hwnd, nullptr, ((LPCREATESTRUCTW)lParam)->hInstance, nullptr
            );

            // DLL Path Edit (Read-Only)
            g_editPath = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"Click \"Select\" to select the dll file",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_READONLY,
                75, 77, 300, 22,
                hwnd, (HMENU)IDC_EDIT_PATH, ((LPCREATESTRUCTW)lParam)->hInstance, nullptr
            );

            // Status bar
            g_statusBar = CreateWindowExW(
                0, STATUSCLASSNAMEW, nullptr,
                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                0, 0, 0, 0,
                hwnd, (HMENU)IDC_STATUSBAR, ((LPCREATESTRUCTW)lParam)->hInstance, nullptr
            );
            SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)L"Version 1.0 | DLL Injector");

            // Apply font to all controls
            EnumChildWindows(hwnd, [](HWND child, LPARAM lp) -> BOOL {
                SendMessageW(child, WM_SETFONT, (WPARAM)g_hFont, TRUE);
                return TRUE;
            }, 0);
            SendMessageW(hDelayLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            // Load and apply configurations
            g_config = ConfigManager::Load();

            if (g_config.customProcName) {
                Button_SetCheck(g_chkCustom, BST_CHECKED);
                EnableWindow(g_editTarget, TRUE);
                SetWindowTextW(g_editTarget, g_config.procName.c_str());
            } else {
                Button_SetCheck(g_chkCustom, BST_UNCHECKED);
                EnableWindow(g_editTarget, FALSE);
                SetWindowTextW(g_editTarget, L"minecraft.windows.exe");
            }

            SetWindowTextW(g_editDelay, std::to_wstring(g_config.delay).c_str());

            if (!g_config.dllPath.empty()) {
                SetWindowTextW(g_editPath, g_config.dllPath.c_str());
            }

            UpdateControlsState(hwnd);
            InitTrayIcon(hwnd);
            break;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case IDC_BTN_INJECT:
                case IDM_TRAY_INJECT:
                    PerformManualInjection(hwnd);
                    break;
                case IDC_BTN_HIDE:
                    HideToTray(hwnd);
                    break;
                case IDC_BTN_SELECT:
                    SelectDLL(hwnd);
                    break;
                case IDC_CHK_CUSTOM: {
                    BOOL customChecked = Button_GetCheck(g_chkCustom) == BST_CHECKED;
                    g_config.customProcName = customChecked;
                    if (customChecked) {
                        EnableWindow(g_editTarget, TRUE);
                    } else {
                        EnableWindow(g_editTarget, FALSE);
                        SetWindowTextW(g_editTarget, L"minecraft.windows.exe");
                    }
                    break;
                }
                case IDC_CHK_AUTO:
                    ToggleAutoInject(hwnd);
                    break;
                case IDM_TRAY_OPEN:
                    ShowWindow(hwnd, SW_SHOW);
                    SetForegroundWindow(hwnd);
                    break;
                case IDM_TRAY_CLOSE:
                    DestroyWindow(hwnd);
                    break;
            }
            break;
        }
        case WM_TIMER: {
            if (wParam == TIMER_AUTOINJECT) {
                DWORD procId = GetProcId(g_config.procName);
                if (procId == 0) {
                    SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)L"AutoInject: Can't find process!");
                    g_lastInjectedProcId = 0;
                } else if (procId == g_lastInjectedProcId) {
                    std::wstring msg = L"AutoInject: Already Injected! | ID: " + std::to_wstring(procId);
                    SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)msg.c_str());
                } else {
                    SetAccessControl(g_config.dllPath);
                    if (PerformInjection(procId, g_config.dllPath)) {
                        std::wstring msg = L"Process found! | ID: " + std::to_wstring(procId) + L" | Injected!";
                        SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)msg.c_str());
                        g_lastInjectedProcId = procId;
                        ConfigManager::Save(g_config);
                    } else {
                        SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)L"AutoInject: Injection failed!");
                    }
                }
            }
            break;
        }
        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
            } else if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();
                if (hMenu) {
                    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Open");
                    // Only enable tray inject option when not in auto-inject mode
                    UINT injectFlags = (Button_GetCheck(g_chkAuto) == BST_CHECKED) ? (MF_STRING | MF_GRAYED) : MF_STRING;
                    AppendMenuW(hMenu, injectFlags, IDM_TRAY_INJECT, L"Inject");
                    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_CLOSE, L"Exit");

                    SetForegroundWindow(hwnd);
                    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                    DestroyMenu(hMenu);
                }
            }
            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkMode(hdcStatic, TRANSPARENT);
            return (INT_PTR)GetSysColorBrush(COLOR_3DFACE);
        }
        case WM_SIZE: {
            SendMessageW(g_statusBar, WM_SIZE, 0, 0);
            break;
        }
        case WM_CLOSE: {
            DestroyWindow(hwnd);
            break;
        }
        case WM_DESTROY: {
            KillTimer(hwnd, TIMER_AUTOINJECT);
            RemoveTrayIcon();
            if (g_hFont) {
                DeleteObject(g_hFont);
            }
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize modern visual styles common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!wc.hIcon) {
        wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszClassName = L"DLLInjectorWindowClass";

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"Window Registration Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Adjust window size to fit client area exactly
    RECT rc = { 0, 0, 395, 130 }; // client width=395, height=130 (leaving room for statusbar)
    AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);

    HWND hwnd = CreateWindowExW(
        0,
        L"DLLInjectorWindowClass",
        L"DLL Injector",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) {
        MessageBoxW(nullptr, L"Window Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
