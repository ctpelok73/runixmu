#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Gdi32.lib")

#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

static void BuxConvert(uint8_t* buffer, size_t size) {
	static const uint8_t key[3] = {0xFC, 0xCF, 0xAB};
	for (size_t i = 0; i < size; ++i) {
		buffer[i] ^= key[i % 3];
	}
}

static std::wstring ToWideFromCodepage(const std::string& bytes, UINT codepage) {
	if (bytes.empty()) return std::wstring();
	int required = MultiByteToWideChar(codepage, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
	if (required <= 0) return std::wstring();
	std::wstring wide(static_cast<size_t>(required), L'\0');
	MultiByteToWideChar(codepage, 0, bytes.data(), static_cast<int>(bytes.size()), wide.data(), required);
	return wide;
}

static bool IsValidUtf8(std::string_view s) {
	size_t i = 0;
	while (i < s.size()) {
		unsigned char c = static_cast<unsigned char>(s[i]);
		if (c <= 0x7F) {
			++i;
			continue;
		}
		if (c >= 0xC2 && c <= 0xDF) {
			if (i + 1 >= s.size()) return false;
			unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
			if ((c1 & 0xC0) != 0x80) return false;
			i += 2;
			continue;
		}
		if (c >= 0xE0 && c <= 0xEF) {
			if (i + 2 >= s.size()) return false;
			unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
			unsigned char c2 = static_cast<unsigned char>(s[i + 2]);
			if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) return false;
			if (c == 0xE0 && c1 < 0xA0) return false;
			if (c == 0xED && c1 >= 0xA0) return false;
			i += 3;
			continue;
		}
		if (c >= 0xF0 && c <= 0xF4) {
			if (i + 3 >= s.size()) return false;
			unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
			unsigned char c2 = static_cast<unsigned char>(s[i + 2]);
			unsigned char c3 = static_cast<unsigned char>(s[i + 3]);
			if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return false;
			if (c == 0xF0 && c1 < 0x90) return false;
			if (c == 0xF4 && c1 > 0x8F) return false;
			i += 4;
			continue;
		}
		return false;
	}
	return true;
}

static std::wstring BytesToWideBestEffort(const std::string& bytes, UINT codepage) {
	if (bytes.empty()) return std::wstring();
	{
		if (IsValidUtf8(bytes)) {
			std::wstring wide = ToWideFromCodepage(bytes, CP_UTF8);
			if (!wide.empty()) return wide;
		}
		std::wstring wide = ToWideFromCodepage(bytes, codepage);
		if (!wide.empty()) return wide;
	}
	{
		std::wstring wide = ToWideFromCodepage(bytes, CP_UTF8);
		if (!wide.empty()) return wide;
	}
	std::wstring wide;
	wide.reserve(bytes.size());
	for (unsigned char ch : bytes) {
		wide.push_back(static_cast<wchar_t>(ch));
	}
	return wide;
}

static std::string FromWideToCodepage(const std::wstring& wide, UINT codepage) {
	if (wide.empty()) return std::string();
	BOOL usedDefault = FALSE;
	int required = WideCharToMultiByte(codepage, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, "?", &usedDefault);
	if (required <= 0) {
		required = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, "?", &usedDefault);
		if (required <= 0) return std::string();
		codepage = CP_UTF8;
	}
	std::string bytes(static_cast<size_t>(required), '\0');
	WideCharToMultiByte(codepage, 0, wide.data(), static_cast<int>(wide.size()), bytes.data(), required, "?", &usedDefault);
	return bytes;
}

static std::string WideToUtf8(const std::wstring& wide) {
	return FromWideToCodepage(wide, CP_UTF8);
}

static std::wstring Utf8ToWide(const std::string& utf8) {
	return BytesToWideBestEffort(utf8, CP_UTF8);
}

static int HexToNibble(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
	if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
	return -1;
}

static std::string EscapeTsvField(const std::string& utf8) {
	std::string out;
	out.reserve(utf8.size());
	for (unsigned char ch : utf8) {
		switch (ch) {
		case '\\': out += "\\\\"; break;
		case '\t': out += "\\t"; break;
		case '\r': out += "\\r"; break;
		case '\n': out += "\\n"; break;
		default:
			if (ch < 0x20) {
				static const char* hexd = "0123456789ABCDEF";
				out += "\\x";
				out += hexd[(ch >> 4) & 0xF];
				out += hexd[ch & 0xF];
			} else {
				out.push_back(static_cast<char>(ch));
			}
		}
	}
	return out;
}

static std::string UnescapeTsvField(std::string_view s) {
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i) {
		char ch = s[i];
		if (ch != '\\') {
			out.push_back(ch);
			continue;
		}
		if (i + 1 >= s.size()) {
			out.push_back('\\');
			break;
		}
		char n = s[++i];
		switch (n) {
		case '\\': out.push_back('\\'); break;
		case 't': out.push_back('\t'); break;
		case 'r': out.push_back('\r'); break;
		case 'n': out.push_back('\n'); break;
		case 'x':
			if (i + 2 < s.size()) {
				int hi = HexToNibble(s[i + 1]);
				int lo = HexToNibble(s[i + 2]);
				if (hi >= 0 && lo >= 0) {
					out.push_back(static_cast<char>((hi << 4) | lo));
					i += 2;
				} else {
					out += "\\x";
				}
			} else {
				out += "\\x";
			}
			break;
		default:
			out.push_back(n);
			break;
		}
	}
	return out;
}

