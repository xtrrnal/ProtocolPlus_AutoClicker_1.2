#define UNICODE
#define _UNICODE

#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <cwchar>
#include <algorithm>
#include <vector>

static HWND g_mainWindow = nullptr;
static HWND g_cpsEdit = nullptr;
static HWND g_offsetEdit = nullptr;
static HWND g_buttonCombo = nullptr;
static HWND g_hotkeyText = nullptr;
static HWND g_status = nullptr;
static HWND g_toggle = nullptr;
static HWND g_changeHotkey = nullptr;

static std::atomic<bool> g_running(false);
static std::atomic<bool> g_stop(false);
static HBRUSH g_bgBrush = nullptr;
static HBRUSH g_panelBrush = nullptr;
static HBRUSH g_statusBrush = nullptr;
static HHOOK g_keyboardHook = nullptr;
static bool g_capturingHotkey = false;

static UINT g_hotkeyVK = VK_F6;
static UINT g_hotkeyMods = 0;

static constexpr int HOTKEY_ID = 1;
static constexpr int BUTTON_START = 1001;
static constexpr int BUTTON_CHANGE_HOTKEY = 1002;

static constexpr COLORREF ICE_BG = RGB(220, 242, 255);
static constexpr COLORREF ICE_PANEL = RGB(241, 250, 255);
static constexpr COLORREF ICE_BLUE = RGB(75, 174, 235);
static constexpr COLORREF ICE_BLUE_DARK = RGB(34, 116, 180);
static constexpr COLORREF ICE_BORDER = RGB(155, 211, 242);
static constexpr COLORREF ICE_STATUS = RGB(211, 238, 253);
static constexpr COLORREF WHITE = RGB(255, 255, 255);
static constexpr COLORREF TEXT = RGB(32, 69, 95);

static const wchar_t* APP_NAME = L"Protocol+ Auto Clicker";

static double GetCPS(HWND edit) {
    wchar_t buffer[64]{};
    GetWindowTextW(edit, buffer, 64);
    return _wtof(buffer);
}

static int GetOffset() {
    wchar_t buffer[64]{};
    GetWindowTextW(g_offsetEdit, buffer, 64);
    int value = _wtoi(buffer);
    if (value < 0) value = 0;
    if (value > 999) value = 999;
    return value;
}

static void SetStatus(const std::wstring& text) {
    if (g_status) {
        SetWindowTextW(g_status, text.c_str());
        InvalidateRect(g_status, nullptr, TRUE);
    }
}

static void ClickLoop(double baseCps, int offset, DWORD downFlag, DWORD upFlag) {
    baseCps = std::clamp(baseCps, 1.0, 1000.0);
    offset = std::max(0, offset);

    std::random_device rd;
    std::mt19937 rng(rd());

    while (!g_stop.load()) {
        double low = std::max(1.0, baseCps - static_cast<double>(offset));
        double high = std::min(1000.0, baseCps + static_cast<double>(offset));

        std::uniform_real_distribution<double> cpsDist(low, high);
        double currentCps = cpsDist(rng);
        currentCps = std::clamp(currentCps, 1.0, 1000.0);

        INPUT inputs[2]{};
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = downFlag;
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = upFlag;
        SendInput(2, inputs, sizeof(INPUT));

        double seconds = 1.0 / currentCps;
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(seconds)
        );

        if (duration.count() < 1)
            duration = std::chrono::milliseconds(1);

        std::this_thread::sleep_for(duration);
    }
}

static void StopClicker() {
    if (!g_running.load()) return;
    g_stop.store(true);
    g_running.store(false);
    SetStatus(L"Stopped");
    if (g_toggle) SetWindowTextW(g_toggle, L"Start");
}

