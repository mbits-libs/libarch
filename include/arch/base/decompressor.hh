// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cstdint>
#include <memory>
#include <span>

namespace arch::base {
	struct decompressor {
		virtual ~decompressor();
		virtual bool eof() const noexcept = 0;
		virtual std::pair<size_t, size_t> decompress(
		    std::span<std::byte> input,
		    std::span<std::byte> output) = 0;

		using ptr = std::unique_ptr<decompressor>;
	};
}  // namespace arch::base
