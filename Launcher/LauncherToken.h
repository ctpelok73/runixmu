#pragma once
#include <windows.h>
#include <stdint.h>

#pragma pack(push, 1)
struct LAUNCHER_TOKEN_DATA
{
    DWORD Magic;
    DWORD Version;
    ULONGLONG Timestamp;
    DWORD Random;
    DWORD Crc;
};
#pragma pack(pop)

void LauncherToken_Create();
