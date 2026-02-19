#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <fstream>
#include <stdarg.h>

#include "LauncherUI.h"
#include "LauncherNetwork.h"
#include "LauncherToken.h"

#pragma comment(lib, "Msimg32.lib")

#define WM_AUTO_UPDATE (WM_APP + 1)

// Global variables for UI
HWND g_hStatus = NULL;
HWND g_hProgress = NULL;
HWND g_hWnd = NULL;
ULONGLONG g_TotalBytesToDownload = 0;
ULONGLONG g_BytesDownloaded = 0;
ULONGLONG g_CurrentFileBytes = 0;
ULONGLONG g_CurrentFileSize = 0;
DWORD g_TotalFilesToDownload = 0;
DWORD g_FilesDownloaded = 0;

// Server Status Globals
bool g_ServerOnline = false;
int g_OnlineCount = 0;
char g_ClientVersion[64] = "0.00";

// Layout Rectangles (Updated for RunixMU Layout)
// Window Size: 960 x 600

// Top Right Exit Button
RECT g_rcExit = { 920, 10, 950, 40 };

// Main Banner (Left)
RECT g_rcBanner = { 30, 80, 580, 430 };

// Sidebar (Right)
RECT g_rcSidebar = { 600, 80, 930, 560 };
// Elements inside sidebar (Relative to window)
RECT g_rcStatus = { 610, 90, 920, 110 }; // Server Status
RECT g_rcFileProgress = { 610, 140, 920, 160 }; // File Progress Bar
RECT g_rcProgress = { 610, 190, 920, 210 }; // Total Progress Bar
RECT g_rcStart = { 610, 250, 920, 310 }; // Start Game Button
RECT g_rcNews = { 610, 330, 920, 550 }; // News Panel

// Footer Buttons (Bottom Left)
RECT g_rcRegister = { 30, 450, 160, 490 };
RECT g_rcWebsite = { 170, 450, 300, 490 };
RECT g_rcDiscord = { 310, 450, 440, 490 };
RECT g_rcSupport = { 450, 450, 580, 490 };

// Hover States
bool g_StartHover = false;
bool g_ExitHover = false;
bool g_RegisterHover = false;
bool g_WebsiteHover = false;
bool g_DiscordHover = false;
bool g_SupportHover = false;

bool g_IsUpdating = false;

// Global string for status
char g_StatusText[256] = "Checking files...";
char g_FileStatusText[256] = "File: -";

// Speed calculation globals
DWORD g_SpeedLastTime = 0;
ULONGLONG g_SpeedLastBytes = 0;
char g_SpeedText[64] = "Speed: 0.0 MB/s";

// Forward declarations
LRESULT CALLBACK Launcher_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static bool Launcher_RunUpdate(HWND hWnd, bool launchAfter, const char* reason);

void LogLauncher(const char* format, ...)
{
    char modulePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, modulePath, MAX_PATH) == 0) return;
    char dirPath[MAX_PATH];
    lstrcpynA(dirPath, modulePath, MAX_PATH);
    char* p = strrchr(dirPath, '\\');
    if (p) *(p + 1) = 0;

    char logPath[MAX_PATH];
    wsprintfA(logPath, "%sLauncher.log", dirPath);

    HANDLE hFile = CreateFileA(logPath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    char prefix[64];
    wsprintfA(prefix, "[%04d-%02d-%02d %02d:%02d:%02d] ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    DWORD written = 0;
    WriteFile(hFile, prefix, (DWORD)strlen(prefix), &written, NULL);

    char buffer[1024];
    va_list args;
    va_start(args, format);
    _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
    va_end(args);

    WriteFile(hFile, buffer, (DWORD)strlen(buffer), &written, NULL);
    WriteFile(hFile, "\r\n", 2, &written, NULL);
    CloseHandle(hFile);
}

static bool ReadLocalClientVersion(const char* clientDir, char* outVersion, size_t outSize)
{
    if (!outVersion || outSize == 0) return false;
    outVersion[0] = 0;
    char path[MAX_PATH];
    wsprintfA(path, "%sData\\client_version.txt", clientDir);
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        wsprintfA(path, "%sclient_version.txt", clientDir);
        hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;
    }
    char buffer[64];
    DWORD read = 0;
    if (!ReadFile(hFile, buffer, sizeof(buffer) - 1, &read, NULL))
    {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);
    buffer[read] = 0;
    size_t len = strlen(buffer);
    while (len > 0 && (buffer[len - 1] == '\r' || buffer[len - 1] == '\n' || buffer[len - 1] == ' ' || buffer[len - 1] == '\t'))
    {
        buffer[len - 1] = 0;
        len--;
    }
    if (len + 1 > outSize) return false;
    memcpy(outVersion, buffer, len + 1);
    return true;
}

