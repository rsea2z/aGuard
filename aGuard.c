/* aGuard — tiny tray keep-alive tool.
 *
 * Periodically reads GetLastInputInfo(); when the system-wide keyboard/mouse
 * idle time exceeds a configurable threshold, it emulates a single F15
 * keypress (ignored by virtually all applications, same approach as
 * Caffeine) to reset the idle timer.
 *
 * Motivation: some VPN / zero-trust clients log you out after an idle
 * timeout based on system-wide input idle time (GetLastInputInfo); plain
 * network activity (ssh ping) does not reset that timer.
 *
 * Independent project; not affiliated with or endorsed by Sangfor.
 *
 * Build:  echo 1 ICON "app.ico" | windres -O coff -o app.res.o
 *         gcc -O2 -s -mwindows -municode -static -o aGuard.exe aGuard.c app.res.o -lshell32
 * Usage:  aGuard.exe [--print-idle]
 *         --print-idle prints current idle seconds to stdout and exits.
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>

#define WM_TRAYICON      (WM_APP + 1)
#define ID_TIMER         1
#define IDM_EXIT         2000
#define IDM_THRESHOLD    2100   /* IDM_THRESHOLD + preset index */

/* Threshold presets (minutes). 0 disables the keep-alive. */
static const struct { UINT minutes; const wchar_t *label; } kPresets[] = {
    {  0, L"禁用（0）" },
    { 15, L"15 分钟后保活" },
    { 25, L"25 分钟后保活" },
    { 55, L"55 分钟后保活" },
};
#define NPRESETS (sizeof(kPresets) / sizeof(kPresets[0]))
#define DEFAULT_THRESHOLD_MIN 25

static HWND g_hwnd;
static NOTIFYICONDATAW g_nid;
static UINT g_threshold_min = DEFAULT_THRESHOLD_MIN;
static UINT g_check_seconds = 60;
static wchar_t g_ini[MAX_PATH];
static UINT g_taskbar_created;

static void IniPath(void)
{
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    wchar_t *slash = wcsrchr(exe, L'\\');
    if (slash) *(slash + 1) = L'\0';
    lstrcpyW(g_ini, exe);
    lstrcpyW(g_ini + lstrlenW(g_ini), L"aGuard.ini");
}

static void LoadSettings(void)
{
    g_threshold_min = GetPrivateProfileIntW(L"aGuard", L"threshold_minutes",
                                            DEFAULT_THRESHOLD_MIN, g_ini);
    UINT chk = GetPrivateProfileIntW(L"aGuard", L"check_seconds", 60, g_ini);
    if (chk >= 5 && chk <= 3600) g_check_seconds = chk;
}

static void SaveSettings(void)
{
    wchar_t val[16];
    wsprintfW(val, L"%u", g_threshold_min);
    WritePrivateProfileStringW(L"aGuard", L"threshold_minutes", val, g_ini);
}

static HICON LoadAppIcon(void)
{
    /* embedded multi-size icon (app.ico resource, id 1) */
    return (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1),
                             IMAGE_ICON,
                             GetSystemMetrics(SM_CXSMICON),
                             GetSystemMetrics(SM_CYSMICON),
                             LR_DEFAULTCOLOR);
}

static void UpdateTooltip(void)
{
    wchar_t tip[128];
    if (g_threshold_min)
        wsprintfW(tip, L"aGuard：空闲超过 %u 分钟自动保活", g_threshold_min);
    else
        lstrcpyW(tip, L"aGuard：已禁用");
    lstrcpynW(g_nid.szTip, tip, ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void AddTrayIcon(void)
{
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
    UpdateTooltip();
}

static void SimulateF15(void)
{
    INPUT inp[2] = {0};
    inp[0].type = INPUT_KEYBOARD;
    inp[0].ki.wVk = VK_F15;                 /* 0x7E, ignored by apps */
    inp[1].type = INPUT_KEYBOARD;
    inp[1].ki.wVk = VK_F15;
    inp[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inp, sizeof(INPUT));
}

static void CheckAndKeepAlive(void)
{
    if (g_threshold_min == 0) return;
    LASTINPUTINFO li;
    li.cbSize = sizeof(li);
    if (!GetLastInputInfo(&li)) return;
    DWORD idle_ms = GetTickCount() - li.dwTime;   /* unsigned: wrap-safe */
    if (idle_ms >= (ULONGLONG)g_threshold_min * 60000)   /* no overflow */
        SimulateF15();
}

static void ShowContextMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    wchar_t status[96];
    if (g_threshold_min)
        wsprintfW(status, L"当前：空闲超过 %u 分钟自动保活", g_threshold_min);
    else
        lstrcpyW(status, L"当前：已禁用");
    AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, status);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    for (UINT i = 0; i < NPRESETS; i++) {
        UINT flags = MF_STRING;
        if (kPresets[i].minutes == g_threshold_min) flags |= MF_CHECKED;
        AppendMenuW(menu, flags, IDM_THRESHOLD + i, kPresets[i].label);
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"退出");
    SetForegroundWindow(hwnd);  /* so the menu dismisses on outside click */
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(menu);
}