static std::vector<std::string> SplitTsvLine(const std::string& line) {
	std::vector<std::string> parts;
	std::string current;
	for (char ch : line) {
		if (ch == '\t') {
			parts.push_back(current);
			current.clear();
		} else {
			current.push_back(ch);
		}
	}
	parts.push_back(current);
	return parts;
}

#pragma pack(push, 1)
struct GlobalTextHeader {
	uint16_t signature;
	uint32_t count;
};
struct GlobalTextStringHeader {
	uint32_t key;
	uint32_t sizeOfString;
};
#pragma pack(pop)

using TextMap = std::map<uint32_t, std::string>;

static bool ReadExact(std::ifstream& in, void* dst, size_t bytes) {
	in.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(bytes));
	return in.good();
}

static bool WriteExact(std::ofstream& out, const void* src, size_t bytes) {
	out.write(reinterpret_cast<const char*>(src), static_cast<std::streamsize>(bytes));
	return out.good();
}

static std::optional<TextMap> LoadBmdTextMap(const std::filesystem::path& path, UINT sourceCodepage) {
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) return std::nullopt;

	GlobalTextHeader header{};
	if (!ReadExact(in, &header, sizeof(header))) return std::nullopt;
	if (header.signature != 0x5447) return std::nullopt;

	TextMap out;
	out.clear();

	for (uint32_t i = 0; i < header.count; ++i) {
		GlobalTextStringHeader sh{};
		if (!ReadExact(in, &sh, sizeof(sh))) return std::nullopt;
		std::vector<uint8_t> buf;
		buf.resize(static_cast<size_t>(sh.sizeOfString));
		if (sh.sizeOfString > 0) {
			if (!ReadExact(in, buf.data(), buf.size())) return std::nullopt;
			BuxConvert(buf.data(), buf.size());
		}
		std::string bytes(reinterpret_cast<const char*>(buf.data()), buf.size());
		std::wstring wide = BytesToWideBestEffort(bytes, sourceCodepage);
		std::string utf8 = WideToUtf8(wide);
		out[sh.key] = utf8;
	}

	return out;
}

static bool SaveBmdTextMap(const std::filesystem::path& path, const TextMap& mapUtf8, UINT targetCodepage) {
	std::ofstream out(path, std::ios::binary);
	if (!out.is_open()) return false;

	GlobalTextHeader header{};
	header.signature = 0x5447;
	header.count = static_cast<uint32_t>(mapUtf8.size());
	if (!WriteExact(out, &header, sizeof(header))) return false;

	for (const auto& [key, utf8] : mapUtf8) {
		std::wstring wide = Utf8ToWide(utf8);
		std::string bytes = FromWideToCodepage(wide, targetCodepage);
		GlobalTextStringHeader sh{};
		sh.key = key;
		sh.sizeOfString = static_cast<uint32_t>(bytes.size());
		if (!WriteExact(out, &sh, sizeof(sh))) return false;

		if (!bytes.empty()) {
			std::vector<uint8_t> buf(bytes.begin(), bytes.end());
			BuxConvert(buf.data(), buf.size());
			if (!WriteExact(out, buf.data(), buf.size())) return false;
		}
	}

	return out.good();
}

static std::optional<TextMap> LoadTsvMap(const std::filesystem::path& path) {
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) return std::nullopt;

	TextMap out;
	std::string line;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF && static_cast<unsigned char>(line[1]) == 0xBB &&
			static_cast<unsigned char>(line[2]) == 0xBF) {
			line.erase(0, 3);
		}
		if (line.empty()) continue;
		if (line.size() >= 1 && line[0] == '#') continue;

		auto parts = SplitTsvLine(line);
		if (parts.empty()) continue;

		char* endPtr = nullptr;
		uint32_t key = static_cast<uint32_t>(std::strtoul(parts[0].c_str(), &endPtr, 10));
		if (endPtr == parts[0].c_str() || *endPtr != '\0') continue;

		std::string value;
		if (parts.size() >= 2) {
			value = UnescapeTsvField(parts[1]);
		} else {
			value.clear();
		}

		out[key] = value;
	}

	return out;
}

static bool SaveTsvMap(const std::filesystem::path& path, const TextMap& mapUtf8) {
	std::ofstream out(path, std::ios::binary);
	if (!out.is_open()) return false;

	const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
	out.write(reinterpret_cast<const char*>(bom), 3);
	if (!out.good()) return false;

	for (const auto& [key, utf8] : mapUtf8) {
		out << key << '\t' << EscapeTsvField(utf8) << "\r\n";
		if (!out.good()) return false;
	}
	return true;
}

static void PrintUsage() {
	std::cerr
		<< "TextBmdTool gui\n"
		<< "TextBmdTool export <input.bmd> <output.tsv> [--cp <codepage>]\n"
		<< "TextBmdTool import <input.tsv> <output.bmd> [--cp <codepage>] [--base <base.bmd>]\n";
}

static void EnsureConsoleForCli() {
	if (GetConsoleWindow() != nullptr) return;

	if (AttachConsole(ATTACH_PARENT_PROCESS) == 0) {
		AllocConsole();
	}

	FILE* out = nullptr;
	FILE* err = nullptr;
	freopen_s(&out, "CONOUT$", "w", stdout);
	freopen_s(&err, "CONOUT$", "w", stderr);
	SetConsoleOutputCP(CP_UTF8);
}