static void WriteLocalClientVersion(const char* clientDir, const char* version)
{
    if (!version || !version[0]) return;
    char dataDir[MAX_PATH];
    wsprintfA(dataDir, "%sData", clientDir);
    CreateDirectoryA(dataDir, NULL);

    char dataPath[MAX_PATH];
    wsprintfA(dataPath, "%sData\\client_version.txt", clientDir);
    HANDLE hFile = CreateFileA(dataPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WriteFile(hFile, version, (DWORD)strlen(version), &written, NULL);
        CloseHandle(hFile);
    }

    char rootPath[MAX_PATH];
    wsprintfA(rootPath, "%sclient_version.txt", clientDir);
    hFile = CreateFileA(rootPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WriteFile(hFile, version, (DWORD)strlen(version), &written, NULL);
        CloseHandle(hFile);
    }
}

static bool GetFileSizeBytesLocal(const char* path, ULONGLONG* outSize)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) return false;
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return false;
    *outSize = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    return true;
}

static bool ScheduleLauncherReplace(const char* currentExe, const char* newExe)
{
    char params[2048];
    sprintf_s(params, "/c ping 127.0.0.1 -n 2 >nul & move /y \"%s\" \"%s\" & start \"\" \"%s\"", newExe, currentExe, currentExe);
    HINSTANCE hInst = ShellExecuteA(NULL, "runas", "cmd.exe", params, NULL, SW_HIDE);
    return ((INT_PTR)hInst > 32);
}

// Thread to check server status
DWORD WINAPI CheckServerStatusThread(LPVOID lpParam)
{
    bool haveLocal = false;
    char localVer[64];
    char modulePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, modulePath, MAX_PATH) != 0)
    {
        char dirPath[MAX_PATH];
        lstrcpynA(dirPath, modulePath, MAX_PATH);
        char* p = strrchr(dirPath, '\\');
        if (p) *(p + 1) = 0;

        if (ReadLocalClientVersion(dirPath, localVer, sizeof(localVer)))
        {
            strncpy_s(g_ClientVersion, localVer, 63);
            haveLocal = true;
        }
    }

    ServerStatusInfo info;
    if (GetServerStatus(&info))
    {
        g_ServerOnline = info.isOnline;
        g_OnlineCount = info.onlineCount;
        if (!haveLocal && !info.version.empty() && _stricmp(info.version.c_str(), "Unknown") != 0)
        {
            strncpy_s(g_ClientVersion, info.version.c_str(), 63);
        }
    }
    else
    {
        g_ServerOnline = false;
        // Keep last known version or "0.00"
    }

    if (g_hWnd) InvalidateRect(g_hWnd, &g_rcStatus, FALSE);
    return 0;
}

void SetLauncherStatus(const char* text)
{
    strncpy_s(g_StatusText, text, 255);
    if (g_hWnd) InvalidateRect(g_hWnd, &g_rcFileProgress, FALSE);
    if (g_hWnd) InvalidateRect(g_hWnd, &g_rcProgress, FALSE);
    if (g_hWnd) UpdateWindow(g_hWnd); // Force repaint
}

