// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include "arch/base/io/seekable.hh"

namespace arch::base::io {
	struct writeable : seekable {
		writeable();
		~writeable();

		virtual std::size_t write(std::span<const std::byte>) = 0;

		using ptr = std::unique_ptr<writeable>;
	};
}  // namespace arch::base::io

namespace arch::io {
	using base::io::writeable;
}  // namespace arch::io
