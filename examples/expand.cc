// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/io/file.hh>
#include <arch/unpacker.hh>
#include <vector>

namespace arch {
	class expand_unpacker : public unpacker {
	public:
		expand_unpacker() : unpacker(fs::path{}) {}
		void on_error(fs::path const& filename,
		              char const* msg) const override {
			fprintf(stderr, "expand: %s: %s\n", filename.string().c_str(), msg);
		}
		void on_note(char const* msg) const override {
			fprintf(stderr, "        note: %s\n", msg);
		}
	};

	bool unpack(fs::path const& path) {
		expand_unpacker unp{};

		auto file = io::file::open(path);
		if (!file) {
			unp.on_error(path, "file not found");
			return false;
		}

		base::archive::ptr archive{};
		auto const result = open(std::move(file), archive);
		switch (result) {
			case open_status::compression_damaged:
				unp.on_error(path, "file compression damaged");
				return false;
			case open_status::archive_damaged:
				unp.on_error(path, "archive damaged");
				return false;
			case open_status::archive_unknown:
				unp.on_error(path, "unrecognized archive");
				return false;
			case open_status::ok:
				break;
		}

		if (!archive)
			return unp.on_error(path, "unknown internal issue"), false;

		return unp.unpack(*archive);
	}
}  // namespace arch

int main(int argc, char** argv) {
	if (argc < 2) {
		fprintf(stderr, "expand <arch> [<arch> ...]\n");
		return 1;
	}

	for (int arg = 1; arg < argc; ++arg) {
		if (!arch::unpack(argv[arg])) return 1;
	}
}