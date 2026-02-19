#include "LauncherNetwork.h"
#include <windows.h>
#include <commctrl.h>
#include <wininet.h>
#include <stdio.h>
#include <shlwapi.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shlwapi.lib")

// Global progress variables (need to be updated from here)
extern ULONGLONG g_TotalBytesToDownload;
extern ULONGLONG g_BytesDownloaded;
extern ULONGLONG g_CurrentFileBytes;
extern ULONGLONG g_CurrentFileSize;
extern DWORD g_TotalFilesToDownload;
extern DWORD g_FilesDownloaded;
extern HWND g_hProgress;
void UpdateProgressBar(); // From UI
void SetStatusText(const char* text); // From UI
extern char g_FileStatusText[256];
void LogLauncher(const char* format, ...);

static void LogInternetError(const char* action, const char* url)
{
    DWORD err = GetLastError();
    char info[256] = "";
    DWORD infoLen = sizeof(info);
    if (err == ERROR_INTERNET_EXTENDED_ERROR)
    {
        DWORD extErr = 0;
        if (InternetGetLastResponseInfoA(&extErr, info, &infoLen))
        {
            err = extErr;
        }
    }
    LogLauncher("HttpGet error. action=%s url=%s err=%lu info=%s", action, url, err, info[0] ? info : "-");
}