static std::optional<UINT> ParseCodepageArg(int argc, wchar_t** argv) {
	for (int i = 0; i < argc; ++i) {
		if (std::wstring_view(argv[i]) == L"--cp" && i + 1 < argc) {
			wchar_t* endPtr = nullptr;
			unsigned long cp = std::wcstoul(argv[i + 1], &endPtr, 10);
			if (endPtr != argv[i + 1] && *endPtr == L'\0') return static_cast<UINT>(cp);
		}
	}
	return std::nullopt;
}

static std::optional<std::filesystem::path> ParsePathArg(int argc, wchar_t** argv, std::wstring_view name) {
	for (int i = 0; i < argc; ++i) {
		if (std::wstring_view(argv[i]) == name && i + 1 < argc) return std::filesystem::path(argv[i + 1]);
	}
	return std::nullopt;
}

static std::filesystem::path GetExeDir() {
	wchar_t buf[MAX_PATH]{};
	DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
	std::filesystem::path p(std::wstring(buf, buf + len));
	return p.parent_path();
}

static std::filesystem::path GetCwd() {
	wchar_t buf[MAX_PATH]{};
	DWORD len = GetCurrentDirectoryW(MAX_PATH, buf);
	if (len == 0) return std::filesystem::path();
	if (len >= MAX_PATH) return std::filesystem::path();
	return std::filesystem::path(std::wstring(buf, buf + len));
}

static std::optional<std::filesystem::path> PickFolder(const wchar_t* title) {
	BROWSEINFOW bi{};
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
	bi.lpszTitle = title;

	PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
	if (pidl == nullptr) return std::nullopt;

	wchar_t path[MAX_PATH]{};
	BOOL ok = SHGetPathFromIDListW(pidl, path);
	CoTaskMemFree(pidl);
	if (!ok) return std::nullopt;

	return std::filesystem::path(path);
}

static void ShowErrorBox(const std::wstring& text) {
	MessageBoxW(nullptr, text.c_str(), L"TextBmdTool", MB_ICONERROR | MB_OK);
}

static void ShowInfoBox(const std::wstring& text) {
	MessageBoxW(nullptr, text.c_str(), L"TextBmdTool", MB_ICONINFORMATION | MB_OK);
}

static std::filesystem::path ResolveDataDir(const std::filesystem::path& picked) {
	if (std::filesystem::exists(picked / L"Local")) return picked;
	if (std::filesystem::exists(picked / L"Data" / L"Local")) return picked / L"Data";
	if (std::filesystem::exists(picked / L"data" / L"Local")) return picked / L"data";
	return std::filesystem::path();
}

static std::optional<std::wstring> DetectLangFolder(const std::filesystem::path& dataDir) {
	const auto localDir = dataDir / L"Local";
	if (!std::filesystem::exists(localDir)) return std::nullopt;

	if (std::filesystem::exists(localDir / L"Ru" / L"Text.bmd") && std::filesystem::exists(localDir / L"Ru" / L"GlobalText.bmd")) {
		return std::wstring(L"Ru");
	}

	if (std::filesystem::exists(localDir / L"Text.bmd") && std::filesystem::exists(localDir / L"GlobalText.bmd")) {
		return std::wstring();
	}

	for (const auto& entry : std::filesystem::directory_iterator(localDir)) {
		if (!entry.is_directory()) continue;
		const auto candidate = entry.path();
		if (std::filesystem::exists(candidate / L"Text.bmd") && std::filesystem::exists(candidate / L"GlobalText.bmd")) {
			return candidate.filename().wstring();
		}
	}

	return std::nullopt;
}

static std::filesystem::path MakeTextPath(const std::filesystem::path& dataDir, const std::optional<std::wstring>& langFolder) {
	if (!langFolder.has_value()) return std::filesystem::path();
	if (langFolder->empty()) return dataDir / L"Local" / L"Text.bmd";
	return dataDir / L"Local" / *langFolder / L"Text.bmd";
}

static std::filesystem::path MakeGlobalTextPath(const std::filesystem::path& dataDir, const std::optional<std::wstring>& langFolder) {
	if (!langFolder.has_value()) return std::filesystem::path();
	if (langFolder->empty()) return dataDir / L"Local" / L"GlobalText.bmd";
	return dataDir / L"Local" / *langFolder / L"GlobalText.bmd";
}

static bool ExportBoth(const std::filesystem::path& dataDir, const std::optional<std::wstring>& langFolder, UINT cp, const std::filesystem::path& outDir) {
	std::filesystem::create_directories(outDir);

	std::filesystem::path textBmd = MakeTextPath(dataDir, langFolder);
	std::filesystem::path globalBmd = MakeGlobalTextPath(dataDir, langFolder);
	std::filesystem::path textTsv = outDir / L"Text.tsv";
	std::filesystem::path globalTsv = outDir / L"GlobalText.tsv";

	auto textMap = LoadBmdTextMap(textBmd, cp);
	if (!textMap) return false;
	if (!SaveTsvMap(textTsv, *textMap)) return false;

	auto globalMap = LoadBmdTextMap(globalBmd, cp);
	if (!globalMap) return false;
	if (!SaveTsvMap(globalTsv, *globalMap)) return false;

	return true;
}

