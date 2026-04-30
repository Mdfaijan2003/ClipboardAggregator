// clipboard_aggregator.cpp
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <regex>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

// -------------------- Config --------------------
constexpr wchar_t kClassName[] = L"ClipboardAggregatorWndClass";
constexpr wchar_t kAppName[] = L"Clipboard Aggregator";
constexpr wchar_t kRunValueName[] = L"ClipboardAggregator";

constexpr int HOTKEY_PASTE_ALL = 1; // Ctrl+Shift+V
constexpr size_t MAX_ITEMS = 12;
constexpr int EXPIRY_SECONDS = 3600;
constexpr UINT_PTR CLEANUP_TIMER_ID = 1001;
constexpr UINT CLEANUP_INTERVAL_MS = 60 * 1000;

constexpr UINT WM_TRAYICON = WM_APP + 1;

// Tray menu IDs
constexpr UINT ID_TRAY_PASTE_ALL = 2001;
constexpr UINT ID_TRAY_PAUSE = 2002;
constexpr UINT ID_TRAY_CLEAR = 2003;
constexpr UINT ID_TRAY_AUTOSTART = 2004;
constexpr UINT ID_TRAY_EXIT = 2005;

// -------------------- Data --------------------
struct ClipItem {
    std::wstring text;
    std::chrono::steady_clock::time_point ts;
};

struct AppState {
    std::vector<ClipItem> history;
    bool paused = false;
    bool internalClipboardWrite = false;
    bool autostartEnabled = false;
} g_state;

// -------------------- Utility --------------------
bool OpenClipboardRetry(HWND owner, int retries = 15, int delayMs = 20) {
    for (int i = 0; i < retries; ++i) {
        if (OpenClipboard(owner)) return true;
        Sleep(delayMs);
    }
    return false;
}

std::wstring GetClipboardText(HWND owner) {
    if (!OpenClipboardRetry(owner)) return L"";
    std::wstring out;

    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            auto* p = static_cast<wchar_t*>(GlobalLock(hData));
            if (p) {
                out = p;
                GlobalUnlock(hData);
            }
        }
    }
    CloseClipboard();
    return out;
}

bool SetClipboardText(HWND owner, const std::wstring& text) {
    if (!OpenClipboardRetry(owner)) return false;

    if (!EmptyClipboard()) {
        CloseClipboard();
        return false;
    }

    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem) {
        CloseClipboard();
        return false;
    }

    void* p = GlobalLock(hMem);
    if (!p) {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }
    memcpy(p, text.c_str(), bytes);
    GlobalUnlock(hMem);

    if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

void ReleaseModifiers() {
    INPUT mods[3] = {};
    mods[0].type = INPUT_KEYBOARD; mods[0].ki.wVk = VK_SHIFT;   mods[0].ki.dwFlags = KEYEVENTF_KEYUP;
    mods[1].type = INPUT_KEYBOARD; mods[1].ki.wVk = VK_MENU;    mods[1].ki.dwFlags = KEYEVENTF_KEYUP;
    mods[2].type = INPUT_KEYBOARD; mods[2].ki.wVk = VK_CONTROL; mods[2].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(3, mods, sizeof(INPUT));
}

void SimulatePaste() {
    ReleaseModifiers();
    Sleep(30);

    INPUT in[4] = {};
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_CONTROL;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = 'V';
    in[2].type = INPUT_KEYBOARD; in[2].ki.wVk = 'V'; in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    in[3].type = INPUT_KEYBOARD; in[3].ki.wVk = VK_CONTROL; in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
}

void CleanupExpired() {
    auto now = std::chrono::steady_clock::now();
    g_state.history.erase(
        std::remove_if(g_state.history.begin(), g_state.history.end(),
            [&](const ClipItem& it) {
                auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it.ts).count();
                return age > EXPIRY_SECONDS;
            }),
        g_state.history.end()
    );
}