static void PickThreshold(UINT index)
{
    if (index >= NPRESETS) return;
    g_threshold_min = kPresets[index].minutes;
    SaveSettings();
    UpdateTooltip();
    wchar_t msg[96];
    if (g_threshold_min)
        wsprintfW(msg, L"空闲超过 %u 分钟时自动模拟 F15 保活", g_threshold_min);
    else
        lstrcpyW(msg, L"已禁用保活");
    lstrcpynW(g_nid.szInfo, msg, ARRAYSIZE(g_nid.szInfo));
    lstrcpynW(g_nid.szInfoTitle, L"aGuard", ARRAYSIZE(g_nid.szInfoTitle));
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.szInfo[0] = L'\0';  /* balloon only once */
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == g_taskbar_created && g_taskbar_created) {
        AddTrayIcon();
        return 0;
    }
    switch (msg) {
    case WM_TIMER:
        if (wp == ID_TIMER) CheckAndKeepAlive();
        return 0;
    case WM_TRAYICON:
        /* NOTIFYICON_VERSION_4: LOWORD(lp) = event, HIWORD(lp) = icon id */
        if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU)
            ShowContextMenu(hwnd);
        return 0;
    case WM_COMMAND:
        if (wp == IDM_EXIT) { DestroyWindow(hwnd); return 0; }
        if (wp >= IDM_THRESHOLD && wp < IDM_THRESHOLD + NPRESETS) {
            PickThreshold((UINT)(wp - IDM_THRESHOLD));
            return 0;
        }
        break;
    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE prev, PWSTR cmdline, int show)
{
    (void)prev; (void)show; (void)cmdline;

    /* --print-idle: print current idle seconds and exit (for testing). */
    {
        int argc = 0;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv && argc >= 2 && !lstrcmpW(argv[1], L"--print-idle")) {
            LocalFree(argv);
            LASTINPUTINFO li;
            li.cbSize = sizeof(li);
            DWORD idle = GetLastInputInfo(&li) ? (GetTickCount() - li.dwTime) / 1000 : 0;
            char line[48];
            int len = wsprintfA(line, "idle_seconds=%lu\r\n", idle);
            /* GUI-subsystem exe: stdout is only valid when redirected;
               otherwise attach to the parent console. */
            HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
            if ((!out || out == INVALID_HANDLE_VALUE) && AttachConsole(ATTACH_PARENT_PROCESS))
                out = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE,
                                  NULL, OPEN_EXISTING, 0, NULL);
            DWORD written;
            WriteFile(out, line, (DWORD)len, &written, NULL);
            return 0;
        }
        if (argv) LocalFree(argv);
    }

    HANDLE mutex = CreateMutexW(NULL, TRUE, L"aGuard_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        MessageBoxW(NULL, L"aGuard 已在运行。", L"aGuard", MB_ICONINFORMATION);
        return 0;
    }

    IniPath();
    LoadSettings();
    g_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"aGuard";
    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"aGuard", 0,
                             0, 0, 0, 0, HWND_MESSAGE, NULL, hInst, NULL);

    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;  /* v4 hides tip without SHOWTIP */
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadAppIcon();
    if (!g_nid.hIcon) g_nid.hIcon = LoadIconW(NULL, IDI_APPLICATION);

    AddTrayIcon();
    lstrcpynW(g_nid.szInfo,
              g_threshold_min
                  ? L"已启动：空闲超过阈值时自动模拟 F15 按键保活"
                  : L"已启动（当前为禁用状态，右键图标可开启）",
              ARRAYSIZE(g_nid.szInfo));
    lstrcpynW(g_nid.szInfoTitle, L"aGuard", ARRAYSIZE(g_nid.szInfoTitle));
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.szInfo[0] = L'\0';

    SetTimer(g_hwnd, ID_TIMER, g_check_seconds * 1000UL, NULL);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    CloseHandle(mutex);
    return 0;
}
