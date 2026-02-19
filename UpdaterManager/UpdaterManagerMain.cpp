#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>

struct CONFIG_DATA
{
    int Port;
    char ClientRoot[MAX_PATH];
    char AdminToken[128];
    char Version[64];
};

static HWND g_hWnd = NULL;
static HWND g_hEditClientRoot = NULL;
static HWND g_hEditPort = NULL;
static HWND g_hEditToken = NULL;
static HWND g_hEditVersion = NULL;
static HWND g_hStatus = NULL;
static HWND g_hLog = NULL;
static PROCESS_INFORMATION g_ServiceProcess = { 0 };

static void JsonEscapeString(const char* src, char* dst, size_t dstSize);

static void TrimTrailingNewlines(char* s)
{
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n' || s[len - 1] == ' ' || s[len - 1] == '\t'))
    {
        s[len - 1] = 0;
        len--;
    }
}

static bool ReadAllText(const char* path, char* buffer, size_t bufferSize)
{
    buffer[0] = 0;
    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") != 0 || !f)
    {
        return false;
    }
    size_t read = fread(buffer, 1, bufferSize - 1, f);
    fclose(f);
    buffer[read] = 0;
    return true;
}

static bool WriteAllText(const char* path, const char* text)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path, "wb") != 0 || !f)
    {
        return false;
    }
    size_t len = strlen(text);
    size_t written = fwrite(text, 1, len, f);
    fclose(f);
    return written == len;
}

static const char* FindKey(const char* json, const char* key)
{
    char pattern[64];
    sprintf_s(pattern, "\"%s\"", key);
    return strstr(json, pattern);
}

static bool ParseStringField(const char* json, const char* key, char* out, size_t outSize)
{
    const char* p = FindKey(json, key);
    if (!p)
    {
        out[0] = 0;
        return false;
    }
    p = strchr(p, ':');
    if (!p)
    {
        out[0] = 0;
        return false;
    }
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
    {
        p++;
    }
    if (*p != '\"')
    {
        out[0] = 0;
        return false;
    }
    p++;
    const char* start = p;
    while (*p && *p != '\"')
    {
        p++;
    }
    size_t len = (size_t)(p - start);
    if (len + 1 > outSize)
    {
        out[0] = 0;
        return false;
    }
    memcpy(out, start, len);
    out[len] = 0;
    return true;
}

static bool ParseIntField(const char* json, const char* key, int* out)
{
    const char* p = FindKey(json, key);
    if (!p)
    {
        return false;
    }
    p = strchr(p, ':');
    if (!p)
    {
        return false;
    }
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
    {
        p++;
    }
    bool neg = false;
    if (*p == '-')
    {
        neg = true;
        p++;
    }
    if (*p < '0' || *p > '9')
    {
        return false;
    }
    int v = 0;
    while (*p >= '0' && *p <= '9')
    {
        v = v * 10 + (*p - '0');
        p++;
    }
    if (neg)
    {
        v = -v;
    }
    *out = v;
    return true;
}

static bool LoadConfig(CONFIG_DATA* cfg, const char* configPath)
{
    char buffer[4096];
    if (!ReadAllText(configPath, buffer, sizeof(buffer)))
    {
        cfg->Port = 4000;
        cfg->ClientRoot[0] = 0;
        cfg->AdminToken[0] = 0;
        cfg->Version[0] = 0;
        return false;
    }

    int port = 0;
    if (!ParseIntField(buffer, "port", &port))
    {
        port = 4000;
    }
    cfg->Port = port;
    ParseStringField(buffer, "clientRoot", cfg->ClientRoot, sizeof(cfg->ClientRoot));
    ParseStringField(buffer, "adminToken", cfg->AdminToken, sizeof(cfg->AdminToken));
    ParseStringField(buffer, "version", cfg->Version, sizeof(cfg->Version));
    return true;
}

