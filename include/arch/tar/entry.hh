// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/entry.hh>
#include <arch/base/io/seekable.hh>

namespace arch::tar {
	class entry final : public base::entry_mixin {
	public:
		entry(fs::path const& filename,
		      fs::path const& link,
		      io::seekable* proxied,
		      size_t offset,
		      io::status const& file_status,
		      io::status const& status);

		fs::path const& filename() const final;
		io::stream::ptr file() const final;

	private:
		fs::path filename_;
		io::seekable* proxied_;
		size_t offset_;
	};
}  // namespace arch::tar