static bool ImportBothMerge(const std::filesystem::path& dataDir, const std::optional<std::wstring>& langFolder, UINT cp, const std::filesystem::path& outDir) {
	std::filesystem::create_directories(outDir);

	std::filesystem::path baseText = MakeTextPath(dataDir, langFolder);
	std::filesystem::path baseGlobal = MakeGlobalTextPath(dataDir, langFolder);

	std::filesystem::path inTextTsv = outDir / L"Text.tsv";
	std::filesystem::path inGlobalTsv = outDir / L"GlobalText.tsv";

	std::filesystem::path outTextBmd = outDir / L"Text.new.bmd";
	std::filesystem::path outGlobalBmd = outDir / L"GlobalText.new.bmd";

	auto updatesText = LoadTsvMap(inTextTsv);
	if (!updatesText) return false;
	auto updatesGlobal = LoadTsvMap(inGlobalTsv);
	if (!updatesGlobal) return false;

	auto baseTextMap = LoadBmdTextMap(baseText, cp);
	if (!baseTextMap) return false;
	for (const auto& [key, value] : *updatesText) {
		(*baseTextMap)[key] = value;
	}
	if (!SaveBmdTextMap(outTextBmd, *baseTextMap, cp)) return false;

	auto baseGlobalMap = LoadBmdTextMap(baseGlobal, cp);
	if (!baseGlobalMap) return false;
	for (const auto& [key, value] : *updatesGlobal) {
		(*baseGlobalMap)[key] = value;
	}
	if (!SaveBmdTextMap(outGlobalBmd, *baseGlobalMap, cp)) return false;

	return true;
}

static bool ExportOne(const std::filesystem::path& inputBmd, UINT cp, const std::filesystem::path& outputTsv) {
	auto mapOpt = LoadBmdTextMap(inputBmd, cp);
	if (!mapOpt) return false;
	return SaveTsvMap(outputTsv, *mapOpt);
}

static bool ImportOneMerge(const std::filesystem::path& inputTsv, UINT cp, const std::filesystem::path& baseBmd, const std::filesystem::path& outputBmd) {
	auto updatesOpt = LoadTsvMap(inputTsv);
	if (!updatesOpt) return false;

	auto baseOpt = LoadBmdTextMap(baseBmd, cp);
	if (!baseOpt) return false;

	TextMap merged = std::move(*baseOpt);
	for (const auto& [key, value] : *updatesOpt) {
		merged[key] = value;
	}
	return SaveBmdTextMap(outputBmd, merged, cp);
}

static std::optional<std::filesystem::path> PickOpenBmdFile(const std::filesystem::path& initialDir) {
	wchar_t fileBuf[MAX_PATH]{};

	OPENFILENAMEW ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = L"BMD files (*.bmd)\0*.bmd\0All files (*.*)\0*.*\0";
	ofn.lpstrFile = fileBuf;
	ofn.nMaxFile = MAX_PATH;
	std::wstring initial = initialDir.wstring();
	ofn.lpstrInitialDir = initial.empty() ? nullptr : initial.c_str();
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetOpenFileNameW(&ofn)) return std::nullopt;
	return std::filesystem::path(fileBuf);
}

static std::filesystem::path MakeNewBmdPath(const std::filesystem::path& inputBmd) {
	std::filesystem::path out = inputBmd;
	auto stem = inputBmd.stem().wstring();
	auto ext = inputBmd.extension().wstring();
	out.replace_filename(stem + L".new" + ext);
	return out;
}

struct EditorCtx {
	HWND hwnd = nullptr;
	HWND list = nullptr;
	HWND edit = nullptr;
	HWND btnOpen = nullptr;
	HWND btnSave = nullptr;
	HWND btnTranslate = nullptr;
	HWND label = nullptr;
	HWND labelCp = nullptr;
	HWND comboCp = nullptr;
	HWND labelSl = nullptr;
	HWND comboSl = nullptr;
	HWND labelTl = nullptr;
	HWND comboTl = nullptr;
	HFONT font = nullptr;
	UINT cp = 1251;
	std::filesystem::path inputPath;
	TextMap mapUtf8;
	std::vector<uint32_t> keys;
	std::optional<uint32_t> currentKey;
};

struct LangItem {
	const wchar_t* label;
	const wchar_t* code;
};

static const LangItem kLangs[] = {
	{L"Auto (auto)", L"auto"},
	{L"Русский (ru)", L"ru"},
	{L"English (en)", L"en"},
	{L"Tiếng Việt (vi)", L"vi"},
	{L"Deutsch (de)", L"de"},
	{L"Français (fr)", L"fr"},
	{L"Español (es)", L"es"},
	{L"Italiano (it)", L"it"},
	{L"Português (pt)", L"pt"},
	{L"Polski (pl)", L"pl"},
	{L"Türkçe (tr)", L"tr"},
	{L"ไทย (th)", L"th"},
	{L"中文(简体) (zh-CN)", L"zh-CN"},
	{L"日本語 (ja)", L"ja"},
	{L"한국어 (ko)", L"ko"},
};

static void FillLangCombo(HWND combo, bool includeAuto, const wchar_t* defaultCode) {
	SendMessageW(combo, CB_RESETCONTENT, 0, 0);
	int defaultIndex = 0;
	int visualIndex = 0;
	for (const auto& li : kLangs) {
		if (!includeAuto && wcscmp(li.code, L"auto") == 0) continue;
		int idx = static_cast<int>(SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(li.label)));
		SendMessageW(combo, CB_SETITEMDATA, idx, reinterpret_cast<LPARAM>(li.code));
		if (defaultCode && wcscmp(li.code, defaultCode) == 0) defaultIndex = visualIndex;
		++visualIndex;
	}
	SendMessageW(combo, CB_SETCURSEL, defaultIndex, 0);
}