std::wstring CombineHistory(const std::wstring& sep = L"\r\n") {
    // oldest -> newest
    std::wstring out;
    for (size_t i = 0; i < g_state.history.size(); ++i) {
        out += g_state.history[i].text;
        if (i + 1 < g_state.history.size()) out += sep;
    }
    return out;
}

// -------------------- Security filter --------------------
bool LooksSensitive(const std::wstring& t) {
    if (t.empty()) return false;

    // trim short circuit
    if (t.size() > 100000) return true; // avoid huge captures

    // basic password/token heuristics
    static const std::wregex rxToken(LR"((sk-[A-Za-z0-9]{10,}|AIza[0-9A-Za-z\-_]{20,}|eyJ[A-Za-z0-9_\-]{20,}\.[A-Za-z0-9_\-]{20,}\.[A-Za-z0-9_\-]{10,}))");
    static const std::wregex rxPwdPair(LR"((password\s*[:=]\s*\S+|pwd\s*[:=]\s*\S+|pass\s*[:=]\s*\S+))", std::regex::icase);
    static const std::wregex rxCardLike(LR"(\b(?:\d[ -]*?){13,19}\b)");

    if (std::regex_search(t, rxToken)) return true;
    if (std::regex_search(t, rxPwdPair)) return true;
    if (std::regex_search(t, rxCardLike)) return true;

    return false;
}

// -------------------- Autostart --------------------
std::wstring GetExePath() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return buf;
}

bool IsAutostartEnabled() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    wchar_t data[2048] = {};
    DWORD type = 0;
    DWORD dataSize = sizeof(data);
    LONG rc = RegQueryValueExW(hKey, kRunValueName, nullptr, &type, reinterpret_cast<LPBYTE>(data), &dataSize);
    RegCloseKey(hKey);

    if (rc != ERROR_SUCCESS || type != REG_SZ) return false;
    return wcslen(data) > 0;
}

bool SetAutostart(bool enable) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    bool ok = false;
    if (enable) {
        std::wstring exe = L"\"" + GetExePath() + L"\"";
        LONG rc = RegSetValueExW(hKey, kRunValueName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(exe.c_str()),
            static_cast<DWORD>((exe.size() + 1) * sizeof(wchar_t)));
        ok = (rc == ERROR_SUCCESS);
    }
    else {
        LONG rc = RegDeleteValueW(hKey, kRunValueName);
        ok = (rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND);
    }

    RegCloseKey(hKey);
    return ok;
}

