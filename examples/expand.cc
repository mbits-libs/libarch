// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/archive.hh>
#include <arch/io/file.hh>
#include <vector>

namespace arch {
	static bool error(char const* path, char const* message) {
		fprintf(stderr, "expand: %s: %s\n", path, message);
		return false;
	}

	static void copy_attributes(fs::path const& filename,
	                            io::status const& status) {
		std::error_code ig1{}, ig2{};
		fs::permissions(filename, status.permissions,
		                fs::perm_options::replace | fs::perm_options::nofollow,
		                ig1);
		fs::last_write_time(filename, status.last_write_time, ig2);
	}

	static bool expand_file(base::entry const& entry) {
		auto const& filename = entry.filename();
		auto const& status = entry.file_status();
		constexpr size_t expected_size = 10 * 1024 * 1024;

		auto input = entry.file();
		if (!input) return false;

		auto output = io::file::open(filename, "wb");
		if (!output) return false;

		std::vector<std::byte> buffer(expected_size);
		auto view = std::span{buffer.data(), buffer.size()};

		bool copied = true;

		auto size = status.size;
		while (size) {
			auto chunk = buffer.size();
			if (chunk > size) chunk = size;

			auto const extracted = input->read(view.subspan(0, chunk));
			if (extracted < chunk) {
				copied = false;
				break;
			}

			auto written = output->write(view.subspan(0, extracted));
			if (written < extracted) {
				copied = false;
				break;
			}

			size -= chunk;
		}

		input.reset();
		output.reset();

		if (!copied) {
			std::error_code ignore;
			fs::remove(filename, ignore);
			return error(filename.string().c_str(), "cannot extract file");
		}

		copy_attributes(filename, status);
		return true;
	}

	static bool make_directory(base::entry const& entry) {
		auto const& dirname = entry.filename();
		auto const& status = entry.file_status();

		std::error_code ec;
		fs::create_directories(dirname, ec);
		if (ec) {
			return error(dirname.string().c_str(), ec.message().c_str());
		}

		copy_attributes(dirname, status);
		return true;
	}

	static bool make_link(base::entry const& entry) {
		auto const& name = entry.filename();
		auto const& status = entry.file_status();
		auto const& linkname = entry.linkname();

		std::error_code ec;
		fs::create_hard_link(linkname, name, ec);

		if (ec) {
			error(name.string().c_str(), ec.message().c_str());
			fprintf(stderr, "expand: while making hard link to: %s\n",
			        linkname.string().c_str());
		}

		copy_attributes(name, status);
		return true;
	}

	static bool make_symlink(base::entry const& entry) {
		auto const& name = entry.filename();
		auto const& status = entry.file_status();
		auto const& linked_type = entry.linked_status().type;
		auto const& linkname = entry.linkname();

		std::error_code ec;
		if (linked_type == fs::file_type::directory) {
			fs::create_directory_symlink(linkname, name, ec);
		} else {
			fs::create_symlink(linkname, name, ec);
		}

		if (ec) {
			error(name.string().c_str(), ec.message().c_str());
			fprintf(stderr, "expand: while making symlink to: %s\n",
			        linkname.string().c_str());
		}

		copy_attributes(name, status);
		return true;
	}

	static bool expand(base::entry const& entry) {
		auto const& status = entry.file_status();
		switch (status.type) {
			case fs::file_type::regular:
				return expand_file(entry);
			case fs::file_type::directory:
				return make_directory(entry);
			case fs::file_type::symlink:
				if (status.hardlink) return make_link(entry);
				return make_symlink(entry);
			default:
				break;
		}
		return true;
	}

	bool unpack(char const* path) {
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

		auto const entry_count = archive->count();
		for (std::size_t ndx = 0; ndx < entry_count; ++ndx) {
			auto entry = archive->entry(ndx);
			if (entry) {
				if (!expand(*entry)) return false;
			}
		}
		return true;
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