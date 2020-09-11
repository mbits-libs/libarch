// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/io/stream.hh>
#include <cstddef>

namespace arch::base::io {
	struct seekable : stream {
		seekable();
		~seekable();

		virtual std::size_t seek(std::size_t pos) = 0;
		virtual std::size_t seek_end() = 0;
		virtual std::size_t tell() const = 0;

		using ptr = std::unique_ptr<seekable>;
	};
}  // namespace arch::base::io

namespace arch::io {
	using base::io::seekable;
}  // namespace arch::io