static bool SaveConfig(const CONFIG_DATA* cfg, const char* configPath)
{
    char buffer[4096];
    char clientEsc[MAX_PATH * 2];
    char tokenEsc[256];
    char versionEsc[128];
    JsonEscapeString(cfg->ClientRoot, clientEsc, sizeof(clientEsc));
    JsonEscapeString(cfg->AdminToken, tokenEsc, sizeof(tokenEsc));
    JsonEscapeString(cfg->Version, versionEsc, sizeof(versionEsc));
    sprintf_s(
        buffer,
        "{\n"
        "  \"port\": %d,\n"
        "  \"clientRoot\": \"%s\",\n"
        "  \"adminToken\": \"%s\",\n"
        "  \"version\": \"%s\"\n"
        "}\n",
        cfg->Port,
        clientEsc,
        tokenEsc,
        versionEsc
    );
    return WriteAllText(configPath, buffer);
}

static void GenerateRandomToken(char* out, size_t outSize)
{
    if (outSize < 33)
    {
        if (outSize > 0) out[0] = 0;
        return;
    }
    HCRYPTPROV hProv = 0;
    BYTE bytes[16];
    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        if (CryptGenRandom(hProv, sizeof(bytes), bytes))
        {
            static const char hex[] = "0123456789abcdef";
            for (size_t i = 0; i < sizeof(bytes); i++)
            {
                out[i * 2] = hex[bytes[i] >> 4];
                out[i * 2 + 1] = hex[bytes[i] & 0x0F];
            }
            out[32] = 0;
            CryptReleaseContext(hProv, 0);
            return;
        }
        CryptReleaseContext(hProv, 0);
    }
    DWORD t = GetTickCount();
    sprintf_s(out, outSize, "%08X%08X", t, t ^ 0xA5C3F18Du);
}