static void ToggleClicker() {
    if (g_running.load()) {
        StopClicker();
        return;
    }

    double cps = GetCPS(g_cpsEdit);
    int offset = GetOffset();

    if (cps < 1.0 || cps > 1000.0) {
        MessageBoxW(g_mainWindow, L"Main CPS must be between 1 and 1000.", APP_NAME, MB_ICONWARNING);
        return;
    }

    int button = static_cast<int>(SendMessageW(g_buttonCombo, CB_GETCURSEL, 0, 0));
    DWORD downFlag = MOUSEEVENTF_LEFTDOWN;
    DWORD upFlag = MOUSEEVENTF_LEFTUP;

    if (button == 1) {
        downFlag = MOUSEEVENTF_RIGHTDOWN;
        upFlag = MOUSEEVENTF_RIGHTUP;
    } else if (button == 2) {
        downFlag = MOUSEEVENTF_MIDDLEDOWN;
        upFlag = MOUSEEVENTF_MIDDLEUP;
    }

    g_stop.store(false);
    g_running.store(true);

    wchar_t status[128]{};
    if (offset > 0) {
        swprintf_s(status, L"Running - %.0f CPS ± %d", cps, offset);
    } else {
        swprintf_s(status, L"Running - %.0f CPS", cps);
    }

    SetStatus(status);
    SetWindowTextW(g_toggle, L"Stop");

    std::thread(ClickLoop, cps, offset, downFlag, upFlag).detach();
}

static bool IsModifierVK(UINT vk) {
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
           vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
           vk == VK_LWIN || vk == VK_RWIN;
}

static UINT CurrentModifierMask() {
    UINT mods = 0;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
    if (GetAsyncKeyState(VK_MENU) & 0x8000) mods |= MOD_ALT;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;
    if ((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000)) mods |= MOD_WIN;
    return mods;
}

static std::wstring KeyName(UINT vk, UINT mods) {
    std::wstring result;
    if (mods & MOD_CONTROL) result += L"Ctrl+";
    if (mods & MOD_ALT) result += L"Alt+";
    if (mods & MOD_SHIFT) result += L"Shift+";
    if (mods & MOD_WIN) result += L"Win+";

    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LONG keyInfo = static_cast<LONG>(scanCode << 16);

    if (vk == VK_LEFT || vk == VK_UP || vk == VK_RIGHT || vk == VK_DOWN ||
        vk == VK_PRIOR || vk == VK_NEXT || vk == VK_END || vk == VK_HOME ||
        vk == VK_INSERT || vk == VK_DELETE) {
        keyInfo |= (1 << 24);
    }

    wchar_t name[64]{};
    if (GetKeyNameTextW(keyInfo, name, 64) > 0)
        result += name;
    else
        result += L"Key " + std::to_wstring(vk);

    return result;
}

static bool RegisterCurrentHotkey() {
    if (!g_mainWindow) return false;
    return RegisterHotKey(g_mainWindow, HOTKEY_ID, g_hotkeyMods, g_hotkeyVK) != FALSE;
}

static void UpdateHotkeyLabel() {
    if (g_hotkeyText)
        SetWindowTextW(g_hotkeyText, KeyName(g_hotkeyVK, g_hotkeyMods).c_str());
}

static void EndHotkeyCapture() {
    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }
    g_capturingHotkey = false;
    if (g_changeHotkey) SetWindowTextW(g_changeHotkey, L"Change");
    if (!g_running.load()) SetStatus(L"Stopped");
}

static LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_capturingHotkey &&
        (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        const KBDLLHOOKSTRUCT* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        UINT vk = key->vkCode;

        if (!IsModifierVK(vk)) {
            UINT mods = CurrentModifierMask();
            UINT oldVK = g_hotkeyVK;
            UINT oldMods = g_hotkeyMods;

            UnregisterHotKey(g_mainWindow, HOTKEY_ID);
            g_hotkeyVK = vk;
            g_hotkeyMods = mods;

            if (!RegisterCurrentHotkey()) {
                g_hotkeyVK = oldVK;
                g_hotkeyMods = oldMods;
                RegisterCurrentHotkey();
                MessageBoxW(g_mainWindow,
                    L"That key combination is already in use by Windows or another application.\n\nChoose a different key.",
                    APP_NAME, MB_ICONWARNING);
            } else {
                UpdateHotkeyLabel();
            }

            EndHotkeyCapture();
            return 1;
        }
    }

    return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
}