// -------------------- Notifications + tray --------------------
void ShowInfoBalloon(HWND hwnd, const std::wstring& title, const std::wstring& msg) {
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, msg.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

bool InitTray(HWND hwnd) {
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcsncpy_s(nid.szTip, L"Clipboard Aggregator", _TRUNCATE);
    return Shell_NotifyIconW(NIM_ADD, &nid) == TRUE;
}

void RemoveTray(HWND hwnd) {
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void ShowTrayMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(menu, MF_STRING, ID_TRAY_PASTE_ALL, L"Paste all (Ctrl+Shift+V)");
    AppendMenuW(menu, MF_STRING, ID_TRAY_PAUSE, g_state.paused ? L"Resume capture" : L"Pause capture");
    AppendMenuW(menu, MF_STRING, ID_TRAY_CLEAR, L"Clear history");
    AppendMenuW(menu, MF_STRING | (g_state.autostartEnabled ? MF_CHECKED : 0), ID_TRAY_AUTOSTART, L"Auto-start with Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
}

// -------------------- Core operations --------------------
static void TryStoreClipboardItem(HWND hwnd) {
    if (g_state.paused) return;
    if (g_state.internalClipboardWrite) {
        g_state.internalClipboardWrite = false;
        return;
    }

    CleanupExpired();

    std::wstring text = GetClipboardText(hwnd);
    if (text.empty()) return;
    if (LooksSensitive(text)) return;

    // Skip exact consecutive duplicate (last captured)
    if (!g_state.history.empty() && g_state.history.back().text == text) return;

    g_state.history.push_back({ text, std::chrono::steady_clock::now() });
    if (g_state.history.size() > MAX_ITEMS) {
        g_state.history.erase(g_state.history.begin()); // remove oldest
    }
}

void PasteAllNow(HWND hwnd) {
    CleanupExpired();
    if (g_state.history.empty()) {
        ShowInfoBalloon(hwnd, kAppName, L"No items to paste.");
        return;
    }

    std::wstring combined = CombineHistory(L"\r\n");
    g_state.internalClipboardWrite = true;
    if (!SetClipboardText(hwnd, combined)) {
        g_state.internalClipboardWrite = false;
        ShowInfoBalloon(hwnd, kAppName, L"Failed to set clipboard.");
        return;
    }

    Sleep(80);
    SimulatePaste();

    std::wstring msg = L"Pasted " + std::to_wstring(g_state.history.size()) + L" item(s).";
    ShowInfoBalloon(hwnd, kAppName, msg);
}

// -------------------- Window proc --------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_state.autostartEnabled = IsAutostartEnabled();

        // Enable autostart on first run if not already set
        if (!g_state.autostartEnabled) {
            if (SetAutostart(true)) {
                g_state.autostartEnabled = true;
            }
        }

        AddClipboardFormatListener(hwnd);
        SetTimer(hwnd, CLEANUP_TIMER_ID, CLEANUP_INTERVAL_MS, nullptr);
        InitTray(hwnd);

        RegisterHotKey(hwnd, HOTKEY_PASTE_ALL, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'V');
        ShowInfoBalloon(hwnd, kAppName, L"Running in background.");
        return 0;
    }

    case WM_CLIPBOARDUPDATE:
        TryStoreClipboardItem(hwnd);
        return 0;

    case WM_TIMER:
        if (wParam == CLEANUP_TIMER_ID) CleanupExpired();
        return 0;

    case WM_HOTKEY:
        if (wParam == HOTKEY_PASTE_ALL) {
            PasteAllNow(hwnd);
        }
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            ShowTrayMenu(hwnd);
        }
        else if (lParam == WM_LBUTTONDBLCLK) {
            PasteAllNow(hwnd);
        }
        return 0;

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_TRAY_PASTE_ALL:
            PasteAllNow(hwnd);
            break;
        case ID_TRAY_PAUSE:
            g_state.paused = !g_state.paused;
            ShowInfoBalloon(hwnd, kAppName, g_state.paused ? L"Capture paused." : L"Capture resumed.");
            break;
        case ID_TRAY_CLEAR:
            g_state.history.clear();
            ShowInfoBalloon(hwnd, kAppName, L"History cleared.");
            break;
        case ID_TRAY_AUTOSTART: {
            bool target = !g_state.autostartEnabled;
            if (SetAutostart(target)) {
                g_state.autostartEnabled = target;
                ShowInfoBalloon(hwnd, kAppName, target ? L"Auto-start enabled." : L"Auto-start disabled.");
            }
            else {
                ShowInfoBalloon(hwnd, kAppName, L"Failed to change auto-start.");
            }
            break;
        }
        case ID_TRAY_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        return 0;
    }

    case WM_DESTROY:
        UnregisterHotKey(hwnd, HOTKEY_PASTE_ALL);
        KillTimer(hwnd, CLEANUP_TIMER_ID);
        RemoveClipboardFormatListener(hwnd);
        RemoveTray(hwnd);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// -------------------- Entry point (no console) --------------------
int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ PWSTR pCmdLine,
    _In_ int nCmdShow) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kClassName;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassW(&wc)) return 1;

    HWND hwnd = CreateWindowExW(
        0, kClassName, kAppName, 0,
        0, 0, 0, 0,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hwnd) return 1;

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}