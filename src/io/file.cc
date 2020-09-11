// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include "arch/io/file.hh"
#include <fcntl.h>
#include <sys/stat.h>

namespace arch::io {
	namespace {
		io::status make_file_status(fs::path const& path) {
			std::error_code ec{};

			auto const stat = fs::symlink_status(path, ec);
			if (ec) return {};

			auto const size =
			    fs::is_symlink(stat) ? 0 : fs::file_size(path, ec);
			if (ec) return {};

			auto const mtime = fs::last_write_time(path, ec);
			if (ec) return {};

			return {size, mtime, stat.type(), stat.permissions()};
		}

		io::status make_linked_status(fs::path const& path_) {
			auto path = path_;

			std::error_code ec{};
			{
				auto real = fs::read_symlink(path, ec);
				if (!ec) path = std::move(real);
			}

			auto const stat = fs::status(path, ec);

			auto const size = fs::file_size(path, ec);
			if (ec) return {};

			auto const mtime = fs::last_write_time(path, ec);
			if (ec) return {};

			return {size, mtime, stat.type(), stat.permissions()};
		}

		fs::path make_linkname(fs::path const& path) {
			std::error_code ec{};
			auto real = fs::read_symlink(path, ec);
			if (ec) return {};

			return real;
		}
	}  // namespace

	std::unique_ptr<file> file::open(fs::path const& path, const char* mode) {
		auto handle = file::fopen(path, mode);
		if (!handle) return {};
		return std::make_unique<file>(
		    private_tag{}, std::move(handle), make_file_status(path),
		    make_linked_status(path), make_linkname(path));
	}

	file::file(private_tag,
	           fptr&& file,
	           io::status const& file_status,
	           io::status const& linked_status,
	           fs::path&& linkname)
	    : io::status_mixin<writeable>{file_status, linked_status,
	                                  std::move(linkname)}
	    , file_{std::move(file)} {}
	file::file() = default;
	file::~file() { close(); }

	void file::close() { file_.reset(); }

	std::size_t file::write(std::span<const std::byte> bytes) {
		return std::fwrite(bytes.data(), 1, bytes.size(), file_.get());
	}

	std::size_t file::read(std::span<std::byte> bytes) {
		return std::fread(bytes.data(), 1, bytes.size(), file_.get());
	}

	std::size_t file::seek(std::size_t pos) {
		constexpr auto max_long = std::numeric_limits<long>::max();
		constexpr auto max_long_s = static_cast<size_t>(max_long);
		std::fseek(file_.get(), 0, SEEK_SET);
		while (pos > max_long_s) {
			std::fseek(file_.get(), max_long, SEEK_CUR);
			pos -= max_long_s;
		}

		if (pos) std::fseek(file_.get(), static_cast<long>(pos), SEEK_CUR);
		return tell();
	}

	std::size_t file::seek_end() {
		std::fseek(file_.get(), 0, SEEK_END);
		return tell();
	}

	std::size_t file::tell() const {
		auto const result = std::ftell(file_.get());
		if (result < 0) return 0;
		return static_cast<size_t>(result);
	}

	file::fptr file::fopen(std::string const& utf8path, const char* mode) {
#ifdef WIN32
		FILE* result{};
		[[maybe_unused]] auto err = fopen_s(&result, utf8path.c_str(), mode);
		return fptr{result};
#else
		return fptr{std::fopen(utf8path.c_str(), mode)};
#endif
	}

#ifdef WIN32
	file::fptr file::fopen(std::wstring const& path, const char* mode) {
		std::wstring wmode{};

		if (!mode) mode = "rb";
		auto const len = std::strlen(mode);
		wmode.reserve(len);
		std::transform(mode, mode + len, std::back_inserter(wmode),
		               [](auto const c) { return c; });

		FILE* result{};
		[[maybe_unused]] auto err =
		    _wfopen_s(&result, path.c_str(), wmode.c_str());
		return fptr{result};
	}
#endif
}  // namespace arch::io
