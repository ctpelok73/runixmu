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

static std::wstring BytesToWideBestEffort(const std::string& bytes, UINT codepage) {
	if (bytes.empty()) return std::wstring();
	{
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

static int RunGui() {
	const std::filesystem::path exeDir = GetExeDir();
	const std::filesystem::path outDir = exeDir;
	const UINT cp = 1251;

	int choice = MessageBoxW(
		nullptr,
		L"Да:  Export (Text.bmd + GlobalText.bmd -> out\\*.tsv)\n"
		L"Нет: Import (out\\*.tsv -> out\\*.new.bmd, merge поверх base)\n"
		L"Отмена: Выход\n\n"
		L"По умолчанию берется папка рядом с программой:\n"
		L"  Text.bmd / GlobalText.bmd / Text.tsv / GlobalText.tsv\n"
		L"Если рядом нет BMD, тогда попросит выбрать папку клиента.\n"
		L"Язык определится автоматически (приоритет Ru). Кодировка: 1251.",
		L"TextBmdTool",
		MB_YESNOCANCEL | MB_ICONQUESTION);

	if (choice == IDCANCEL) return 0;

	const auto localTextBmd = exeDir / L"Text.bmd";
	const auto localGlobalBmd = exeDir / L"GlobalText.bmd";
	const bool hasLocalTextBmd = std::filesystem::exists(localTextBmd);
	const bool hasLocalGlobalBmd = std::filesystem::exists(localGlobalBmd);

	std::optional<std::filesystem::path> resolvedDataDir;
	std::optional<std::optional<std::wstring>> langFolder;
	if (!hasLocalTextBmd && !hasLocalGlobalBmd) {
		auto picked = PickFolder(L"Выбери папку Data клиента (или корень клиента)");
		if (!picked) return 0;

		auto resolved = ResolveDataDir(*picked);
		if (resolved.empty()) {
			ShowErrorBox(L"Не удалось найти папку Data.\n\nВыбери папку, в которой есть:\n  Data\\Local\\...\nили саму папку Data (где есть Local).");
			return 1;
		}
		resolvedDataDir = resolved;

		auto detected = DetectLangFolder(*resolvedDataDir);
		if (!detected) {
			ShowErrorBox(L"Не удалось найти файлы:\n  Local\\<lang>\\Text.bmd\n  Local\\<lang>\\GlobalText.bmd\n\nПроверь структуру папок и язык.");
			return 1;
		}
		langFolder = detected;
	}

	std::wstring err;
	bool ok = false;
	if (choice == IDYES) {
		bool okAny = false;
		if (hasLocalTextBmd) {
			okAny = true;
			ok = ExportOne(localTextBmd, cp, outDir / L"Text.tsv");
			if (!ok) err = L"Export failed:\n" + localTextBmd.wstring();
		}
		if (ok && hasLocalGlobalBmd) {
			okAny = true;
			ok = ExportOne(localGlobalBmd, cp, outDir / L"GlobalText.tsv");
			if (!ok) err = L"Export failed:\n" + localGlobalBmd.wstring();
		}
		if (!okAny) {
			ok = ExportBoth(*resolvedDataDir, *langFolder, cp, outDir);
			if (!ok) {
				const auto textPath = MakeTextPath(*resolvedDataDir, *langFolder).wstring();
				const auto globalPath = MakeGlobalTextPath(*resolvedDataDir, *langFolder).wstring();
				err = L"Export failed.\n\nExpected:\n" + textPath + L"\n" + globalPath;
			}
		}
	} else if (choice == IDNO) {
		bool okAny = false;
		const auto localTextTsv = exeDir / L"Text.tsv";
		const auto localGlobalTsv = exeDir / L"GlobalText.tsv";
		if (hasLocalTextBmd && std::filesystem::exists(localTextTsv)) {
			okAny = true;
			ok = ImportOneMerge(localTextTsv, cp, localTextBmd, outDir / L"Text.new.bmd");
			if (!ok) err = L"Import failed:\n" + localTextTsv.wstring();
		}
		if (ok && hasLocalGlobalBmd && std::filesystem::exists(localGlobalTsv)) {
			okAny = true;
			ok = ImportOneMerge(localGlobalTsv, cp, localGlobalBmd, outDir / L"GlobalText.new.bmd");
			if (!ok) err = L"Import failed:\n" + localGlobalTsv.wstring();
		}
		if (!okAny) {
			ok = ImportBothMerge(*resolvedDataDir, *langFolder, cp, outDir);
			if (!ok) {
				err = L"Import failed.\n\nExpected:\n" + (outDir.wstring() + L"\\Text.tsv\n" + outDir.wstring() + L"\\GlobalText.tsv");
			}
		}
	}

	if (!ok) {
		ShowErrorBox(err.empty() ? L"Operation failed." : err);
		return 1;
	}

	std::wstring info = L"Done.\n\nFolder:\n" + exeDir.wstring();
	if (resolvedDataDir && langFolder) {
		std::wstring usedLang = (*langFolder)->empty() ? L"(no lang folder)" : **langFolder;
		info += L"\n\nData:\n" + resolvedDataDir->wstring() + L"\nLang:\n" + usedLang;
	}
	ShowInfoBox(info);
	return 0;
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