static bool Launcher_RunUpdate(HWND hWnd, bool launchAfter, const char* reason)
{
    char modulePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, modulePath, MAX_PATH) == 0) return false;

    char dirPath[MAX_PATH];
    lstrcpynA(dirPath, modulePath, MAX_PATH);
    char* p = strrchr(dirPath, '\\');
    if (p) *(p + 1) = 0;

    char clientPath[MAX_PATH];
    wsprintfA(clientPath, "%smain.exe", dirPath);
    bool clientExists = (GetFileAttributesA(clientPath) != INVALID_FILE_ATTRIBUTES);
    LogLauncher("Update started. reason=%s clientDir=%s clientExists=%d", reason ? reason : "unknown", dirPath, clientExists ? 1 : 0);

    if (g_IsUpdating) return false;
    g_IsUpdating = true;
    g_CurrentFileBytes = 0;
    g_CurrentFileSize = 0;
    strcpy_s(g_FileStatusText, "File: -");

    SetLauncherStatus("Checking for updates...");
    
    // Process messages to redraw
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UpdateStatus status;
    if (CheckForUpdates(dirPath, &status))
    {
        LogLauncher("CheckForUpdates ok. local=%s remote=%s updateRequired=%d", status.currentVersion.c_str(), status.remoteVersion.c_str(), status.isUpdateRequired ? 1 : 0);
        const size_t manifestMax = 4 * 1024 * 1024;
        char* manifestBuffer = (char*)HeapAlloc(GetProcessHeap(), 0, manifestMax);
        if (!manifestBuffer)
        {
            SetLauncherStatus("Failed to allocate manifest buffer.");
            LogLauncher("Failed to allocate manifest buffer.");
            g_IsUpdating = false;
            return false;
        }

        SetLauncherStatus("Downloading update list...");
        if (!DownloadUpdateManifest(dirPath, manifestBuffer, manifestMax))
        {
            SetLauncherStatus("Failed to download manifest.");
            LogLauncher("DownloadUpdateManifest failed.");
            HeapFree(GetProcessHeap(), 0, manifestBuffer);
            g_IsUpdating = false;
            return false;
        }
        LogLauncher("DownloadUpdateManifest ok.");

        bool hasMainInManifest = ManifestHasFile(manifestBuffer, "main.exe");
        LogLauncher("hasMainInManifest=%d", hasMainInManifest ? 1 : 0);
        if (!clientExists && !hasMainInManifest)
        {
            SetLauncherStatus("Update list missing main.exe");
            LogLauncher("Update list missing main.exe. Aborting update.");
            HeapFree(GetProcessHeap(), 0, manifestBuffer);
            g_IsUpdating = false;
            return false;
        }

        bool missingFiles = HasMissingFiles(dirPath, manifestBuffer);
        bool needUpdate = status.isUpdateRequired || !clientExists || missingFiles;
        LogLauncher("missingFiles=%d needUpdate=%d", missingFiles ? 1 : 0, needUpdate ? 1 : 0);

        if (needUpdate)
        {
            SetLauncherStatus("Downloading updates...");
            g_IsUpdating = true;
            
            g_SpeedLastTime = GetTickCount();
            g_SpeedLastBytes = 0;
            strcpy_s(g_SpeedText, "Speed: Calculating...");

            if (ProcessUpdate(dirPath, manifestBuffer))
            {
                SetLauncherStatus("Ready to Play");
                LogLauncher("ProcessUpdate ok.");
                WriteLocalClientVersion(dirPath, status.remoteVersion.c_str());

                char launcherRelPath[512];
                DWORD launcherRemoteSize = 0;
                if (GetManifestFileInfo(manifestBuffer, "Launcher.exe", launcherRelPath, sizeof(launcherRelPath), &launcherRemoteSize))
                {
                    ULONGLONG localSize = 0;
                    if (launcherRemoteSize > 0 && GetFileSizeBytesLocal(modulePath, &localSize) && localSize != launcherRemoteSize)
                    {
                        char newLauncherPath[MAX_PATH];
                        wsprintfA(newLauncherPath, "%sLauncher.exe.new", dirPath);
                        SetLauncherStatus("Updating launcher...");
                        LogLauncher("Launcher update available. local=%llu remote=%lu", localSize, launcherRemoteSize);
                        if (DownloadUpdateFile(dirPath, launcherRelPath, newLauncherPath, launcherRemoteSize))
                        {
                            LogLauncher("Launcher update downloaded: %s", newLauncherPath);
                            if (ScheduleLauncherReplace(modulePath, newLauncherPath))
                            {
                                LogLauncher("Launcher update scheduled.");
                                HeapFree(GetProcessHeap(), 0, manifestBuffer);
                                g_IsUpdating = false;
                                PostQuitMessage(0);
                                return true;
                            }
                            LogLauncher("Launcher update schedule failed.");
                        }
                        else
                        {
                            LogLauncher("Launcher update download failed.");
                        }
                    }
                }
            }
            else
            {
                SetLauncherStatus("Update failed.");
                LogLauncher("ProcessUpdate failed.");
                HeapFree(GetProcessHeap(), 0, manifestBuffer);
                g_IsUpdating = false;
                return false;
            }
            g_IsUpdating = false;

            clientExists = (GetFileAttributesA(clientPath) != INVALID_FILE_ATTRIBUTES);
            LogLauncher("After update clientExists=%d", clientExists ? 1 : 0);
        }
        else
        {
            SetLauncherStatus("Ready to Play");
            LogLauncher("No update needed.");
        }

        g_IsUpdating = false;
        HeapFree(GetProcessHeap(), 0, manifestBuffer);
    }
    else
    {
        SetLauncherStatus(status.errorMessage.c_str());
        LogLauncher("CheckForUpdates failed: %s", status.errorMessage.c_str());
        g_IsUpdating = false;
        return false;
    }

    // Launch Game
    if (!launchAfter)
    {
        return true;
    }

    if (!clientExists)
    {
        MessageBoxA(hWnd, "main.exe not found", "Error", MB_OK | MB_ICONERROR);
        LogLauncher("Launch aborted: main.exe not found.");
        return false;
    }

    LauncherToken_Create();

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    if (CreateProcessA(clientPath, NULL, NULL, NULL, FALSE, 0, NULL, dirPath, &si, &pi))
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        LogLauncher("CreateProcess ok. main.exe launched.");
        PostQuitMessage(0);
    }
    else
    {
        DWORD err = GetLastError();
        const char* verb = (err == ERROR_ELEVATION_REQUIRED) ? "runas" : "open";
        HINSTANCE hInst = ShellExecuteA(hWnd, verb, clientPath, NULL, dirPath, SW_SHOWNORMAL);
        LogLauncher("CreateProcess failed. error=%lu ShellExecute verb=%s result=%ld", err, verb, (LONG_PTR)hInst);
        PostQuitMessage(0);
    }
    return true;
}

