#include "LauncherToken.h"
#include <stdio.h>
#include <shlwapi.h>

static DWORD LauncherToken_HashString(const char* text)
{
    DWORD h = 2166136261u;
    while (*text)
    {
        char c = *text++;
        if (c >= 'A' && c <= 'Z')
        {
            c = char(c - 'A' + 'a');
        }
        h ^= (BYTE)c;
        h *= 16777619u;
    }
    return h;
}

static void LauncherToken_BuildPath(char* outPath, size_t size)
{
    char modulePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, modulePath, MAX_PATH) == 0)
    {
        if (size > 0)
        {
            outPath[0] = 0;
        }
        return;
    }

    char dirPath[MAX_PATH];
    lstrcpynA(dirPath, modulePath, MAX_PATH);
    char* p = strrchr(dirPath, '\\');
    if (p)
    {
        *(p + 1) = 0;
    }

    DWORD hash = LauncherToken_HashString(dirPath);

    char fileName[64];
    wsprintfA(fileName, "tex_%08X.bin", hash);

    char dataDir[MAX_PATH];
    wsprintfA(dataDir, "%s%s", dirPath, "Data\\");

    if (size == 0)
    {
        return;
    }

    lstrcpynA(outPath, dataDir, (int)size);
    size_t len = strlen(outPath);
    size_t need = len + strlen(fileName) + 1;
    if (need > size)
    {
        outPath[0] = 0;
        return;
    }
    strcat_s(outPath, size, fileName);
}

static void LauncherToken_EncryptBuffer(BYTE* buffer, size_t size)
{
    DWORD state = 0xA5C3F18Du;
    for (size_t i = 0; i < size; i++)
    {
        state = state * 1664525u + 1013904223u;
        buffer[i] ^= (BYTE)(state >> 24);
    }
}

static DWORD LauncherToken_ComputeCrc(DWORD magic, DWORD version, ULONGLONG timestamp, DWORD random)
{
    DWORD crc = 0x1F123BB5u;
    crc ^= magic;
    crc = (crc << 5) | (crc >> (32 - 5));
    crc ^= version;
    crc = (crc << 7) | (crc >> (32 - 7));
    crc ^= (DWORD)(timestamp & 0xFFFFFFFFu);
    crc = (crc << 9) | (crc >> (32 - 9));
    crc ^= random;
    return crc;
}

void LauncherToken_Create()
{
    char path[MAX_PATH];
    LauncherToken_BuildPath(path, MAX_PATH);
    if (path[0] == 0)
    {
        return;
    }

    char dirPath[MAX_PATH];
    lstrcpynA(dirPath, path, MAX_PATH);
    char* p = strrchr(dirPath, '\\');
    if (p)
    {
        *p = 0;
        CreateDirectoryA(dirPath, NULL);
    }

    LAUNCHER_TOKEN_DATA data;
    data.Magic = 0x4C544B31u;
    data.Version = 1;
    data.Timestamp = GetTickCount64();

    LARGE_INTEGER counter;
    if (QueryPerformanceCounter(&counter))
    {
        data.Random = (DWORD)(counter.LowPart ^ GetTickCount());
    }
    else
    {
        data.Random = GetTickCount();
    }

    data.Crc = LauncherToken_ComputeCrc(data.Magic, data.Version, data.Timestamp, data.Random);

    LauncherToken_EncryptBuffer((BYTE*)&data, sizeof(data));

    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DWORD written = 0;
    WriteFile(hFile, &data, sizeof(data), &written, NULL);
    CloseHandle(hFile);
}
