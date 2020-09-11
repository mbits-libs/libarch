// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/entry.hh>
#include <arch/base/fs.hh>
#include <arch/base/io/seekable.hh>

namespace arch::base {
	struct archive {
		virtual ~archive();
		virtual bool open(io::seekable::ptr) = 0;
		virtual void close() = 0;
		virtual size_t count() const = 0;
		virtual base::entry::ptr entry(size_t) const = 0;

		using ptr = std::unique_ptr<archive>;
	};
}  // namespace arch::base
