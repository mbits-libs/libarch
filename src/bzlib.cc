// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include "arch/bzlib.hh"
#include <limits>
#include "decompress_impl.hh"

namespace arch::impl {
	template <>
	struct stream_traits<bz_stream> : stream_traits_base<bz_stream> {
		static inline int decompress(bz_stream* stream) noexcept {
			return BZ2_bzDecompress(stream);
		}

		static inline constexpr bool ok(int ret) noexcept {
			return ret == BZ_OK;
		}
		static inline constexpr bool stream_end(int ret) noexcept {
			return ret == BZ_STREAM_END;
		}
	};
}  // namespace arch::impl

namespace arch::bzlib {
	decompressor::decompressor() {
		auto const err = BZ2_bzDecompressInit(&bz_, 0, 0);
		is_initialised_ = err == BZ_OK;
	}

	decompressor::~decompressor() {
		if (is_initialised_) BZ2_bzDecompressEnd(&bz_);
	}

	std::pair<size_t, size_t> decompressor::decompress(
	    std::span<std::byte> input,
	    std::span<std::byte> output) {
		return impl::decompress(input, output, bz_);
	}
}  // namespace arch::bzlib
