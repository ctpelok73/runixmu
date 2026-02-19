#include <windows.h>
#include <stdint.h>
#include <wininet.h>
#include <commctrl.h>
#include <shellapi.h>

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

static void LauncherToken_Create()
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

static bool CheckAndUpdateClient(HWND hWnd, const char* clientDir);
static void Launcher_StartGame(HWND hWnd);

static HWND g_hStatus = NULL;
static HWND g_hProgress = NULL;
static ULONGLONG g_TotalBytesToDownload = 0;
static ULONGLONG g_BytesDownloaded = 0;
static DWORD g_TotalFilesToDownload = 0;
static DWORD g_FilesDownloaded = 0;

static void SetStatusText(const char* text)
{
	if (g_hStatus)
	{
		SetWindowTextA(g_hStatus, text);
	}
}

static void ResetProgress()
{
	g_TotalBytesToDownload = 0;
	g_BytesDownloaded = 0;
	g_TotalFilesToDownload = 0;
	g_FilesDownloaded = 0;
	if (g_hProgress)
	{
		SendMessageA(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
		SendMessageA(g_hProgress, PBM_SETPOS, 0, 0);
	}
}

static void UpdateProgressBar()
{
	if (!g_hProgress)
	{
		return;
	}
	if (g_TotalBytesToDownload == 0)
	{
		SendMessageA(g_hProgress, PBM_SETPOS, 0, 0);
		return;
	}
	ULONGLONG percent = 0;
	if (g_BytesDownloaded >= g_TotalBytesToDownload)
	{
		percent = 100;
	}
	else
	{
		percent = g_BytesDownloaded * 100 / g_TotalBytesToDownload;
		if (percent > 100)
		{
			percent = 100;
		}
	}
	SendMessageA(g_hProgress, PBM_SETPOS, (WPARAM)percent, 0);
}

static void ShowLastErrorMessage(HWND hWnd, const char* title, const char* prefix)
{
	DWORD err = GetLastError();
	char msg[512];
	wsprintfA(msg, "%s (error %lu)", prefix, (unsigned long)err);
	MessageBoxA(hWnd, msg, title, MB_OK | MB_ICONERROR);
}

static void Launcher_StartGame(HWND hWnd)
{
	char modulePath[MAX_PATH];
	if (GetModuleFileNameA(NULL, modulePath, MAX_PATH) == 0)
	{
		MessageBoxA(hWnd, "Cannot get launcher path", "Error", MB_OK | MB_ICONERROR);
		return;
	}

	char dirPath[MAX_PATH];
	lstrcpynA(dirPath, modulePath, MAX_PATH);
	char* p = strrchr(dirPath, '\\');
	if (p)
	{
		*(p + 1) = 0;
	}

	if (!CheckAndUpdateClient(hWnd, dirPath))
	{
		return;
	}

	char clientPath[MAX_PATH];
	wsprintfA(clientPath, "%smain.exe", dirPath);

	if (GetFileAttributesA(clientPath) == INVALID_FILE_ATTRIBUTES)
	{
		MessageBoxA(hWnd, "main.exe not found", "Error", MB_OK | MB_ICONERROR);
		return;
	}

	LauncherToken_Create();

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);

	BOOL res = CreateProcessA(clientPath, NULL, NULL, NULL, FALSE, 0, NULL, dirPath, &si, &pi);
	if (!res)
	{
		HINSTANCE hInst = ShellExecuteA(hWnd, "open", clientPath, NULL, dirPath, SW_SHOWNORMAL);
		if ((UINT_PTR)hInst <= 32)
		{
			char prefix[512];
			wsprintfA(prefix, "Failed to start game\n%s", clientPath);
			ShowLastErrorMessage(hWnd, "Error", prefix);
			return;
		}
		PostQuitMessage(0);
		return;
	}

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	PostQuitMessage(0);
}

