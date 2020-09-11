// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/fs.hh>
#include <cstdint>
#include <memory>
#include <span>

namespace arch::base::io {
	struct status {
		uintmax_t size{};
		fs::file_time_type last_write_time{};
		fs::file_type type{fs::file_type::none};
		fs::perms permissions{fs::perms::none};
		bool hardlink{false};
	};

	std::chrono::system_clock::time_point to_system_clock(
	    fs::file_time_type last_write_time);

	fs::file_time_type to_file_clock(
	    std::chrono::system_clock::time_point last_write_time);

	inline fs::file_time_type to_file_clock(time_t last_write_time) {
		return to_file_clock(
		    std::chrono::system_clock::from_time_t(last_write_time));
	}

	struct stream {
		stream();
		virtual ~stream();

		virtual void close() = 0;
		virtual io::status const& file_status() const = 0;
		virtual io::status const& linked_status() const = 0;
		virtual fs::path const& linkname() const = 0;
		virtual std::size_t read(std::span<std::byte>);

		using ptr = std::unique_ptr<stream>;
	};

	template <typename Next = stream>
	struct status_mixin : Next {
		status_mixin() = default;
		status_mixin(io::status const& file_status,
		             io::status const& linked_status,
		             fs::path const& linkname)
		    : file_status_{file_status}
		    , linked_status_{linked_status}
		    , linkname_{linkname} {}

		io::status const& file_status() const final { return file_status_; }
		io::status const& linked_status() const final { return linked_status_; }
		fs::path const& linkname() const final { return linkname_; }

	private:
		io::status file_status_;
		io::status linked_status_;
		fs::path linkname_;
	};

	template <typename Next = stream>
	struct dos_status_mixin : Next {
		dos_status_mixin() = default;
		explicit dos_status_mixin(io::status const& status) : status_{status} {}

		io::status const& file_status() const final { return status_; }
		io::status const& linked_status() const final { return status_; }
		fs::path const& linkname() const final {
			static fs::path const null{};
			return null;
		}

	private:
		io::status status_;
	};

	using stream_mixin = status_mixin<>;
	using dos_stream_mixin = dos_status_mixin<>;
}  // namespace arch::base::io

namespace arch::io {
	using base::io::dos_status_mixin;
	using base::io::dos_stream_mixin;
	using base::io::status;
	using base::io::status_mixin;
	using base::io::stream;
	using base::io::stream_mixin;
}  // namespace arch::io