// Logic to start game
void Launcher_StartGame(HWND hWnd)
{
    Launcher_RunUpdate(hWnd, true, "start_button");
}

void UpdateProgressBar()
{
    // Pump messages to keep UI responsive
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            ExitProcess(0);
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Calculate Speed
    DWORD curTime = GetTickCount();
    if (curTime - g_SpeedLastTime >= 1000) // Update every second
    {
        ULONGLONG diffBytes = g_BytesDownloaded - g_SpeedLastBytes;
        double speedMB = (double)diffBytes / (1024.0 * 1024.0);
        
        sprintf_s(g_SpeedText, "Speed: %.1f MB/s", speedMB);
        
        g_SpeedLastTime = curTime;
        g_SpeedLastBytes = g_BytesDownloaded;
    }

    if (g_hWnd) 
    {
        InvalidateRect(g_hWnd, &g_rcFileProgress, FALSE);
        InvalidateRect(g_hWnd, &g_rcProgress, FALSE);
        UpdateWindow(g_hWnd); // Force immediate repaint
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Launcher_WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "RunixMuLauncher";

    if (!RegisterClassExA(&wc)) return 0;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - LAUNCHER_WIDTH) / 2;
    int y = (screenH - LAUNCHER_HEIGHT) / 2;

    // WS_POPUP for frameless window
    g_hWnd = CreateWindowExA(WS_EX_APPWINDOW, wc.lpszClassName, LAUNCHER_TITLE,
        WS_POPUP | WS_VISIBLE,
        x, y, LAUNCHER_WIDTH, LAUNCHER_HEIGHT, NULL, NULL, hInstance, NULL);

    if (!g_hWnd) return 0;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    LogLauncher("Launcher started.");
    PostMessage(g_hWnd, WM_AUTO_UPDATE, 0, 0);

    // Initial server status check
    CreateThread(NULL, 0, CheckServerStatusThread, NULL, 0, NULL);

    // Timer for periodic updates (every 60 seconds)
    SetTimer(g_hWnd, 1, 60000, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK Launcher_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        
        // Double buffering
        HDC hMemDC = CreateCompatibleDC(hdc);
        HBITMAP hMemBitmap = CreateCompatibleBitmap(hdc, LAUNCHER_WIDTH, LAUNCHER_HEIGHT);
        SelectObject(hMemDC, hMemBitmap);

        RECT rcClient;
        GetClientRect(hWnd, &rcClient);

        // 1. Draw Background (Space)
        DrawSpaceBackground(hMemDC, &rcClient);
        
        // 2. Draw Main Metallic Frame
        DrawMetallicFrame(hMemDC, &rcClient, 6);

        // 3. Draw Title (Top Center)
        RECT rcTitle = { 0, 20, LAUNCHER_WIDTH, 70 };
        SetBkMode(hMemDC, TRANSPARENT);
        SetTextColor(hMemDC, COL_NEON_BLUE); // Neon Blue Title
        // Use a large decorative font
        HFONT hTitleFont = CreateFontA(52, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Cinzel Decorative");
        if (!hTitleFont) hTitleFont = CreateFontA(52, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Times New Roman");
        HFONT hOldFont = (HFONT)SelectObject(hMemDC, hTitleFont);
        
        // Draw Shadow first
        RECT rcShadow = rcTitle;
        OffsetRect(&rcShadow, 2, 2);
        SetTextColor(hMemDC, RGB(0, 0, 0));
        DrawTextA(hMemDC, LAUNCHER_TITLE, -1, &rcShadow, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        // Draw Main Text
        SetTextColor(hMemDC, COL_NEON_BLUE);
        DrawTextA(hMemDC, LAUNCHER_TITLE, -1, &rcTitle, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        SelectObject(hMemDC, hOldFont);
        DeleteObject(hTitleFont);

        // 4. Draw Banner Area (Left)
        DrawBannerArea(hMemDC, &g_rcBanner);

        // 5. Draw Footer Buttons (Left)
        DrawNeonButton(hMemDC, &g_rcRegister, "Register", g_RegisterHover, ButtonStyle::Secondary);
        DrawNeonButton(hMemDC, &g_rcWebsite, "Website", g_WebsiteHover, ButtonStyle::Secondary);
        DrawNeonButton(hMemDC, &g_rcDiscord, "Discord", g_DiscordHover, ButtonStyle::Secondary);
        DrawNeonButton(hMemDC, &g_rcSupport, "Support", g_SupportHover, ButtonStyle::Secondary);

        // 6. Draw Sidebar Content (Right)
        // Status
        DrawStatusPanel(hMemDC, &g_rcStatus, g_ServerOnline, g_OnlineCount, g_ClientVersion);
        
        float fileProgress = 0.0f;
        if (g_CurrentFileSize > 0)
            fileProgress = (float)((double)g_CurrentFileBytes / (double)g_CurrentFileSize * 100.0);
        
        DrawProgressBarModern(hMemDC, &g_rcFileProgress, fileProgress, g_FileStatusText, "");

        float progress = 0.0f;
        if (g_TotalBytesToDownload > 0)
            progress = (float)((double)g_BytesDownloaded / (double)g_TotalBytesToDownload * 100.0);
        
        DrawProgressBarModern(hMemDC, &g_rcProgress, progress, g_StatusText, g_SpeedText);
        
        // Start Button
        DrawNeonButton(hMemDC, &g_rcStart, "START GAME", g_StartHover, ButtonStyle::Primary);
        
        // News Panel
        DrawNewsPanel(hMemDC, &g_rcNews);

        // 7. Exit Button
        DrawNeonButton(hMemDC, &g_rcExit, "X", g_ExitHover, ButtonStyle::Icon);

        // Blit to screen
        BitBlt(hdc, 0, 0, LAUNCHER_WIDTH, LAUNCHER_HEIGHT, hMemDC, 0, 0, SRCCOPY);

        DeleteObject(hMemBitmap);
        DeleteDC(hMemDC);
        EndPaint(hWnd, &ps);
    }
    break;

    case WM_MOUSEMOVE:
    {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        bool prevStart = g_StartHover;
        bool prevExit = g_ExitHover;
        bool prevReg = g_RegisterHover;
        bool prevWeb = g_WebsiteHover;
        bool prevDisc = g_DiscordHover;
        bool prevSupp = g_SupportHover;

        g_StartHover = PtInRect(&g_rcStart, pt);
        g_ExitHover = PtInRect(&g_rcExit, pt);
        g_RegisterHover = PtInRect(&g_rcRegister, pt);
        g_WebsiteHover = PtInRect(&g_rcWebsite, pt);
        g_DiscordHover = PtInRect(&g_rcDiscord, pt);
        g_SupportHover = PtInRect(&g_rcSupport, pt);

        if (prevStart != g_StartHover || prevExit != g_ExitHover ||
            prevReg != g_RegisterHover || prevWeb != g_WebsiteHover ||
            prevDisc != g_DiscordHover || prevSupp != g_SupportHover)
        {
            InvalidateRect(hWnd, NULL, FALSE);
            
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme);
        }
    }
    break;

    case WM_MOUSELEAVE:
        g_StartHover = false;
        g_ExitHover = false;
        g_RegisterHover = false;
        g_WebsiteHover = false;
        g_DiscordHover = false;
        g_SupportHover = false;
        InvalidateRect(hWnd, NULL, FALSE);
        break;

    case WM_LBUTTONDOWN:
    {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        if (PtInRect(&g_rcStart, pt) && !g_IsUpdating)
        {
            Launcher_StartGame(hWnd);
        }
        else if (PtInRect(&g_rcExit, pt))
        {
            PostQuitMessage(0);
        }
        else if (PtInRect(&g_rcRegister, pt))
        {
            ShellExecuteA(NULL, "open", "http://runixmu.online/register", NULL, NULL, SW_SHOWNORMAL);
        }
        else if (PtInRect(&g_rcWebsite, pt))
        {
            ShellExecuteA(NULL, "open", "http://runixmu.online", NULL, NULL, SW_SHOWNORMAL);
        }
        else if (PtInRect(&g_rcDiscord, pt))
        {
            ShellExecuteA(NULL, "open", "https://discord.gg/runixmu", NULL, NULL, SW_SHOWNORMAL);
        }
        else if (PtInRect(&g_rcSupport, pt))
        {
            ShellExecuteA(NULL, "open", "http://runixmu.online/support", NULL, NULL, SW_SHOWNORMAL);
        }
        else
        {
            SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
    }
    break;

    case WM_TIMER:
        if (wParam == 1)
        {
            CreateThread(NULL, 0, CheckServerStatusThread, NULL, 0, NULL);
        }
        break;

    case WM_AUTO_UPDATE:
        Launcher_RunUpdate(hWnd, false, "auto_start");
        break;

    case WM_NCHITTEST:
    {
        // Default processing first
        LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);
        
        // Check buttons
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        POINT clientPt = pt;
        ScreenToClient(hWnd, &clientPt);

        if (PtInRect(&g_rcStart, clientPt) || PtInRect(&g_rcExit, clientPt) ||
            PtInRect(&g_rcRegister, clientPt) || PtInRect(&g_rcWebsite, clientPt) ||
            PtInRect(&g_rcDiscord, clientPt) || PtInRect(&g_rcSupport, clientPt))
        {
            return HTCLIENT;
        }

        // If not over a button, and we are in client area, return HTCAPTION to allow dragging
        if (hit == HTCLIENT)
        {
            return HTCAPTION;
        }
        return hit;
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
