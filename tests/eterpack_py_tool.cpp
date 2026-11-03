// MT2009 - EterPack (.data/.index) arac: motorun KENDI CEterPack kodunu kullanarak
// root/locale arsivlerini cikarir, yeniden paketler ve dogrular. (Native x64, MSVC).
// PackMakerLite.exe 32-bit (i386) oldugu icin kullanilmaz; format tek kaynaktan
// (client/src/EterPack) gelir, boylece yukleyici ile bire bir uyumludur.
//
//   eterpack_py_tool extract <packbase> <outdir>
//   eterpack_py_tool pack    <srcdir>   <packbase>   (tip 2 = SECURITY, indeks sifreli)
//   eterpack_py_tool verify  <packbase> <srcdir>     (arsiv icerigi == dizin icerigi)
//   eterpack_py_tool list    <packbase>
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

#include "../client/src/EterBase/lzo.h"
#include "../client/src/EterBase/Timer.h"
#include "../client/src/EterPack/EterPackManager.h"

namespace fs = std::filesystem;

static std::string NormName(const fs::path& rel)
{
	std::string s = rel.generic_string();
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

static bool ReadAll(const fs::path& p, std::string& out)
{
	std::ifstream f(p, std::ios::binary);
	if (!f) return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

static int DoExtract(const char* base, const char* outDir)
{
	CEterFileDict dict;
	CEterPack pack;
	if (!pack.Create(dict, base, "", true)) { std::fprintf(stderr, "Create failed: %s\n", base); return 2; }
	int n = 0, bad = 0;
	for (const auto& kv : pack.GetIndexMap())
	{
		if (!kv.second || !kv.second->filename[0]) continue;
		CMappedFile mf;
		LPCVOID data = nullptr;
		if (!pack.Get2(mf, kv.second->filename, kv.second, &data)) { std::fprintf(stderr, "GET FAIL %s\n", kv.second->filename); ++bad; continue; }
		fs::path dst = fs::path(outDir) / kv.second->filename;
		fs::create_directories(dst.parent_path());
		std::ofstream o(dst, std::ios::binary);
		o.write(static_cast<const char*>(data), mf.Size());
		++n;
	}
	std::printf("EXTRACT files=%d bad=%d type_note=see list\n", n, bad);
	return bad ? 3 : 0;
}

static int DoList(const char* base)
{
	CEterFileDict dict;
	CEterPack pack;
	if (!pack.Create(dict, base, "", true)) return 2;
	std::map<std::string, TEterPackIndex*> sorted;
	for (const auto& kv : pack.GetIndexMap())
		if (kv.second && kv.second->filename[0]) sorted[kv.second->filename] = kv.second;
	std::map<int, int> types;
	for (auto& kv : sorted)
	{
		std::printf("%d\t%ld\t%ld\t%s\n", (int)kv.second->compressed_type, kv.second->real_data_size, kv.second->data_size, kv.first.c_str());
		++types[kv.second->compressed_type];
	}
	for (auto& t : types) std::printf("TYPE %d count=%d\n", t.first, t.second);
	std::printf("TOTAL %zu\n", sorted.size());
	return 0;
}

static int DoPack(const char* srcDir, const char* base)
{
	std::string idx = std::string(base) + ".index", dat = std::string(base) + ".data";
	if (fs::exists(idx) || fs::exists(dat)) { std::fprintf(stderr, "Hedef arsiv zaten var, uzerine yazilmaz: %s\n", base); return 4; }
	CEterFileDict dict;
	CEterPack pack;
	if (!pack.Create(dict, base, "", false)) { std::fprintf(stderr, "Create(write) failed\n"); return 2; }
	std::vector<fs::path> files;
	for (auto& e : fs::recursive_directory_iterator(srcDir))
		if (e.is_regular_file()) files.push_back(e.path());
	std::sort(files.begin(), files.end());
	int n = 0;
	for (auto& p : files)
	{
		std::string name = NormName(fs::relative(p, srcDir));
		std::string data;
		if (!ReadAll(p, data)) { std::fprintf(stderr, "READ FAIL %s\n", name.c_str()); return 3; }
		if (!pack.Put(name.c_str(), data.data(), static_cast<long>(data.size()), COMPRESSED_TYPE_SECURITY))
		{ std::fprintf(stderr, "PUT FAIL %s\n", name.c_str()); return 3; }
		++n;
	}
	if (!pack.EncryptIndexFile()) { std::fprintf(stderr, "EncryptIndexFile failed\n"); return 5; }
	std::printf("PACK files=%d\n", n);
	return 0;
}

static int DoVerify(const char* base, const char* srcDir)
{
	CEterFileDict dict;
	CEterPack pack;
	if (!pack.Create(dict, base, "", true)) { std::fprintf(stderr, "Create failed: %s\n", base); return 2; }
	std::map<std::string, std::string> disk;
	for (auto& e : fs::recursive_directory_iterator(srcDir))
		if (e.is_regular_file()) { std::string d; ReadAll(e.path(), d); disk[NormName(fs::relative(e.path(), srcDir))] = std::move(d); }
	int same = 0, diff = 0, missingInDisk = 0, entries = 0;
	for (const auto& kv : pack.GetIndexMap())
	{
		if (!kv.second || !kv.second->filename[0]) continue;
		++entries;
		CMappedFile mf;
		LPCVOID data = nullptr;
		if (!pack.Get2(mf, kv.second->filename, kv.second, &data)) { ++diff; continue; }
		auto it = disk.find(kv.second->filename);
		if (it == disk.end()) { ++missingInDisk; continue; }
		if (it->second.size() == mf.Size() && std::memcmp(it->second.data(), data, mf.Size()) == 0) ++same; else ++diff;
		disk.erase(it);
	}
	std::printf("VERIFY pack_entries=%d identical=%d different=%d pack_only=%d disk_only=%zu\n", entries, same, diff, missingInDisk, disk.size());
	return (diff || missingInDisk || !disk.empty()) ? 1 : 0;
}

int main(int argc, char** argv)
{
	if (argc < 3) { std::fprintf(stderr, "usage: extract|pack|verify|list ...\n"); return 64; }
	CTimer timer;
	ELTimer_Init();
	CLZO lzo;
	std::string cmd = argv[1];
	if (cmd == "extract" && argc == 4) return DoExtract(argv[2], argv[3]);
	if (cmd == "pack" && argc == 4) return DoPack(argv[2], argv[3]);
	if (cmd == "verify" && argc == 4) return DoVerify(argv[2], argv[3]);
	if (cmd == "list" && argc == 3) return DoList(argv[2]);
	std::fprintf(stderr, "bad args\n");
	return 64;
}