static bool HttpGetToBuffer(const char* url, char* buffer, DWORD bufferSize, DWORD* outSize)
{
	*outSize = 0;
	HINTERNET hInternet = InternetOpenA("MULauncher", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (!hInternet)
	{
		return false;
	}
	HINTERNET hFile = InternetOpenUrlA(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
	if (!hFile)
	{
		InternetCloseHandle(hInternet);
		return false;
	}
	DWORD total = 0;
	while (total < bufferSize)
	{
		DWORD toRead = bufferSize - total;
		DWORD read = 0;
		if (!InternetReadFile(hFile, buffer + total, toRead, &read))
		{
			InternetCloseHandle(hFile);
			InternetCloseHandle(hInternet);
			return false;
		}
		if (read == 0)
		{
			break;
		}
		total += read;
	}
	InternetCloseHandle(hFile);
	InternetCloseHandle(hInternet);
	*outSize = total;
	if (total < bufferSize)
	{
		buffer[total] = 0;
	}
	return true;
}

static bool ParseVersionFromJson(const char* json, char* outVersion, size_t outSize)
{
	const char* p = strstr(json, "\"version\"");
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
	if (*p == '\"')
	{
		p++;
		const char* start = p;
		while (*p && *p != '\"')
		{
			p++;
		}
		size_t len = (size_t)(p - start);
		if (len + 1 > outSize)
		{
			return false;
		}
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
		outVersion[0] = 0;
		return false;
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

static bool SaveLocalVersion(const char* clientDir, const char* version)
{
	char path[MAX_PATH];
	wsprintfA(path, "%sData\\client_version.txt", clientDir);
	char dirPath[MAX_PATH];
	lstrcpynA(dirPath, path, MAX_PATH);
	char* p = strrchr(dirPath, '\\');
	if (p)
	{
		*p = 0;
		CreateDirectoryA(dirPath, NULL);
	}
	HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	DWORD len = (DWORD)strlen(version);
	DWORD written = 0;
	BOOL ok = WriteFile(hFile, version, len, &written, NULL);
	CloseHandle(hFile);
	return ok && written == len;
}

static bool VersionsEqual(const char* a, const char* b)
{
	if (!a || !b)
	{
		return false;
	}
	return lstrcmpA(a, b) == 0;
}

static void BuildLocalPath(char* outPath, size_t outSize, const char* clientDir, const char* relPath)
{
	if (outSize == 0)
	{
		return;
	}
	outPath[0] = 0;
	size_t dirLen = strlen(clientDir);
	if (dirLen + 1 >= outSize)
	{
		return;
	}
	memcpy(outPath, clientDir, dirLen);
	outPath[dirLen] = 0;
	const char* src = relPath;
	char temp[512];
	size_t pos = 0;
	while (*src && pos + 1 < sizeof(temp))
	{
		char c = *src++;
		if (c == '/')
		{
			c = '\\';
		}
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
			if (dirPath[0] != 0)
			{
				CreateDirectoryA(dirPath, NULL);
			}
			dirPath[i] = ch;
		}
	}
}

static bool DownloadFileFromServer(const char* baseUrl, const char* relPath, const char* localPath, DWORD remoteSize)
{
	char url[1024];
	if (strlen(baseUrl) + strlen("/update/files/") + strlen(relPath) + 1 > sizeof(url))
	{
		return false;
	}
	wsprintfA(url, "%s/update/files/%s", baseUrl, relPath);

	char tempPath[MAX_PATH * 2];
	lstrcpynA(tempPath, localPath, sizeof(tempPath));
	size_t tempLen = strlen(tempPath);
	if (tempLen + 10 >= sizeof(tempPath))
	{
		return false;
	}
	strcat_s(tempPath, sizeof(tempPath), ".download");

	ULONGLONG existing = 0;
	WIN32_FILE_ATTRIBUTE_DATA fadTemp;
	if (GetFileAttributesExA(tempPath, GetFileExInfoStandard, &fadTemp))
	{
		existing = ((ULONGLONG)fadTemp.nFileSizeHigh << 32) | fadTemp.nFileSizeLow;
	}
	if (existing > (ULONGLONG)remoteSize)
	{
		DeleteFileA(tempPath);
		existing = 0;
	}

	HINTERNET hInternet = InternetOpenA("MULauncher", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (!hInternet)
	{
		return false;
	}

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
		InternetCloseHandle(hInternet);
		return false;
	}

	EnsureDirectoriesForPath(localPath);
	DWORD creationDisposition = offset > 0 ? OPEN_EXISTING : CREATE_ALWAYS;
	HANDLE hOut = CreateFileA(tempPath, GENERIC_WRITE, 0, NULL, creationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hOut == INVALID_HANDLE_VALUE)
	{
		InternetCloseHandle(hFile);
		InternetCloseHandle(hInternet);
		return false;
	}
	if (offset > 0)
	{
		if (SetFilePointer(hOut, offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
		{
			CloseHandle(hOut);
			InternetCloseHandle(hFile);
			InternetCloseHandle(hInternet);
			return false;
		}
	}

	char buffer[8192];
	DWORD read = 0;
	BOOL ok = TRUE;
	for (;;)
	{
		if (!InternetReadFile(hFile, buffer, sizeof(buffer), &read))
		{
			ok = FALSE;
			break;
		}
		if (read == 0)
		{
			break;
		}
		DWORD written = 0;
		if (!WriteFile(hOut, buffer, read, &written, NULL) || written != read)
		{
			ok = FALSE;
			break;
		}
		g_BytesDownloaded += read;
		UpdateProgressBar();
	}

	CloseHandle(hOut);
	InternetCloseHandle(hFile);
	InternetCloseHandle(hInternet);

	if (!ok)
	{
		return false;
	}

	WIN32_FILE_ATTRIBUTE_DATA fadFinalTemp;
	if (!GetFileAttributesExA(tempPath, GetFileExInfoStandard, &fadFinalTemp))
	{
		return false;
	}
	ULONGLONG finalSize = ((ULONGLONG)fadFinalTemp.nFileSizeHigh << 32) | fadFinalTemp.nFileSizeLow;
	if (finalSize != (ULONGLONG)remoteSize)
	{
		return false;
	}

	DeleteFileA(localPath);
	if (!MoveFileExA(tempPath, localPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
	{
		return false;
	}

	return true;
}

static bool EnsureFileUpdated(const char* baseUrl, const char* clientDir, const char* relPath, DWORD remoteSize)
{
	char localPath[MAX_PATH * 2];
	BuildLocalPath(localPath, sizeof(localPath), clientDir, relPath);
	if (localPath[0] == 0)
	{
		return false;
	}

	WIN32_FILE_ATTRIBUTE_DATA fad;
	if (GetFileAttributesExA(localPath, GetFileExInfoStandard, &fad))
	{
		ULONGLONG sz = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
		if (sz == (ULONGLONG)remoteSize)
		{
			return true;
		}
	}

	if (!DownloadFileFromServer(baseUrl, relPath, localPath, remoteSize))
	{
		return false;
	}

	return true;
}

static bool ComputeManifestTotals(const char* json, const char* clientDir, ULONGLONG* outBytes, DWORD* outFiles)
{
	const char* p = json;
	*outBytes = 0;
	*outFiles = 0;
	while (1)
	{
		const char* pathKey = strstr(p, "\"path\"");
		if (!pathKey)
		{
			break;
		}
		const char* colon = strchr(pathKey, ':');
		if (!colon)
		{
			return false;
		}
		const char* q = colon + 1;
		while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')
		{
			q++;
		}
		if (*q != '\"')
		{
			return false;
		}
		q++;
		const char* start = q;
		while (*q && *q != '\"')
		{
			q++;
		}
		if (!*q)
		{
			return false;
		}
		size_t pathLen = (size_t)(q - start);
		if (pathLen == 0 || pathLen >= 260)
		{
			return false;
		}
		char relPath[260];
		memcpy(relPath, start, pathLen);
		relPath[pathLen] = 0;

		const char* sizeKey = strstr(q, "\"size\"");
		if (!sizeKey)
		{
			return false;
		}
		const char* sizeColon = strchr(sizeKey, ':');
		if (!sizeColon)
		{
			return false;
		}
		const char* s = sizeColon + 1;
		while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		{
			s++;
		}
		if (*s < '0' || *s > '9')
		{
			return false;
		}
		DWORD remoteSize = 0;
		while (*s >= '0' && *s <= '9')
		{
			DWORD digit = (DWORD)(*s - '0');
			if (remoteSize > 429496729)
			{
				return false;
			}
			remoteSize = remoteSize * 10 + digit;
			s++;
		}

		char localPath[MAX_PATH * 2];
		BuildLocalPath(localPath, sizeof(localPath), clientDir, relPath);
		if (localPath[0] == 0)
		{
			return false;
		}

		ULONGLONG existingFinal = 0;
		WIN32_FILE_ATTRIBUTE_DATA fadFinal;
		if (GetFileAttributesExA(localPath, GetFileExInfoStandard, &fadFinal))
		{
			existingFinal = ((ULONGLONG)fadFinal.nFileSizeHigh << 32) | fadFinal.nFileSizeLow;
		}
		if (existingFinal == (ULONGLONG)remoteSize)
		{
			p = q;
			continue;
		}

		char tempPath[MAX_PATH * 2];
		lstrcpynA(tempPath, localPath, sizeof(tempPath));
		size_t tempLen = strlen(tempPath);
		if (tempLen + 10 >= sizeof(tempPath))
		{
			return false;
		}
		strcat_s(tempPath, sizeof(tempPath), ".download");

		ULONGLONG existingTemp = 0;
		WIN32_FILE_ATTRIBUTE_DATA fadTemp;
		if (GetFileAttributesExA(tempPath, GetFileExInfoStandard, &fadTemp))
		{
			existingTemp = ((ULONGLONG)fadTemp.nFileSizeHigh << 32) | fadTemp.nFileSizeLow;
		}
		if (existingTemp >= (ULONGLONG)remoteSize)
		{
			existingTemp = 0;
		}

		ULONGLONG need = (ULONGLONG)remoteSize;
		if (existingTemp > 0 && existingTemp < (ULONGLONG)remoteSize)
		{
			need = (ULONGLONG)remoteSize - existingTemp;
		}

		*outBytes += need;
		if (need > 0)
		{
			(*outFiles)++;
		}

		p = q;
	}

	return true;
}

static bool ProcessManifest(const char* json, const char* baseUrl, const char* clientDir)
{
	const char* p = json;
	while (1)
	{
		const char* pathKey = strstr(p, "\"path\"");
		if (!pathKey)
		{
			break;
		}
		const char* colon = strchr(pathKey, ':');
		if (!colon)
		{
			return false;
		}
		const char* q = colon + 1;
		while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')
		{
			q++;
		}
		if (*q != '\"')
		{
			return false;
		}
		q++;
		const char* start = q;
		while (*q && *q != '\"')
		{
			q++;
		}
		if (!*q)
		{
			return false;
		}
		size_t pathLen = (size_t)(q - start);
		if (pathLen == 0 || pathLen >= 260)
		{
			return false;
		}
		char relPath[260];
		memcpy(relPath, start, pathLen);
		relPath[pathLen] = 0;

		const char* sizeKey = strstr(q, "\"size\"");
		if (!sizeKey)
		{
			return false;
		}
		const char* sizeColon = strchr(sizeKey, ':');
		if (!sizeColon)
		{
			return false;
		}
		const char* s = sizeColon + 1;
		while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		{
			s++;
		}
		if (*s < '0' || *s > '9')
		{
			return false;
		}
		DWORD remoteSize = 0;
		while (*s >= '0' && *s <= '9')
		{
			DWORD digit = (DWORD)(*s - '0');
			if (remoteSize > 429496729)
			{
				return false;
			}
			remoteSize = remoteSize * 10 + digit;
			s++;
		}

		if (!EnsureFileUpdated(baseUrl, clientDir, relPath, remoteSize))
		{
			return false;
		}

		p = q;
	}

	return true;
}

static bool CheckAndUpdateClient(HWND hWnd, const char* clientDir)
{
	const char* baseUrl = "http://update.runixmu.online:4000";
	char versionUrl[512];
	wsprintfA(versionUrl, "%s/update/version", baseUrl);

	char versionBuffer[1024];
	DWORD size = 0;
	SetStatusText("Checking for updates...");
	ResetProgress();

	if (!HttpGetToBuffer(versionUrl, versionBuffer, sizeof(versionBuffer) - 1, &size))
	{
		MessageBoxA(hWnd, "Failed to retrieve update version", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	char remoteVersion[64];
	if (!ParseVersionFromJson(versionBuffer, remoteVersion, sizeof(remoteVersion)))
	{
		MessageBoxA(hWnd, "Invalid update version format", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	char localVersion[64];
	bool haveLocal = LoadLocalVersion(clientDir, localVersion, sizeof(localVersion));
	if (haveLocal && VersionsEqual(localVersion, remoteVersion))
	{
		SetStatusText("Client is up to date");
		return true;
	}

	wsprintfA(versionBuffer, "Client update to version %s will be installed", remoteVersion);
	MessageBoxA(hWnd, versionBuffer, "Update", MB_OK | MB_ICONINFORMATION);

	char manifestUrl[512];
	wsprintfA(manifestUrl, "%s/update/manifest", baseUrl);

	const DWORD manifestMax = 512 * 1024;
	char* manifestBuffer = (char*)HeapAlloc(GetProcessHeap(), 0, manifestMax);
	if (!manifestBuffer)
	{
		MessageBoxA(hWnd, "Not enough memory to load manifest", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	DWORD manifestSize = 0;
	bool ok = HttpGetToBuffer(manifestUrl, manifestBuffer, manifestMax - 1, &manifestSize);
	if (!ok)
	{
		HeapFree(GetProcessHeap(), 0, manifestBuffer);
		MessageBoxA(hWnd, "Failed to download update manifest", "Error", MB_OK | MB_ICONERROR);
		return false;
	}
	if (manifestSize >= manifestMax)
	{
		HeapFree(GetProcessHeap(), 0, manifestBuffer);
		MessageBoxA(hWnd, "Update manifest is too large", "Error", MB_OK | MB_ICONERROR);
		return false;
	}
	manifestBuffer[manifestSize] = 0;

	ULONGLONG totalBytes = 0;
	DWORD totalFiles = 0;
	if (!ComputeManifestTotals(manifestBuffer, clientDir, &totalBytes, &totalFiles))
	{
		HeapFree(GetProcessHeap(), 0, manifestBuffer);
		MessageBoxA(hWnd, "Invalid update manifest format", "Error", MB_OK | MB_ICONERROR);
		return false;
	}
	g_TotalBytesToDownload = totalBytes;
	g_TotalFilesToDownload = totalFiles;
	g_BytesDownloaded = 0;
	g_FilesDownloaded = 0;
	if (g_hProgress)
	{
		SendMessageA(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
		SendMessageA(g_hProgress, PBM_SETPOS, 0, 0);
	}
	SetStatusText("Downloading updates...");

	if (!ProcessManifest(manifestBuffer, baseUrl, clientDir))
	{
		HeapFree(GetProcessHeap(), 0, manifestBuffer);
		MessageBoxA(hWnd, "Error while applying client update", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	HeapFree(GetProcessHeap(), 0, manifestBuffer);

	if (!SaveLocalVersion(clientDir, remoteVersion))
	{
		MessageBoxA(hWnd, "Failed to save local client version", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	return true;
}

static LRESULT CALLBACK Launcher_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
	{
		HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

		HWND hTitle = CreateWindowExA(0, "STATIC", "MU Online Launcher", WS_CHILD | WS_VISIBLE | SS_CENTER,
			20, 20, 360, 30, hWnd, NULL, GetModuleHandle(NULL), NULL);
		if (hTitle && hFont)
		{
			SendMessage(hTitle, WM_SETFONT, (WPARAM)hFont, TRUE);
		}

		HWND hPlay = CreateWindowExA(0, "BUTTON", "Play", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			120, 100, 160, 40, hWnd, (HMENU)1, GetModuleHandle(NULL), NULL);
		if (hPlay && hFont)
		{
			SendMessage(hPlay, WM_SETFONT, (WPARAM)hFont, TRUE);
		}

		HWND hExit = CreateWindowExA(0, "BUTTON", "Exit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			120, 160, 160, 40, hWnd, (HMENU)2, GetModuleHandle(NULL), NULL);
		if (hExit && hFont)
		{
			SendMessage(hExit, WM_SETFONT, (WPARAM)hFont, TRUE);
		}

		g_hStatus = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_LEFT,
			20, 210, 360, 20, hWnd, (HMENU)101, GetModuleHandle(NULL), NULL);
		if (g_hStatus && hFont)
		{
			SendMessage(g_hStatus, WM_SETFONT, (WPARAM)hFont, TRUE);
		}

		g_hProgress = CreateWindowExA(0, PROGRESS_CLASSA, NULL, WS_CHILD | WS_VISIBLE,
			20, 240, 360, 20, hWnd, (HMENU)102, GetModuleHandle(NULL), NULL);

		ResetProgress();
	}
	break;
	case WM_COMMAND:
	{
		UINT id = LOWORD(wParam);
		if (id == 1)
		{
			Launcher_StartGame(hWnd);
		}
		else if (id == 2)
		{
			PostQuitMessage(0);
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
	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&icc);

	WNDCLASSEXA wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = Launcher_WndProc;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.lpszClassName = "MuLauncherWindowClass";
	wc.hIconSm = wc.hIcon;

	if (!RegisterClassExA(&wc))
	{
		return 0;
	}

	int width = 400;
	int height = 320;

	RECT rc;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
	int screenW = rc.right - rc.left;
	int screenH = rc.bottom - rc.top;
	int x = rc.left + (screenW - width) / 2;
	int y = rc.top + (screenH - height) / 2;

	HWND hWnd = CreateWindowExA(WS_EX_APPWINDOW, wc.lpszClassName, "MU Launcher",
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