static void AddLogLine(const char* text)
{
    if (!g_hLog)
    {
        return;
    }
    int len = GetWindowTextLengthA(g_hLog);
    SendMessageA(g_hLog, EM_SETSEL, len, len);
    SendMessageA(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageA(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
}

static void SetStatus(const char* text)
{
    if (g_hStatus)
    {
        SetWindowTextA(g_hStatus, text);
    }
    AddLogLine(text);
}

static void JsonEscapeString(const char* src, char* dst, size_t dstSize)
{
    if (dstSize == 0)
    {
        return;
    }
    size_t j = 0;
    for (size_t i = 0; src[i] != 0; i++)
    {
        char c = src[i];
        if (c == '\\' || c == '\"')
        {
            if (j + 2 >= dstSize)
            {
                break;
            }
            dst[j++] = '\\';
            dst[j++] = c;
        }
        else if (c == '\n')
        {
            if (j + 2 >= dstSize)
            {
                break;
            }
            dst[j++] = '\\';
            dst[j++] = 'n';
        }
        else if (c == '\r')
        {
            if (j + 2 >= dstSize)
            {
                break;
            }
            dst[j++] = '\\';
            dst[j++] = 'r';
        }
        else if (c == '\t')
        {
            if (j + 2 >= dstSize)
            {
                break;
            }
            dst[j++] = '\\';
            dst[j++] = 't';
        }
        else
        {
            if (j + 1 >= dstSize)
            {
                break;
            }
            dst[j++] = c;
        }
    }
    dst[j] = 0;
}

static void GetExeDirectory(char* outPath, size_t outSize)
{
    if (outSize == 0)
    {
        return;
    }
    DWORD len = GetModuleFileNameA(NULL, outPath, (DWORD)outSize);
    if (len == 0 || len >= outSize)
    {
        outPath[0] = 0;
        return;
    }
    PathRemoveFileSpecA(outPath);
}

static void GetConfigPath(char* outPath, size_t outSize)
{
    GetExeDirectory(outPath, outSize);
    if (outPath[0] == 0)
    {
        return;
    }
    size_t len = strlen(outPath);
    if (len + 1 + strlen("config.json") + 1 > outSize)
    {
        outPath[0] = 0;
        return;
    }
    strcat_s(outPath, outSize, "\\config.json");
}

static void GetServiceDirectory(char* outPath, size_t outSize)
{
    GetExeDirectory(outPath, outSize);
    if (outPath[0] == 0)
    {
        return;
    }
}

static bool AreNodeModulesInstalled()
{
    char serviceDir[MAX_PATH];
    GetServiceDirectory(serviceDir, sizeof(serviceDir));
    if (serviceDir[0] == 0)
    {
        return false;
    }
    char path[MAX_PATH * 2];
    size_t len = strlen(serviceDir);
    if (len + 1 + strlen("node_modules") + 1 > sizeof(path))
    {
        return false;
    }
    memcpy(path, serviceDir, len);
    path[len] = 0;
    strcat_s(path, sizeof(path), "\\node_modules");
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static void InstallNodeModules()
{
    char serviceDir[MAX_PATH];
    GetServiceDirectory(serviceDir, sizeof(serviceDir));
    if (serviceDir[0] == 0)
    {
        SetStatus("Failed to determine UpdaterService folder");
        return;
    }

    SetStatus("Running npm install...");

    char cmdLine[260];
    sprintf_s(cmdLine, "npm install");

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    BOOL ok = CreateProcessA(
        NULL,
        cmdLine,
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        serviceDir,
        &si,
        &pi
    );
    if (!ok)
    {
        SetStatus("Failed to start npm install");
        return;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode == 0 && AreNodeModulesInstalled())
    {
        SetStatus("Node modules installed");
    }
    else
    {
        SetStatus("npm install failed");
    }
}

static void OpenFirewallPort()
{
    char configPath[MAX_PATH * 2];
    GetConfigPath(configPath, sizeof(configPath));
    CONFIG_DATA cfg;
    LoadConfig(&cfg, configPath);
    int port = cfg.Port;
    if (port <= 0 || port > 65535)
    {
        port = 4000;
    }

    char exeDir[MAX_PATH];
    GetExeDirectory(exeDir, sizeof(exeDir));
    if (exeDir[0] == 0)
    {
        SetStatus("Failed to determine exe folder");
        return;
    }

    char cmdLine[512];
    sprintf_s(
        cmdLine,
        "netsh advfirewall firewall add rule name=\"MU Updater Service %d\" dir=in action=allow protocol=TCP localport=%d",
        port,
        port
    );

    SetStatus("Adding firewall rule...");

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    BOOL ok = CreateProcessA(
        NULL,
        cmdLine,
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        exeDir,
        &si,
        &pi
    );
    if (!ok)
    {
        SetStatus("Failed to start netsh");
        MessageBoxA(
            g_hWnd,
            "Failed to start netsh. Run MU Updater Manager as Administrator to modify firewall.",
            "MU Updater Manager",
            MB_OK | MB_ICONERROR
        );
        return;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode == 0)
    {
        SetStatus("Firewall rule added");
    }
    else
    {
        SetStatus("Failed to add firewall rule");
        MessageBoxA(
            g_hWnd,
            "Failed to add firewall rule. Run MU Updater Manager as Administrator and try again.",
            "MU Updater Manager",
            MB_OK | MB_ICONERROR
        );
    }
}

static void CheckNodeModulesOnStartup(HWND hWnd)
{
    if (AreNodeModulesInstalled())
    {
        return;
    }
    int res = MessageBoxA(
        hWnd,
        "Node modules are not installed for updater service.\nRun \"npm install\" now?",
        "MU Updater Manager",
        MB_ICONQUESTION | MB_YESNO
    );
    if (res == IDYES)
    {
        InstallNodeModules();
    }
}

static bool IsServiceRunning()
{
    if (!g_ServiceProcess.hProcess)
    {
        return false;
    }
    DWORD code = 0;
    if (!GetExitCodeProcess(g_ServiceProcess.hProcess, &code))
    {
        return false;
    }
    return code == STILL_ACTIVE;
}

static bool StartServiceProcess();

static void StopServiceProcess()
{
    if (!g_ServiceProcess.hProcess)
    {
        SetStatus("Service is not running");
        return;
    }
    DWORD code = 0;
    if (!GetExitCodeProcess(g_ServiceProcess.hProcess, &code) || code != STILL_ACTIVE)
    {
        CloseHandle(g_ServiceProcess.hProcess);
        CloseHandle(g_ServiceProcess.hThread);
        ZeroMemory(&g_ServiceProcess, sizeof(g_ServiceProcess));
        SetStatus("Service stopped");
        return;
    }

    TerminateProcess(g_ServiceProcess.hProcess, 0);
    WaitForSingleObject(g_ServiceProcess.hProcess, 5000);

    CloseHandle(g_ServiceProcess.hProcess);
    CloseHandle(g_ServiceProcess.hThread);
    ZeroMemory(&g_ServiceProcess, sizeof(g_ServiceProcess));
    SetStatus("Service stopped");
}

static void RestartServiceProcess()
{
    StopServiceProcess();
    StartServiceProcess();
}

static bool StartServiceProcess()
{
    if (IsServiceRunning())
    {
        SetStatus("Service is already running");
        return true;
    }

    char serviceDir[MAX_PATH];
    GetServiceDirectory(serviceDir, sizeof(serviceDir));
    if (serviceDir[0] == 0)
    {
        SetStatus("Failed to determine UpdaterService folder");
        return false;
    }

    char cmdLine[260];
    sprintf_s(cmdLine, "node server.js");

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    BOOL ok = CreateProcessA(
        NULL,
        cmdLine,
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        serviceDir,
        &si,
        &pi
    );
    if (!ok)
    {
        SetStatus("Failed to start node server.js");
        return false;
    }

    if (g_ServiceProcess.hProcess)
    {
        CloseHandle(g_ServiceProcess.hProcess);
    }
    if (g_ServiceProcess.hThread)
    {
        CloseHandle(g_ServiceProcess.hThread);
    }
    g_ServiceProcess = pi;

    SetStatus("Service started");
    return true;
}

static void LoadConfigToControls()
{
    char configPath[MAX_PATH * 2];
    GetConfigPath(configPath, sizeof(configPath));

    CONFIG_DATA cfg;
    LoadConfig(&cfg, configPath);

    char portBuf[32];
    sprintf_s(portBuf, "%d", cfg.Port);

    SetWindowTextA(g_hEditClientRoot, cfg.ClientRoot);
    SetWindowTextA(g_hEditPort, portBuf);
    SetWindowTextA(g_hEditToken, cfg.AdminToken);
    SetWindowTextA(g_hEditVersion, cfg.Version);
}

static void SaveControlsToConfig()
{
    char configPath[MAX_PATH * 2];
    GetConfigPath(configPath, sizeof(configPath));
    if (configPath[0] == 0)
    {
        SetStatus("Config path is not defined");
        return;
    }

    CONFIG_DATA cfg;
    char buf[512];

    GetWindowTextA(g_hEditClientRoot, cfg.ClientRoot, sizeof(cfg.ClientRoot));
    GetWindowTextA(g_hEditToken, cfg.AdminToken, sizeof(cfg.AdminToken));
    GetWindowTextA(g_hEditVersion, cfg.Version, sizeof(cfg.Version));

    GetWindowTextA(g_hEditPort, buf, sizeof(buf));
    TrimTrailingNewlines(buf);
    int port = atoi(buf);
    if (port <= 0 || port > 65535)
    {
        port = 4000;
    }
    cfg.Port = port;

    if (SaveConfig(&cfg, configPath))
    {
        SetStatus("Config saved");
    }
    else
    {
        SetStatus("Failed to save config.json");
    }
}

static void GenerateTokenToControl()
{
    char token[128];
    GenerateRandomToken(token, sizeof(token));
    SetWindowTextA(g_hEditToken, token);
    SetStatus("Token generated");
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_hWnd = hWnd;
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        CreateWindowExA(0, "STATIC", "MU Updater Manager", WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, 10, 380, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);

        CreateWindowExA(0, "STATIC", "Client path:", WS_CHILD | WS_VISIBLE,
            10, 40, 120, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);
        g_hEditClientRoot = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            140, 40, 240, 20, hWnd, (HMENU)101, GetModuleHandle(NULL), NULL);

        CreateWindowExA(0, "STATIC", "Service port:", WS_CHILD | WS_VISIBLE,
            10, 70, 120, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);
        g_hEditPort = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            140, 70, 80, 20, hWnd, (HMENU)102, GetModuleHandle(NULL), NULL);

        CreateWindowExA(0, "STATIC", "Admin token:", WS_CHILD | WS_VISIBLE,
            10, 100, 120, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);
        g_hEditToken = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            140, 100, 240, 20, hWnd, (HMENU)103, GetModuleHandle(NULL), NULL);

        CreateWindowExA(0, "STATIC", "Version:", WS_CHILD | WS_VISIBLE,
            10, 130, 120, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);
        g_hEditVersion = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            140, 130, 80, 20, hWnd, (HMENU)104, GetModuleHandle(NULL), NULL);

        HWND hBtnSave = CreateWindowExA(0, "BUTTON", "Save config", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 170, 140, 30, hWnd, (HMENU)201, GetModuleHandle(NULL), NULL);
        HWND hBtnGen = CreateWindowExA(0, "BUTTON", "Generate token", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            160, 170, 140, 30, hWnd, (HMENU)202, GetModuleHandle(NULL), NULL);
        HWND hBtnStart = CreateWindowExA(0, "BUTTON", "Start service", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            310, 170, 140, 30, hWnd, (HMENU)203, GetModuleHandle(NULL), NULL);

        HWND hBtnStop = CreateWindowExA(0, "BUTTON", "Stop service", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 210, 140, 30, hWnd, (HMENU)204, GetModuleHandle(NULL), NULL);
        HWND hBtnRestart = CreateWindowExA(0, "BUTTON", "Restart service", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            160, 210, 180, 30, hWnd, (HMENU)205, GetModuleHandle(NULL), NULL);
        HWND hBtnInstall = CreateWindowExA(0, "BUTTON", "Install Node modules", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 250, 200, 30, hWnd, (HMENU)206, GetModuleHandle(NULL), NULL);
        HWND hBtnFirewall = CreateWindowExA(0, "BUTTON", "Open firewall port", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            220, 250, 200, 30, hWnd, (HMENU)207, GetModuleHandle(NULL), NULL);

        g_hLog = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
            10, 290, 440, 120, hWnd, (HMENU)302, GetModuleHandle(NULL), NULL);

        g_hStatus = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_LEFT,
            10, 420, 440, 20, hWnd, (HMENU)301, GetModuleHandle(NULL), NULL);

        HWND controls[] = { g_hEditClientRoot, g_hEditPort, g_hEditToken, g_hEditVersion, hBtnSave, hBtnGen, hBtnStart, hBtnStop, hBtnRestart, hBtnInstall, hBtnFirewall, g_hLog, g_hStatus };
        for (int i = 0; i < (int)(sizeof(controls) / sizeof(controls[0])); i++)
        {
            if (controls[i] && hFont)
            {
                SendMessage(controls[i], WM_SETFONT, (WPARAM)hFont, TRUE);
            }
        }

        LoadConfigToControls();
        CheckNodeModulesOnStartup(hWnd);
        StartServiceProcess();
    }
    break;
    case WM_COMMAND:
    {
        UINT id = LOWORD(wParam);
        if (id == 201)
        {
            SaveControlsToConfig();
        }
        else if (id == 202)
        {
            GenerateTokenToControl();
        }
        else if (id == 203)
        {
            StartServiceProcess();
        }
        else if (id == 204)
        {
            StopServiceProcess();
        }
        else if (id == 205)
        {
            RestartServiceProcess();
        }
        else if (id == 206)
        {
            InstallNodeModules();
        }
        else if (id == 207)
        {
            OpenFirewallPort();
        }
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEXA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "MuUpdaterManagerWindowClass";
    wc.hIconSm = wc.hIcon;

    if (!RegisterClassExA(&wc))
    {
        return 0;
    }

    int width = 480;
    int height = 480;

    RECT rc;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
    int screenW = rc.right - rc.left;
    int screenH = rc.bottom - rc.top;
    int x = rc.left + (screenW - width) / 2;
    int y = rc.top + (screenH - height) / 2;

    HWND hWnd = CreateWindowExA(WS_EX_APPWINDOW, wc.lpszClassName, "MU Updater Manager",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, width, height, NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