static const wchar_t* GetSelectedLangCode(HWND combo, const wchar_t* fallback) {
	int sel = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
	if (sel == CB_ERR) return fallback;
	auto data = SendMessageW(combo, CB_GETITEMDATA, sel, 0);
	if (data == CB_ERR) return fallback;
	return reinterpret_cast<const wchar_t*>(data);
}

static std::wstring CpToLabel(UINT cp) {
	switch (cp) {
	case 1251: return L"1251";
	case 65001: return L"65001";
	case 1252: return L"1252";
	default: break;
	}
	return std::to_wstring(cp);
}

static void SetWindowTitle(EditorCtx& ctx) {
	std::wstring title = L"TextBmdTool";
	if (!ctx.inputPath.empty()) {
		title += L" - ";
		title += ctx.inputPath.filename().wstring();
		title += L" (cp ";
		title += CpToLabel(ctx.cp);
		title += L")";
	}
	SetWindowTextW(ctx.hwnd, title.c_str());
}

static bool CommitCurrentEdit(EditorCtx& ctx) {
	if (!ctx.currentKey.has_value()) return true;
	if (!IsWindow(ctx.edit)) return true;

	int len = GetWindowTextLengthW(ctx.edit);
	if (len < 0) return false;
	std::wstring wide(static_cast<size_t>(len) + 1, L'\0');
	if (len > 0) {
		GetWindowTextW(ctx.edit, wide.data(), len + 1);
	}
	if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
	std::string utf8 = WideToUtf8(wide);
	ctx.mapUtf8[*ctx.currentKey] = utf8;
	return true;
}

static std::wstring GetEditText(EditorCtx& ctx) {
	if (!IsWindow(ctx.edit)) return std::wstring();
	int len = GetWindowTextLengthW(ctx.edit);
	if (len <= 0) return std::wstring();
	std::wstring wide(static_cast<size_t>(len) + 1, L'\0');
	GetWindowTextW(ctx.edit, wide.data(), len + 1);
	if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
	return wide;
}

static void LoadKeyIntoEdit(EditorCtx& ctx, uint32_t key) {
	auto it = ctx.mapUtf8.find(key);
	std::wstring wide;
	if (it != ctx.mapUtf8.end()) {
		wide = Utf8ToWide(it->second);
	}
	SetWindowTextW(ctx.edit, wide.c_str());
	ctx.currentKey = key;
}

static bool LoadBmdIntoUi(EditorCtx& ctx, const std::filesystem::path& bmdPath) {
	auto mapOpt = LoadBmdTextMap(bmdPath, ctx.cp);
	if (!mapOpt) return false;

	ctx.inputPath = bmdPath;
	ctx.mapUtf8 = std::move(*mapOpt);
	ctx.keys.clear();
	ctx.keys.reserve(ctx.mapUtf8.size());
	for (const auto& [k, _] : ctx.mapUtf8) ctx.keys.push_back(k);
	std::sort(ctx.keys.begin(), ctx.keys.end());

	SendMessageW(ctx.list, LB_RESETCONTENT, 0, 0);
	for (uint32_t k : ctx.keys) {
		wchar_t buf[64]{};
		wsprintfW(buf, L"%u", static_cast<unsigned>(k));
		int idx = static_cast<int>(SendMessageW(ctx.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(buf)));
		SendMessageW(ctx.list, LB_SETITEMDATA, idx, static_cast<LPARAM>(k));
	}

	if (!ctx.keys.empty()) {
		SendMessageW(ctx.list, LB_SETCURSEL, 0, 0);
		LoadKeyIntoEdit(ctx, ctx.keys[0]);
	} else {
		SetWindowTextW(ctx.edit, L"");
		ctx.currentKey.reset();
	}

	if (IsWindow(ctx.label)) {
		std::wstring label = L"File: " + ctx.inputPath.wstring();
		SetWindowTextW(ctx.label, label.c_str());
	}
	EnableWindow(ctx.btnSave, TRUE);
	SetWindowTitle(ctx);
	return true;
}

static std::string UrlEncodeUtf8(const std::string& s) {
	static const char* hex = "0123456789ABCDEF";
	std::string out;
	out.reserve(s.size() * 3);
	for (unsigned char c : s) {
		bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
			c == '~';
		if (unreserved) {
			out.push_back(static_cast<char>(c));
		} else {
			out.push_back('%');
			out.push_back(hex[(c >> 4) & 0xF]);
			out.push_back(hex[c & 0xF]);
		}
	}
	return out;
}

static void OpenTranslate(EditorCtx& ctx) {
	std::wstring text = GetEditText(ctx);
	if (text.empty()) {
		ShowErrorBox(L"Строка пустая.");
		return;
	}

	const wchar_t* sl = GetSelectedLangCode(ctx.comboSl, L"auto");
	const wchar_t* tl = GetSelectedLangCode(ctx.comboTl, L"ru");

	std::string url = "https://translate.google.com/?sl=";
	url += UrlEncodeUtf8(WideToUtf8(std::wstring(sl)));
	url += "&tl=";
	url += UrlEncodeUtf8(WideToUtf8(std::wstring(tl)));
	url += "&text=";
	url += UrlEncodeUtf8(WideToUtf8(text));
	url += "&op=translate";

	std::wstring wurl = Utf8ToWide(url);
	auto ret = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", wurl.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
	if (ret <= 32) {
		ShowErrorBox(L"Не удалось открыть браузер.");
	}
}

