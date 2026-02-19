#include <windows.h>
#include <stdint.h>
#include <wininet.h>

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
	strcat(outPath, fileName);
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
		MessageBoxA(hWnd, "Failed to start game", "Error", MB_OK | MB_ICONERROR);
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

static bool CheckAndUpdateClient(HWND hWnd, const char* clientDir)
{
	const char* baseUrl = "http://127.0.0.1:4000";
	char versionUrl[512];
	wsprintfA(versionUrl, "%s/update/version", baseUrl);

	char buffer[8192];
	DWORD size = 0;
	if (!HttpGetToBuffer(versionUrl, buffer, sizeof(buffer) - 1, &size))
	{
		MessageBoxA(hWnd, "Не удалось получить версию обновления", "Ошибка", MB_OK | MB_ICONERROR);
		return false;
	}

	char remoteVersion[64];
	if (!ParseVersionFromJson(buffer, remoteVersion, sizeof(remoteVersion)))
	{
		MessageBoxA(hWnd, "Неверный формат версии обновления", "Ошибка", MB_OK | MB_ICONERROR);
		return false;
	}

	char localVersion[64];
	bool haveLocal = LoadLocalVersion(clientDir, localVersion, sizeof(localVersion));
	if (haveLocal && VersionsEqual(localVersion, remoteVersion))
	{
		return true;
	}

	wsprintfA(buffer, "Будет установлено обновление клиента до версии %s", remoteVersion);
	MessageBoxA(hWnd, buffer, "Обновление", MB_OK | MB_ICONINFORMATION);

	if (!SaveLocalVersion(clientDir, remoteVersion))
	{
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

		HWND hPlay = CreateWindowExA(0, "BUTTON", "Играть", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			120, 100, 160, 40, hWnd, (HMENU)1, GetModuleHandle(NULL), NULL);
		if (hPlay && hFont)
		{
			SendMessage(hPlay, WM_SETFONT, (WPARAM)hFont, TRUE);
		}

		HWND hExit = CreateWindowExA(0, "BUTTON", "Выход", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			120, 160, 160, 40, hWnd, (HMENU)2, GetModuleHandle(NULL), NULL);
		if (hExit && hFont)
		{
			SendMessage(hExit, WM_SETFONT, (WPARAM)hFont, TRUE);
		}
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
	int height = 260;

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