static void BeginHotkeyCapture() {
    if (g_running.load()) StopClicker();

    UnregisterHotKey(g_mainWindow, HOTKEY_ID);
    g_capturingHotkey = true;
    SetWindowTextW(g_changeHotkey, L"Press a key...");
    SetStatus(L"Press any key to bind the toggle hotkey");

    g_keyboardHook = SetWindowsHookExW(
        WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);

    if (!g_keyboardHook) {
        g_capturingHotkey = false;
        RegisterCurrentHotkey();
        SetWindowTextW(g_changeHotkey, L"Change");
        SetStatus(L"Stopped");
        MessageBoxW(g_mainWindow, L"Windows could not start hotkey capture.", APP_NAME, MB_ICONERROR);
    }
}

static void ApplyFont(HWND control, HFONT font) {
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

static void PaintGlassButton(const DRAWITEMSTRUCT* dis) {
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;

    COLORREF fill = disabled ? RGB(170, 195, 210) :
                    pressed ? ICE_BLUE_DARK : ICE_BLUE;

    HBRUSH brush = CreateSolidBrush(fill);
    FillRect(dc, &rc, brush);
    DeleteObject(brush);

    HPEN pen = CreatePen(PS_SOLID, 1, ICE_BORDER);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, WHITE);

    wchar_t text[64]{};
    GetWindowTextW(dis->hwndItem, text, 64);
    DrawTextW(dc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_mainWindow = hwnd;

        g_bgBrush = CreateSolidBrush(ICE_BG);
        g_panelBrush = CreateSolidBrush(ICE_PANEL);
        g_statusBrush = CreateSolidBrush(ICE_STATUS);

        HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        HWND title = CreateWindowW(L"STATIC", L"Protocol+ Auto Clicker",
            WS_CHILD | WS_VISIBLE, 24, 16, 360, 32, hwnd, nullptr, nullptr, nullptr);
        ApplyFont(title, font);

        HWND subtitle = CreateWindowW(L"STATIC", L"Ice Blue Glass Edition",
            WS_CHILD | WS_VISIBLE, 24, 43, 360, 24, hwnd, nullptr, nullptr, nullptr);
        ApplyFont(subtitle, font);

        HWND cpsLabel = CreateWindowW(L"STATIC", L"Main CPS:",
            WS_CHILD | WS_VISIBLE, 24, 82, 200, 24, hwnd, nullptr, nullptr, nullptr);
        ApplyFont(cpsLabel, font);

        g_cpsEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"13",
            WS_CHILD | WS_VISIBLE | ES_NUMBER, 270, 78, 100, 28,
            hwnd, nullptr, nullptr, nullptr);
        ApplyFont(g_cpsEdit, font);

        HWND offsetLabel = CreateWindowW(L"STATIC", L"Random Offset:",
            WS_CHILD | WS_VISIBLE, 24, 118, 200, 24, hwnd, nullptr, nullptr, nullptr);
        ApplyFont(offsetLabel, font);

        g_offsetEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"12",
            WS_CHILD | WS_VISIBLE | ES_NUMBER, 270, 114, 100, 28,
            hwnd, nullptr, nullptr, nullptr);
        ApplyFont(g_offsetEdit, font);

        HWND offsetHelp = CreateWindowW(L"STATIC", L"Random range: Main CPS ± Offset",
            WS_CHILD | WS_VISIBLE, 24, 145, 230, 20, hwnd, nullptr, nullptr, nullptr);
        ApplyFont(offsetHelp, font);

        HWND mouseLabel = CreateWindowW(L"STATIC", L"Mouse Button:",
            WS_CHILD | WS_VISIBLE, 24, 177, 200, 24, hwnd, nullptr, nullptr, nullptr);
        ApplyFont(mouseLabel, font);

        g_buttonCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 270, 173, 100, 120,
            hwnd, nullptr, nullptr, nullptr);
        ApplyFont(g_buttonCombo, font);

        const wchar_t* buttons[] = { L"Left", L"Right", L"Middle" };
        for (const wchar_t* item : buttons)
            SendMessageW(g_buttonCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        SendMessageW(g_buttonCombo, CB_SETCURSEL, 0, 0);

        HWND hotkeyLabel = CreateWindowW(L"STATIC", L"Toggle Hotkey:",
            WS_CHILD | WS_VISIBLE, 24, 213, 200, 24, hwnd, nullptr, nullptr, nullptr);
        ApplyFont(hotkeyLabel, font);

        g_hotkeyText = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"F6",
            WS_CHILD | WS_VISIBLE | SS_CENTER, 270, 209, 100, 28,
            hwnd, nullptr, nullptr, nullptr);
        ApplyFont(g_hotkeyText, font);

        g_changeHotkey = CreateWindowW(L"BUTTON", L"Change",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 270, 243, 100, 28,
            hwnd, reinterpret_cast<HMENU>(BUTTON_CHANGE_HOTKEY), nullptr, nullptr);
        ApplyFont(g_changeHotkey, font);

        g_status = CreateWindowW(L"STATIC", L"Stopped",
            WS_CHILD | WS_VISIBLE | SS_CENTER, 24, 282, 346, 30,
            hwnd, nullptr, nullptr, nullptr);
        ApplyFont(g_status, font);

        g_toggle = CreateWindowW(L"BUTTON", L"Start",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 24, 322, 346, 44,
            hwnd, reinterpret_cast<HMENU>(BUTTON_START), nullptr, nullptr);
        ApplyFont(g_toggle, font);

        HWND help = CreateWindowW(L"STATIC",
            L"F6 is the default. Use Change to create your own hotkey.",
            WS_CHILD | WS_VISIBLE | SS_CENTER, 24, 375, 346, 24,
            hwnd, nullptr, nullptr, nullptr);
        ApplyFont(help, font);

        g_hotkeyVK = VK_F6;
        g_hotkeyMods = 0;

        if (!RegisterCurrentHotkey()) {
            MessageBoxW(hwnd,
                L"F6 is already in use. Click Change to select another hotkey.",
                APP_NAME, MB_ICONWARNING);
        }

        UpdateHotkeyLabel();
        return 0;
    }

    case WM_COMMAND: {
        int controlID = LOWORD(wParam);
        int notification = HIWORD(wParam);

        if (controlID == BUTTON_START && notification == BN_CLICKED) {
            ToggleClicker();
            return 0;
        }

        if (controlID == BUTTON_CHANGE_HOTKEY && notification == BN_CLICKED) {
            if (g_capturingHotkey) EndHotkeyCapture();
            else BeginHotkeyCapture();
            return 0;
        }
        break;
    }

    case WM_DRAWITEM:
        if (wParam == BUTTON_START) {
            PaintGlassButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
            return TRUE;
        }
        break;

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        SetBkMode(dc, TRANSPARENT);

        if (control == g_status) {
            SetTextColor(dc, ICE_BLUE_DARK);
            return reinterpret_cast<LRESULT>(g_statusBrush);
        }

        SetTextColor(dc, TEXT);
        return reinterpret_cast<LRESULT>(g_bgBrush);
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, TEXT);
        SetBkColor(dc, WHITE);
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }

    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, TEXT);
        SetBkColor(dc, WHITE);
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }

    case WM_HOTKEY:
        if (wParam == HOTKEY_ID && !g_capturingHotkey)
            ToggleClicker();
        return 0;

    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, g_bgBrush);
        return 1;
    }

    case WM_DESTROY:
        if (g_keyboardHook) {
            UnhookWindowsHookEx(g_keyboardHook);
            g_keyboardHook = nullptr;
        }

        if (g_mainWindow)
            UnregisterHotKey(g_mainWindow, HOTKEY_ID);

        g_stop.store(true);
        g_running.store(false);

        if (g_bgBrush) DeleteObject(g_bgBrush);
        if (g_panelBrush) DeleteObject(g_panelBrush);
        if (g_statusBrush) DeleteObject(g_statusBrush);

        g_bgBrush = nullptr;
        g_panelBrush = nullptr;
        g_statusBrush = nullptr;

        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = instance;
    wc.lpszClassName = L"ProtocolPlusAutoClicker";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc))
        return 1;

    HWND window = CreateWindowW(
        wc.lpszClassName,
        APP_NAME,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        410, 450,
        nullptr, nullptr, instance, nullptr
    );

    if (!window)
        return 1;

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