static void ApplyUiFont(EditorCtx& ctx) {
	NONCLIENTMETRICSW ncm{};
	ncm.cbSize = sizeof(ncm);
	if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) return;
	if (ctx.font) DeleteObject(ctx.font);
	ctx.font = CreateFontIndirectW(&ncm.lfMessageFont);
	if (!ctx.font) return;

	const HWND controls[] = {ctx.btnOpen, ctx.btnSave, ctx.btnTranslate, ctx.label, ctx.labelSl, ctx.comboSl, ctx.labelTl, ctx.comboTl, ctx.labelCp,
		ctx.comboCp, ctx.list, ctx.edit};
	for (HWND h : controls) {
		if (IsWindow(h)) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(ctx.font), TRUE);
	}
}

static int CpIndexFromValue(UINT cp) {
	switch (cp) {
	case 1251: return 0;
	case 65001: return 1;
	case 1252: return 2;
	default: return 0;
	}
}

static UINT CpValueFromIndex(int idx) {
	switch (idx) {
	case 0: return 1251;
	case 1: return 65001;
	case 2: return 1252;
	default: return 1251;
	}
}

static UINT DetectBmdCodepageLikely(const std::filesystem::path& bmdPath, UINT fallbackCp) {
	std::ifstream in(bmdPath, std::ios::binary);
	if (!in.is_open()) return fallbackCp;

	GlobalTextHeader header{};
	if (!ReadExact(in, &header, sizeof(header))) return fallbackCp;
	if (header.signature != 0x5447) return fallbackCp;

	uint32_t checked = 0;
	uint32_t utf8Likely = 0;
	const uint32_t maxCheck = 200;
	for (uint32_t i = 0; i < header.count && checked < maxCheck; ++i) {
		GlobalTextStringHeader sh{};
		if (!ReadExact(in, &sh, sizeof(sh))) break;
		std::vector<uint8_t> buf;
		buf.resize(static_cast<size_t>(sh.sizeOfString));
		if (sh.sizeOfString > 0) {
			if (!ReadExact(in, buf.data(), buf.size())) break;
			BuxConvert(buf.data(), buf.size());
		}
		std::string bytes(reinterpret_cast<const char*>(buf.data()), buf.size());
		bool hasNonAscii = false;
		for (unsigned char ch : bytes) {
			if (ch >= 0x80) {
				hasNonAscii = true;
				break;
			}
		}
		if (hasNonAscii && IsValidUtf8(bytes)) ++utf8Likely;
		++checked;
	}

	if (checked >= 8 && utf8Likely * 3 >= checked) return 65001;
	return fallbackCp;
}

static void DoOpen(EditorCtx& ctx) {
	auto picked = PickOpenBmdFile(ctx.inputPath.empty() ? GetExeDir() : ctx.inputPath.parent_path());
	if (!picked) return;
	UINT detected = DetectBmdCodepageLikely(*picked, ctx.cp);
	if (detected != ctx.cp) {
		ctx.cp = detected;
		SendMessageW(ctx.comboCp, CB_SETCURSEL, CpIndexFromValue(ctx.cp), 0);
		SetWindowTitle(ctx);
	}
	if (!LoadBmdIntoUi(ctx, *picked)) {
		ShowErrorBox(L"Не удалось прочитать BMD.");
	}
}

static void DoSaveNew(EditorCtx& ctx) {
	if (ctx.inputPath.empty()) return;
	if (!CommitCurrentEdit(ctx)) {
		ShowErrorBox(L"Ошибка чтения текста из поля.");
		return;
	}
	auto outPath = MakeNewBmdPath(ctx.inputPath);
	if (!SaveBmdTextMap(outPath, ctx.mapUtf8, ctx.cp)) {
		ShowErrorBox(L"Не удалось сохранить новый BMD.");
		return;
	}
	ShowInfoBox(L"Сохранено:\n" + outPath.wstring());
}

