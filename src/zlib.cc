// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include "arch/zlib.hh"
#include <limits>
#include "decompress_impl.hh"

namespace arch::impl {
	template <>
	struct stream_traits<z_stream> : stream_traits_base<z_stream> {
		static inline int decompress(z_stream* stream) noexcept {
			return inflate(stream, Z_SYNC_FLUSH);
		}

		static inline constexpr bool ok(int ret) noexcept {
			return ret == Z_OK;
		}
		static inline constexpr bool stream_end(int ret) noexcept {
			return ret == Z_STREAM_END;
		}
	};
}  // namespace arch::impl

namespace arch::zlib {
	decompressor::decompressor(int wbits) {
		auto const err = inflateInit2(&z_, wbits);
		is_initialised_ = err == Z_OK;
	}

	decompressor::~decompressor() {
		if (is_initialised_) inflateEnd(&z_);
	}

	std::pair<size_t, size_t> decompressor::decompress(
	    std::span<std::byte> input,
	    std::span<std::byte> output) {
		return impl::decompress(input, output, z_);
	}
}  // namespace arch::zlib
