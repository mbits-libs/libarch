// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <zlib.h>
#include <arch/base/decompressor.hh>

namespace arch::zlib {
	class decompressor final : public base::decompressor {
	public:
		explicit decompressor(int wbits);
		~decompressor();
		bool eof() const noexcept final { return eof_; }
		std::pair<size_t, size_t> decompress(std::span<std::byte> input,
		                                     std::span<std::byte> output) final;

	private:
		bool eof_{false};
		int is_initialised_{false};
		z_stream z_{};
	};
}  // namespace arch::zlib