static LRESULT CALLBACK EditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	EditorCtx* ctx = reinterpret_cast<EditorCtx*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	switch (msg) {
	case WM_CREATE: {
		auto* created = new EditorCtx();
		created->hwnd = hwnd;
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
		ctx = created;

		ctx->btnOpen = CreateWindowExW(0, L"BUTTON", L"Open BMD", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 24, hwnd,
			reinterpret_cast<HMENU>(1001), GetModuleHandleW(nullptr), nullptr);
		ctx->btnSave = CreateWindowExW(0, L"BUTTON", L"Save New", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 24, hwnd,
			reinterpret_cast<HMENU>(1002), GetModuleHandleW(nullptr), nullptr);
		EnableWindow(ctx->btnSave, FALSE);

		ctx->btnTranslate = CreateWindowExW(0, L"BUTTON", L"Translate", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 24, hwnd,
			reinterpret_cast<HMENU>(1008), GetModuleHandleW(nullptr), nullptr);

		ctx->label = CreateWindowExW(0, L"STATIC", L"File: (none)", WS_CHILD | WS_VISIBLE, 0, 0, 100, 18, hwnd,
			reinterpret_cast<HMENU>(1003), GetModuleHandleW(nullptr), nullptr);

		ctx->labelSl = CreateWindowExW(0, L"STATIC", L"From:", WS_CHILD | WS_VISIBLE, 0, 0, 100, 18, hwnd,
			reinterpret_cast<HMENU>(1009), GetModuleHandleW(nullptr), nullptr);
		ctx->comboSl = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 160, 240, hwnd,
			reinterpret_cast<HMENU>(1010), GetModuleHandleW(nullptr), nullptr);
		ctx->labelTl = CreateWindowExW(0, L"STATIC", L"To:", WS_CHILD | WS_VISIBLE, 0, 0, 100, 18, hwnd,
			reinterpret_cast<HMENU>(1011), GetModuleHandleW(nullptr), nullptr);
		ctx->comboTl = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 160, 240, hwnd,
			reinterpret_cast<HMENU>(1012), GetModuleHandleW(nullptr), nullptr);
		FillLangCombo(ctx->comboSl, true, L"auto");
		FillLangCombo(ctx->comboTl, false, L"ru");

		ctx->labelCp = CreateWindowExW(0, L"STATIC", L"Codepage:", WS_CHILD | WS_VISIBLE, 0, 0, 100, 18, hwnd,
			reinterpret_cast<HMENU>(1006), GetModuleHandleW(nullptr), nullptr);
		ctx->comboCp = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 180, 200, hwnd,
			reinterpret_cast<HMENU>(1007), GetModuleHandleW(nullptr), nullptr);
		SendMessageW(ctx->comboCp, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1251 (Cyrillic)"));
		SendMessageW(ctx->comboCp, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"65001 (UTF-8)"));
		SendMessageW(ctx->comboCp, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1252 (Latin)"));
		SendMessageW(ctx->comboCp, CB_SETCURSEL, CpIndexFromValue(ctx->cp), 0);

		ctx->list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 0, 0, 200, 400,
			hwnd, reinterpret_cast<HMENU>(1004), GetModuleHandleW(nullptr), nullptr);

		ctx->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 0, 0, 400, 400, hwnd,
			reinterpret_cast<HMENU>(1005), GetModuleHandleW(nullptr), nullptr);

		ApplyUiFont(*ctx);
		SetWindowTitle(*ctx);

		const auto exeDir = GetExeDir();
		const auto autoText = exeDir / L"Text.bmd";
		const auto autoGlobal = exeDir / L"GlobalText.bmd";
		if (std::filesystem::exists(autoText)) {
			ctx->cp = DetectBmdCodepageLikely(autoText, ctx->cp);
			SendMessageW(ctx->comboCp, CB_SETCURSEL, CpIndexFromValue(ctx->cp), 0);
			SetWindowTitle(*ctx);
			LoadBmdIntoUi(*ctx, autoText);
		} else if (std::filesystem::exists(autoGlobal)) {
			ctx->cp = DetectBmdCodepageLikely(autoGlobal, ctx->cp);
			SendMessageW(ctx->comboCp, CB_SETCURSEL, CpIndexFromValue(ctx->cp), 0);
			SetWindowTitle(*ctx);
			LoadBmdIntoUi(*ctx, autoGlobal);
		} else {
			DoOpen(*ctx);
		}
		return 0;
	}
	case WM_SIZE: {
		if (!ctx) break;
		int w = LOWORD(lParam);
		int h = HIWORD(lParam);
		int pad = 8;
		int top = pad;

		int btnH = 24;
		int labelH = 18;
		int btnW = 110;
		int comboH = 240;

		MoveWindow(ctx->btnOpen, pad, top, btnW, btnH, TRUE);
		MoveWindow(ctx->btnSave, pad + btnW + pad, top, btnW, btnH, TRUE);
		MoveWindow(ctx->btnTranslate, pad + (btnW + pad) * 2, top, btnW, btnH, TRUE);

		int slLabelW = 40;
		int slW = 160;
		int tlLabelW = 25;
		int tlW = 160;
		int cpLabelW = 70;
		int cpW = 170;

		int minRightX = pad + (btnW + pad) * 3 + pad;
		int totalRightW = slLabelW + pad + slW + pad + tlLabelW + pad + tlW + pad + cpLabelW + pad + cpW;
		int startX = w - pad - totalRightW;
		if (startX < minRightX) startX = minRightX;

		int x = startX;
		MoveWindow(ctx->labelSl, x, top + 3, slLabelW, labelH, TRUE);
		x += slLabelW + pad;
		MoveWindow(ctx->comboSl, x, top, slW, comboH, TRUE);
		x += slW + pad;
		MoveWindow(ctx->labelTl, x, top + 3, tlLabelW, labelH, TRUE);
		x += tlLabelW + pad;
		MoveWindow(ctx->comboTl, x, top, tlW, comboH, TRUE);
		x += tlW + pad;
		MoveWindow(ctx->labelCp, x, top + 3, cpLabelW, labelH, TRUE);
		x += cpLabelW + pad;
		MoveWindow(ctx->comboCp, x, top, cpW, comboH, TRUE);
		top += btnH + pad;

		MoveWindow(ctx->label, pad, top, w - 2 * pad, labelH, TRUE);
		top += labelH + pad;

		int listW = 140;
		int contentH = h - top - pad;
		if (contentH < 0) contentH = 0;
		MoveWindow(ctx->list, pad, top, listW, contentH, TRUE);
		int editW = w - (pad + listW + 2 * pad);
		if (editW < 0) editW = 0;
		MoveWindow(ctx->edit, pad + listW + pad, top, editW, contentH, TRUE);
		return 0;
	}
	case WM_COMMAND: {
		if (!ctx) break;
		WORD id = LOWORD(wParam);
		WORD code = HIWORD(wParam);

		if (id == 1001 && code == BN_CLICKED) {
			CommitCurrentEdit(*ctx);
			DoOpen(*ctx);
			return 0;
		}
		if (id == 1002 && code == BN_CLICKED) {
			DoSaveNew(*ctx);
			return 0;
		}
		if (id == 1008 && code == BN_CLICKED) {
			OpenTranslate(*ctx);
			return 0;
		}
		if (id == 1007 && code == CBN_SELCHANGE) {
			int sel = static_cast<int>(SendMessageW(ctx->comboCp, CB_GETCURSEL, 0, 0));
			if (sel == CB_ERR) return 0;
			UINT newCp = CpValueFromIndex(sel);
			if (newCp == ctx->cp) return 0;

			UINT oldCp = ctx->cp;
			if (!ctx->inputPath.empty()) {
				int ans = MessageBoxW(hwnd, L"Сменить кодировку и перечитать файл?\nНесохраненные изменения будут потеряны.", L"TextBmdTool",
					MB_YESNO | MB_ICONQUESTION);
				if (ans != IDYES) {
					SendMessageW(ctx->comboCp, CB_SETCURSEL, CpIndexFromValue(oldCp), 0);
					return 0;
				}
			}

			ctx->cp = newCp;
			SetWindowTitle(*ctx);
			if (!ctx->inputPath.empty()) {
				if (!LoadBmdIntoUi(*ctx, ctx->inputPath)) {
					ShowErrorBox(L"Не удалось перечитать BMD с выбранной кодировкой.");
				}
			}
			return 0;
		}
		if (id == 1004 && code == LBN_SELCHANGE) {
			int sel = static_cast<int>(SendMessageW(ctx->list, LB_GETCURSEL, 0, 0));
			if (sel != LB_ERR) {
				CommitCurrentEdit(*ctx);
				auto key = static_cast<uint32_t>(SendMessageW(ctx->list, LB_GETITEMDATA, sel, 0));
				LoadKeyIntoEdit(*ctx, key);
			}
			return 0;
		}
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_NCDESTROY:
		if (ctx) {
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
			if (ctx->font) DeleteObject(ctx->font);
			delete ctx;
		}
		return 0;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static int RunGui() {
	WNDCLASSW wc{};
	wc.lpfnWndProc = EditorWndProc;
	wc.hInstance = GetModuleHandleW(nullptr);
	wc.lpszClassName = L"TextBmdToolEditorWnd";
	wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	RegisterClassW(&wc);

	HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"TextBmdTool", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 900, 600,
		nullptr, nullptr, wc.hInstance, nullptr);
	if (!hwnd) return 1;

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return static_cast<int>(msg.wParam);
}

