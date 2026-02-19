#pragma once
#include <windows.h>
#include <string>

// Networking constants
#define UPDATE_SERVER_URL "http://update.runixmu.online:4000"

struct UpdateStatus
{
    bool isUpdateRequired;
    std::string remoteVersion;
    std::string currentVersion;
    std::string errorMessage;
};

struct ServerStatusInfo
{
    bool isOnline;
    int onlineCount;
    std::string version;
};

bool CheckForUpdates(const char* clientDir, UpdateStatus* status);
bool DownloadUpdateManifest(const char* clientDir, char* buffer, size_t size);
bool HasMissingFiles(const char* clientDir, const char* manifestBuffer);
bool ManifestHasFile(const char* manifestBuffer, const char* fileName);
bool GetManifestFileInfo(const char* manifestBuffer, const char* fileName, char* outRelPath, size_t outRelPathSize, DWORD* outSize);
bool ProcessUpdate(const char* clientDir, const char* manifestBuffer);
bool DownloadUpdateFile(const char* clientDir, const char* relPath, const char* localPath, DWORD remoteSize);
bool GetServerStatus(ServerStatusInfo* info);