// Helper function to perform HTTP GET
static bool HttpGetToBuffer(const char* url, char* buffer, DWORD bufferSize, DWORD* outSize)
{
    *outSize = 0;
    for (int attempt = 1; attempt <= 3; attempt++)
    {
        HINTERNET hInternet = InternetOpenA("MULauncher", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet)
        {
            LogInternetError("InternetOpenA", url);
            Sleep(300 * attempt);
            continue;
        }
        DWORD timeout = 15000;
        InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOption(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

        HINTERNET hFile = InternetOpenUrlA(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
        if (!hFile)
        {
            LogInternetError("InternetOpenUrlA", url);
            InternetCloseHandle(hInternet);
            Sleep(300 * attempt);
            continue;
        }
        DWORD total = 0;
        bool ok = true;
        while (total < bufferSize)
        {
            DWORD toRead = bufferSize - total;
            DWORD read = 0;
            if (!InternetReadFile(hFile, buffer + total, toRead, &read))
            {
                LogInternetError("InternetReadFile", url);
                ok = false;
                break;
            }
            if (read == 0)
            {
                break;
            }
            total += read;
        }
        InternetCloseHandle(hFile);
        InternetCloseHandle(hInternet);
        if (ok)
        {
            *outSize = total;
            if (total < bufferSize)
            {
                buffer[total] = 0;
            }
            return true;
        }
        Sleep(300 * attempt);
    }
    return false;
}

static bool ParseVersionFromJson(const char* json, char* outVersion, size_t outSize)
{
    const char* p = strstr(json, "\"version\"");
    if (!p) return false;
    p = strchr(p, ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p == '\"')
    {
        p++;
        const char* start = p;
        while (*p && *p != '\"') p++;
        size_t len = (size_t)(p - start);
        if (len + 1 > outSize) return false;
        memcpy(outVersion, start, len);
        outVersion[len] = 0;
        return true;
    }
    return false;
}

static bool LoadLocalVersion(const char* clientDir, char* outVersion, size_t outSize)
{
    char path[MAX_PATH];
    wsprintfA(path, "%sData\\client_version.txt", clientDir);
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        // Try root folder as fallback
        wsprintfA(path, "%sclient_version.txt", clientDir);
        hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            outVersion[0] = 0;
            return false;
        }
    }
    char buffer[64];
    DWORD read = 0;
    if (!ReadFile(hFile, buffer, sizeof(buffer) - 1, &read, NULL))
    {
        CloseHandle(hFile);
        outVersion[0] = 0;
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
    if (len + 1 > outSize)
    {
        outVersion[0] = 0;
        return false;
    }
    memcpy(outVersion, buffer, len + 1);
    return true;
}

static bool VersionsEqual(const char* a, const char* b)
{
    if (!a || !b) return false;
    return lstrcmpA(a, b) == 0;
}

bool CheckForUpdates(const char* clientDir, UpdateStatus* status)
{
    status->isUpdateRequired = false;
    
    char versionUrl[512];
    wsprintfA(versionUrl, "%s/update/version", UPDATE_SERVER_URL);

    char versionBuffer[1024];
    DWORD size = 0;
    
    if (!HttpGetToBuffer(versionUrl, versionBuffer, sizeof(versionBuffer) - 1, &size))
    {
        status->errorMessage = "Failed to retrieve update version";
        return false;
    }

    char remoteVersion[64];
    if (!ParseVersionFromJson(versionBuffer, remoteVersion, sizeof(remoteVersion)))
    {
        status->errorMessage = "Invalid update version format";
        return false;
    }
    status->remoteVersion = remoteVersion;

    char localVersion[64];
    bool haveLocal = LoadLocalVersion(clientDir, localVersion, sizeof(localVersion));
    if (!haveLocal)
    {
        strcpy_s(localVersion, "0.00"); // Default if missing
    }
    status->currentVersion = localVersion;

    if (haveLocal && VersionsEqual(localVersion, remoteVersion))
    {
        return true; // Up to date
    }

    status->isUpdateRequired = true;
    return true;
}

bool DownloadUpdateManifest(const char* clientDir, char* buffer, size_t size)
{
    char manifestUrl[512];
    wsprintfA(manifestUrl, "%s/update/manifest", UPDATE_SERVER_URL);
    
    DWORD downloadedSize = 0;
    return HttpGetToBuffer(manifestUrl, buffer, (DWORD)size, &downloadedSize);
}

// ... Rest of the download logic (EnsureDirectories, BuildLocalPath, DownloadFileFromServer, etc.) ...
// Simplified for brevity in this step, but needs to be fully implemented based on original code.
// I will copy the helper functions here.

static void BuildLocalPath(char* outPath, size_t outSize, const char* clientDir, const char* relPath)
{
    if (outSize == 0) return;
    outPath[0] = 0;
    size_t dirLen = strlen(clientDir);
    if (dirLen + 1 >= outSize) return;
    memcpy(outPath, clientDir, dirLen);
    outPath[dirLen] = 0;
    const char* src = relPath;
    char temp[512];
    size_t pos = 0;
    while (*src && pos + 1 < sizeof(temp))
    {
        char c = *src++;
        if (c == '/') c = '\\';
        temp[pos++] = c;
    }
    temp[pos] = 0;
    size_t need = dirLen + pos + 1;
    if (need > outSize)
    {
        outPath[0] = 0;
        return;
    }
    strcat_s(outPath, outSize, temp);
}

static void EnsureDirectoriesForPath(const char* fullPath)
{
    char dirPath[MAX_PATH * 2];
    lstrcpynA(dirPath, fullPath, sizeof(dirPath));
    size_t len = strlen(dirPath);
    for (size_t i = 0; i < len; i++)
    {
        if (dirPath[i] == '\\' || dirPath[i] == '/')
        {
            char ch = dirPath[i];
            dirPath[i] = 0;
            if (dirPath[0] != 0) CreateDirectoryA(dirPath, NULL);
            dirPath[i] = ch;
        }
    }
}

static bool BuildTempRootPath(char* outPath, size_t outSize, const char* clientDir)
{
    size_t dirLen = strlen(clientDir);
    if (dirLen + 13 >= outSize) return false;
    lstrcpynA(outPath, clientDir, outSize);
    strcat_s(outPath, outSize, "_update_tmp\\");
    return true;
}

static bool BuildTempPath(char* outPath, size_t outSize, const char* clientDir, const char* relPath)
{
    char tempRoot[MAX_PATH * 2];
    if (!BuildTempRootPath(tempRoot, sizeof(tempRoot), clientDir)) return false;
    BuildLocalPath(outPath, outSize, tempRoot, relPath);
    return outPath[0] != 0;
}

static bool BuildTempDownloadPath(char* outPath, size_t outSize, const char* clientDir, const char* relPath)
{
    if (!BuildTempPath(outPath, outSize, clientDir, relPath)) return false;
    strcat_s(outPath, outSize, ".download");
    return true;
}

static bool DeleteDirectoryRecursive(const char* dir)
{
    char pattern[MAX_PATH * 2];
    wsprintfA(pattern, "%s*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;
    do
    {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char path[MAX_PATH * 2];
        wsprintfA(path, "%s%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            char subdir[MAX_PATH * 2];
            wsprintfA(subdir, "%s%s\\", dir, fd.cFileName);
            DeleteDirectoryRecursive(subdir);
            RemoveDirectoryA(subdir);
        }
        else
        {
            DeleteFileA(path);
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    return true;
}

static bool GetFileSizeBytes(const char* path, ULONGLONG* outSize)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) return false;
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return false;
    *outSize = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    return true;
}

static const char* GetBaseName(const char* path)
{
    const char* base = path;
    const char* slash = strrchr(path, '/');
    const char* backslash = strrchr(path, '\\');
    if (slash && backslash) base = (slash > backslash) ? slash + 1 : backslash + 1;
    else if (slash) base = slash + 1;
    else if (backslash) base = backslash + 1;
    return base;
}

static bool NormalizePath(const char* relPath, char* outPath, size_t outSize)
{
    if (!relPath || !outPath || outSize == 0) return false;
    size_t pos = 0;
    const char* p = relPath;
    while (*p && pos + 1 < outSize)
    {
        char c = *p++;
        if (c == '\\') c = '/';
        outPath[pos++] = c;
    }
    outPath[pos] = 0;
    return true;
}

static bool UrlEncodeBytes(const unsigned char* bytes, int length, char* outPath, size_t outSize)
{
    if (!bytes || !outPath || outSize == 0) return false;
    size_t pos = 0;
    for (int i = 0; i < length; i++)
    {
        unsigned char c = bytes[i];
        bool unreserved =
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~' || c == '/';
        if (unreserved)
        {
            if (pos + 1 >= outSize) return false;
            outPath[pos++] = (char)c;
        }
        else
        {
            if (pos + 3 >= outSize) return false;
            static const char* hex = "0123456789ABCDEF";
            outPath[pos++] = '%';
            outPath[pos++] = hex[(c >> 4) & 0xF];
            outPath[pos++] = hex[c & 0xF];
        }
    }
    outPath[pos] = 0;
    return true;
}

static bool BuildDownloadUrl(const char* baseUrl, const char* relPath, int encodeMode, char* outUrl, size_t outSize)
{
    char normalized[512];
    if (!NormalizePath(relPath, normalized, sizeof(normalized))) return false;

    char pathPart[1024];
    if (encodeMode == 1)
    {
        int wideLen = MultiByteToWideChar(CP_ACP, 0, normalized, -1, NULL, 0);
        if (wideLen <= 0) return false;
        wchar_t* wide = (wchar_t*)_alloca(sizeof(wchar_t) * wideLen);
        if (!MultiByteToWideChar(CP_ACP, 0, normalized, -1, wide, wideLen)) return false;
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
        if (utf8Len <= 0) return false;
        char* utf8 = (char*)_alloca(utf8Len);
        if (!WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8Len, NULL, NULL)) return false;
        if (!UrlEncodeBytes((const unsigned char*)utf8, utf8Len - 1, pathPart, sizeof(pathPart))) return false;
    }
    else if (encodeMode == 2)
    {
        int len = (int)strlen(normalized);
        if (!UrlEncodeBytes((const unsigned char*)normalized, len, pathPart, sizeof(pathPart))) return false;
    }
    else
    {
        lstrcpynA(pathPart, normalized, sizeof(pathPart));
    }
    if (strlen(baseUrl) + strlen("/update/files/") + strlen(pathPart) + 1 > outSize) return false;
    wsprintfA(outUrl, "%s/update/files/%s", baseUrl, pathPart);
    return true;
}

static bool DownloadFileFromUrl(const char* url, const char* clientDir, const char* relPath, const char* localPath, DWORD remoteSize)
{
    char tempPath[MAX_PATH * 2];
    if (!BuildTempDownloadPath(tempPath, sizeof(tempPath), clientDir, relPath)) return false;

    ULONGLONG existing = 0;
    GetFileSizeBytes(tempPath, &existing);
    if (existing > (ULONGLONG)remoteSize)
    {
        DeleteFileA(tempPath);
        existing = 0;
    }

    const char* baseName = GetBaseName(relPath);
    if (baseName && *baseName)
    {
        sprintf_s(g_FileStatusText, "File: %s", baseName);
    }
    else
    {
        strcpy_s(g_FileStatusText, "File: -");
    }

    for (int attempt = 1; attempt <= 3; attempt++)
    {
        existing = 0;
        GetFileSizeBytes(tempPath, &existing);
        if (existing > (ULONGLONG)remoteSize)
        {
            DeleteFileA(tempPath);
            existing = 0;
        }

        g_CurrentFileSize = remoteSize;
        g_CurrentFileBytes = existing;
        UpdateProgressBar();

        HINTERNET hInternet = InternetOpenA("MULauncher", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet)
        {
            LogInternetError("InternetOpenA", url);
            Sleep(300 * attempt);
            continue;
        }
        
        DWORD timeout = 15000;
        InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOption(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

        const char* headers = NULL;
        char headersBuf[64];
        DWORD headersLen = 0;
        DWORD offset = (DWORD)existing;
        if (offset > 0 && offset < remoteSize)
        {
            wsprintfA(headersBuf, "Range: bytes=%lu-\r\n", offset);
            headers = headersBuf;
            headersLen = (DWORD)-1;
        }

        HINTERNET hFile = InternetOpenUrlA(hInternet, url, headers, headers ? headersLen : 0, INTERNET_FLAG_RELOAD, 0);
        if (!hFile)
        {
            LogInternetError("InternetOpenUrlA", url);
            InternetCloseHandle(hInternet);
            Sleep(300 * attempt);
            continue;
        }
        DWORD status = 0;
        DWORD statusLen = sizeof(status);
        if (HttpQueryInfoA(hFile, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &statusLen, NULL))
        {
            if (status >= 400)
            {
                LogLauncher("Http status %lu url=%s", status, url);
                InternetCloseHandle(hFile);
                InternetCloseHandle(hInternet);
                Sleep(300 * attempt);
                continue;
            }
        }

        EnsureDirectoriesForPath(tempPath);
        DWORD creationDisposition = offset > 0 ? OPEN_EXISTING : CREATE_ALWAYS;
        HANDLE hOut = CreateFileA(tempPath, GENERIC_WRITE, 0, NULL, creationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hOut == INVALID_HANDLE_VALUE)
        {
            InternetCloseHandle(hFile);
            InternetCloseHandle(hInternet);
            Sleep(300 * attempt);
            continue;
        }
        if (offset > 0)
        {
            if (SetFilePointer(hOut, offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
            {
                CloseHandle(hOut);
                InternetCloseHandle(hFile);
                InternetCloseHandle(hInternet);
                Sleep(300 * attempt);
                continue;
            }
        }

        char buffer[8192];
        DWORD read = 0;
        BOOL ok = TRUE;
        for (;;)
        {
            if (!InternetReadFile(hFile, buffer, sizeof(buffer), &read))
            {
                LogInternetError("InternetReadFile", url);
                ok = FALSE;
                break;
            }
            if (read == 0) break;
            DWORD written = 0;
            if (!WriteFile(hOut, buffer, read, &written, NULL) || written != read)
            {
                ok = FALSE;
                break;
            }
            g_BytesDownloaded += read;
            g_CurrentFileBytes += read;
            UpdateProgressBar();
        }
        
        CloseHandle(hOut);
        InternetCloseHandle(hFile);
        InternetCloseHandle(hInternet);
        
        if (ok)
        {
            EnsureDirectoriesForPath(localPath);
            DeleteFileA(localPath);
            MoveFileA(tempPath, localPath);
            return true;
        }
        Sleep(300 * attempt);
    }
    return false;
}

static bool DownloadFileFromServer(const char* baseUrl, const char* clientDir, const char* relPath, const char* localPath, DWORD remoteSize)
{
    char urlEscaped[1024];
    if (!BuildDownloadUrl(baseUrl, relPath, 1, urlEscaped, sizeof(urlEscaped))) return false;
    if (DownloadFileFromUrl(urlEscaped, clientDir, relPath, localPath, remoteSize)) return true;
    char urlAnsi[1024];
    if (BuildDownloadUrl(baseUrl, relPath, 2, urlAnsi, sizeof(urlAnsi)) && _stricmp(urlAnsi, urlEscaped) != 0)
    {
        if (DownloadFileFromUrl(urlAnsi, clientDir, relPath, localPath, remoteSize)) return true;
    }
    char urlRaw[1024];
    if (BuildDownloadUrl(baseUrl, relPath, 0, urlRaw, sizeof(urlRaw)) && _stricmp(urlRaw, urlEscaped) != 0)
    {
        return DownloadFileFromUrl(urlRaw, clientDir, relPath, localPath, remoteSize);
    }
    return false;
}

static bool ParseManifestNextEntry(const char* json, const char* start, char* outPath, size_t outPathSize, DWORD* outSize, const char** outNext)
{
    const char* p = strstr(start, "\"path\"");
    if (!p) return false;
    p = strchr(p, ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != '\"') return false;
    p++;
    const char* pathStart = p;
    while (*p && *p != '\"') p++;
    if (*p != '\"') return false;
    size_t pathLen = (size_t)(p - pathStart);
    if (pathLen + 1 > outPathSize) return false;
    memcpy(outPath, pathStart, pathLen);
    outPath[pathLen] = 0;
    const char* afterPath = p + 1;

    const char* nextPath = strstr(afterPath, "\"path\"");
    const char* sizePos = strstr(afterPath, "\"size\"");
    if (!sizePos || (nextPath && sizePos > nextPath))
    {
        *outSize = 0;
    }
    else
    {
        sizePos = strchr(sizePos, ':');
        if (!sizePos) return false;
        sizePos++;
        while (*sizePos == ' ' || *sizePos == '\t' || *sizePos == '\r' || *sizePos == '\n') sizePos++;
        *outSize = (DWORD)atoi(sizePos);
    }

    *outNext = afterPath;
    return true;
}

bool HasMissingFiles(const char* clientDir, const char* manifestBuffer)
{
    const char* p = manifestBuffer;
    int count = 0;
    for (;;)
    {
        char path[512];
        DWORD size = 0;
        const char* next = NULL;
        if (!ParseManifestNextEntry(manifestBuffer, p, path, sizeof(path), &size, &next)) break;
        count++;
        const char* base = GetBaseName(path);
        if (base && _stricmp(base, "Launcher.exe") == 0)
        {
            p = next;
            continue;
        }
        char localPath[MAX_PATH];
        BuildLocalPath(localPath, sizeof(localPath), clientDir, path);
        ULONGLONG localSize = 0;
        bool localOk = GetFileSizeBytes(localPath, &localSize);
        if (!localOk)
        {
            LogLauncher("Missing file: %s", path);
            return true;
        }
        if (size > 0 && localSize != size)
        {
            LogLauncher("Size mismatch: %s local=%llu remote=%lu", path, localSize, size);
            return true;
        }
        p = next;
    }
    return count == 0;
}

bool ManifestHasFile(const char* manifestBuffer, const char* fileName)
{
    const char* p = manifestBuffer;
    for (;;)
    {
        char path[512];
        DWORD size = 0;
        const char* next = NULL;
        if (!ParseManifestNextEntry(manifestBuffer, p, path, sizeof(path), &size, &next)) break;
        const char* base = path;
        const char* slash = strrchr(path, '/');
        const char* backslash = strrchr(path, '\\');
        if (slash && backslash)
        {
            base = (slash > backslash) ? slash + 1 : backslash + 1;
        }
        else if (slash)
        {
            base = slash + 1;
        }
        else if (backslash)
        {
            base = backslash + 1;
        }
        if (_stricmp(base, fileName) == 0)
        {
            return true;
        }
        p = next;
    }
    return false;
}

bool GetManifestFileInfo(const char* manifestBuffer, const char* fileName, char* outRelPath, size_t outRelPathSize, DWORD* outSize)
{
    if (outRelPath && outRelPathSize > 0) outRelPath[0] = 0;
    const char* p = manifestBuffer;
    for (;;)
    {
        char path[512];
        DWORD size = 0;
        const char* next = NULL;
        if (!ParseManifestNextEntry(manifestBuffer, p, path, sizeof(path), &size, &next)) break;
        const char* base = GetBaseName(path);
        if (base && _stricmp(base, fileName) == 0)
        {
            if (outRelPath && outRelPathSize > 0)
            {
                lstrcpynA(outRelPath, path, outRelPathSize);
            }
            if (outSize) *outSize = size;
            return true;
        }
        p = next;
    }
    return false;
}

bool ProcessUpdate(const char* clientDir, const char* manifestBuffer)
{
    const char* p = manifestBuffer;
    int count = 0;
    g_TotalFilesToDownload = 0;
    g_TotalBytesToDownload = 0;
    for (;;)
    {
        char path[512];
        DWORD size = 0;
        const char* next = NULL;
        if (!ParseManifestNextEntry(manifestBuffer, p, path, sizeof(path), &size, &next)) break;
        count++;
        const char* base = GetBaseName(path);
        if (base && _stricmp(base, "Launcher.exe") == 0)
        {
            p = next;
            continue;
        }
        char localPath[MAX_PATH];
        char tempDownloadPath[MAX_PATH * 2];
        BuildLocalPath(localPath, sizeof(localPath), clientDir, path);
        BuildTempDownloadPath(tempDownloadPath, sizeof(tempDownloadPath), clientDir, path);
        ULONGLONG localSize = 0;
        ULONGLONG tempSize = 0;
        bool localOk = GetFileSizeBytes(localPath, &localSize) && localSize == size;
        bool tempOk = GetFileSizeBytes(tempDownloadPath, &tempSize) && tempSize == size;
        if (!localOk && !tempOk)
        {
            ULONGLONG remaining = size;
            if (tempSize > 0 && tempSize < size)
            {
                remaining = size - tempSize;
            }
            g_TotalFilesToDownload++;
            g_TotalBytesToDownload += remaining;
        }
        p = next;
    }
    if (count == 0)
    {
        return false;
    }
    
    // Pass 2: Download
    p = manifestBuffer;
    g_BytesDownloaded = 0;
    g_FilesDownloaded = 0;
    
    for (;;)
    {
        char path[512];
        DWORD size = 0;
        const char* next = NULL;
        if (!ParseManifestNextEntry(manifestBuffer, p, path, sizeof(path), &size, &next)) break;
        const char* base = GetBaseName(path);
        if (base && _stricmp(base, "Launcher.exe") == 0)
        {
            p = next;
            continue;
        }
        char localPath[MAX_PATH];
        char tempDownloadPath[MAX_PATH * 2];
        BuildLocalPath(localPath, sizeof(localPath), clientDir, path);
        BuildTempDownloadPath(tempDownloadPath, sizeof(tempDownloadPath), clientDir, path);
        ULONGLONG localSize = 0;
        ULONGLONG tempSize = 0;
        bool localOk = GetFileSizeBytes(localPath, &localSize) && localSize == size;
        bool tempOk = GetFileSizeBytes(tempDownloadPath, &tempSize) && tempSize == size;
        if (localOk)
        {
            DeleteFileA(tempDownloadPath);
            p = next;
            continue;
        }
        if (tempOk)
        {
            EnsureDirectoriesForPath(localPath);
            DeleteFileA(localPath);
            MoveFileA(tempDownloadPath, localPath);
            p = next;
            continue;
        }
        if (!DownloadFileFromServer(UPDATE_SERVER_URL, clientDir, path, localPath, size))
        {
            LogLauncher("Download failed: %s", path);
            return false;
        }
        g_FilesDownloaded++;
        p = next;
    }
    if (HasMissingFiles(clientDir, manifestBuffer))
    {
        LogLauncher("Post-update verification failed.");
        return false;
    }
    char tempRoot[MAX_PATH * 2];
    if (BuildTempRootPath(tempRoot, sizeof(tempRoot), clientDir))
    {
        DeleteDirectoryRecursive(tempRoot);
        RemoveDirectoryA(tempRoot);
    }
    return true;
}

bool DownloadUpdateFile(const char* clientDir, const char* relPath, const char* localPath, DWORD remoteSize)
{
    return DownloadFileFromServer(UPDATE_SERVER_URL, clientDir, relPath, localPath, remoteSize);
}

bool GetServerStatus(ServerStatusInfo* info)
{
    // Defaults
    info->isOnline = false;
    info->onlineCount = 0;
    info->version = "Unknown";
    
    char url[512];
    wsprintfA(url, "%s/status", UPDATE_SERVER_URL);
    
    char buffer[1024];
    DWORD size = 0;
    
    if (!HttpGetToBuffer(url, buffer, sizeof(buffer)-1, &size))
    {
        return false;
    }
    
    // Simple JSON parsing
    // {"online": true, "players": 123, "version": "1.04d"}
    
    // Online
    if (strstr(buffer, "\"online\":true") || strstr(buffer, "\"online\": true"))
        info->isOnline = true;
    else
        info->isOnline = false;
        
    // Players
    const char* p = strstr(buffer, "\"players\"");
    if (p)
    {
        p = strchr(p, ':');
        if (p)
        {
            p++;
            info->onlineCount = atoi(p);
        }
    }
    
    // Version
    char ver[64];
    if (ParseVersionFromJson(buffer, ver, sizeof(ver)))
    {
        info->version = ver;
    }
    
    return true;
}
