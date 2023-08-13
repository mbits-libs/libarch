// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/io/file.hh>
#include <arch/unpacker.hh>
#include <array>

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

	bool unpacker::unpack(fs::path const& path) const {
		using namespace arch;

		auto file = io::file::open(path);
		if (!file) {
			on_error(path, "cannot open");
			return false;
		}

		base::archive::ptr archive{};
		auto const result = open(std::move(file), archive);
		switch (result) {
			case open_status::compression_damaged:
				on_error(path, "file compression damaged");
				return false;
			case open_status::archive_damaged:
				on_error(path, "archive damaged");
				return false;
			case open_status::archive_unknown:
				on_error(path, "unrecognized archive");
				return false;
			case open_status::ok:
				break;
		}

		if (!archive) {
			on_error(path, "unknown internal issue");
			return false;
		}

		return unpack(*archive);
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

	struct expander {
		static constexpr size_t expected_size = 32 * 1024;
		std::filesystem::path name;

		static base::io::stream::ptr open_entry(base::entry const& entry) {
			return entry.file();
		}

		bool ensure_directory(unpacker const& self) {
			if (auto const parent = name.parent_path(); !parent.empty()) {
				std::error_code ec;
				fs::create_directories(parent, ec);
				if (ec) {
					self.on_error(parent, ec.message().c_str());
					return false;
				}
			}
			return true;
		}

		std::unique_ptr<arch::io::file> open_dst() const {
			return io::file::open(name, "wb");
		}

		bool expand_entry(base::io::stream& src,
		                  io::file& dst,
		                  base::io::status const& status) {
			thread_local std::array<std::byte, expected_size> buffer{};
			auto view = std::span{buffer.data(), buffer.size()};
			auto size = status.size;
			bool copied = true;
			while (size) {
				auto chunk = buffer.size();
				if (chunk > size) chunk = size;

				auto const extracted = src.read(view.subspan(0, chunk));
				if (extracted < chunk) {
					copied = false;
					break;
				}

				auto written = dst.write(view.subspan(0, extracted));
				if (written < extracted) {
					copied = false;
					break;
				}

				size -= chunk;
			}

			return copied;
		}

		enum result { ok, copy_issues, setup_issues };
		result expand(unpacker const& self, base::entry const& entry) {
			auto input = expander::open_entry(entry);
			if (!input) return setup_issues;

			if (!ensure_directory(self)) return setup_issues;

			auto output = open_dst();
			if (!output) return setup_issues;

			auto const copied =
			    expand_entry(*input, *output, entry.file_status());
			return copied ? ok : copy_issues;
		}

		void clean(unpacker const& self) const {
			std::error_code ignore;
			fs::remove(name, ignore);
			self.on_error(name, "cannot extract file");
		}

		void copy_attributes(base::io::status const& status) {
			arch::copy_attributes(name, status);
		}
	};

	bool unpacker::expand_file(base::entry const& entry) const {
		expander exp{.name = filename(entry)};
		if (exp.name.empty()) return true;

		auto const result = exp.expand(*this, entry);
		if (result == expander::ok) {
			exp.copy_attributes(entry.file_status());
			return true;
		}

		if (result == expander::copy_issues) exp.clean(*this);
		return false;
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
