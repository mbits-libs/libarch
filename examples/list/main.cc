// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/archive.hh>
#include <arch/io/file.hh>
#include <concepts>
#include "colors.hh"
#include "dirent.hh"

#ifdef _WIN32
#include <Windows.h>
#undef min
#undef max
#endif

template <typename String>
using path_add_cref =
    std::conditional_t<std::is_same_v<String, arch::fs::path::string_type>,
                       String const&,
                       String>;

#ifdef _WIN32
std::string narrow(std::wstring const& s) {
	size_t size = WideCharToMultiByte(CP_UTF8, 0, (wchar_t*)s.c_str(), -1,
	                                  nullptr, 0, nullptr, nullptr);
	std::unique_ptr<char[]> out{new char[size + 1]};
	WideCharToMultiByte(CP_UTF8, 0, (wchar_t*)s.c_str(), -1, out.get(),
	                    static_cast<int>(size + 1), nullptr, nullptr);
	return out.get();
}
#endif

template <typename NativeString>
path_add_cref<std::string> printable(NativeString const& native_path) {
	if constexpr (std::is_same_v<std::string, NativeString>) {
		return native_path;
	} else {
		return narrow(native_path);
	}
}

path_add_cref<std::string> printable(arch::fs::path const& path) {
	return printable(path.native());
}

namespace arch {
	static bool error(char const* path, char const* message) {
		fprintf(stderr, "list: %s: %s\n", path, message);
		return false;
	}

	bool unpack(char const* path, dirent::dirnode& root) {
		auto file = io::file::open(path);
		if (!file) return error(path, "file not found");

		base::archive::ptr archive{};
		auto const result = open(std::move(file), archive);
		switch (result) {
			case open_status::compression_damaged:
				return error(path, "file compression damaged");
			case open_status::archive_damaged:
				return error(path, "archive damaged");
			case open_status::archive_unknown:
				return error(path, "unrecognized archive");
			case open_status::ok:
				break;
		}

		if (!archive) return error(path, "unknown internal issue");
		root.append(*archive);
		return true;
	}
}  // namespace arch

int main(int argc, char** argv) {
	if (argc < 2) {
		fprintf(stderr, "list <arch> [<arch> ...]\n");
		return 1;
	}

	setlocale(LC_ALL, "");

	arch::dirent::dirnode root{};

	for (int arg = 1; arg < argc; ++arg) {
		if (!arch::unpack(argv[arg], root)) return 1;
	}

	auto const now_ish =
	    arch::base::io::to_file_clock(std::chrono::system_clock::now());
	root.minimize();
	root.print(stdout, now_ish);
}