static int RunCli(int argc, wchar_t** argv) {
	if (argc < 2) {
		PrintUsage();
		return 2;
	}

	std::wstring command = argv[1];
	UINT cp = ParseCodepageArg(argc, argv).value_or(1252);

	if (command == L"gui") {
		return RunGui();
	}

	if (command == L"export") {
		if (argc < 4) {
			PrintUsage();
			return 2;
		}
		std::filesystem::path inputBmd = argv[2];
		std::filesystem::path outputTsv = argv[3];
		auto mapOpt = LoadBmdTextMap(inputBmd, cp);
		if (!mapOpt) {
			std::cerr << "Failed to read BMD: " << inputBmd.u8string() << "\n";
			return 1;
		}
		if (!SaveTsvMap(outputTsv, *mapOpt)) {
			std::cerr << "Failed to write TSV: " << outputTsv.u8string() << "\n";
			return 1;
		}
		return 0;
	}

	if (command == L"import") {
		if (argc < 4) {
			PrintUsage();
			return 2;
		}
		std::filesystem::path inputTsv = argv[2];
		std::filesystem::path outputBmd = argv[3];
		auto baseBmd = ParsePathArg(argc, argv, L"--base");

		auto updatesOpt = LoadTsvMap(inputTsv);
		if (!updatesOpt) {
			std::cerr << "Failed to read TSV: " << inputTsv.u8string() << "\n";
			return 1;
		}

		TextMap merged;
		if (baseBmd) {
			auto baseOpt = LoadBmdTextMap(*baseBmd, cp);
			if (!baseOpt) {
				std::cerr << "Failed to read base BMD: " << baseBmd->u8string() << "\n";
				return 1;
			}
			merged = std::move(*baseOpt);
			for (const auto& [key, value] : *updatesOpt) {
				merged[key] = value;
			}
		} else {
			merged = std::move(*updatesOpt);
		}

		if (!SaveBmdTextMap(outputBmd, merged, cp)) {
			std::cerr << "Failed to write BMD: " << outputBmd.u8string() << "\n";
			return 1;
		}
		return 0;
	}

	PrintUsage();
	return 2;
}

int wmain(int argc, wchar_t** argv) {
	EnsureConsoleForCli();
	return RunCli(argc, argv);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv == nullptr) return 1;

	int exitCode = 0;
	if (argc <= 1 || (argc >= 2 && std::wstring_view(argv[1]) == L"gui")) {
		exitCode = RunGui();
	} else {
		EnsureConsoleForCli();
		exitCode = RunCli(argc, argv);
	}

	LocalFree(argv);
	return exitCode;
}
