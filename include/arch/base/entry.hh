// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/fs.hh>
#include <arch/base/io/stream.hh>

namespace arch::base {
	struct entry {
		virtual ~entry();
		virtual fs::path const& filename() const = 0;
		virtual io::stream::ptr file() const = 0;
		virtual io::status const& file_status() const = 0;
		virtual io::status const& linked_status() const = 0;
		virtual fs::path const& linkname() const = 0;

		using ptr = std::unique_ptr<entry>;
	};

	using entry_mixin = io::status_mixin<entry>;
	using dos_entry_mixin = io::dos_status_mixin<entry>;
}  // namespace arch::base
