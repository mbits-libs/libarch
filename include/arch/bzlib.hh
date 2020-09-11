// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <bzlib.h>
#ifdef _WIN32
#undef min
#undef max
#endif
#include <arch/base/decompressor.hh>

namespace arch::bzlib {
	class decompressor final : public base::decompressor {
	public:
		decompressor();
		~decompressor();
		bool eof() const noexcept final { return eof_; }
		std::pair<size_t, size_t> decompress(std::span<std::byte> input,
		                                     std::span<std::byte> output) final;

	private:
		bool eof_{false};
		int is_initialised_{false};
		bz_stream bz_{};
	};
}  // namespace arch::bzlib
