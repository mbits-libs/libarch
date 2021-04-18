// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/io/file.hh>
#include <arch/unpacker.hh>

namespace arch {
	namespace {
		void copy_attributes(fs::path const& filename,
		                     io::status const& status) {
			std::error_code ig1{}, ig2{};
			fs::permissions(
			    filename, status.permissions,
			    fs::perm_options::replace | fs::perm_options::nofollow, ig1);
			fs::last_write_time(filename, status.last_write_time, ig2);
		}
	}  // namespace

	unpacker::~unpacker() = default;

	fs::path unpacker::modify_path(fs::path const& filename) const {
		return filename;
	}

	void unpacker::on_error(fs::path const& filename, char const* msg) const {
		fprintf(stderr, "%s: error: %s\n", filename.string().c_str(), msg);
	}
	void unpacker::on_note(char const* msg) const {
		fprintf(stderr, "  note: %s\n", msg);
	}

	fs::path unpacker::filename(base::entry const& entry) const {
		auto name = modify_path(entry.filename());
		if (rootdir_.empty() || name.is_absolute()) {
			return name;
		}
		return rootdir_ / name;
	}

	bool unpacker::unpack(base::archive& archive) const {
		auto const entry_count = archive.count();
		for (std::size_t ndx = 0; ndx < entry_count; ++ndx) {
			auto entry = archive.entry(ndx);
			if (!entry) continue;
			if (!expand(*entry)) return false;
		}
		return true;
	}

	bool unpacker::expand(base::entry const& entry) const {
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

	bool unpacker::expand_file(base::entry const& entry) const {
		auto const name = filename(entry);
		if (name.empty()) return true;

		auto const& status = entry.file_status();
		constexpr size_t expected_size = 10 * 1024 * 1024;

		auto input = entry.file();
		if (!input) return false;

		if (auto const parent = name.parent_path(); !parent.empty()) {
			std::error_code ec;
			fs::create_directories(parent, ec);
			if (ec) {
				on_error(parent, ec.message().c_str());
				return false;
			}
		}

		auto output = io::file::open(name, "wb");
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
			fs::remove(name, ignore);
			on_error(name, "cannot extract file");
			return false;
		}

		copy_attributes(name, status);
		return true;
	}

	bool unpacker::make_directory(base::entry const& entry) const {
		auto const name = filename(entry);
		if (name.empty()) return true;

		auto const& status = entry.file_status();

		std::error_code ec;
		fs::create_directories(name, ec);
		if (ec) {
			on_error(name, ec.message().c_str());
			return false;
		}

		copy_attributes(name, status);
		return true;
	}

	bool unpacker::make_link(base::entry const& entry) const {
		auto const name = filename(entry);
		if (name.empty()) return true;

		auto const& status = entry.file_status();
		auto const& linkname = entry.linkname();

		if (auto const parent = name.parent_path(); !parent.empty()) {
			std::error_code ec;
			fs::create_directories(parent, ec);
			if (ec) {
				on_error(parent, ec.message().c_str());
				return false;
			}
		}

		std::error_code ec;
		fs::create_hard_link(linkname, name, ec);

		if (ec) {
			on_error(name, ec.message().c_str());
			on_note(
			    ("while making hard link to: " + linkname.string()).c_str());

			return false;
		}

		copy_attributes(name, status);
		return true;
	}

	bool unpacker::make_symlink(base::entry const& entry) const {
		auto const name = filename(entry);
		if (name.empty()) return true;

		auto const& status = entry.file_status();
		auto const& linked_type = entry.linked_status().type;
		auto const& linkname = entry.linkname();

		if (auto const parent = name.parent_path(); !parent.empty()) {
			std::error_code ec;
			fs::create_directories(parent, ec);
			if (ec) {
				on_error(parent, ec.message().c_str());
				return false;
			}
		}

		std::error_code ec;
		if (linked_type == fs::file_type::directory) {
			fs::create_directory_symlink(linkname, name, ec);
		} else {
			fs::create_symlink(linkname, name, ec);
		}

		if (ec) {
			on_error(name, ec.message().c_str());
			on_note(("while making symlink to: " + linkname.string()).c_str());
			return false;
		}

		copy_attributes(name, status);
		return true;
	}

	fs::path skip_root::modify_path(fs::path const& filename) const {
		auto cur = filename.begin();
		auto end = filename.end();
		auto root_cur = root_.begin();
		auto root_end = root_.end();

		fs::path result{};

		while (root_cur != root_end && cur != end && *root_cur == *cur) {
			++root_cur;
			++cur;
		}

		if (root_cur != root_end || cur == end) return result;
		result = *cur++;

		while (cur != end)
			result /= *cur++;

		return result;
	}
}  // namespace arch
